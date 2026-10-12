#pragma once

#include <cydaw/engine/SoundModel.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cydaw::engine {

// The sampler: recorded notes played back as they are, each over the keys and
// velocities it is mapped to, at the pitch the key asks for. A sample map is
// a list of zones (SFZ's regions); every zone a note falls in plays, stacked,
// so velocity layers, key splits and layered sounds are the same thing.
//
// The audio of a file is decoded off the audio thread (the host's SampleStore)
// into a SampleData shared by every zone and every track that plays it. A
// note that starts before its sample is decoded waits for it, then comes in
// where it would be by then, faded in over a few milliseconds.

// One decoded file: 16-bit frames, interleaved, at most two channels. A
// loader fills `pcm`, then sets `ready` (release); a voice reads only after
// seeing it (acquire), so nothing is locked on the audio thread.
struct SampleData {
    std::string path;
    int channels { 1 };
    std::int64_t frames {};
    double sampleRate { 48'000.0 };
    // pcm * scale is the sample's value: the file normalised into 16 bits,
    // its own level kept here.
    float scale { 1.0f / 32'767.0f };
    std::vector<std::int16_t> pcm;
    // A loop the file itself carries (a WAV's sampler chunk); -1: none.
    std::int64_t fileLoopStart { -1 };
    std::int64_t fileLoopEnd { -1 };
    std::atomic<bool> ready { false };
    std::atomic<bool> failed { false };

    [[nodiscard]] bool playable() const noexcept { return ready.load(std::memory_order_acquire) && frames > 0; }
};
using SampleDataRef = std::shared_ptr<SampleData>;

struct SamplerZone {
    SampleDataRef sample;
    int rootNote { 60 };
    int lowKey {};
    int highKey { 127 };
    int lowVelocity { 1 };
    int highVelocity { 127 };
    float tuneCents {};
    float gainDb {};
    // -1 left .. 1 right.
    float pan {};
    // Frames of the file. end -1: its last frame.
    std::int64_t start {};
    std::int64_t end { -1 };
    // Held between these while it loops; -1: no loop.
    std::int64_t loopStart { -1 };
    std::int64_t loopEnd { -1 };
    // Frames before loopEnd blended into the frames before loopStart.
    int loopCrossfade {};
    // Round robin (SFZ's seq_length / seq_position): with a length above 1,
    // the zone plays one note in `sequenceLength`, the `sequencePosition`th.
    int sequenceLength { 1 };
    int sequencePosition { 1 };

    [[nodiscard]] bool loops() const noexcept { return loopStart >= 0 && loopEnd > loopStart + 1; }
};

struct SamplerMap {
    std::string name;
    std::vector<SamplerZone> zones;
    // Which round of the round robin the next note plays; every voice of
    // every track playing this map counts on it.
    mutable std::atomic<std::uint32_t> round {};
};

// Shared by the plan's copies (one per voice per block); the zones are fixed
// once made, only the samples' data arrives later.
using SamplerMapRef = std::shared_ptr<const SamplerMap>;

// How a map is played: the sampler card's knobs, in this order in its payload
// (NativeDspNodeParameters::sampler).
enum class SamplerSetting : std::uint8_t {
    transpose,         // semitones, -48..48
    fine,              // cents, -100..100
    start,             // ms skipped at the start of every sample, 0..2000
    velocityDb,        // the whole velocity range's level, 0..48 dB
    velocityCrossfade, // 1: the two velocity layers around the note blended
    loop,              // 0 none; 1 the zones' loops, held to the end; 2 looped while the key is down
    keyTrack,          // 1: the pitch follows the key; 0: every key plays the root (drums)
    reverse,           // 1: backwards (loops ignored)
    width,             // stereo width, 0 mono .. 2
    oneShot,           // 1: plays to its end whatever the key does
    count
};
inline constexpr std::size_t samplerSettingCount = static_cast<std::size_t>(SamplerSetting::count);

inline constexpr std::array<SoundModelSettingInfo, samplerSettingCount> samplerSettings { {
    { "transpose", "sampler.transpose", "sampler.parameter.transpose", "st", -48.0f, 48.0f, 0.0f },
    { "fine", "sampler.fine", "sampler.parameter.fine", "c", -100.0f, 100.0f, 0.0f },
    { "start", "sampler.start", "sampler.parameter.start", "ms", 0.0f, 2'000.0f, 0.0f },
    { "velocity-db", "sampler.velocityDb", "sampler.parameter.velocityDb", "dB", 0.0f, 48.0f, 24.0f },
    { "velocity-crossfade", "sampler.velocityCrossfade", "sampler.parameter.velocityCrossfade", "", 0.0f, 1.0f, 0.0f },
    { "loop", "sampler.loop", "sampler.parameter.loop", "", 0.0f, 2.0f, 1.0f },
    { "key-track", "sampler.keyTrack", "sampler.parameter.keyTrack", "", 0.0f, 1.0f, 1.0f },
    { "reverse", "sampler.reverse", "sampler.parameter.reverse", "", 0.0f, 1.0f, 0.0f },
    { "width", "sampler.width", "sampler.parameter.width", "", 0.0f, 2.0f, 1.0f },
    { "one-shot", "sampler.oneShot", "sampler.parameter.oneShot", "", 0.0f, 1.0f, 0.0f },
} };

using SamplerValues = std::array<float, 16>;

[[nodiscard]] constexpr SamplerValues defaultSamplerValues() noexcept
{
    SamplerValues values {};
    for (std::size_t index = 0; index < samplerSettingCount; ++index)
        values[index] = samplerSettings[index].defaultValue;
    return values;
}

[[nodiscard]] constexpr int samplerSettingIndex(const std::string_view key) noexcept
{
    for (std::size_t index = 0; index < samplerSettingCount; ++index)
        if (samplerSettings[index].key == key || samplerSettings[index].suffix == key)
            return static_cast<int>(index);
    return -1;
}

[[nodiscard]] inline float samplerValue(const SamplerValues& values, const SamplerSetting setting) noexcept
{
    return values[static_cast<std::size_t>(setting)];
}

// The zones a note plays and their weights. Without the crossfade: every
// zone whose keys and velocities hold the note (round robin applied). With
// it: of the zones holding the key, the two velocity layers around the
// velocity, blended at equal power. `round` is the note's round-robin count.
struct SamplerZoneChoice {
    static constexpr int capacity = 4;
    std::array<int, capacity> zones { -1, -1, -1, -1 };
    std::array<double, capacity> weights {};
    int count {};
};
[[nodiscard]] SamplerZoneChoice chooseSamplerZones(const SamplerMap& map, int midiNote, double velocity,
                                                   bool crossfade, std::uint32_t round) noexcept;

// One voice playing a map, in stereo. Nothing here allocates.
class SamplerVoice final {
public:
    void reset() noexcept;
    // The map is read again every block from the plan (an edit may replace
    // it); the zones chosen here are kept by their index.
    void start(const SamplerMap& map, const SamplerValues& values, double midiNote, double velocity,
               double sampleRate) noexcept;
    // Key up: a loop held only while the key is down lets go.
    void release() noexcept;
    [[nodiscard]] bool active() const noexcept { return active_; }
    void next(const SamplerMap* map, const SamplerValues& values, double& left, double& right) noexcept;

private:
    struct Lane {
        int zone { -1 };
        const SampleData* sample {};
        double position {};
        double increment {};
        double semitones {};
        double gainLeft {};
        double gainRight {};
        // Its sample not decoded yet: the output samples waited so far.
        bool pending {};
        double waited {};
        // 0..1 while a late lane comes in.
        double fadeIn { 1.0 };
        bool looping {};
        bool reversed {};
        bool done { true };
    };
    // Where a lane starts reading once its sample is there, `elapsed` output
    // samples after the note-on (0: on time).
    void place(Lane& lane, const SamplerZone& zone, const SampleData& sample, double elapsed) const noexcept;

    std::array<Lane, SamplerZoneChoice::capacity> lanes_ {};
    int laneCount_ {};
    bool active_ {};
    // The note-on's settings, for lanes placed late.
    double rate_ { 48'000.0 };
    double skippedMs_ {};
    int loopMode_ {};
    bool reversed_ {};
    // The loop setting at the note-on said: loop while the key is down.
    bool letGoOnRelease_ {};
    bool released_ {};
};

} // namespace cydaw::engine

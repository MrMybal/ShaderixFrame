#pragma once

#include <cydaw/engine/AudioBuffer.h>
#include <cydaw/engine/Transport.h>

#include <cstdint>
#include <span>

namespace cydaw::engine {

struct ProcessSpec {
    double sampleRate { 48'000.0 };
    std::uint32_t maximumBlockSize { 512 };
    std::uint32_t channelCount { 2 };

    [[nodiscard]] bool operator==(const ProcessSpec&) const = default;
};

struct ProcessContext {
    TransportSnapshot transport;
};

struct AudioInputBusView {
    std::uint64_t portId {};
    AudioBlockView audio;
};

// Optional processor interface for nodes whose authored input sockets must
// remain distinct at render time (mixers, sidechains and multibus effects).
// The bus array and all backing audio storage are prepared off the audio
// thread by AudioRenderPlanBuilder.
class MultiBusAudioProcessor {
public:
    virtual ~MultiBusAudioProcessor() = default;
    virtual void processAudioBuses(
        std::span<const AudioInputBusView> inputs, AudioBlockView output,
        const ProcessContext& context) noexcept = 0;
};

class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;
    virtual void prepare(const ProcessSpec& spec) = 0;
    virtual void reset() noexcept = 0;
    virtual void process(AudioBlockView input,
                         AudioBlockView output,
                         const ProcessContext& context) noexcept = 0;
};

} // namespace cydaw::engine

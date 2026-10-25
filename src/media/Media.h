#pragma once
#include "adapters/CyDawAudio.h"
#include "core/Project.h"
#include "media/Loudness.h"
#include <array>
#include <cydaw/engine/AudioAnalysis.h>
#include <functional>
#include <memory>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmsystem.h>
#include <windows.h>
#include <wrl/client.h>

namespace sf {
template <class T> using Com = Microsoft::WRL::ComPtr<T>;
void check(HRESULT hr, const char *what);
struct Image {
    unsigned width = 0, height = 0;
    std::vector<unsigned char> pixels;
    std::vector<float> rgba; // optional original precision, straight RGBA
    std::string colorSpace = "srgb";
};
struct FloatImage {
    unsigned width = 0, height = 0;
    std::vector<float> rgba;
    std::string colorSpace = "linear709";
};
void writePng16(const FloatImage &image, const std::string &path);
void writeExr(const FloatImage &image, const std::string &path);
Image readExr(const std::string &path);
struct PcmMapping {
    HANDLE file = INVALID_HANDLE_VALUE, mapping = nullptr;
    const float *data = nullptr;
    const void *view = nullptr;
    size_t count = 0;
    ~PcmMapping();
};
struct MediaAsset {
    std::string path, error;
    bool video = false, image = false, hasAudio = false;
    bool loading = false;
    double duration = 0, lastTime = -1, lastRequest = -1;
    unsigned seeks = 0; // repositionings of the decoder, for diagnostics and tests
    // Decoded video buffers are often taller than the picture (H.264 codes 1080 lines as 1088):
    // frames are cropped to the display aperture inside the coded size.
    unsigned codedWidth = 0, codedHeight = 0;
    unsigned cropX = 0, cropY = 0;
    Image frame;
    Image thumbnail;
    std::vector<float> pcm; // interleaved stereo float, 48 kHz
    std::unique_ptr<PcmMapping> diskPcm;
    size_t samples() const { return diskPcm ? diskPcm->count : pcm.size(); }
    float sample(size_t n) const { return diskPcm ? diskPcm->data[n] : pcm[n]; }
    std::vector<float> peaks;
    float peakCeiling = 1e-6f;
    Com<IMFSourceReader> reader;
    void open(const std::string &source, const std::filesystem::path &cacheDirectory = {},
              bool decodeAudio = true);
    const Image &at(double seconds);
};
class MediaCache {
    struct Background;
    std::unique_ptr<Background> background;
    std::map<std::string, std::unique_ptr<MediaAsset>> assets;
    AudioEffects effects;
    std::filesystem::path diskCacheDirectory;
    MediaAsset &getBlocking(const std::string &source);
    struct Delay {
        std::vector<float> samples;
        size_t cursor = 0;
        void apply(std::vector<float> &data, unsigned frames);
    };
    std::map<std::tuple<Id, Id, int>, Delay> delays;
    struct Stretch {
        int64_t grain = -1;
        double speed = 0, in = 0, previous = 0, current = 0;
        int mode = 0;
    };
    std::map<Id, Stretch> stretches;
    void mixEngine(const Project &project, double time, unsigned frames, std::vector<float> &out);
    struct Nested {
        std::string identity;
        Project project;
        std::unique_ptr<MediaCache> media;
        double audioStart = -1;
        std::vector<float> audio;
    };
    std::map<Id, Nested> nested;
    double lastRequestedEnd = -1;

  public:
    MediaCache();
    ~MediaCache();
    void enableBackground(const std::filesystem::path &cacheDirectory);
    void configureDiskCache(const std::filesystem::path &directory) { diskCacheDirectory = directory; }
    void poll();
    const Image &frame(const std::string &source, double time);
    bool pending() const;
    std::filesystem::path proxyPath(const std::string &source) const;
    void useProxy(const std::string &source, const std::string &path);
    bool proxiesEnabled = true;
    Loudness loudness;
    double lastMixEnd = -1;
    unsigned latencySamples = 0;
    std::map<Id, std::array<float, 2>> mixPeaks; // post-fader, before the final output clamp
    std::map<Id, std::array<float, 2>> mixRms;
    MediaAsset &get(const std::string &source);
    void clear();
    void resetAudio() {
        loudness.reset();
        lastMixEnd = -1;
        delays.clear();
        stretches.clear();
        lastRequestedEnd = -1;
        latencySamples = 0;
        effects.reset();
        for (auto &[id, entry] : nested) {
            if (entry.media)
                entry.media->resetAudio();
            entry.audio.clear();
            entry.audioStart = -1;
        }
        mixPeaks.clear();
        mixRms.clear();
    }
    void mix(const Project &project, double time, unsigned frames, std::vector<float> &out);
};
class AudioOutput {
    HWAVEOUT device = nullptr;
    static constexpr unsigned blockFrames = 1024, blockCount = 8;
    struct Block {
        WAVEHDR header{};
        std::array<short, blockFrames * 2> samples{};
        bool prepared = false;
    };
    std::array<Block, blockCount> blocks;
    double origin = 0;
    uint64_t queued = 0;
    bool running = false;

  public:
    cydaw::engine::AudioAnalysisFrame analysis;
    ~AudioOutput();
    bool available();
    void start(double time);
    void stop();
    void pump(const Project &p, MediaCache &media);
    double time() const;
    bool playing() const { return running; }
};
class Renderer;
struct ExportCancelled : std::runtime_error {
    ExportCancelled() : std::runtime_error("Export canceled") {}
};
// Part of the sequence to render, in sequence seconds; an empty range is the whole sequence.
// Frames are rendered at their own time, so effects, graphs and mixes match the preview.
struct ExportRange {
    double begin = 0, end = 0;
    double from(const Project &p) const { return end > begin ? std::clamp(begin, 0., p.duration) : 0.; }
    double to(const Project &p) const { return end > begin ? std::clamp(end, from(p), p.duration) : p.duration; }
};
void exportVideo(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path,
                 const std::function<bool(float)> &progress = {}, ExportRange range = {});
void exportAvi(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path,
               const std::function<bool(float)> &progress = {}, ExportRange range = {});
bool readFrameAviAudio(const std::string &path, const std::function<void(const float *, size_t)> &write);
void exportWav(const Project &p, MediaCache &media, const std::string &path,
               const std::function<bool(float)> &progress = {}, ExportRange range = {});
void writePng(const Image &image, const std::string &path);
// Animated GIF of a range, looping: frames rendered at width (height kept in proportion) and fps, each
// reduced to its own 256-colour palette with error diffusion.
void exportGif(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path, ExportRange range,
               unsigned width, int fps, const std::function<bool(float)> &progress = {});
void exportImages(const Project &p, MediaCache &media, Renderer &renderer, const std::string &directory,
                  const std::function<bool(float)> &progress = {}, ExportRange range = {});
// Attacks (onsets) of a decoded medium between two source times, in source seconds: transient energy
// flux against a moving threshold. Sensitivity 0..1 finds more attacks as it rises; minGap in seconds.
std::vector<double> detectOnsets(const MediaAsset &asset, double begin, double end, float sensitivity,
                                 double minGap);
// Spans quieter than thresholdDb (RMS over 10 ms) lasting at least minLength, in source seconds.
std::vector<std::pair<double, double>> detectSilences(const MediaAsset &asset, double begin, double end,
                                                      float thresholdDb, double minLength);
// Picture changes between begin and end, in source seconds: luminance histograms of consecutive frames
// (step seconds apart) compared against a threshold 0..1 and their neighbourhood.
std::vector<double> detectScenes(MediaAsset &asset, double begin, double end, double step, float threshold,
                                 const std::function<bool(float)> &progress = {});
// Timeline spans to remove from a sounding clip: its silences, less a padding kept around the speech,
// on whole frames.
// Offset in seconds that best lines up the sound of `moved` (from movedIn) with `reference` (from
// referenceIn): positive when the moved sound's content happens later in its file. RMS envelopes at
// 100 per second, normalised cross-correlation within ±maxLag; confidence is the correlation 0..1.
double soundOffset(const MediaAsset &reference, double referenceIn, double referenceLength, const MediaAsset &moved,
                   double movedIn, double movedLength, double maxLag, double &confidence);
std::vector<std::pair<double, double>> silenceSpans(const Clip &clip, const MediaAsset &asset, float thresholdDb,
                                                    double minLength, double padding, int fps);
} // namespace sf

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
    double duration = 0, lastTime = -1;
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
    ExportCancelled() : std::runtime_error("Export annulé") {}
};
void exportVideo(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path,
                 const std::function<bool(float)> &progress = {});
void exportAvi(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path,
               const std::function<bool(float)> &progress = {});
bool readFrameAviAudio(const std::string &path, const std::function<void(const float *, size_t)> &write);
void exportWav(const Project &p, MediaCache &media, const std::string &path,
               const std::function<bool(float)> &progress = {});
void writePng(const Image &image, const std::string &path);
void exportImages(const Project &p, MediaCache &media, Renderer &renderer, const std::string &directory,
                  const std::function<bool(float)> &progress = {});
} // namespace sf

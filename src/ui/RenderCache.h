#pragma once
#include "render/Renderer.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
namespace sf {
// Pre-render of a range of the sequence into memory, as Premiere's render bar: while the document stays
// the same, playback shows these frames instead of rendering the effects again. The worker owns its
// COM/MF runtime, renderer and media cache, and renders the frames in order so temporal effects follow.
class RenderCache {
    std::thread worker;
    mutable std::mutex mutex;
    std::atomic<bool> stopping = false, running = false;
    std::atomic<int> done = 0;
    std::shared_ptr<const Project> project;
    EffectLibrary library;
    std::vector<Image> frames; // by frame index from `first`; empty pixels: not rendered yet
    int first = 0, fps = 30;
    unsigned width = 0, height = 0;
    std::string key;
    Com<ID3D11ShaderResourceView> uploaded;
    int uploadedIndex = -1;
    void run();

  public:
    ~RenderCache() { clear(); }
    // Starts rendering [begin, end) at the given size; frames beyond the memory budget are left out.
    void start(const Project &p, const EffectLibrary &effects, std::string signature, double begin, double end,
               unsigned w, unsigned h, std::size_t budget);
    void clear();
    bool empty() const { return frames.empty(); }
    bool rendering() const { return running; }
    const std::string &signature() const { return key; }
    int frameCount() const { return int(frames.size()); }
    int framesDone() const { return done; }
    double begin() const { return double(first) / fps; }
    double end() const { return double(first + int(frames.size())) / fps; }
    int frameRate() const { return fps; }
    bool ready(int index) const;
    // The frame shown at a sequence time, uploaded on the owner's device; null when not rendered.
    ID3D11ShaderResourceView *frame(double time, ID3D11Device *device);
};
} // namespace sf

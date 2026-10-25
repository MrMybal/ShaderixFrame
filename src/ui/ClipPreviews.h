#pragma once
#include "render/Renderer.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
namespace sf {
// Worker owns its COM/MF runtime, media cache and immediate GPU context.
// Thumbnails are keyed by document signature and frame index, as CyDAW Cinema's filmstrip:
// the newest requests are rendered first and textures are recycled least recently used.
class ClipPreviews {
    struct Request {
        std::string key;
        std::shared_ptr<const Project> project;
        double time;
        unsigned width, height;
    };
    struct Texture {
        Com<ID3D11ShaderResourceView> view;
        std::uint64_t used = 0;
    };
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Request> pending;
    std::map<std::string, Image> ready;
    std::set<std::string> scheduled;
    std::map<std::string, Texture> textures;
    std::thread worker;
    EffectLibrary library;
    // Isolated clip documents survive across frames: rebuilt at once when the clip's structure changes,
    // otherwise re-signed a few times a second so that parameter edits reach the thumbnails.
    struct Prepared {
        std::shared_ptr<const Project> project;
        std::string signature;
        std::size_t fingerprint = 0;
        std::uint64_t checked = 0;
    };
    std::map<Id, Prepared> prepared;
    std::shared_ptr<const Project> sequence;
    std::string sequenceSignature;
    std::uint64_t clock = 0, sequenceChecked = ~0ull;
    unsigned requestsThisFrame = 0;
    std::atomic<bool> stopping = false;
    void run();
    ID3D11ShaderResourceView *fetch(const std::string &key, const std::shared_ptr<const Project> &project,
                                    double time, Renderer &owner);

  public:
    static constexpr unsigned thumbnailWidth = 160;
    ~ClipPreviews();
    // Frame of one clip rendered alone; `time` is a sequence time.
    ID3D11ShaderResourceView *get(const Project &project, const Clip &clip, double time, Renderer &owner);
    // Frame of the final composition, all tracks; the document signature refreshes a few times a second.
    ID3D11ShaderResourceView *getSequence(const Project &project, double time, Renderer &owner);
    void clear();
    void beginFrame() {
        if (prepared.size() > 1024)
            prepared.clear();
        requestsThisFrame = 0;
        ++clock;
    }
};
} // namespace sf

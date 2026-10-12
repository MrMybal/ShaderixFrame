#pragma once
#include "render/Renderer.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
namespace sf {
// Worker owns its COM/MF runtime, media cache and immediate GPU context.
class ClipPreviews {
    struct Request {
        std::string key;
        Project project;
        double time;
    };
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Request> pending;
    std::map<std::string, Image> ready;
    std::set<std::string> scheduled;
    std::map<std::string, Com<ID3D11ShaderResourceView>> textures;
    std::thread worker;
    EffectLibrary library;
    std::map<Id, std::pair<Project, std::string>> prepared;
    unsigned requestsThisFrame = 0;
    std::atomic<bool> stopping = false;
    void run();

  public:
    ~ClipPreviews();
    ID3D11ShaderResourceView *get(const Project &project, const Clip &clip, double time, Renderer &owner);
    void clear();
    void beginFrame() {
        prepared.clear();
        requestsThisFrame = 0;
    }
};
} // namespace sf

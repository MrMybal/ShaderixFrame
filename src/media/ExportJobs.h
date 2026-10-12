#pragma once
#include "render/Renderer.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace sf {
enum class ExportKind { Video, Audio, Images, Analysis };
struct ExportPreset {
    std::string id, name;
    int width = 0, height = 0, fps = 0;
};
const std::vector<ExportPreset> &exportPresets();
enum class ExportState { Queued, Running, Cancelling, Finalizing, Completed, Cancelled, Failed };
struct ExportInfo {
    Id id = 0;
    ExportKind kind = ExportKind::Video;
    ExportState state = ExportState::Queued;
    std::string path, project, error, preset = "project";
    std::string imageFormat="png8";
    int width = 0, height = 0, fps = 0;
    double duration = 0, elapsed = 0;
    float progress = 0;
    bool active() const;
    bool cancellable() const;
    Json json() const;
};
// A job owns an immutable document/library and a separate COM/media/GPU runtime.
// No worker ever touches Session, ImGui, the preview context, or playback DSP state.
class ExportJobs {
    struct Job;
    mutable std::mutex mutex;
    std::deque<std::shared_ptr<Job>> jobs;
    Id nextId = 1;
    bool stopped = false;
    std::condition_variable wake;
    std::thread worker;
    void run();
    void prune();

  public:
    ExportJobs() = default;
    ExportJobs(const ExportJobs &) = delete;
    ExportJobs &operator=(const ExportJobs &) = delete;
    ~ExportJobs();
    Id start(const Project &snapshot, const EffectLibrary &library, ExportKind kind, const std::string &path,
             const std::string &preset = "project");
    ExportInfo status(Id id) const;
    std::vector<ExportInfo> list() const;
    bool cancel(Id id);
    void shutdown();
};
} // namespace sf

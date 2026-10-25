#include "media/ExportJobs.h"
#include "core/AtomicFile.h"
#include <chrono>
#include <fstream>
#include <mfapi.h>

namespace sf {
const std::vector<ExportPreset> &exportPresets() {
    static const std::vector<ExportPreset> presets = {{"project", "Project Format"},
                                                      {"preview720", "720p Preview", 1280, 720, 30},
                                                      {"web1080", "Web 1080p", 1920, 1080, 30},
                                                      {"portrait1080", "Portrait 1080p", 1080, 1920, 30},
                                                      {"cinema24", "1080p Cinema", 1920, 1080, 24},
                                                      {"portrait1080_60", "Vertical 1080p 60 i/s (Shorts, Reels, TikTok)", 1080, 1920, 60},
                                                      {"square1080", "1080 Square (Instagram)", 1080, 1080, 30},
                                                      {"uhd4k", "4K UHD", 3840, 2160, 30}};
    return presets;
}
using Clock = std::chrono::steady_clock;
static const char *stateName(ExportState state) {
    switch (state) {
    case ExportState::Queued:
        return "queued";
    case ExportState::Running:
        return "running";
    case ExportState::Cancelling:
        return "cancelling";
    case ExportState::Finalizing:
        return "finalizing";
    case ExportState::Completed:
        return "completed";
    case ExportState::Cancelled:
        return "cancelled";
    case ExportState::Failed:
        return "failed";
    }
    return "failed";
}
bool ExportInfo::active() const {
    return state == ExportState::Queued || state == ExportState::Running ||
           state == ExportState::Cancelling || state == ExportState::Finalizing;
}
bool ExportInfo::cancellable() const {
    return state == ExportState::Queued || state == ExportState::Running;
}
Json ExportInfo::json() const {
    Json result = Json::Object();
    result["job"] = id;
    result["kind"] = kind == ExportKind::Video    ? "video"
                     : kind == ExportKind::Audio  ? "audio"
                     : kind == ExportKind::Images ? "images"
                                                  : "analysis";
    result["preset"] = preset;
    auto extension = sx::Utf8Path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    result["format"] = kind == ExportKind::Video    ? (extension == L".avi" ? "avi" : "mp4")
                       : kind == ExportKind::Audio  ? "wav"
                       : kind == ExportKind::Images ? (imageFormat=="png8"?"png":imageFormat.c_str())
                                                    : "json";
    result["state"] = stateName(state);
    result["path"] = path;
    result["project"] = project;
    result["width"] = width;
    result["height"] = height;
    result["fps"] = fps;
    result["duration"] = duration;
    result["elapsed"] = elapsed;
    result["progress"] = progress;
    result["cancellable"] = cancellable();
    result["error"] = error;
    return result;
}
struct ExportJobs::Job {
    mutable std::mutex mutex;
    ExportInfo info;
    bool cancellation = false;
    bool claimed = false; // guarded by the catalogue mutex
    Clock::time_point began = Clock::now();
    Project document;
    EffectLibrary effects;
    ExportRange range;
    ExportInfo read() const {
        std::lock_guard lock(mutex);
        auto copy = info;
        if (copy.active())
            copy.elapsed = std::chrono::duration<double>(Clock::now() - began).count();
        return copy;
    }
    bool requestCancel() {
        std::lock_guard lock(mutex);
        if (!info.cancellable())
            return false;
        cancellation = true;
        info.state = ExportState::Cancelling;
        return true;
    }
    bool advance(float progress) {
        std::lock_guard lock(mutex);
        if (cancellation)
            return false;
        info.progress = std::max(info.progress, std::min(.99f, progress));
        info.state = progress >= 1 ? ExportState::Finalizing : ExportState::Running;
        return true;
    }
    void finish(ExportState state, std::string error = {}) {
        std::lock_guard lock(mutex);
        info.state = state;
        info.error = std::move(error);
        if (state == ExportState::Completed)
            info.progress = 1;
        info.elapsed = std::chrono::duration<double>(Clock::now() - began).count();
    }
};
ExportJobs::~ExportJobs() {
    shutdown();
}
Id ExportJobs::start(const Project &snapshot, const EffectLibrary &library, ExportKind kind,
                     const std::string &path, const std::string &preset, ExportRange range) {
    snapshot.validate();
    if (path.empty())
        throw std::runtime_error("Output path required");
    auto destination = std::filesystem::absolute(sx::Utf8Path(path)).lexically_normal();
    auto extension = destination.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (kind != ExportKind::Images && !(kind == ExportKind::Video && extension == L".avi") &&
        extension != (kind == ExportKind::Video   ? L".mp4"
                      : kind == ExportKind::Audio ? L".wav"
                                                  : L".json"))
        throw std::runtime_error(
            "Expected extension: .mp4/.avi for video, .wav for audio, .json for analysis");
    // Prepare copies before registering the job; allocation errors leave the catalogue intact.
    Project document = snapshot;
    auto selected = std::find_if(exportPresets().begin(), exportPresets().end(),
                                 [&](const auto &item) { return item.id == preset; });
    if (selected == exportPresets().end())
        throw std::runtime_error("Unknown export preset");
    if (selected->width && kind != ExportKind::Audio) {
        document.width = selected->width;
        document.height = selected->height;
        document.fps = selected->fps;
    }
    if (kind == ExportKind::Images && std::filesystem::exists(destination))
        throw std::runtime_error("A PNG sequence requires a new destination folder");
    EffectLibrary effects = library;
    auto job = std::make_shared<Job>();
    job->info.kind = kind;
    job->info.path = sx::Narrow(destination.wstring());
    job->info.project = snapshot.name;
    job->info.imageFormat=snapshot.imageFormat;
    job->info.preset = preset;
    job->info.width = document.width;
    job->info.height = document.height;
    job->info.fps = document.fps;
    job->info.duration = range.to(snapshot) - range.from(snapshot);
    job->range = range;
    std::lock_guard lock(mutex);
    if (stopped)
        throw std::runtime_error("Export manager is closed");
    size_t active = 0;
    for (auto &previous : jobs) {
        if (previous->read().active()) {
            ++active;
            if (_wcsicmp(sx::Widen(previous->info.path).c_str(), destination.c_str()) == 0)
                throw std::runtime_error("This destination is already in the render queue");
        }
    }
    if (active >= 32)
        throw std::runtime_error("32 renders maximum in the queue");
    if (nextId > 2147483647)
        throw std::runtime_error("Too many exports in this session");
    job->info.id = nextId;
    job->document = std::move(document);
    job->effects = std::move(effects);
    jobs.push_back(job);
    try {
        if (!worker.joinable())
            worker = std::thread([this] { run(); });
    } catch (...) {
        jobs.pop_back();
        throw;
    }
    ++nextId;
    prune();
    wake.notify_all();
    return job->info.id;
}
ExportInfo ExportJobs::status(Id id) const {
    std::lock_guard lock(mutex);
    for (auto &job : jobs)
        if (job->info.id == id)
            return job->read();
    throw std::runtime_error("Export not found in the queue or the eight most recent completed renders");
}
std::vector<ExportInfo> ExportJobs::list() const {
    std::lock_guard lock(mutex);
    std::vector<ExportInfo> result;
    for (auto job = jobs.rbegin(); job != jobs.rend(); ++job)
        result.push_back((*job)->read());
    return result;
}
bool ExportJobs::cancel(Id id) {
    std::lock_guard lock(mutex);
    for (auto &job : jobs)
        if (job->info.id == id) {
            bool accepted = job->requestCancel();
            if (accepted && !job->claimed) {
                job->finish(ExportState::Cancelled);
                job->document = {};
                job->effects = {};
            }
            prune();
            wake.notify_all();
            return accepted;
        }
    throw std::runtime_error("Export not found among the eight most recent renders");
}
void ExportJobs::prune() {
    // Keep every active job and the eight newest terminal jobs.
    size_t terminal = 0;
    for (auto it = jobs.rbegin(); it != jobs.rend(); ++it)
        if (!(*it)->read().active())
            ++terminal;
    for (auto it = jobs.begin(); it != jobs.end() && terminal > 8;) {
        if (!(*it)->read().active()) {
            it = jobs.erase(it);
            --terminal;
        } else
            ++it;
    }
}
void ExportJobs::run() {
    for (;;) {
        std::shared_ptr<Job> job;
        {
            std::unique_lock lock(mutex);
            wake.wait(lock, [&] {
                return stopped || std::any_of(jobs.begin(), jobs.end(), [](const auto &j) {
                           auto state = j->read().state;
                           return state == ExportState::Queued || state == ExportState::Cancelling;
                       });
            });
            if (stopped)
                return;
            for (auto &candidate : jobs) {
                auto state = candidate->read().state;
                if (state == ExportState::Queued || state == ExportState::Cancelling) {
                    job = candidate;
                    job->claimed = true;
                    break;
                }
            }
        }
        ExportState state = ExportState::Completed;
        std::string failure;
        try {
            if (!job->advance(0))
                throw ExportCancelled();
            check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Render COM initialization");
            struct ComRuntime {
                ~ComRuntime() { CoUninitialize(); }
            } com;
            check(MFStartup(MF_VERSION), "Render media initialization");
            struct MediaRuntime {
                ~MediaRuntime() { MFShutdown(); }
            } mediaRuntime;
            MediaCache media;
            wchar_t executable[32768];
            GetModuleFileNameW(nullptr, executable, 32768);
            media.configureDiskCache(std::filesystem::path(executable).parent_path() / "cache/audio");
            auto progress = [job](float value) { return job->advance(value); };
            if (job->info.kind == ExportKind::Analysis) {
                double from = job->range.from(job->document);
                uint64_t total = uint64_t((job->range.to(job->document) - from) * 48000);
                std::vector<float> data;
                for (uint64_t at = 0; at < total; at += 4096) {
                    if (!progress(float(double(at) / total)))
                        throw ExportCancelled();
                    media.mix(job->document, from + at / 48000., unsigned(std::min(uint64_t(4096), total - at)),
                              data);
                }
                AtomicFile output(job->info.path);
                std::ofstream file(output.path());
                file << media.loudness.json().Dump(2);
                file.close();
                if (!file)
                    throw std::runtime_error("Unable to read the loudness report");
                if (!progress(1))
                    throw ExportCancelled();
                output.commit();
            } else if (job->info.kind != ExportKind::Audio) {
                Renderer renderer;
                renderer.initialize();
                renderer.library = job->effects;
                if (job->info.kind == ExportKind::Video)
                    exportVideo(job->document, media, renderer, job->info.path, progress, job->range);
                else
                    exportImages(job->document, media, renderer, job->info.path, progress, job->range);
            } else
                exportWav(job->document, media, job->info.path, progress, job->range);
        } catch (const ExportCancelled &) {
            state = ExportState::Cancelled;
        } catch (const std::exception &error) {
            state = ExportState::Failed;
            failure = error.what();
        } catch (...) {
            state = ExportState::Failed;
            failure = "Unknown rendering error";
        }
        job->finish(state, std::move(failure));
        // Release potentially large snapshots after completion, retaining only metadata.
        job->document = {};
        job->effects = {};
        std::lock_guard lock(mutex);
        prune();
    }
}
void ExportJobs::shutdown() {
    {
        std::lock_guard lock(mutex);
        stopped = true;
        for (auto &job : jobs) {
            auto state = job->read().state;
            if (!job->claimed && (state == ExportState::Queued || state == ExportState::Cancelling)) {
                job->requestCancel();
                job->finish(ExportState::Cancelled);
                job->document = {};
                job->effects = {};
            } else
                job->requestCancel();
        }
    }
    wake.notify_all();
    if (worker.joinable())
        worker.join();
    std::lock_guard lock(mutex);
    prune();
}
} // namespace sf

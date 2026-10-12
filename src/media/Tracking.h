#pragma once
#include "media/Media.h"
#include <atomic>
#include <mutex>
#include <thread>
namespace sf {
class TrackingJobs {
    mutable std::mutex mutex;
    std::thread worker;
    std::atomic<bool> cancellation = false;
    Id id = 0, clipId = 0;
    int gradeIndex = -1;
    std::string state = "idle", error, identity;
    float progress = 0;
    std::vector<Keyframe> x, y;
    void run(Clip clip, double begin, double end, int fps, int radius);

  public:
    ~TrackingJobs();
    Id start(const Project &project, Id clip, double begin, double end, int radius, int effect = -1);
    Json status() const;
    bool cancel();
    void apply(Project &project, Id job);
};
} // namespace sf

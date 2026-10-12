#pragma once
#include "core/Project.h"

namespace sf {
struct RecoveryEntry {
    std::filesystem::path file;
    std::string name, originalPath;
    double time = 0;
};
class Recovery {
    std::filesystem::path directory, current;
    void rotate();

  public:
    void configure(std::filesystem::path path);
    bool enabled() const { return !directory.empty(); }
    void capture(const Project &project, const std::string &originalPath, double time);
    void preserve(const Project &project, const std::string &originalPath, double time);
    void clear();
    std::vector<RecoveryEntry> list() const;
    Project restore(const RecoveryEntry &entry) const;
    void discard(const RecoveryEntry &entry) const;
};
} // namespace sf

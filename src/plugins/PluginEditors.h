#pragma once
#include "plugins/IsolatedAudio.h"
namespace sf {
Effect &pluginEffect(Project &project, const Json &target, bool editable = true);
class PluginEditors {
    struct Entry {
        Json target;
        std::string path, state;
        std::map<std::string, float> observed;
        std::unique_ptr<IsolatedAudio> process;
    };
    std::map<std::string, Entry> entries;
    double nextPoll = 0;
    Entry &get(Project &project, const Json &target, double time);

  public:
    void clear() { entries.clear(); }
    bool empty() const { return entries.empty(); }
    bool pollDue() const;
    void show(Project &project, const Json &target, double time, bool visible);
    bool capture(Project &project, bool force = false);
    void restore(Project &project, const Json &target, const std::string &state, double time);
    std::string snapshot(Project &project, const Json &target, double time);
};
} // namespace sf

#pragma once
#include "core/Recovery.h"
#include "media/ExportJobs.h"
#include "media/Tracking.h"
#include "plugins/PluginEditors.h"
#include "render/Renderer.h"
namespace sf {
struct Session {
    Project project = Project::demo();
    History history;
    MediaCache media;
    Renderer renderer;
    Recovery recovery;
    ExportJobs exports;
    TrackingJobs tracking;
    PluginEditors pluginEditors;
    void syncPluginEditors(bool force = false) {
        if (pluginEditors.empty() || (!force && !pluginEditors.pollDue()))
            return;
        auto before = project;
        try {
            if (pluginEditors.capture(project, force)) {
                project.validate();
                history.checkpoint(before);
                dirty = true;
                media.resetAudio();
            }
        } catch (...) {
            project = std::move(before);
            pluginEditors.clear();
            throw;
        }
    }
    std::string projectPath, lastCommand;
    double time = 0;
    bool playing = false, dirty = false;
    Id selected = 0;
    std::vector<Id> selection;
    struct ProxyJob {
        Id id;
        std::string source, path;
    };
    std::vector<ProxyJob> proxyJobs;
    void pollJobs() {
        for (auto it = proxyJobs.begin(); it != proxyJobs.end();) {
            ExportInfo info;
            try {
                info = exports.status(it->id);
            } catch (...) {
                it = proxyJobs.erase(it);
                continue;
            }
            if (info.active()) {
                ++it;
                continue;
            }
            if (info.state == ExportState::Completed)
                media.useProxy(it->source, it->path);
            it = proxyJobs.erase(it);
        }
    }
    void selectClip(Id id, bool toggle = false, bool preserve = false) {
        if (!id) {
            selected = 0;
            selection.clear();
            return;
        }
        auto group = project.linked(id, false);
        bool present = std::find(selection.begin(), selection.end(), id) != selection.end();
        if (toggle && present) {
            std::erase_if(selection, [&](Id member) {
                return std::find(group.begin(), group.end(), member) != group.end();
            });
            selected = selection.empty() ? 0 : selection.front();
        } else {
            if (!toggle && !(preserve && present))
                selection.clear();
            selection.insert(selection.end(), group.begin(), group.end());
            selection = project.selection(selection, false);
            selected = id;
        }
    }
    void setSelection(const std::vector<Id> &ids) {
        auto expanded = project.selection(ids, false);
        selection = std::move(expanded);
        selected = ids.empty() ? 0 : ids.front();
    }
    void tidySelection() {
        std::erase_if(selection, [&](Id id) { return !project.findClip(id); });
        if (selected && !project.findClip(selected))
            selected = 0;
        if (selection.empty() && selected)
            selection = project.linked(selected, false);
        selection = project.selection(selection, false);
        if (!selected || std::find(selection.begin(), selection.end(), selected) == selection.end())
            selected = selection.empty() ? 0 : selection.front();
    }
};
class Protocol {
    Session &session;

  public:
    explicit Protocol(Session &s) : session(s) {}
    Json tools() const;
    Json execute(const std::string &name, const Json &args);
    Json respond(const Json &request);
};
} // namespace sf

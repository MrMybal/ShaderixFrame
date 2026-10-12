#include "plugins/PluginEditors.h"
#include <chrono>
namespace sf {
Effect &pluginEffect(Project &p, const Json &target, bool editable) {
    std::vector<Effect> *chain = nullptr;
    auto scope = target["scope"].S();
    if (scope == "clip") {
        auto *clip = p.findClip(target["clip"].U());
        if (!clip || p.owner(clip->id)->kind != TrackKind::Audio || (editable && p.owner(clip->id)->locked))
            throw std::runtime_error("Clip audio absent/verrouillé");
        chain = &clip->effects;
    } else if (scope == "master")
        chain = &p.masterMix.effects;
    else if (scope == "track") {
        auto *track = p.findTrack(target["target"].U());
        if (!track || track->kind != TrackKind::Audio || (editable && track->locked))
            throw std::runtime_error("Piste audio absente/verrouillée");
        chain = &track->mix.effects;
    } else if (scope == "bus") {
        auto *bus = p.findBus(target["target"].U());
        if (!bus || (editable && bus->locked))
            throw std::runtime_error("Bus absent/verrouillé");
        chain = &bus->mix.effects;
    }
    int index = target["effect"].I(-1);
    if (!chain || index < 0 || index >= int(chain->size()) || (*chain)[index].id != "vst3")
        throw std::runtime_error("Effet VST3 requis");
    return (*chain)[index];
}
static std::string key(const Json &a) {
    return a["scope"].S() + ":" + std::to_string(a["target"].U()) + ":" + std::to_string(a["clip"].U()) +
           ":" + std::to_string(a["effect"].I(-1));
}
bool PluginEditors::pollDue() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() >=
           nextPoll;
}
PluginEditors::Entry &PluginEditors::get(Project &project, const Json &target, double time) {
    auto &effect = pluginEffect(project, target);
    auto &entry = entries[key(target)];
    if (!entry.process || entry.path != effect.code || entry.state != effect.pluginState) {
        auto process = std::make_unique<IsolatedAudio>("vst3", effect.code);
        process->restoreState(effect.pluginState);
        for (auto &[key, parameter] : effect.params)
            process->setParameter(unsigned(std::stoul(key.substr(1))), parameter.at(time));
        entry.target = target;
        entry.path = effect.code;
        entry.state = effect.pluginState;
        entry.observed = process->parameters();
        entry.process = std::move(process);
    }
    return entry;
}
void PluginEditors::show(Project &project, const Json &target, double time, bool visible) {
    if (visible)
        get(project, target, time).process->showEditor(true);
    else {
        auto found = entries.find(key(target));
        if (found != entries.end())
            found->second.process->showEditor(false);
    }
}
bool PluginEditors::capture(Project &project, bool force) {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!force && now < nextPoll)
        return false;
    nextPoll = now + .25;
    bool changed = false;
    for (auto it = entries.begin(); it != entries.end();) {
        Effect *effect = nullptr;
        try {
            effect = &pluginEffect(project, it->second.target);
        } catch (...) {
            it = entries.erase(it);
            continue;
        }
        auto &entry = it->second;
        if (effect->code != entry.path || effect->pluginState != entry.state) {
            it = entries.erase(it);
            continue;
        }
        auto values = entry.process->parameters();
        bool controls = values != entry.observed;
        // A project-side parameter edit also reaches the open plugin window.
        if (!controls)
            for (auto &[key, parameter] : effect->params) {
                if (parameter.keys.empty() && entry.observed.contains(key) &&
                    std::abs(parameter.value - entry.observed.at(key)) > 1e-6f)
                    entry.process->setParameter(unsigned(std::stoul(key.substr(1))), parameter.value);
            }
        if (force || controls) {
            auto state = entry.process->state();
            changed |= state != effect->pluginState;
            effect->pluginState = state;
            entry.state = state;
            if (controls)
                for (auto &[key, value] : values) {
                    changed |= effect->params[key].value != value;
                    effect->params[key].value = value;
                }
        }
        entry.observed = entry.process->parameters();
        ++it;
    }
    return changed;
}
void PluginEditors::restore(Project &project, const Json &target, const std::string &state, double time) {
    auto &effect = pluginEffect(project, target);
    auto process = std::make_unique<IsolatedAudio>("vst3", effect.code);
    process->restoreState(state);
    auto values = process->parameters();
    effect.pluginState = process->state();
    effect.parameterNames = process->parameterNames();
    for (auto &[key, value] : values)
        effect.params[key].value = value;
    entries.erase(key(target));
}
std::string PluginEditors::snapshot(Project &project, const Json &target, double time) {
    auto &entry = get(project, target, time);
    auto state = entry.process->state();
    pluginEffect(project, target).pluginState = state;
    entry.state = state;
    return state;
}
} // namespace sf

#include "core/Project.h"
#include "adapters/CyDawAudio.h"
#include "adapters/DspGraph.h"
#include "adapters/LayerGraph.h"
#include "core/AtomicFile.h"
#include "core/Binary.h"
#include "core/Motion.h"
#include "plugins/IsolatedOfx.h"
#include "render/Grading.h"
#include <fstream>
#include <functional>
#include <set>
#include <windows.h>

namespace sf {
float shapedAmount(float raw, float shape) {
    float t = std::clamp(raw, 0.f, 1.f);
    return std::clamp(t + std::clamp(shape, -1.f, 1.f) * 2 * t * (1 - t), 0.f, 1.f);
}
double snapTime(double s, int fps, bool snap) {
    return std::max(0., snap ? std::round(s * fps) / fps : s);
}
float Parameter::at(double t) const {
    if (keys.empty())
        return value;
    if (t <= keys.front().time)
        return keys.front().value;
    for (size_t i = 1; i < keys.size(); ++i)
        if (t < keys[i].time) {
            const auto &a = keys[i - 1];
            const auto &b = keys[i];
            float u = float((t - a.time) / (b.time - a.time));
            if (a.curve == Curve::Hold)
                u = 0;
            if (a.curve == Curve::Smooth)
                u = u * u * (3 - 2 * u);
            if (a.curve == Curve::Shaped)
                u = shapedAmount(u, a.shape);
            return a.value + (b.value - a.value) * u;
        }
    return keys.back().value;
}
void Parameter::setKey(Keyframe k) {
    if (!std::isfinite(k.time) || !std::isfinite(k.value) || !std::isfinite(k.shape))
        throw std::runtime_error("Point d'animation invalide");
    auto i = std::lower_bound(keys.begin(), keys.end(), k.time, [](auto &a, double t) { return a.time < t; });
    if (i != keys.end() && std::abs(i->time - k.time) < 1e-7)
        *i = k;
    else
        keys.insert(i, k);
}
float Clip::param(const std::string &k, double t, float f) const {
    auto i = params.find(k);
    return i == params.end() ? f : i->second.at(t);
}
float MixChannel::param(const std::string &key, double time, float fallback) const {
    auto it = params.find(key);
    return it == params.end() ? fallback : it->second.at(time);
}
float Clip::envelope(double time) const {
    if (!active(time))
        return 0;
    double value = 1;
    if (fadeIn > 0)
        value = std::min(value, (time - start) / fadeIn);
    if (fadeOut > 0)
        value = std::min(value, (start + duration - time) / fadeOut);
    return float(std::clamp(value, 0., 1.));
}
static void shiftKeys(Clip &clip, double delta) {
    auto shift = [&](auto &params) {
        for (auto &[name, parameter] : params)
            for (auto &key : parameter.keys)
                key.time += delta;
    };
    shift(clip.params);
    for (auto &effect : clip.effects)
        shift(effect.params);
}
float Clip::transitionProgress(double time) const {
    return std::clamp(param("transition_progress", time, float((time - start) / transition.duration)), 0.f,
                      1.f);
}
std::vector<Id> Project::linked(Id id, bool editable) const {
    auto *selected = findClip(id);
    if (!selected)
        throw std::runtime_error("Clip introuvable");
    std::vector<Id> ids;
    for (auto &track : tracks)
        for (auto &clip : track.clips)
            if (clip.id == id || (selected->link && clip.link == selected->link)) {
                if (editable && track.locked)
                    throw std::runtime_error("Une piste du groupe lié est verrouillée");
                ids.push_back(clip.id);
            }
    return ids;
}
std::vector<Id> Project::selection(const std::vector<Id> &clips, bool editable) const {
    std::set<Id> selected;
    for (auto id : clips) {
        auto members = linked(id, editable);
        selected.insert(members.begin(), members.end());
    }
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (auto &t : tracks)
            for (auto &c : t.clips)
                if (!c.widget.IsNull() && c.widget["parent"].U() &&
                    selected.contains(c.widget["parent"].U()) && !selected.contains(c.id)) {
                    auto members = linked(c.id, editable);
                    for (auto member : members)
                        expanded |= selected.insert(member).second;
                }
    }
    return {selected.begin(), selected.end()};
}
void Project::moveClips(const std::vector<Id> &clips, double delta) {
    auto ids = selection(clips);
    if (ids.empty() || !std::isfinite(delta))
        throw std::runtime_error("Sélection ou déplacement invalide");
    auto candidate = *this;
    for (auto id : ids) {
        auto &c = *candidate.findClip(id);
        c.start += delta;
        shiftKeys(c, delta);
        candidate.duration = std::max(candidate.duration, c.start + c.duration);
    }
    candidate.validate();
    for (auto id : ids)
        *findClip(id) = *candidate.findClip(id);
    duration = candidate.duration;
}
void Project::removeClips(const std::vector<Id> &clips, bool ripple) {
    auto ids = selection(clips);
    if (ids.empty())
        throw std::runtime_error("Sélection vide");
    std::set<Id> removed(ids.begin(), ids.end());
    std::vector<std::pair<double, double>> intervals;
    if (ripple) {
        for (auto id : ids) {
            auto &c = *findClip(id);
            if (c.start < duration)
                intervals.emplace_back(c.start, std::min(c.start + c.duration, duration));
        }
        std::sort(intervals.begin(), intervals.end());
        std::vector<std::pair<double, double>> merged;
        for (auto range : intervals) {
            if (!merged.empty() && range.first <= merged.back().second + 1e-7)
                merged.back().second = std::max(merged.back().second, range.second);
            else
                merged.push_back(range);
        }
        intervals = std::move(merged);
    }
    auto candidate = *this;
    std::map<Id, double> groupOffsets;
    for (auto &track : candidate.tracks) {
        for (auto &c : track.clips) {
            if (removed.contains(c.id))
                continue;
            double shift = 0;
            for (auto [begin, end] : intervals) {
                if (c.start < end - 1e-7 && c.start + c.duration > begin + 1e-7)
                    throw std::runtime_error("Un clip non sélectionné traverse l'espace à fermer : " +
                                             c.name);
                if (c.start >= end - 1e-7)
                    shift -= end - begin;
            }
            if (shift != 0 && track.locked)
                throw std::runtime_error("Une piste à déplacer est verrouillée : " + track.name);
            if (c.link) {
                auto [entry, inserted] = groupOffsets.emplace(c.link, shift);
                if (!inserted && std::abs(entry->second - shift) > 1e-7)
                    throw std::runtime_error("Un groupe lié traverse l'espace à fermer");
            }
            c.start = std::max(0., c.start + shift);
            shiftKeys(c, shift);
        }
        std::erase_if(track.clips, [&](const Clip &c) { return removed.contains(c.id); });
    }
    for (auto [begin, end] : intervals)
        candidate.duration -= end - begin;
    candidate.duration = std::max(1. / fps, candidate.duration);
    candidate.validate();
    *this = std::move(candidate);
}
std::vector<Id> Project::duplicateClips(const std::vector<Id> &clips) {
    auto ids = selection(clips);
    if (ids.empty())
        throw std::runtime_error("Sélection vide");
    double begin = std::numeric_limits<double>::infinity(), end = 0;
    for (auto id : ids) {
        auto &c = *findClip(id);
        begin = std::min(begin, c.start);
        end = std::max(end, c.start + c.duration);
    }
    auto candidate = *this;
    std::map<Id, Id> groups;
    std::vector<Id> copies;
    std::map<Id, Id> copiedWidgets;
    for (auto id : ids) {
        auto copy = *findClip(id);
        copy.id = candidate.allocate();
        if (copy.link) {
            auto [entry, inserted] = groups.emplace(copy.link, 0);
            if (inserted)
                entry->second = candidate.allocate();
            copy.link = entry->second;
        }
        copy.start += end - begin;
        shiftKeys(copy, end - begin);
        candidate.duration = std::max(candidate.duration, copy.start + copy.duration);
        copiedWidgets[id] = copy.id;
        copies.push_back(copy.id);
        candidate.owner(id)->clips.push_back(std::move(copy));
    }
    for (auto id : copies) {
        auto *c = candidate.findClip(id);
        const auto &layout = c->widget;
        auto parent = layout["parent"].U();
        if (!layout.IsNull() && copiedWidgets.contains(parent))
            c->widget["parent"] = copiedWidgets.at(parent);
    }
    candidate.validate();
    *this = std::move(candidate);
    return copies;
}
void Project::splitClips(const std::vector<Id> &clips, double time) {
    auto ids = selection(clips, false);
    if (ids.empty() || !std::isfinite(time))
        throw std::runtime_error("Sélection ou temps invalide");
    auto candidate = *this;
    std::set<Id> processed;
    bool cut = false;
    for (auto id : ids) {
        if (processed.contains(id))
            continue;
        auto members = linked(id, false);
        processed.insert(members.begin(), members.end());
        bool intersects = false;
        for (auto member : members) {
            auto &c = *findClip(member);
            intersects |= time > c.start + 1e-5 && time < c.start + c.duration - 1e-5;
        }
        if (intersects) {
            candidate.split(id, time);
            cut = true;
        }
    }
    if (!cut)
        throw std::runtime_error("Aucun clip sélectionné ne traverse la tête de lecture");
    candidate.validate();
    *this = std::move(candidate);
}
void Project::linkClips(Id id, Id other) {
    auto ids = linked(id), others = linked(other);
    if (id == other)
        throw std::runtime_error("Choisir deux clips distincts");
    Id group = findClip(id)->link;
    if (!group)
        group = allocate();
    for (auto c : ids)
        findClip(c)->link = group;
    for (auto c : others)
        findClip(c)->link = group;
}
void Project::unlink(Id id) {
    for (auto member : linked(id))
        findClip(member)->link = 0;
}
void Project::setTiming(Id id, double start, double length, double in, double speed, bool moveKeys) {
    auto ids = linked(id);
    auto original = *findClip(id);
    Project candidate = *this;
    for (auto member : ids) {
        auto &c = *candidate.findClip(member);
        double delta = start - original.start;
        c.start += delta;
        c.duration += length - original.duration;
        c.in += moveKeys ? in - original.in : delta * c.speed;
        c.speed *= speed / original.speed;
        c.fadeIn = std::min(c.fadeIn, c.duration);
        c.fadeOut = std::min(c.fadeOut, c.duration);
        if (c.transition.enabled)
            c.transition.duration = std::min(c.transition.duration, c.duration);
        if (moveKeys)
            shiftKeys(c, delta);
    }
    candidate.validate();
    for (auto member : ids)
        *findClip(member) = *candidate.findClip(member);
}
void Project::rippleTrim(Id id, double delta, bool left) {
    if (!std::isfinite(delta))
        throw std::runtime_error("Trim invalide");
    auto members = linked(id);
    auto candidate = *this;
    auto original = *findClip(id);
    double boundary = original.start + original.duration;
    std::set<Id> selected(members.begin(), members.end());
    for (auto member : members) {
        auto &clip = *candidate.findClip(member);
        if (std::abs(clip.start + clip.duration - boundary) > 1e-6)
            throw std::runtime_error("Les fins du groupe lié doivent être alignées pour un trim ripple");
        clip.duration += delta;
        if (left)
            clip.in -= delta * clip.speed;
        clip.fadeIn = std::min(clip.fadeIn, clip.duration);
        clip.fadeOut = std::min(clip.fadeOut, clip.duration);
        clip.transition.duration = std::min(clip.transition.duration, clip.duration);
    }
    std::map<Id, double> shifts;
    for (auto &track : candidate.tracks)
        for (auto &clip : track.clips) {
            if (selected.contains(clip.id))
                continue;
            double shift = clip.start >= boundary - 1e-7 ? delta : 0;
            if (clip.start < boundary - 1e-7 && clip.start + clip.duration > boundary + 1e-7)
                throw std::runtime_error("Un clip traverse le point ripple ; le découper au préalable");
            if (shift && track.locked)
                throw std::runtime_error("Piste verrouillée dans le déplacement ripple");
            if (clip.link) {
                auto [it, added] = shifts.emplace(clip.link, shift);
                if (!added && std::abs(it->second - shift) > 1e-7)
                    throw std::runtime_error("Le groupe lié traverse le point ripple");
            }
            clip.start += shift;
            shiftKeys(clip, shift);
        }
    candidate.duration = std::max(1. / fps, candidate.duration + delta);
    candidate.validate();
    *this = std::move(candidate);
}
void Project::rollTrim(Id id, Id next, double delta) {
    if (!std::isfinite(delta) || id == next)
        throw std::runtime_error("Roll invalide");
    auto outgoing = linked(id), incoming = linked(next);
    for (auto member : outgoing)
        if (std::find(incoming.begin(), incoming.end(), member) != incoming.end())
            throw std::runtime_error("Deux groupes distincts requis");
    auto first = *findClip(id), second = *findClip(next);
    if (owner(id)->id != owner(next)->id || std::abs(first.start + first.duration - second.start) > 1e-6)
        throw std::runtime_error("Choisir deux clips adjacents de la même piste");
    auto candidate = *this;
    candidate.setTiming(id, first.start, first.duration + delta, first.in, first.speed, false);
    candidate.setTiming(next, second.start + delta, second.duration - delta, second.in + delta * second.speed,
                        second.speed, false);
    candidate.validate();
    *this = std::move(candidate);
}
void Project::overlapTransition(Id id, Id next, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
        throw std::runtime_error("Durée de transition invalide");
    auto outgoing = linked(id), incoming = linked(next);
    for (auto member : outgoing)
        if (std::find(incoming.begin(), incoming.end(), member) != incoming.end())
            throw std::runtime_error("Deux groupes distincts requis");
    auto first = *findClip(id), second = *findClip(next);
    if (owner(id)->id != owner(next)->id || owner(id)->kind != TrackKind::Video ||
        std::abs(first.start + first.duration - second.start) > 1e-6 || seconds > first.duration)
        throw std::runtime_error(
            "Deux clips vidéo adjacents et une durée contenue dans le premier sont requis");
    auto candidate = *this;
    // Keep the incoming end fixed. Extend its source handle before the cut.
    for (auto member : incoming) {
        auto &clip = *candidate.findClip(member);
        clip.start -= seconds;
        clip.duration += seconds;
        if (!clip.generator && !clip.title.enabled)
            clip.in -= seconds * clip.speed;
        if (candidate.owner(member)->kind == TrackKind::Video) {
            clip.transition.enabled = true;
            clip.transition.id = "transition.crossfade";
            clip.transition.code.clear();
            clip.transition.duration = seconds;
            clip.params.erase("transition_progress");
        } else
            clip.fadeIn = seconds;
    }
    for (auto member : outgoing)
        if (candidate.owner(member)->kind == TrackKind::Audio)
            candidate.findClip(member)->fadeOut = seconds;
    candidate.validate();
    *this = std::move(candidate);
}
void Project::removeRange(double begin, double end) {
    if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end <= begin || end > duration)
        throw std::runtime_error("Intervalle à fermer invalide");
    auto candidate = *this;
    for (auto &track : tracks)
        if (track.locked)
            for (auto &clip : track.clips)
                if (clip.start + clip.duration > begin)
                    throw std::runtime_error("Une piste touchée par l'intervalle est verrouillée");
    for (auto boundary : {end, begin}) {
        std::vector<Id> clips;
        for (auto &track : candidate.tracks)
            for (auto &clip : track.clips)
                if (clip.start + 1e-5 < boundary && clip.start + clip.duration - 1e-5 > boundary)
                    clips.push_back(clip.id);
        if (!clips.empty())
            candidate.splitClips(clips, boundary);
    }
    // Boundary splits preserve links; reject links with incompatible timing instead of silently breaking
    // them.
    std::set<Id> removed;
    for (auto &track : candidate.tracks)
        for (auto &clip : track.clips)
            if (clip.start >= begin - 1e-7 && clip.start + clip.duration <= end + 1e-7)
                removed.insert(clip.id);
    for (auto id : removed)
        for (auto member : candidate.linked(id))
            if (!removed.contains(member))
                throw std::runtime_error("Groupe lié non aligné dans l'intervalle à retirer");
    for (auto &track : candidate.tracks) {
        std::erase_if(track.clips, [&](const auto &clip) { return removed.contains(clip.id); });
        for (auto &clip : track.clips)
            if (clip.start >= end - 1e-7) {
                clip.start -= end - begin;
                shiftKeys(clip, begin - end);
            }
    }
    candidate.duration = std::max(1. / fps, duration - (end - begin));
    candidate.validate();
    *this = std::move(candidate);
}
Id Project::duplicate(Id id) {
    auto ids = linked(id);
    double delta = findClip(id)->duration;
    Id group = ids.size() > 1 ? allocate() : 0, selected = 0;
    for (auto member : ids) {
        auto copy = *findClip(member);
        copy.id = allocate();
        copy.link = group;
        copy.start += delta;
        shiftKeys(copy, delta);
        if (member == id)
            selected = copy.id;
        duration = std::max(duration, copy.start + copy.duration);
        owner(member)->clips.push_back(std::move(copy));
    }
    return selected;
}
Track &Project::addTrack(TrackKind k) {
    Track t;
    t.id = allocate();
    t.kind = k;
    t.name = (k == TrackKind::Audio ? "Audio " : "Vidéo ") + std::to_string(tracks.size() + 1);
    tracks.push_back(t);
    return tracks.back();
}
AudioBus &Project::addBus() {
    AudioBus bus;
    bus.id = allocate();
    bus.name = "Bus " + std::to_string(buses.size() + 1);
    buses.push_back(bus);
    return buses.back();
}
Track *Project::findTrack(Id id) {
    for (auto &track : tracks)
        if (track.id == id)
            return &track;
    return nullptr;
}
AudioBus *Project::findBus(Id id) {
    for (auto &bus : buses)
        if (bus.id == id)
            return &bus;
    return nullptr;
}
const AudioBus *Project::findBus(Id id) const {
    return const_cast<Project *>(this)->findBus(id);
}
std::vector<Id> Project::audioBusOrder() const {
    std::map<Id, int> state;
    std::vector<Id> order;
    std::function<void(Id)> visit = [&](Id id) {
        if (!id)
            return;
        auto *bus = findBus(id);
        if (!bus)
            throw std::runtime_error("Bus audio introuvable");
        if (state[id] == 1)
            throw std::runtime_error("Boucle de routage audio interdite");
        if (state[id] == 2)
            return;
        state[id] = 1;
        visit(bus->mix.output);
        for (auto &send : bus->mix.sends)
            visit(send.bus);
        state[id] = 2;
        order.push_back(id);
    };
    for (auto &bus : buses)
        visit(bus.id);
    std::reverse(order.begin(), order.end());
    return order;
}
void Project::removeBus(Id id) {
    auto *bus = findBus(id);
    if (!bus || bus->locked)
        throw std::runtime_error("Bus introuvable ou verrouillé");
    auto references = [&](const MixChannel &mix) {
        return mix.output == id || std::any_of(mix.sends.begin(), mix.sends.end(),
                                               [&](const AudioSend &send) { return send.bus == id; });
    };
    for (auto &track : tracks)
        if (references(track.mix) && track.locked)
            throw std::runtime_error("Une piste raccordée est verrouillée");
    for (auto &other : buses)
        if (references(other.mix) && other.locked)
            throw std::runtime_error("Un bus raccordé est verrouillé");
    auto disconnect = [&](MixChannel &mix) {
        if (mix.output == id)
            mix.output = 0;
        std::erase_if(mix.sends, [&](const AudioSend &send) { return send.bus == id; });
        mix.params.erase("send." + std::to_string(id));
    };
    for (auto &track : tracks)
        disconnect(track.mix);
    for (auto &other : buses)
        disconnect(other.mix);
    std::erase_if(buses, [&](const AudioBus &entry) { return entry.id == id; });
}
Clip *Project::findClip(Id id) {
    for (auto &t : tracks)
        for (auto &c : t.clips)
            if (c.id == id)
                return &c;
    return nullptr;
}
const Clip *Project::findClip(Id id) const {
    return const_cast<Project *>(this)->findClip(id);
}
Track *Project::owner(Id id) {
    for (auto &t : tracks)
        for (auto &c : t.clips)
            if (c.id == id)
                return &t;
    return nullptr;
}
bool Project::audible(const Track &t) const {
    bool soloed = false;
    for (auto &a : tracks)
        if (a.kind == t.kind && a.solo)
            soloed = true;
    return !t.muted && (!soloed || t.solo);
}
void Project::split(Id id, double time) {
    auto ids = linked(id);
    for (auto member : ids) {
        auto &c = *findClip(member);
        if (!std::isfinite(time) || time <= c.start + 1e-5 || time >= c.start + c.duration - 1e-5)
            throw std::runtime_error("Placer la tête de lecture à l'intérieur de tous les clips liés");
    }
    Id group = ids.size() > 1 ? allocate() : 0;
    for (auto member : ids) {
        auto *c = findClip(member);
        Clip right = *c;
        right.id = allocate();
        right.link = group;
        double left = time - c->start;
        right.start = time;
        right.in += left * c->speed;
        right.duration -= left;
        right.fadeIn = 0;
        right.transition = {};
        right.fadeOut = std::min(right.fadeOut, right.duration);
        c->duration = left;
        c->fadeOut = 0;
        c->fadeIn = std::min(c->fadeIn, left);
        if (c->transition.enabled)
            c->transition.duration = std::min(c->transition.duration, left);
        owner(member)->clips.push_back(std::move(right));
    }
}
void Project::remove(Id id) {
    auto ids = selection({id});
    for (auto &t : tracks)
        std::erase_if(t.clips,
                      [&](const Clip &c) { return std::find(ids.begin(), ids.end(), c.id) != ids.end(); });
}
void Project::makeContainer(Id id) {
    auto *clip = findClip(id);
    if (!clip || owner(id)->kind != TrackKind::Video || owner(id)->locked)
        throw std::runtime_error("Clip vidéo éditable requis");
    if (!clip->composition.IsNull())
        return;
    Project child;
    child.name = clip->name;
    child.width = width;
    child.height = height;
    child.fps = fps;
    child.duration = clip->duration;
    child.timeOrigin = timeOrigin + clip->start;
    child.backgroundAlpha = 0;
    child.workingSpace = workingSpace;
    auto inner = *clip;
    inner.id = child.allocate();
    inner.link = 0;
    shiftKeys(inner, -inner.start);
    inner.start = 0;
    child.addTrack(TrackKind::Video).clips = {inner};
    child.tracks.front().clips.front().transition.enabled = false;
    child.tracks.front().clips.front().widget = Json();
    std::map<std::string, Parameter> layoutParams;
    for (auto &[key, value] : clip->params)
        if (key.starts_with("widget_") || key.starts_with("transition_"))
            layoutParams[key] = value;
    clip->composition = child.json();
    clip->source.clear();
    clip->generator = false;
    clip->title = {};
    clip->mask = {};
    clip->effects.clear();
    clip->params = std::move(layoutParams);
    clip->in = 0;
    clip->speed = 1;
    clip->fadeIn = clip->fadeOut = 0;
}
static Json paramJson(const Parameter &p) {
    Json j = Json::Object();
    j["value"] = p.value;
    j["keys"] = Json::Array();
    for (auto &k : p.keys) {
        Json a = Json::Object();
        a["time"] = k.time;
        a["value"] = k.value;
        a["curve"] = int(k.curve);
        a["shape"] = k.shape;
        j["keys"].Push(a);
    }
    return j;
}
static Json paramsJson(const std::map<std::string, Parameter> &ps) {
    Json j = Json::Object();
    for (auto &[k, p] : ps)
        j[k] = paramJson(p);
    return j;
}
static std::map<std::string, Parameter> readParams(const Json &j) {
    std::map<std::string, Parameter> ps;
    for (auto &[n, a] : j.Items()) {
        Parameter p;
        p.value = a["value"].F();
        for (auto &k : a["keys"].Elements()) {
            int curve = k["curve"].I();
            if (curve < 0 || curve > 3)
                throw std::runtime_error("Interpolation inconnue");
            p.setKey({k["time"].Num(), k["value"].F(), Curve(curve), k["shape"].F()});
        }
        ps[n] = p;
    }
    return ps;
}
static Json mixJson(const MixChannel &mix) {
    Json j = Json::Object();
    j["output"] = mix.output;
    j["params"] = paramsJson(mix.params);
    j["sends"] = Json::Array();
    for (auto &send : mix.sends) {
        Json item = Json::Object();
        item["bus"] = send.bus;
        item["gain"] = send.gain;
        item["preFader"] = send.preFader;
        item["enabled"] = send.enabled;
        j["sends"].Push(item);
    }
    j["effects"] = Json::Array();
    for (auto &effect : mix.effects) {
        Json item = Json::Object();
        item["id"] = effect.id;
        if (externalAudio(effect.id))
            item["code"] = effect.code;
        if (effect.id == "cydaw.graph")
            item["graph"] = effect.graph;
        item["enabled"] = effect.enabled;
        item["params"] = paramsJson(effect.params);
        if (!effect.pluginState.empty())
            item["pluginState"] = effect.pluginState;
        if (!effect.parameterNames.empty()) {
            item["parameterNames"] = Json::Object();
            for (auto &[key, value] : effect.parameterNames)
                item["parameterNames"][key] = value;
        }
        j["effects"].Push(item);
    }
    return j;
}
static MixChannel readMix(const Json &j) {
    MixChannel mix;
    mix.output = j["output"].U();
    mix.params = readParams(j["params"]);
    for (auto &item : j["sends"].Elements())
        mix.sends.push_back(
            {item["bus"].U(), item["gain"].F(-12), item["preFader"].B(), item["enabled"].B(true)});
    for (auto &item : j["effects"].Elements()) {
        Effect effect;
        effect.id = item["id"].S();
        effect.code = item["code"].S();
        effect.graph = item["graph"];
        effect.pluginState = item["pluginState"].S();
        for (auto &[key, value] : item["parameterNames"].Items())
            effect.parameterNames[key] = value.S();
        effect.enabled = item["enabled"].B(true);
        effect.params = readParams(item["params"]);
        mix.effects.push_back(effect);
    }
    return mix;
}
Json Project::json() const {
    Json j = Json::Object();
    j["format"] = "shaderixframe";
    j["schema"] = 10;
    j["name"] = name;
    j["width"] = width;
    j["height"] = height;
    j["fps"] = fps;
    j["duration"] = duration;
    j["timeOrigin"] = timeOrigin;
    j["master"] = master;
    j["color"] = Json::Object();
    j["color"]["space"] = workingSpace;
    j["color"]["tone"] = toneMap;
    j["color"]["exposure"] = exposure;
    j["color"]["backgroundAlpha"] = backgroundAlpha;
    j["imageFormat"] = imageFormat;
    j["masterMix"] = mixJson(masterMix);
    if (!layerGraph.IsNull())
        j["layerGraph"] = layerGraph;
    j["buses"] = Json::Array();
    for (auto &bus : buses) {
        Json item = Json::Object();
        item["id"] = bus.id;
        item["name"] = bus.name;
        item["gain"] = bus.gain;
        item["pan"] = bus.pan;
        item["muted"] = bus.muted;
        item["locked"] = bus.locked;
        item["mix"] = mixJson(bus.mix);
        j["buses"].Push(item);
    }
    j["nextId"] = nextId;
    j["tracks"] = Json::Array();
    for (auto &t : tracks) {
        Json a = Json::Object();
        a["id"] = t.id;
        a["name"] = t.name;
        a["kind"] = int(t.kind);
        a["muted"] = t.muted;
        a["solo"] = t.solo;
        a["locked"] = t.locked;
        a["gain"] = t.gain;
        a["pan"] = t.pan;
        a["mix"] = mixJson(t.mix);
        a["clips"] = Json::Array();
        for (auto &c : t.clips) {
            Json b = Json::Object();
            b["id"] = c.id;
            b["name"] = c.name;
            b["source"] = c.source;
            if (!c.composition.IsNull())
                b["composition"] = c.composition;
            if (!c.widget.IsNull())
                b["widget"] = c.widget;
            b["start"] = c.start;
            b["duration"] = c.duration;
            b["in"] = c.in;
            b["speed"] = c.speed;
            b["link"] = c.link;
            b["fadeIn"] = c.fadeIn;
            b["fadeOut"] = c.fadeOut;
            b["generator"] = c.generator;
            b["sourceColor"] = c.sourceColor;
            b["sourcePremultiplied"] = c.sourcePremultiplied;
            auto &title = b["title"];
            title = Json::Object();
            title["enabled"] = c.title.enabled;
            title["text"] = c.title.text;
            title["font"] = c.title.font;
            title["alignment"] = c.title.alignment;
            title["bold"] = c.title.bold;
            title["italic"] = c.title.italic;
            b["mask"] = Json::Object();
            b["mask"]["shape"] = int(c.mask.shape);
            b["mask"]["inverted"] = c.mask.inverted;
            b["mask"]["points"] = Json::Array();
            for (auto &point : c.mask.points) {
                Json item = Json::Array();
                for (float value : {point.x, point.y, point.inX, point.inY, point.outX, point.outY})
                    item.Push(value);
                b["mask"]["points"].Push(item);
            }
            b["transition"] = Json::Object();
            b["transition"]["enabled"] = c.transition.enabled;
            b["transition"]["id"] = c.transition.id;
            b["transition"]["code"] = c.transition.code;
            b["transition"]["duration"] = c.transition.duration;
            b["params"] = paramsJson(c.params);
            b["effects"] = Json::Array();
            for (auto &e : c.effects) {
                Json f = Json::Object();
                f["id"] = e.id;
                f["enabled"] = e.enabled;
                f["code"] = e.code;
                f["graph"] = e.graph;
                if (e.id == "grading")
                    f["grading"] = e.grading;
                f["params"] = paramsJson(e.params);
                if (!e.pluginState.empty())
                    f["pluginState"] = e.pluginState;
                if (!e.parameterNames.empty()) {
                    f["parameterNames"] = Json::Object();
                    for (auto &[key, value] : e.parameterNames)
                        f["parameterNames"][key] = value;
                }
                b["effects"].Push(f);
            }
            a["clips"].Push(b);
        }
        j["tracks"].Push(a);
    }
    return j;
}
Project Project::fromJson(const Json &j) {
    if (j["format"].S() != "shaderixframe" || j["schema"].I() < 1 || j["schema"].I() > 10)
        throw std::runtime_error("Format ou version de projet non pris en charge");
    Project p;
    p.name = j["name"].S();
    p.width = j["width"].I();
    p.height = j["height"].I();
    p.fps = j["fps"].I();
    p.duration = j["duration"].Num();
    p.timeOrigin = j["timeOrigin"].Num();
    p.master = j["master"].F();
    if (j["schema"].I() >= 7) {
        p.workingSpace = j["color"]["space"].S("srgb");
        p.toneMap = j["color"]["tone"].S("clip");
        p.exposure = j["color"]["exposure"].F();
        p.backgroundAlpha = j["color"]["backgroundAlpha"].F(1);
        p.imageFormat = j["imageFormat"].S("png8");
    }
    p.nextId = j["nextId"].U();
    if (j["schema"].I() >= 5) {
        p.masterMix = readMix(j["masterMix"]);
        for (auto &item : j["buses"].Elements()) {
            AudioBus bus;
            bus.id = item["id"].U();
            bus.name = item["name"].S("Bus");
            bus.gain = item["gain"].F();
            bus.pan = item["pan"].F();
            bus.muted = item["muted"].B();
            bus.locked = item["locked"].B();
            bus.mix = readMix(item["mix"]);
            p.buses.push_back(bus);
        }
    }
    for (auto &a : j["tracks"].Elements()) {
        Track t;
        t.id = a["id"].U();
        t.name = a["name"].S();
        int kind = a["kind"].I();
        if (kind < 0 || kind > 1)
            throw std::runtime_error("Type de piste inconnu");
        t.kind = TrackKind(kind);
        t.muted = a["muted"].B();
        t.solo = a["solo"].B();
        t.locked = a["locked"].B();
        t.gain = a["gain"].F();
        t.pan = a["pan"].F();
        if (j["schema"].I() >= 5)
            t.mix = readMix(a["mix"]);
        for (auto &b : a["clips"].Elements()) {
            Clip c;
            c.id = b["id"].U();
            c.name = b["name"].S();
            c.source = b["source"].S();
            c.composition = b["composition"];
            c.widget = b["widget"];
            c.start = b["start"].Num();
            c.duration = b["duration"].Num();
            c.in = b["in"].Num();
            c.speed = b["speed"].Num(1);
            if (j["schema"].I() >= 2) {
                c.link = b["link"].U();
                c.fadeIn = b["fadeIn"].Num();
                c.fadeOut = b["fadeOut"].Num();
            }
            c.generator = b["generator"].B();
            if (j["schema"].I() >= 7) {
                c.sourceColor = b["sourceColor"].S("auto");
                c.sourcePremultiplied = b["sourcePremultiplied"].B();
            }
            if (j["schema"].I() >= 3) {
                auto &title = b["title"];
                c.title.enabled = title["enabled"].B();
                c.title.text = title["text"].S("Votre titre");
                c.title.font = title["font"].S("Segoe UI");
                c.title.alignment = title["alignment"].I(1);
                c.title.bold = title["bold"].B(true);
                c.title.italic = title["italic"].B();
                int shape = b["mask"]["shape"].I();
                if (shape < 0 || shape > 3)
                    throw std::runtime_error("Forme de masque inconnue");
                c.mask.shape = MaskShape(shape);
                c.mask.inverted = b["mask"]["inverted"].B();
                for (auto &point : b["mask"]["points"].Elements()) {
                    if (point.Size() != 6)
                        throw std::runtime_error("Point Bézier invalide");
                    c.mask.points.push_back({point.At(0).F(), point.At(1).F(), point.At(2).F(),
                                             point.At(3).F(), point.At(4).F(), point.At(5).F()});
                }
            }
            c.params = readParams(b["params"]);
            if (j["schema"].I() >= 4) {
                auto &transition = b["transition"];
                c.transition.enabled = transition["enabled"].B();
                c.transition.id = transition["id"].S("transition.crossfade");
                c.transition.code = transition["code"].S();
                c.transition.duration = transition["duration"].Num(1);
            }
            for (auto &f : b["effects"].Elements()) {
                Effect e;
                e.id = f["id"].S();
                e.enabled = f["enabled"].B(true);
                e.code = f["code"].S();
                e.graph = f["graph"];
                e.grading = f["grading"];
                e.params = readParams(f["params"]);
                e.pluginState = f["pluginState"].S();
                for (auto &[key, value] : f["parameterNames"].Items())
                    e.parameterNames[key] = value.S();
                c.effects.push_back(e);
            }
            t.clips.push_back(c);
        }
        p.tracks.push_back(t);
    }
    p.layerGraph = j["layerGraph"];
    p.validate();
    return p;
}
void Project::validate() const {
    static thread_local unsigned depth = 0;
    if (depth >= 8)
        throw std::runtime_error("Imbrication de clips limitée à huit niveaux");
    struct Guard {
        unsigned &depth;
        Guard(unsigned &d) : depth(d) { ++depth; }
        ~Guard() { --depth; }
    } guard(depth);

    if (!std::isfinite(timeOrigin) || std::abs(timeOrigin) > 86400)
        throw std::runtime_error("Origine temporelle des shaders invalide");
    validateWidgets(*this);
    if (!layerGraph.IsNull())
        decodeLayerGraph(layerGraph, *this);
    auto finite = [](double n) { return std::isfinite(n); };
    auto color = [](const std::string &s) { return s == "srgb" || s == "linear709" || s == "linear2020"; };
    if (!color(workingSpace) || (toneMap != "clip" && toneMap != "reinhard" && toneMap != "aces") ||
        !finite(exposure) || std::abs(exposure) > 20 || !finite(backgroundAlpha) || backgroundAlpha < 0 ||
        backgroundAlpha > 1 || (imageFormat != "png8" && imageFormat != "png16" && imageFormat != "exr16"))
        throw std::runtime_error("Gestion couleur invalide");
    if (width < 64 || height < 64 || width > 8192 || height > 8192 || width % 2 || height % 2 || fps < 1 ||
        fps > 120 || !finite(duration) || duration <= 0 || duration > 86400 || !finite(master) ||
        master < -80 || master > 12)
        throw std::runtime_error("Réglages de séquence invalides (dimensions paires, 1–120 fps)");
    std::set<Id> ids;
    Id largest = 0;
    auto idCheck = [&](Id id) {
        if (!id || !ids.insert(id).second)
            throw std::runtime_error("Identifiant manquant ou dupliqué");
        largest = std::max(id, largest);
    };
    auto checkParams = [&](const auto &ps) {
        for (auto &[n, p] : ps) {
            if (n.empty() || !finite(p.value))
                throw std::runtime_error("Paramètre invalide");
            double previous = -std::numeric_limits<double>::infinity();
            for (auto &k : p.keys) {
                if (!finite(k.time) || k.time <= previous || !finite(k.value) || !finite(k.shape))
                    throw std::runtime_error("Animation invalide");
                previous = k.time;
            }
        }
    };
    auto checkMix = [&](const MixChannel &mix, bool masterChannel = false) {
        if (mix.effects.size() > 64 || mix.sends.size() > 64 ||
            (masterChannel && (mix.output || !mix.sends.empty())))
            throw std::runtime_error("Canal de mixage invalide");
        if (mix.output && !findBus(mix.output))
            throw std::runtime_error("Sortie audio introuvable");
        std::set<Id> destinations;
        for (auto &send : mix.sends)
            if (!send.bus || !findBus(send.bus) || !destinations.insert(send.bus).second ||
                !finite(send.gain) || send.gain < -80 || send.gain > 12)
                throw std::runtime_error("Envoi audio invalide");
        auto rangedParams = [&](const auto &params, auto range) {
            checkParams(params);
            for (auto &[key, parameter] : params) {
                auto [low, high] = range(key);
                auto check = [&](float value) {
                    if (value < low || value > high)
                        throw std::runtime_error("Paramètre de mixage hors plage : " + key);
                };
                check(parameter.value);
                for (auto &frame : parameter.keys)
                    check(frame.value);
            }
        };
        rangedParams(mix.params, [&](const std::string &key) -> std::pair<float, float> {
            if (key == "gain")
                return {-80, 12};
            if (key == "pan")
                return {-1, 1};
            for (auto &send : mix.sends)
                if (key == "send." + std::to_string(send.bus))
                    return {-80, 12};
            throw std::runtime_error("Paramètre de canal inconnu : " + key);
        });
        for (auto &effect : mix.effects) {
            if (!effect.pluginState.empty()) {
                if (effect.id != "vst3")
                    throw std::runtime_error("État opaque incompatible avec cet effet");
                decodeBase64(effect.pluginState);
            }
            if (effect.parameterNames.size() > 512)
                throw std::runtime_error("Trop de noms de paramètres plugin");
            for (auto &[key, value] : effect.parameterNames)
                if (!externalAudio(effect.id) || !externalParameter(key) || value.size() > 256)
                    throw std::runtime_error("Nom de paramètre plugin invalide");
            auto *definition = audioEffect(effect.id);
            if (!definition || (!externalAudio(effect.id) && !effect.code.empty()) ||
                (effect.id != "cydaw.graph" && !effect.graph.IsNull()) ||
                (externalAudio(effect.id) && effect.code.empty()))
                throw std::runtime_error("DSP de mixage inconnu ou incompatible");
            if (effect.id == "cydaw.graph")
                decodeDspGraph(effect.graph);
            rangedParams(effect.params, [&](const std::string &key) -> std::pair<float, float> {
                if (externalAudio(effect.id) && externalParameter(key))
                    return {0, 1};
                for (auto &parameter : definition->parameters)
                    if (key == parameter.key)
                        return {parameter.minimum, parameter.maximum};
                throw std::runtime_error("Paramètre DSP inconnu : " + key);
            });
        }
    };
    checkMix(masterMix, true);
    if (buses.size() > 64)
        throw std::runtime_error("64 bus audio maximum");
    for (auto &bus : buses) {
        idCheck(bus.id);
        if (bus.name.empty() || bus.name.size() > 256 || !finite(bus.gain) || bus.gain < -80 ||
            bus.gain > 12 || !finite(bus.pan) || std::abs(bus.pan) > 1)
            throw std::runtime_error("Bus audio invalide");
        checkMix(bus.mix);
    }
    audioBusOrder();
    for (auto &t : tracks) {
        idCheck(t.id);
        checkMix(t.mix);
        if (t.kind != TrackKind::Audio &&
            (t.mix.output || !t.mix.sends.empty() || !t.mix.effects.empty() || !t.mix.params.empty()))
            throw std::runtime_error("Une piste vidéo ne peut pas héberger un canal audio");
        if (!finite(t.gain) || t.gain < -80 || t.gain > 12 || !finite(t.pan) || std::abs(t.pan) > 1)
            throw std::runtime_error("Mixage invalide");
        for (auto &c : t.clips) {
            idCheck(c.id);
            largest = std::max(largest, c.link);
            if (!finite(c.start) || c.start < 0 || !finite(c.duration) || c.duration < 1e-5 ||
                !finite(c.in) || c.in < 0 || !finite(c.speed) || c.speed < 0.05 || c.speed > 16 ||
                !finite(c.fadeIn) || c.fadeIn < 0 || c.fadeIn > c.duration || !finite(c.fadeOut) ||
                c.fadeOut < 0 || c.fadeOut > c.duration)
                throw std::runtime_error("Temps de clip invalide");
            checkParams(c.params);
            if (!c.composition.IsNull()) {
                if (t.kind != TrackKind::Video || c.generator || c.title.enabled || !c.source.empty() ||
                    c.composition.Dump(0).size() > 16 * 1024 * 1024)
                    throw std::runtime_error("Conteneur vidéo invalide");
                Project::fromJson(c.composition);
            }

            if (c.sourceColor != "auto" && c.sourceColor != "logc3-awg3-ei800" && !color(c.sourceColor))
                throw std::runtime_error("Espace couleur du média inconnu");
            if (!finite(c.transition.duration) || c.transition.duration < 1e-5 ||
                (c.transition.enabled && (t.kind != TrackKind::Video || c.transition.id.empty() ||
                                          c.transition.duration > c.duration)))
                throw std::runtime_error("Transition vidéo invalide");
            if (c.title.enabled &&
                (t.kind != TrackKind::Video || c.generator || !c.source.empty() ||
                 c.title.text.size() > 16384 || c.title.font.empty() || c.title.font.size() > 256 ||
                 c.title.alignment < 0 || c.title.alignment > 2))
                throw std::runtime_error("Titre invalide");
            if (int(c.mask.shape) < 0 || int(c.mask.shape) > 3 ||
                (c.mask.shape != MaskShape::None && t.kind != TrackKind::Video))
                throw std::runtime_error("Masque vidéo invalide");
            if (c.mask.points.size() > 16 || (c.mask.shape == MaskShape::Bezier && c.mask.points.size() < 3))
                throw std::runtime_error("3 à 16 points Bézier requis");
            for (auto &point : c.mask.points)
                for (float value : {point.x, point.y, point.inX, point.inY, point.outX, point.outY})
                    if (!finite(value) || std::abs(value) > 2)
                        throw std::runtime_error("Coordonnée Bézier hors plage");
            auto range = [&](const char *key, float low, float high) {
                auto it = c.params.find(key);
                if (it == c.params.end())
                    return;
                auto check = [&](float v) {
                    if (v < low || v > high)
                        throw std::runtime_error(std::string("Paramètre hors plage : ") + key);
                };
                check(it->second.value);
                for (auto &keyframe : it->second.keys)
                    check(keyframe.value);
            };
            range("transition_progress", 0, 1);
            range("transition_softness", 0, 1);
            range("transition_direction", 0, 3);
            range("font_size", 4, 1024);
            range("preserve_pitch", 0, 1);
            range("stretch_mode", 0, 3);
            range("text_outline", 0, 16);
            range("text_shadow", 0, 1);
            range("text_shadow_blur", 0, 32);
            range("text_shadow_x", -256, 256);
            range("text_shadow_y", -256, 256);
            for (auto key : {"text_r", "text_g", "text_b", "text_a"})
                range(key, 0, 1);
            range("mask_x", -2, 2);
            range("mask_y", -2, 2);
            range("mask_width", .001f, 4);
            range("mask_height", .001f, 4);
            range("mask_feather", 0, 1024);
            range("mask_rotation", -36000, 36000);
            for (auto &e : c.effects) {
                checkParams(e.params);
                if (e.id == "grading") {
                    if (t.kind != TrackKind::Video)
                        throw std::runtime_error("Étalonnage : clip vidéo requis");
                    validateGrade(e);
                }
                if (e.id == "ofx") {
                    if (t.kind != TrackKind::Video)
                        throw std::runtime_error("OFX exige un clip vidéo");
                    validateOfx(e);
                }
                if (e.id == "cydaw.graph" || externalAudio(e.id)) {
                    if (t.kind != TrackKind::Audio)
                        throw std::runtime_error("Un DSP exige un clip audio");
                    MixChannel chain;
                    chain.effects = {e};
                    checkMix(chain);
                }
            }
        }
    }
    if (nextId <= largest)
        throw std::runtime_error("Compteur d'identifiants invalide");
}
void Project::save(const std::string &path) const {
    validate();
    AtomicFile output(path);
    {
        std::ofstream f(output.path(), std::ios::binary);
        if (!f)
            throw std::runtime_error("Impossible d'écrire le projet");
        f << json().Dump(2);
        f.flush();
        if (!f)
            throw std::runtime_error("Écriture du projet échouée");
    }
    output.commit();
}
Project Project::load(const std::string &path) {
    std::string text;
    if (!sx::ReadTextFile(path, text))
        throw std::runtime_error("Projet introuvable");
    Json j;
    std::string error;
    if (!Json::Parse(text, j, &error))
        throw std::runtime_error(error);
    auto p = fromJson(j);
    auto base = sx::Utf8Path(path).parent_path();
    for (auto &t : p.tracks)
        for (auto &c : t.clips) {
            if (!c.composition.IsNull()) {
                auto child = Project::fromJson(c.composition);
                auto resolve = [&](auto &&self, Project &scope) -> void {
                    auto plugins = [&](MixChannel &mix) {
                        for (auto &e : mix.effects)
                            if (externalAudio(e.id) && sx::Utf8Path(e.code).is_relative())
                                e.code = sx::Narrow((base / sx::Utf8Path(e.code)).wstring());
                    };
                    plugins(scope.masterMix);
                    for (auto &b : scope.buses)
                        plugins(b.mix);
                    for (auto &t : scope.tracks)
                        plugins(t.mix);
                    for (auto &track : scope.tracks)
                        for (auto &clip : track.clips) {
                            if (!clip.source.empty() && sx::Utf8Path(clip.source).is_relative())
                                clip.source = sx::Narrow((base / sx::Utf8Path(clip.source)).wstring());
                            for (auto &e : clip.effects)
                                if ((externalAudio(e.id) || e.id == "ofx") &&
                                    sx::Utf8Path(e.code).is_relative())
                                    e.code = sx::Narrow((base / sx::Utf8Path(e.code)).wstring());
                            if (!clip.composition.IsNull()) {
                                auto inner = Project::fromJson(clip.composition);
                                self(self, inner);
                                clip.composition = inner.json();
                            }
                        }
                };
                resolve(resolve, child);
                c.composition = child.json();
            }
            if (!c.source.empty() && sx::Utf8Path(c.source).is_relative())
                c.source = sx::Narrow((base / sx::Utf8Path(c.source)).wstring());
            for (auto &effect : c.effects)
                if ((externalAudio(effect.id) || effect.id == "ofx") &&
                    sx::Utf8Path(effect.code).is_relative())
                    effect.code = sx::Narrow((base / sx::Utf8Path(effect.code)).wstring());
        }
    auto plugins = [&](MixChannel &mix) {
        for (auto &effect : mix.effects)
            if (externalAudio(effect.id) && sx::Utf8Path(effect.code).is_relative())
                effect.code = sx::Narrow((base / sx::Utf8Path(effect.code)).wstring());
    };
    for (auto &t : p.tracks)
        plugins(t.mix);
    for (auto &bus : p.buses)
        plugins(bus.mix);
    plugins(p.masterMix);
    return p;
}
Project Project::demo() {
    Project p;
    p.name = "Première lumière";
    auto &t = p.addTrack(TrackKind::Video);
    t.name = "01  /  Lumière procédurale";
    Clip c;
    c.id = p.allocate();
    c.name = "Aurora · générateur HLSL";
    c.duration = 12;
    c.generator = true;
    c.params["opacity"] = {1};
    Effect color;
    color.id = "color";
    color.params["exposure"] = {0.2f};
    color.params["saturation"] = {1.1f};
    c.effects.push_back(color);
    t.clips.push_back(c);
    auto &sound = p.addTrack(TrackKind::Audio);
    sound.name = "02  /  Bande sonore";
    return p;
}
void History::checkpoint(const Project &p) {
    past.push_back(p);
    if (past.size() > 64)
        past.erase(past.begin());
    future.clear();
}
bool History::undo(Project &p) {
    if (past.empty())
        return false;
    future.push_back(p);
    p = past.back();
    past.pop_back();
    return true;
}
bool History::redo(Project &p) {
    if (future.empty())
        return false;
    past.push_back(p);
    p = future.back();
    future.pop_back();
    return true;
}
} // namespace sf

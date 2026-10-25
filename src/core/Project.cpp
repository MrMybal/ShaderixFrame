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
        throw std::runtime_error("Invalid animation point");
    auto i = std::lower_bound(keys.begin(), keys.end(), k.time, [](auto &a, double t) { return a.time < t; });
    if (i != keys.end() && std::abs(i->time - k.time) < 1e-7)
        *i = k;
    else
        keys.insert(i, k);
}
bool Clip::ramped() const {
    auto it = params.find("speed_ramp");
    return it != params.end() && !it->second.keys.empty();
}
double Clip::sourceTime(double time) const {
    if (hold)
        return in;
    double from = reverse ? time : start, to = reverse ? start + duration : time;
    double elapsed = to - from;
    if (auto it = params.find("speed_ramp"); it != params.end() && !it->second.keys.empty() && elapsed > 0) {
        // Trapezoids at 240 per second at most; the ramp cannot run the source backwards.
        auto &ramp = it->second;
        int steps = std::clamp(int(std::ceil(elapsed * 240)), 1, 4096);
        double h = elapsed / steps, sum = 0, previous = std::max(0.f, ramp.at(from));
        for (int i = 1; i <= steps; ++i) {
            double next = std::max(0.f, ramp.at(from + h * i));
            sum += (previous + next) * .5 * h;
            previous = next;
        }
        elapsed = sum;
    }
    return reverse ? in + std::max(0., elapsed * speed - 1e-4) : in + elapsed * speed;
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
        throw std::runtime_error("Clip not found");
    std::vector<Id> ids;
    for (auto &track : tracks)
        for (auto &clip : track.clips)
            if (clip.id == id || (selected->link && clip.link == selected->link)) {
                if (editable && track.locked)
                    throw std::runtime_error("A track in the linked group is locked");
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
        throw std::runtime_error("Invalid selection or movement");
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
// Markers follow a closed span: those inside it gather at its start, the later ones move back; a
// region loses the part of its length inside the span.
static void closeMarkers(std::vector<Marker> &markers, double begin, double end) {
    auto closed = [&](double t) { return t >= end - 1e-7 ? t - (end - begin) : t > begin ? begin : t; };
    for (auto &marker : markers) {
        double last = closed(marker.end());
        marker.time = closed(marker.time);
        marker.duration = marker.duration > 0 ? std::max(0., last - marker.time) : 0;
    }
}
// Markers follow an opened span: the later ones move on, a region across it grows.
static void openMarkers(std::vector<Marker> &markers, double at, double length) {
    for (auto &marker : markers)
        if (marker.time >= at - 1e-7)
            marker.time += length;
        else if (marker.duration > 0 && marker.end() > at + 1e-7)
            marker.duration += length;
}
void Project::removeClips(const std::vector<Id> &clips, bool ripple) {
    auto ids = selection(clips);
    if (ids.empty())
        throw std::runtime_error("Empty selection");
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
                    throw std::runtime_error("An unselected clip crosses the gap to close: " +
                                             c.name);
                if (c.start >= end - 1e-7)
                    shift -= end - begin;
            }
            if (shift != 0 && track.locked)
                throw std::runtime_error("A track to move is locked: " + track.name);
            if (c.link) {
                auto [entry, inserted] = groupOffsets.emplace(c.link, shift);
                if (!inserted && std::abs(entry->second - shift) > 1e-7)
                    throw std::runtime_error("A linked group crosses the gap to close");
            }
            c.start = std::max(0., c.start + shift);
            shiftKeys(c, shift);
        }
        std::erase_if(track.clips, [&](const Clip &c) { return removed.contains(c.id); });
    }
    for (auto it = intervals.rbegin(); it != intervals.rend(); ++it) {
        candidate.duration -= it->second - it->first;
        closeMarkers(candidate.markers, it->first, it->second);
    }
    candidate.duration = std::max(1. / fps, candidate.duration);
    candidate.validate();
    *this = std::move(candidate);
}
std::vector<Id> Project::duplicateClips(const std::vector<Id> &clips) {
    auto ids = selection(clips);
    if (ids.empty())
        throw std::runtime_error("Empty selection");
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
        throw std::runtime_error("Invalid selection or time");
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
        throw std::runtime_error("No selected clip crosses the playhead");
    candidate.validate();
    *this = std::move(candidate);
}
void Project::linkClips(Id id, Id other) {
    auto ids = linked(id), others = linked(other);
    if (id == other)
        throw std::runtime_error("Choose two different clips");
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
        throw std::runtime_error("Invalid trim");
    auto members = linked(id);
    auto candidate = *this;
    auto original = *findClip(id);
    double boundary = original.start + original.duration;
    std::set<Id> selected(members.begin(), members.end());
    for (auto member : members) {
        auto &clip = *candidate.findClip(member);
        if (std::abs(clip.start + clip.duration - boundary) > 1e-6)
            throw std::runtime_error("Linked clips must have aligned ends for a ripple trim");
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
                throw std::runtime_error("A clip crosses the ripple point; split it first");
            if (shift && track.locked)
                throw std::runtime_error("A track affected by the ripple move is locked");
            if (clip.link) {
                auto [it, added] = shifts.emplace(clip.link, shift);
                if (!added && std::abs(it->second - shift) > 1e-7)
                    throw std::runtime_error("The linked group crosses the ripple point");
            }
            clip.start += shift;
            shiftKeys(clip, shift);
        }
    candidate.duration = std::max(1. / fps, candidate.duration + delta);
    if (delta < 0)
        closeMarkers(candidate.markers, boundary + delta, boundary);
    else
        openMarkers(candidate.markers, boundary, delta);
    candidate.validate();
    *this = std::move(candidate);
}
void Project::trimClip(Id id, double time, bool head, bool ripple) {
    auto *found = findClip(id);
    if (!found || !std::isfinite(time))
        throw std::runtime_error("Invalid trim");
    auto c = *found;
    if (time <= c.start + 1e-7 || time >= c.start + c.duration - 1e-7)
        throw std::runtime_error("Time must be inside the clip");
    if (owner(id)->locked)
        throw std::runtime_error("Track is locked");
    double cut = head ? time - c.start : c.start + c.duration - time;
    if (!ripple) {
        setTiming(id, head ? time : c.start, c.duration - cut, c.in, c.speed, false);
        return;
    }
    auto candidate = *this;
    candidate.rippleTrim(id, -cut, head);
    if (head)
        for (auto member : candidate.linked(id))
            shiftKeys(*candidate.findClip(member), -cut);
    *this = std::move(candidate);
}
std::vector<std::string> Project::renameClips(const std::vector<Id> &clips, const std::string &pattern,
                                              int first) {
    if (pattern.empty() || pattern.size() > 200)
        throw std::runtime_error("Invalid name template");
    std::vector<std::pair<std::pair<double, int>, Id>> order;
    for (int t = 0; t < int(tracks.size()); ++t)
        for (auto &clip : tracks[size_t(t)].clips)
            if (std::find(clips.begin(), clips.end(), clip.id) != clips.end())
                order.push_back({{clip.start, t}, clip.id});
    if (order.empty())
        throw std::runtime_error("Empty selection");
    std::sort(order.begin(), order.end());
    auto candidate = *this;
    std::vector<std::string> names;
    int number = first;
    for (auto &[place, id] : order) {
        auto &clip = *candidate.findClip(id);
        std::string name;
        for (size_t i = 0; i < pattern.size();) {
            if (pattern[i] == '#') {
                size_t run = pattern.find_first_not_of('#', i);
                size_t width = (run == std::string::npos ? pattern.size() : run) - i;
                auto digits = std::to_string(number);
                name += std::string(width > digits.size() ? width - digits.size() : 0, '0') + digits;
                i += width;
            } else if (pattern.compare(i, 5, "{nom}") == 0) {
                name += clip.name;
                i += 5;
            } else
                name += pattern[i++];
        }
        clip.name = name.substr(0, 256);
        names.push_back(clip.name);
        ++number;
    }
    candidate.validate();
    *this = std::move(candidate);
    return names;
}
std::vector<Subtitle> parseSrt(const std::string &source) {
    auto text = source;
    if (text.starts_with("\xEF\xBB\xBF"))
        text.erase(0, 3);
    std::erase(text, '\r');
    auto stamp = [](const std::string &value, double &seconds) {
        int h = 0, m = 0, s = 0, ms = 0;
        char separator = 0;
        if (sscanf(value.c_str(), " %d:%d:%d%c%d", &h, &m, &s, &separator, &ms) != 5 ||
            (separator != ',' && separator != '.'))
            return false;
        seconds = h * 3600. + m * 60. + s + ms / 1000.;
        return true;
    };
    std::vector<Subtitle> cues;
    std::vector<std::string> lines;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        lines.push_back(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
        if (end == std::string::npos)
            break;
        at = end + 1;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        auto arrow = lines[i].find("-->");
        if (arrow == std::string::npos)
            continue;
        Subtitle cue;
        if (!stamp(lines[i].substr(0, arrow), cue.start) || !stamp(lines[i].substr(arrow + 3), cue.end))
            continue;
        // Text until the blank line; simple tags such as <i> are dropped.
        for (++i; i < lines.size() && !lines[i].empty(); ++i) {
            std::string line;
            bool tag = false;
            for (char c : lines[i]) {
                if (c == '<' || c == '{')
                    tag = true;
                else if (c == '>' || c == '}')
                    tag = false;
                else if (!tag)
                    line += c;
            }
            cue.text += (cue.text.empty() ? "" : "\n") + line;
        }
        if (cue.end > cue.start && !cue.text.empty())
            cues.push_back(std::move(cue));
    }
    std::stable_sort(cues.begin(), cues.end(), [](const Subtitle &a, const Subtitle &b) { return a.start < b.start; });
    return cues;
}
// The subtitle look: low in the frame, outlined, regular weight.
static Clip subtitleClip(Id id, const std::string &text) {
    Clip c;
    c.id = id;
    c.title.enabled = true;
    c.title.text = text.substr(0, 4096);
    c.title.bold = false;
    c.name = c.title.text.substr(0, c.title.text.find('\n')).substr(0, 48);
    c.params["font_size"] = {40};
    c.params["y"] = {.38f};
    c.params["text_outline"] = {2};
    for (auto key : {"text_r", "text_g", "text_b", "text_a"})
        c.params[key] = {1};
    return c;
}
Id Project::addSubtitle(Id trackId, double start, double length, const std::string &text) {
    auto *track = findTrack(trackId);
    if (!track || track->kind != TrackKind::Video || track->locked)
        throw std::runtime_error("Choose an available video track for subtitles");
    if (!std::isfinite(start) || !std::isfinite(length) || start < 0 || length <= 0)
        throw std::runtime_error("Invalid subtitle cue");
    start = std::round(start * fps) / fps;
    const Clip *before = nullptr;
    double next = 1e30;
    for (auto &clip : track->clips) {
        if (start >= clip.start - 1e-7 && start < clip.start + clip.duration - 1e-7)
            throw std::runtime_error("A subtitle cue already occupies this time");
        if (clip.start > start)
            next = std::min(next, clip.start);
        else if (clip.title.enabled && (!before || clip.start > before->start))
            before = &clip;
    }
    double end = std::min(start + length, next);
    auto candidate = *this;
    Clip c = subtitleClip(candidate.allocate(), text.empty() ? "Subtitle cue" : text);
    if (before) {
        // The look of the cue before: font, weight and the text parameters.
        c.title.font = before->title.font;
        c.title.bold = before->title.bold;
        c.title.italic = before->title.italic;
        c.title.alignment = before->title.alignment;
        for (auto &[key, param] : before->params)
            if (key == "font_size" || key == "x" || key == "y" || key.starts_with("text_"))
                c.params[key] = Parameter{param.value};
    }
    c.start = start;
    c.duration = std::max(1. / fps, std::round(end * fps) / fps - start);
    Id id = c.id;
    candidate.findTrack(trackId)->clips.push_back(std::move(c));
    candidate.duration = std::max(candidate.duration, start + length);
    candidate.validate();
    *this = std::move(candidate);
    return id;
}
int Project::setSubtitleStyle(Id trackId, const std::map<std::string, float> &values) {
    auto *track = findTrack(trackId);
    if (!track)
        throw std::runtime_error("Track not found");
    auto candidate = *this;
    int count = 0;
    for (auto &clip : candidate.findTrack(trackId)->clips)
        if (clip.title.enabled) {
            for (auto &[key, value] : values)
                clip.params[key] = Parameter{value};
            ++count;
        }
    if (!count)
        throw std::runtime_error("No title clips on this track");
    candidate.validate();
    *this = std::move(candidate);
    return count;
}
Id Project::importSubtitles(const std::vector<Subtitle> &cues, const std::string &trackName) {
    if (cues.empty())
        throw std::runtime_error("No readable subtitles");
    auto candidate = *this;
    auto &track = candidate.addTrack(TrackKind::Video);
    track.name = trackName;
    Id trackId = track.id;
    for (size_t i = 0; i < cues.size(); ++i) {
        // A cue that runs into the next one stops there: one track holds no overlap.
        double end = i + 1 < cues.size() ? std::min(cues[i].end, cues[i + 1].start) : cues[i].end;
        double start = std::round(cues[i].start * fps) / fps;
        double length = std::round(end * fps) / fps - start;
        if (length < 1. / fps)
            continue;
        Clip c = subtitleClip(candidate.allocate(), cues[i].text);
        c.start = start;
        c.duration = length;
        candidate.findTrack(trackId)->clips.push_back(std::move(c));
        candidate.duration = std::max(candidate.duration, start + length);
    }
    appendLayerSurface(candidate, trackId, trackName);
    std::rotate(candidate.tracks.begin(), candidate.tracks.end() - 1, candidate.tracks.end());
    candidate.validate();
    *this = std::move(candidate);
    return trackId;
}
std::string Project::subtitlesSrt(Id trackId) const {
    std::vector<const Clip *> titles;
    for (auto &track : tracks)
        if (!trackId || track.id == trackId)
            for (auto &clip : track.clips)
                if (clip.title.enabled && clip.enabled)
                    titles.push_back(&clip);
    std::stable_sort(titles.begin(), titles.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
    auto stamp = [](double seconds) {
        long ms = long(std::llround(std::max(0., seconds) * 1000));
        char text[32];
        snprintf(text, sizeof(text), "%02ld:%02ld:%02ld,%03ld", ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
        return std::string(text);
    };
    std::string out;
    int index = 0;
    for (auto *clip : titles)
        out += std::to_string(++index) + "\n" + stamp(clip->start) + " --> " + stamp(clip->start + clip->duration) +
               "\n" + clip->title.text + "\n\n";
    return out;
}
std::vector<std::string> Project::mediaPaths() const {
    std::vector<std::string> paths = bin;
    for (auto &track : tracks)
        for (auto &clip : track.clips)
            if (!clip.source.empty() && std::find(paths.begin(), paths.end(), clip.source) == paths.end())
                paths.push_back(clip.source);
    return paths;
}
int Project::relinkMedia(const std::map<std::string, std::string> &moves) {
    int changed = 0;
    auto candidate = *this;
    for (auto &path : candidate.bin)
        if (auto it = moves.find(path); it != moves.end())
            path = it->second;
    for (auto &track : candidate.tracks)
        for (auto &clip : track.clips)
            if (auto it = moves.find(clip.source); it != moves.end()) {
                clip.source = it->second;
                ++changed;
            }
    candidate.validate();
    *this = std::move(candidate);
    return changed;
}
int Project::pasteAttributes(const Clip &source, TrackKind sourceKind, const std::vector<Id> &targets,
                             const AttributeChoice &choice) {
    auto candidate = *this;
    int pasted = 0;
    auto shifted = [](Parameter param, double delta) {
        for (auto &key : param.keys)
            key.time += delta;
        return param;
    };
    for (auto id : targets) {
        auto *target = candidate.findClip(id);
        if (!target || id == source.id || candidate.owner(id)->locked)
            continue;
        bool sameKind = candidate.owner(id)->kind == sourceKind;
        double delta = target->start - source.start;
        // A parameter the source does not set returns to its default on the target.
        auto carry = [&](const std::string &key) {
            if (auto it = source.params.find(key); it != source.params.end())
                target->params[key] = shifted(it->second, delta);
            else
                target->params.erase(key);
        };
        if (choice.transform && sameKind && sourceKind == TrackKind::Video)
            for (auto key : {"x", "y", "scale", "rotation"})
                carry(key);
        if (choice.opacity && sameKind && sourceKind == TrackKind::Video)
            carry("opacity");
        if (choice.audio && sameKind && sourceKind == TrackKind::Audio)
            for (auto key : {"gain", "pan"})
                carry(key);
        if (choice.mask && sameKind && sourceKind == TrackKind::Video) {
            target->mask = source.mask;
            std::erase_if(target->params, [](const auto &entry) { return entry.first.starts_with("mask_"); });
            for (auto &[key, param] : source.params)
                if (key.starts_with("mask_"))
                    target->params[key] = shifted(param, delta);
        }
        if (choice.effects && sameKind) {
            if (choice.replaceEffects)
                target->effects.clear();
            for (auto effect : source.effects) {
                for (auto &[key, param] : effect.params)
                    param = shifted(param, delta);
                target->effects.push_back(std::move(effect));
            }
        }
        ++pasted;
    }
    if (!pasted)
        throw std::runtime_error("No editable clips in the selection");
    candidate.validate();
    *this = std::move(candidate);
    return pasted;
}
std::vector<Id> Project::anglesAt(double time) const {
    std::vector<Id> angles;
    for (auto &track : tracks) {
        if (track.kind != TrackKind::Video || track.locked)
            continue;
        for (auto &clip : track.clips)
            if (clip.active(time) && !clip.title.enabled && !clip.adjustment) {
                angles.push_back(clip.id);
                break;
            }
    }
    return angles;
}
void Project::switchAngle(double time, int angle, bool followSound) {
    auto angles = anglesAt(time);
    if (angle < 1 || angle > int(angles.size()))
        throw std::runtime_error("Angle " + std::to_string(angle) + " missing: " + std::to_string(angles.size()) +
                                 " angle(s) at the playhead");
    auto candidate = *this;
    for (int index = 0; index < int(angles.size()); ++index) {
        Id trackId = candidate.owner(angles[size_t(index)])->id;
        if (auto *clip = candidate.findClip(angles[size_t(index)]); clip->start < time - 1e-6)
            candidate.splitClips({clip->id}, time);
        // The piece that starts at the time carries the switch until the next one.
        Id started = 0;
        for (auto &piece : candidate.findTrack(trackId)->clips)
            if (std::abs(piece.start - time) < 1e-6 && !piece.title.enabled && !piece.adjustment) {
                piece.enabled = index + 1 == angle;
                started = piece.id;
            }
        // The cut also split the linked sound; its piece from the time follows the picture.
        if (followSound && started)
            for (auto member : candidate.linked(started))
                if (auto *sound = candidate.findClip(member); sound && member != started)
                    sound->enabled = index + 1 == angle;
    }
    candidate.validate();
    *this = std::move(candidate);
}
int Project::closeGaps(Id id) {
    if (!id) {
        // Spans where no track has a clip, closed from the last one so the earlier stay in place.
        std::vector<std::pair<double, double>> spans, gaps;
        for (auto &track : tracks)
            for (auto &clip : track.clips)
                spans.push_back({clip.start, clip.start + clip.duration});
        std::sort(spans.begin(), spans.end());
        double reach = 0;
        for (auto [begin, end] : spans) {
            if (begin > reach + 1e-6)
                gaps.push_back({reach, begin});
            reach = std::max(reach, end);
        }
        if (gaps.empty())
            return 0;
        auto candidate = *this;
        for (auto it = gaps.rbegin(); it != gaps.rend(); ++it)
            candidate.removeRange(it->first, it->second);
        *this = std::move(candidate);
        return int(gaps.size());
    }
    auto *track = findTrack(id);
    if (!track)
        throw std::runtime_error("Track not found");
    if (track->locked)
        throw std::runtime_error("The track is locked");
    std::vector<const Clip *> order;
    for (auto &clip : track->clips)
        order.push_back(&clip);
    std::sort(order.begin(), order.end(), [](auto *a, auto *b) { return a->start < b->start; });
    // Each clip moves by the sum of the gaps before it; its linked members move by the same amount.
    std::map<Id, double> moves;
    double reach = 0, shift = 0;
    int gaps = 0;
    for (auto *clip : order) {
        double start = clip->start + shift;
        if (start > reach + 1e-6) {
            shift -= start - reach;
            start = reach;
            ++gaps;
        }
        if (shift < 0)
            for (auto member : linked(clip->id)) {
                auto [it, inserted] = moves.emplace(member, shift);
                if (!inserted && std::abs(it->second - shift) > 1e-7)
                    throw std::runtime_error("Two clips in the same linked group cannot have different offsets");
            }
        reach = std::max(reach, start + clip->duration);
    }
    if (!gaps)
        return 0;
    auto candidate = *this;
    for (auto &[member, delta] : moves) {
        auto &clip = *candidate.findClip(member);
        clip.start += delta;
        shiftKeys(clip, delta);
    }
    for (auto &other : candidate.tracks) {
        if (other.id == id)
            continue;
        for (auto &moved : other.clips)
            if (moves.contains(moved.id))
                for (auto &still : other.clips)
                    if (!moves.contains(still.id) && still.start < moved.start + moved.duration - 1e-6 &&
                        still.start + still.duration > moved.start + 1e-6)
                        throw std::runtime_error("A linked clip would overlap ' " + still.name + " ' on its track");
    }
    candidate.validate();
    *this = std::move(candidate);
    return gaps;
}
int Project::fitToMarkers(const std::vector<Id> &clips, const std::function<double(const Clip &)> &available) {
    std::vector<const Clip *> chosen;
    for (auto id : clips)
        if (auto *clip = findClip(id))
            chosen.push_back(clip);
    if (chosen.empty())
        throw std::runtime_error("Choose clips");
    auto earliest = *std::min_element(chosen.begin(), chosen.end(),
                                      [](auto *a, auto *b) { return a->start < b->start; });
    Id track = owner(earliest->id)->id;
    std::erase_if(chosen, [&](auto *clip) { return owner(clip->id)->id != track; });
    std::sort(chosen.begin(), chosen.end(), [](auto *a, auto *b) { return a->start < b->start; });
    double first = chosen.front()->start;
    std::vector<double> beats{first};
    for (auto &marker : markers)
        if (marker.time > first + 1e-6)
            beats.push_back(marker.time);
    std::sort(beats.begin() + 1, beats.end());
    if (beats.size() < 2)
        throw std::runtime_error("No markers after the first clip");
    auto candidate = *this;
    std::set<Id> moved;
    double cursor = first;
    for (size_t i = 0; i < chosen.size(); ++i) {
        auto original = *chosen[i];
        double start = i < beats.size() ? beats[i] : cursor;
        double length = i + 1 < beats.size() ? beats[i + 1] - start : original.duration;
        length = std::min(length, available(original));
        if (!(length > 1e-6))
            throw std::runtime_error("The source of ' " + original.name + " » is too short");
        cursor = start + length;
        candidate.duration = std::max(candidate.duration, cursor);
        candidate.setTiming(original.id, start, length, original.in, original.speed);
        for (auto member : candidate.linked(original.id))
            moved.insert(member);
    }
    for (auto &t : candidate.tracks)
        for (auto &clip : t.clips)
            if (moved.contains(clip.id))
                for (auto &still : t.clips)
                    if (!moved.contains(still.id) && still.start < clip.start + clip.duration - 1e-6 &&
                        still.start + still.duration > clip.start + 1e-6)
                        throw std::runtime_error("« " + still.name + " ' occupies this space: select it too or move it");
    candidate.validate();
    *this = std::move(candidate);
    return int(chosen.size());
}
int Project::splitAtMarkers(const std::vector<Id> &clips) {
    auto candidate = *this;
    std::set<Id> targets;
    if (clips.empty()) {
        for (auto &track : candidate.tracks)
            if (!track.locked)
                for (auto &clip : track.clips)
                    targets.insert(clip.id);
    } else
        for (auto id : candidate.selection(clips))
            targets.insert(id);
    int cuts = 0;
    for (auto &marker : markers) {
        std::vector<Id> across;
        std::set<Id> existing;
        for (auto &track : candidate.tracks)
            for (auto &clip : track.clips) {
                existing.insert(clip.id);
                if (targets.contains(clip.id) && !track.locked && clip.start + 1e-5 < marker.time &&
                    clip.start + clip.duration - 1e-5 > marker.time)
                    across.push_back(clip.id);
            }
        if (across.empty())
            continue;
        candidate.splitClips(across, marker.time);
        ++cuts;
        // The pieces after the cut are new clips: they are cut again at the following markers.
        for (auto &track : candidate.tracks)
            for (auto &clip : track.clips)
                if (!existing.contains(clip.id))
                    targets.insert(clip.id);
    }
    candidate.validate();
    *this = std::move(candidate);
    return cuts;
}
void Project::insertGap(double time, double length) {
    if (!std::isfinite(time) || !std::isfinite(length) || time < 0 || time > duration || length <= 0 ||
        duration + length > 86400)
        throw std::runtime_error("Invalid gap to insert");
    for (auto &track : tracks)
        if (track.locked)
            for (auto &clip : track.clips)
                if (clip.start + clip.duration > time + 1e-7)
                    throw std::runtime_error("A locked track has clips after this point");
    auto candidate = *this;
    std::vector<Id> across;
    for (auto &track : candidate.tracks)
        for (auto &clip : track.clips)
            if (clip.start + 1e-5 < time && clip.start + clip.duration - 1e-5 > time)
                across.push_back(clip.id);
    if (!across.empty())
        candidate.splitClips(across, time);
    for (auto &track : candidate.tracks)
        for (auto &clip : track.clips)
            if (clip.start >= time - 1e-7) {
                clip.start += length;
                shiftKeys(clip, length);
            }
    openMarkers(candidate.markers, time, length);
    candidate.duration += length;
    candidate.validate();
    *this = std::move(candidate);
}
Id Project::freezeFrame(Id id, double time, double length) {
    auto *found = findClip(id);
    if (!found || owner(id)->kind != TrackKind::Video)
        throw std::runtime_error("Choose a video clip");
    auto original = *found;
    if (time < original.start - 1e-7 || time >= original.start + original.duration - 1e-7)
        throw std::runtime_error("The playhead must be on the clip");
    Id track = owner(id)->id;
    auto candidate = *this;
    candidate.insertGap(time, length);
    Clip held = original;
    held.id = candidate.allocate();
    held.name = original.name + " (freeze)";
    held.link = 0;
    held.start = time;
    held.duration = length;
    held.in = original.sourceTime(time);
    held.speed = 1;
    held.hold = true;
    held.fadeIn = held.fadeOut = 0;
    held.transition.enabled = false;
    auto freeze = [&](std::map<std::string, Parameter> &params) {
        for (auto &[key, param] : params) {
            param.value = param.at(time);
            param.keys.clear();
        }
    };
    freeze(held.params);
    for (auto &effect : held.effects)
        freeze(effect.params);
    auto &clips = candidate.findTrack(track)->clips;
    auto at = std::find_if(clips.begin(), clips.end(), [&](const Clip &c) { return c.start > time; });
    Id heldId = held.id;
    clips.insert(at, std::move(held));
    candidate.validate();
    *this = std::move(candidate);
    return heldId;
}
void Project::replaceSource(Id id, const std::string &path, double in, double mediaLength, bool hasVideo,
                            bool hasAudio) {
    auto *found = findClip(id);
    if (!found || found->source.empty() || path.empty() || !std::isfinite(in) || in < 0)
        throw std::runtime_error("Invalid replacement");
    if (owner(id)->locked)
        throw std::runtime_error("Track is locked");
    auto original = found->source;
    auto name = sx::Narrow(sx::Utf8Path(path).stem().wstring());
    auto candidate = *this;
    for (auto member : linked(id)) {
        auto &clip = *candidate.findClip(member);
        bool video = candidate.owner(member)->kind == TrackKind::Video;
        if (clip.source != original || (video ? !hasVideo : !hasAudio))
            continue;
        if (mediaLength > 0 && in + clip.duration * clip.speed > mediaLength + 1e-6)
            throw std::runtime_error("The media is too short for the clip duration from this point");
        clip.source = path;
        clip.in = in;
        clip.name = name;
    }
    candidate.validate();
    *this = std::move(candidate);
}
void Project::rollTrim(Id id, Id next, double delta) {
    if (!std::isfinite(delta) || id == next)
        throw std::runtime_error("Invalid roll");
    auto outgoing = linked(id), incoming = linked(next);
    for (auto member : outgoing)
        if (std::find(incoming.begin(), incoming.end(), member) != incoming.end())
            throw std::runtime_error("Two different groups required");
    auto first = *findClip(id), second = *findClip(next);
    if (owner(id)->id != owner(next)->id || std::abs(first.start + first.duration - second.start) > 1e-6)
        throw std::runtime_error("Choose two adjacent clips on the same track");
    auto candidate = *this;
    candidate.setTiming(id, first.start, first.duration + delta, first.in, first.speed, false);
    candidate.setTiming(next, second.start + delta, second.duration - delta, second.in + delta * second.speed,
                        second.speed, false);
    candidate.validate();
    *this = std::move(candidate);
}
void Project::overlapTransition(Id id, Id next, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
        throw std::runtime_error("Invalid transition duration");
    auto outgoing = linked(id), incoming = linked(next);
    for (auto member : outgoing)
        if (std::find(incoming.begin(), incoming.end(), member) != incoming.end())
            throw std::runtime_error("Two different groups required");
    auto first = *findClip(id), second = *findClip(next);
    if (owner(id)->id != owner(next)->id || owner(id)->kind != TrackKind::Video ||
        std::abs(first.start + first.duration - second.start) > 1e-6 || seconds > first.duration)
        throw std::runtime_error(
            "Two adjacent video clips and a duration within the first clip are required");
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
        throw std::runtime_error("Invalid range to close");
    auto candidate = *this;
    for (auto &track : tracks)
        if (track.locked)
            for (auto &clip : track.clips)
                if (clip.start + clip.duration > begin)
                    throw std::runtime_error("A track affected by the range is locked");
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
                throw std::runtime_error("A linked group is not aligned within the range to remove");
    for (auto &track : candidate.tracks) {
        std::erase_if(track.clips, [&](const auto &clip) { return removed.contains(clip.id); });
        for (auto &clip : track.clips)
            if (clip.start >= end - 1e-7) {
                clip.start -= end - begin;
                shiftKeys(clip, begin - end);
            }
    }
    candidate.duration = std::max(1. / fps, duration - (end - begin));
    closeMarkers(candidate.markers, begin, end);
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
    t.name = (k == TrackKind::Audio ? "Audio " : "Video ") + std::to_string(tracks.size() + 1);
    tracks.push_back(t);
    return tracks.back();
}
void Project::removeTrack(Id id) {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const Track &t) { return t.id == id; });
    if (it == tracks.end())
        throw std::runtime_error("Track not found");
    if (it->locked)
        throw std::runtime_error("Track is locked");
    std::vector<Id> partners;
    for (auto &clip : it->clips)
        if (clip.link)
            for (auto &other : tracks)
                if (&other != &*it)
                    for (auto &candidate : other.clips)
                        if (candidate.link == clip.link)
                            partners.push_back(candidate.id);
    // Le graphe des calques explicite garde ses branches : la surface retirée devient une entrée vide.
    if (it->kind == TrackKind::Video && !layerGraph.IsNull()) {
        std::set<Id> removed;
        for (auto &node : layerGraph["nodes"].Elements())
            if (node["kind"].S() == "surface" && node["surface"].U() == id)
                removed.insert(node["id"].U());
        auto nodes = Json::Array();
        for (auto node : layerGraph["nodes"].Elements()) {
            if (removed.count(node["id"].U()))
                continue;
            auto inputs = Json::Array();
            for (auto &input : node["inputs"].Elements())
                inputs.Push(removed.count(input.U()) ? Json(sx::u32(0)) : input);
            node["inputs"] = inputs;
            nodes.Push(node);
        }
        layerGraph["nodes"] = nodes;
        if (removed.count(layerGraph["output"].U()))
            layerGraph = Json();
    }
    tracks.erase(it);
    std::erase_if(groups, [&](const TrackGroup &g) {
        return std::none_of(tracks.begin(), tracks.end(), [&](const Track &t) { return t.group == g.id; });
    });
    // Un groupe lié qui n'a plus qu'un membre n'est plus un groupe.
    for (auto partner : partners)
        if (auto *clip = findClip(partner); clip && linked(partner, false).size() < 2)
            clip->link = 0;
}
Track &Project::duplicateTrack(Id id) {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const Track &t) { return t.id == id; });
    if (it == tracks.end())
        throw std::runtime_error("Track not found");
    auto candidate = *this;
    Track copy = *candidate.findTrack(id);
    copy.id = candidate.allocate();
    copy.name = copy.name + " (copy)";
    copy.locked = false;
    for (auto &clip : copy.clips) {
        clip.id = candidate.allocate();
        clip.link = 0;
    }
    auto at = std::find_if(candidate.tracks.begin(), candidate.tracks.end(), [&](const Track &t) { return t.id == id; });
    auto inserted = candidate.tracks.insert(at + 1, std::move(copy));
    Id copyId = inserted->id;
    if (inserted->kind == TrackKind::Video)
        appendLayerSurface(candidate, copyId, inserted->name);
    candidate.validate();
    *this = std::move(candidate);
    return *findTrack(copyId);
}
void Project::moveTrack(Id id, int index) {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const Track &t) { return t.id == id; });
    if (it == tracks.end())
        throw std::runtime_error("Track not found");
    index = std::clamp(index, 0, int(tracks.size()) - 1);
    Track moved = std::move(*it);
    tracks.erase(it);
    tracks.insert(tracks.begin() + index, std::move(moved));
}
std::string Project::markerChapters() const {
    auto clock = [](double seconds) {
        long total = long(std::floor(seconds + 1e-6));
        char text[32];
        if (total >= 3600)
            snprintf(text, sizeof(text), "%ld:%02ld:%02ld", total / 3600, total / 60 % 60, total % 60);
        else
            snprintf(text, sizeof(text), "%ld:%02ld", total / 60, total % 60);
        return std::string(text);
    };
    std::string text;
    if (markers.empty() || markers.front().time >= 1)
        text = "0:00 " + name + "\n";
    for (auto &marker : markers)
        text += clock(marker.time) + " " + marker.name + "\n";
    return text;
}
// Timecode HH:MM:SS:FF at the project's frame rate (non drop frame).
static std::string frameCode(double seconds, int fps) {
    long frames = long(std::llround(std::max(0., seconds) * fps));
    char text[32];
    snprintf(text, sizeof(text), "%02ld:%02ld:%02ld:%02ld", frames / fps / 3600, frames / fps / 60 % 60, frames / fps % 60,
             frames % fps);
    return text;
}
std::vector<EdlEvent> parseEdl(const std::string &source, int fps) {
    auto text = source;
    std::erase(text, '\r');
    auto seconds = [&](const std::string &code, double &value) {
        int h = 0, m = 0, s = 0, f = 0;
        char a = 0, b = 0, c = 0;
        if (sscanf(code.c_str(), "%d%c%d%c%d%c%d", &h, &a, &m, &b, &s, &c, &f) != 7)
            return false;
        value = h * 3600. + m * 60. + s + double(f) / std::max(1, fps);
        return true;
    };
    std::vector<EdlEvent> events;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        auto line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? text.size() : end + 1;
        char number[16] = {}, reel[64] = {}, channels[16] = {}, kind[16] = {}, a[16] = {}, b[16] = {}, c[16] = {},
             d[16] = {};
        if (line.size() > 4 && std::isdigit((unsigned char)line[0]) &&
            sscanf(line.c_str(), "%15s %63s %15s %15s %15s %15s %15s %15s", number, reel, channels, kind, a, b, c, d) == 8) {
            // Cuts only: a dissolve line carries a duration before its timecodes, which shifts the fields.
            EdlEvent event;
            event.reel = reel;
            event.channels = channels;
            if (seconds(a, event.sourceIn) && seconds(b, event.sourceOut) && seconds(c, event.recordIn) &&
                seconds(d, event.recordOut) && event.recordOut > event.recordIn) {
                event.speed = std::max(.05, (event.sourceOut - event.sourceIn) / (event.recordOut - event.recordIn));
                events.push_back(event);
            }
        } else if (line.starts_with("M2") && !events.empty()) {
            // Speed in frames per second of source, for the event just above.
            char speedReel[64] = {};
            double framesPerSecond = 0;
            if (sscanf(line.c_str(), "M2 %63s %lf", speedReel, &framesPerSecond) == 2 && framesPerSecond > 0)
                events.back().speed = framesPerSecond / std::max(1, fps);
        } else if (line.starts_with("* FROM CLIP NAME:") && !events.empty()) {
            events.back().clipName = line.substr(17);
            events.back().clipName.erase(0, events.back().clipName.find_first_not_of(' '));
        } else if (line.starts_with("* SOURCE FILE:") && !events.empty()) {
            events.back().sourceFile = line.substr(14);
            events.back().sourceFile.erase(0, events.back().sourceFile.find_first_not_of(' '));
        }
    }
    return events;
}
std::string Project::edl(Id trackId) const {
    const Track *track = nullptr;
    for (auto &t : tracks)
        if (t.kind == TrackKind::Video && (trackId ? t.id == trackId : !t.clips.empty()) && !track)
            track = &t;
    if (!track)
        throw std::runtime_error("No video track to export");
    std::vector<const Clip *> clips;
    for (auto &clip : track->clips)
        if (clip.enabled)
            clips.push_back(&clip);
    std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
    std::string out = "TITLE: " + name + "\nFCM: NON-DROP FRAME\n\n";
    int event = 0;
    for (auto *clip : clips) {
        bool black = clip->source.empty();
        bool sounding = false;
        for (auto member : linked(clip->id, false))
            if (member != clip->id && owner(member)->kind == TrackKind::Audio)
                sounding = true;
        // Reel names stay within the eight characters CMX allows; the file name goes in a comment.
        auto reel = black ? std::string("BL") : "AX";
        double sourceIn = black ? 0 : clip->in, sourceOut = sourceIn + clip->duration * (black ? 1 : clip->speed);
        char line[160];
        snprintf(line, sizeof(line), "%03d  %-8s %-5s C        %s %s %s %s\n", ++event, reel.c_str(),
                 sounding ? "B" : "V", frameCode(sourceIn, fps).c_str(), frameCode(sourceOut, fps).c_str(),
                 frameCode(clip->start, fps).c_str(), frameCode(clip->start + clip->duration, fps).c_str());
        out += line;
        if (!black && std::abs(clip->speed - 1) > 1e-6) {
            snprintf(line, sizeof(line), "M2   %-8s %05.1f    %s\n", reel.c_str(), clip->speed * fps,
                     frameCode(sourceIn, fps).c_str());
            out += line;
        }
        out += "* FROM CLIP NAME: " + clip->name + "\n";
        if (!black)
            out += "* SOURCE FILE: " + clip->source + "\n";
        if (clip->transition.enabled)
            out += "* TRANSITION: " + std::to_string(long(std::llround(clip->transition.duration * fps))) + " FRAMES\n";
        out += "\n";
    }
    return out;
}
std::string Project::shotListCsv() const {
    auto quote = [](const std::string &field) {
        std::string out = "\"";
        for (char c : field)
            out += c == '"' ? std::string("\"\"") : std::string(1, c);
        return out + "\"";
    };
    std::string text = "No,Track,Name,Start,End,Duration,Source,Source in,Source out,Speed %,Effects,Enabled\n";
    int number = 0;
    for (auto &track : tracks) {
        std::vector<const Clip *> clips;
        for (auto &clip : track.clips)
            clips.push_back(&clip);
        std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
        for (auto *clip : clips) {
            char speed[16];
            snprintf(speed, sizeof(speed), "%.0f", clip->speed * 100);
            text += std::to_string(++number) + "," + quote(track.name) + "," + quote(clip->name) + "," +
                    frameCode(clip->start, fps) + "," + frameCode(clip->start + clip->duration, fps) + "," +
                    frameCode(clip->duration, fps) + "," + quote(clip->source) + "," +
                    (clip->source.empty() ? "" : frameCode(clip->in, fps)) + "," +
                    (clip->source.empty() ? "" : frameCode(clip->in + clip->duration * clip->speed, fps)) + "," +
                    speed + "," + std::to_string(clip->effects.size()) + "," + (clip->enabled ? "oui" : "non") + "\n";
        }
    }
    return text;
}
std::string Project::markerCsv() const {
    auto quote = [](const std::string &field) {
        std::string out = "\"";
        for (char c : field)
            out += c == '"' ? std::string("\"\"") : std::string(1, c);
        return out + "\"";
    };
    std::string text = "Name,Timecode,Seconds,Color,Note,Duration\n";
    for (auto &marker : markers) {
        long frames = long(std::llround(marker.time * fps));
        char code[48], seconds[32], colour[16], length[32] = "";
        if (marker.duration > 0)
            snprintf(length, sizeof(length), "%.3f", marker.duration);
        snprintf(code, sizeof(code), "%02ld:%02ld:%02ld:%02ld", frames / fps / 3600, frames / fps / 60 % 60,
                 frames / fps % 60, frames % fps);
        snprintf(seconds, sizeof(seconds), "%.3f", marker.time);
        snprintf(colour, sizeof(colour), "%06X", unsigned(marker.color));
        text += quote(marker.name) + "," + code + "," + seconds + "," + (marker.color ? colour : "") + "," +
                quote(marker.note) + "," + length + "\n";
    }
    return text;
}
Marker &Project::addMarker(double time, const std::string &label) {
    Marker marker;
    marker.id = allocate();
    marker.time = time;
    marker.name = label.empty() ? "Marker " + std::to_string(markers.size() + 1) : label;
    auto at = std::upper_bound(markers.begin(), markers.end(), time,
                               [](double t, const Marker &m) { return t < m.time; });
    return *markers.insert(at, marker);
}
void Project::removeMarker(Id id) {
    if (!std::erase_if(markers, [&](const Marker &m) { return m.id == id; }))
        throw std::runtime_error("Marker not found");
}
TrackGroup *Project::findGroup(Id id) {
    for (auto &group : groups)
        if (group.id == id)
            return &group;
    return nullptr;
}
TrackGroup &Project::groupTracks(const std::vector<Id> &ids, const std::string &label) {
    std::vector<Id> members;
    for (auto &track : tracks)
        if (std::find(ids.begin(), ids.end(), track.id) != ids.end())
            members.push_back(track.id);
    if (members.empty() || members.size() != std::set<Id>(ids.begin(), ids.end()).size())
        throw std::runtime_error("Tracks for the group were not found");
    TrackGroup group;
    group.id = allocate();
    group.name = label.empty() ? "Group " + std::to_string(groups.size() + 1) : label;
    groups.push_back(group);
    auto at = [&](Id id) {
        return std::find_if(tracks.begin(), tracks.end(), [&](const Track &t) { return t.id == id; });
    };
    for (size_t i = 1; i < members.size(); ++i) {
        auto it = at(members[i]);
        Track moved = std::move(*it);
        tracks.erase(it);
        tracks.insert(at(members[i - 1]) + 1, std::move(moved));
    }
    for (auto id : members)
        findTrack(id)->group = group.id;
    // Groups left without members disappear.
    std::erase_if(groups, [&](const TrackGroup &g) {
        return std::none_of(tracks.begin(), tracks.end(), [&](const Track &t) { return t.group == g.id; });
    });
    return *findGroup(group.id);
}
std::vector<Id> Project::placeSource(const SourceEdit &edit) {
    double span = edit.out - edit.in, length = edit.fill > 0 ? edit.fill : span;
    if (edit.path.empty() || !std::isfinite(edit.in) || !std::isfinite(edit.out) || !std::isfinite(edit.at) ||
        !std::isfinite(edit.fill) || edit.in < 0 || edit.at < 0 || span < 1. / fps - 1e-9 ||
        length < 1. / fps - 1e-9 || (!edit.video && !edit.audio))
        throw std::runtime_error("Invalid source range");
    double speed = span / length;
    if (speed < .05 || speed > 20)
        throw std::runtime_error("Fit-to-fill speed is out of range (x0.05 to x20)");
    auto candidate = *this;
    auto &p = candidate;
    auto target = [&](TrackKind kind, Id wanted) -> Track & {
        if (wanted) {
            auto *track = p.findTrack(wanted);
            if (!track || track->kind != kind || track->locked)
                throw std::runtime_error("Target track is missing, locked, or has a different type");
            return *track;
        }
        for (auto &track : p.tracks)
            if (track.kind == kind && !track.locked)
                return track;
        p.addTrack(kind);
        if (kind != TrackKind::Video)
            return p.tracks.back();
        std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end()); // new picture tracks go on top
        return p.tracks.front();
    };
    auto crossing = [&](double time, const std::function<bool(const Track &)> &concerned) {
        std::vector<Id> ids;
        for (auto &track : p.tracks)
            if (concerned(track))
                for (auto &clip : track.clips)
                    if (time > clip.start + 1e-5 && time < clip.start + clip.duration - 1e-5)
                        ids.push_back(clip.id);
        return ids;
    };
    Id videoId = edit.video ? target(TrackKind::Video, edit.videoTrack).id : 0;
    Id audioId = edit.audio ? target(TrackKind::Audio, edit.audioTrack).id : 0;
    auto isTarget = [&](const Track &track) { return track.id == videoId || track.id == audioId; };
    if (edit.insert) {
        // Insert: cut everything at the edit point, then push what follows by the inserted length.
        if (auto cut = crossing(edit.at, [](const Track &) { return true; }); !cut.empty())
            p.splitClips(cut, edit.at);
        std::vector<Id> later;
        for (auto &track : p.tracks)
            for (auto &clip : track.clips)
                if (clip.start >= edit.at - 1e-7)
                    later.push_back(clip.id);
        if (!later.empty())
            p.moveClips(later, length);
    } else {
        // Overwrite: the span is cleared on the target tracks only.
        p.clearRange(edit.at, edit.at + length, isTarget);
    }
    std::vector<Id> placed;
    auto name = edit.name.empty() ? sx::Narrow(sx::Utf8Path(edit.path).filename().wstring()) : edit.name;
    for (Id track : {videoId, audioId})
        if (track) {
            Clip c;
            c.id = p.allocate();
            c.name = name;
            c.source = edit.path;
            c.start = edit.at;
            c.duration = length;
            c.in = edit.in;
            c.speed = speed;
            p.findTrack(track)->clips.push_back(c);
            placed.push_back(c.id);
        }
    if (placed.size() == 2)
        p.linkClips(placed[0], placed[1]);
    p.duration = std::max(p.duration, edit.at + length);
    if (std::find(p.bin.begin(), p.bin.end(), edit.path) == p.bin.end())
        p.bin.push_back(edit.path);
    p.validate();
    *this = std::move(candidate);
    return placed;
}
void Project::clearRange(double begin, double end, const std::function<bool(const Track &)> &concerned) {
    if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end <= begin)
        throw std::runtime_error("Invalid range");
    auto candidate = *this;
    for (auto &track : candidate.tracks)
        if (concerned(track) && track.locked)
            for (auto &clip : track.clips)
                if (clip.start < end - 1e-7 && clip.start + clip.duration > begin + 1e-7)
                    throw std::runtime_error("A track affected by the range is locked");
    for (double boundary : {begin, end}) {
        std::vector<Id> cut;
        for (auto &track : candidate.tracks)
            if (concerned(track))
                for (auto &clip : track.clips)
                    if (boundary > clip.start + 1e-5 && boundary < clip.start + clip.duration - 1e-5)
                        cut.push_back(clip.id);
        if (!cut.empty())
            candidate.splitClips(cut, boundary);
    }
    std::vector<Id> partners;
    for (auto &track : candidate.tracks)
        if (concerned(track))
            std::erase_if(track.clips, [&](const Clip &clip) {
                bool inside = clip.start >= begin - 1e-7 && clip.start + clip.duration <= end + 1e-7;
                if (inside && clip.link)
                    partners.push_back(clip.link);
                return inside;
            });
    // Links left with a single member are dissolved.
    for (auto group : partners) {
        std::vector<Clip *> members;
        for (auto &track : candidate.tracks)
            for (auto &clip : track.clips)
                if (clip.link == group)
                    members.push_back(&clip);
        if (members.size() == 1)
            members[0]->link = 0;
    }
    candidate.validate();
    *this = std::move(candidate);
}
void Project::liftRange(double begin, double end) {
    clearRange(begin, end, [](const Track &) { return true; });
}
bool Project::gapAt(Id id, double time, double &begin, double &end) const {
    auto *track = const_cast<Project *>(this)->findTrack(id);
    if (!track)
        return false;
    begin = 0;
    end = -1;
    for (auto &clip : track->clips) {
        if (time >= clip.start && time < clip.start + clip.duration)
            return false;
        if (clip.start + clip.duration <= time + 1e-9)
            begin = std::max(begin, clip.start + clip.duration);
        if (clip.start >= time - 1e-9 && (end < 0 || clip.start < end))
            end = clip.start;
    }
    return end > begin + 1e-6;
}
bool Project::spanEmpty(double begin, double end) const {
    for (auto &track : tracks)
        for (auto &clip : track.clips)
            if (clip.start < end - 1e-6 && clip.start + clip.duration > begin + 1e-6)
                return false;
    return true;
}
void Project::ungroup(Id id) {
    if (!findGroup(id))
        throw std::runtime_error("Group not found");
    for (auto &track : tracks)
        if (track.group == id)
            track.group = 0;
    std::erase_if(groups, [&](const TrackGroup &g) { return g.id == id; });
}
Marker *Project::findMarker(Id id) {
    for (auto &marker : markers)
        if (marker.id == id)
            return &marker;
    return nullptr;
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
            throw std::runtime_error("Audio bus not found");
        if (state[id] == 1)
            throw std::runtime_error("Audio routing loops are not allowed");
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
        throw std::runtime_error("Bus not found or locked");
    auto references = [&](const MixChannel &mix) {
        return mix.output == id || std::any_of(mix.sends.begin(), mix.sends.end(),
                                               [&](const AudioSend &send) { return send.bus == id; });
    };
    for (auto &track : tracks)
        if (references(track.mix) && track.locked)
            throw std::runtime_error("A connected track is locked");
    for (auto &other : buses)
        if (references(other.mix) && other.locked)
            throw std::runtime_error("A connected bus is locked");
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
            throw std::runtime_error("Place the playhead inside all linked clips");
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
        throw std::runtime_error("An editable video clip is required");
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
                throw std::runtime_error("Unknown interpolation");
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
        if (t.color)
            a["color"] = t.color;
        if (t.height > 0)
            a["height"] = t.height;
        if (t.group)
            a["group"] = t.group;
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
            if (!c.enabled)
                b["enabled"] = false;
            if (c.hold)
                b["hold"] = true;
            if (c.reverse)
                b["reverse"] = true;
            if (c.adjustment)
                b["adjustment"] = true;
            if (c.color)
                b["color"] = c.color;
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
    if (!bin.empty()) {
        j["bin"] = Json::Array();
        for (auto &path : bin)
            j["bin"].Push(path);
    }
    if (!groups.empty()) {
        j["groups"] = Json::Array();
        for (auto &group : groups) {
            Json g = Json::Object();
            g["id"] = group.id;
            g["name"] = group.name;
            if (group.color)
                g["color"] = group.color;
            g["collapsed"] = group.collapsed;
            j["groups"].Push(g);
        }
    }
    if (!markers.empty()) {
        j["markers"] = Json::Array();
        for (auto &marker : markers) {
            Json m = Json::Object();
            m["id"] = marker.id;
            m["time"] = marker.time;
            m["name"] = marker.name;
            if (marker.color)
                m["color"] = marker.color;
            if (!marker.note.empty())
                m["note"] = marker.note;
            if (marker.duration > 0)
                m["duration"] = marker.duration;
            j["markers"].Push(m);
        }
    }
    if (hasWorkArea()) {
        j["workArea"] = Json::Array();
        j["workArea"].Push(workIn);
        j["workArea"].Push(workOut);
    }
    return j;
}
Project Project::fromJson(const Json &j) {
    if (j["format"].S() != "shaderixframe" || j["schema"].I() < 1 || j["schema"].I() > 10)
        throw std::runtime_error("Unsupported project format or version");
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
            throw std::runtime_error("Unknown track type");
        t.kind = TrackKind(kind);
        t.muted = a["muted"].B();
        t.solo = a["solo"].B();
        t.locked = a["locked"].B();
        t.gain = a["gain"].F();
        t.pan = a["pan"].F();
        t.color = a["color"].U();
        t.height = a["height"].F();
        t.group = a["group"].U();
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
            c.enabled = b["enabled"].B(true);
        c.hold = b["hold"].B(false);
        c.reverse = b["reverse"].B(false);
        c.adjustment = b["adjustment"].B(false);
            c.color = b["color"].U();
            if (j["schema"].I() >= 7) {
                c.sourceColor = b["sourceColor"].S("auto");
                c.sourcePremultiplied = b["sourcePremultiplied"].B();
            }
            if (j["schema"].I() >= 3) {
                auto &title = b["title"];
                c.title.enabled = title["enabled"].B();
                c.title.text = title["text"].S("Your title");
                c.title.font = title["font"].S("Segoe UI");
                c.title.alignment = title["alignment"].I(1);
                c.title.bold = title["bold"].B(true);
                c.title.italic = title["italic"].B();
                int shape = b["mask"]["shape"].I();
                if (shape < 0 || shape > 3)
                    throw std::runtime_error("Unknown mask shape");
                c.mask.shape = MaskShape(shape);
                c.mask.inverted = b["mask"]["inverted"].B();
                for (auto &point : b["mask"]["points"].Elements()) {
                    if (point.Size() != 6)
                        throw std::runtime_error("Invalid Bezier point");
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
    for (auto &path : j["bin"].Elements())
        p.bin.push_back(path.S());
    for (auto &g : j["groups"].Elements()) {
        TrackGroup group;
        group.id = g["id"].U();
        group.name = g["name"].S("Group");
        group.color = g["color"].U();
        group.collapsed = g["collapsed"].B();
        p.groups.push_back(group);
    }
    for (auto &m : j["markers"].Elements()) {
        Marker marker;
        marker.id = m["id"].U();
        marker.time = m["time"].Num();
        marker.name = m["name"].S();
        marker.color = m["color"].U();
        marker.note = m["note"].S();
        marker.duration = m["duration"].Num(0);
        p.markers.push_back(marker);
    }
    std::stable_sort(p.markers.begin(), p.markers.end(),
                     [](const Marker &a, const Marker &b) { return a.time < b.time; });
    if (j["workArea"].Size() == 2) {
        p.workIn = j["workArea"].At(0).Num();
        p.workOut = j["workArea"].At(1).Num();
    }
    p.layerGraph = j["layerGraph"];
    p.validate();
    return p;
}
void Project::validate() const {
    static thread_local unsigned depth = 0;
    if (depth >= 8)
        throw std::runtime_error("Clip nesting is limited to eight levels");
    struct Guard {
        unsigned &depth;
        Guard(unsigned &d) : depth(d) { ++depth; }
        ~Guard() { --depth; }
    } guard(depth);

    if (!std::isfinite(timeOrigin) || std::abs(timeOrigin) > 86400)
        throw std::runtime_error("Invalid shader time origin");
    validateWidgets(*this);
    if (!layerGraph.IsNull())
        decodeLayerGraph(layerGraph, *this);
    auto finite = [](double n) { return std::isfinite(n); };
    auto color = [](const std::string &s) { return s == "srgb" || s == "linear709" || s == "linear2020"; };
    if (!color(workingSpace) || (toneMap != "clip" && toneMap != "reinhard" && toneMap != "aces") ||
        !finite(exposure) || std::abs(exposure) > 20 || !finite(backgroundAlpha) || backgroundAlpha < 0 ||
        backgroundAlpha > 1 || (imageFormat != "png8" && imageFormat != "png16" && imageFormat != "exr16"))
        throw std::runtime_error("Invalid color management");
    if (width < 64 || height < 64 || width > 8192 || height > 8192 || width % 2 || height % 2 || fps < 1 ||
        fps > 120 || !finite(duration) || duration <= 0 || duration > 86400 || !finite(master) ||
        master < -80 || master > 12)
        throw std::runtime_error("Invalid sequence settings (even dimensions, 1-120 fps)");
    std::set<Id> ids;
    Id largest = 0;
    auto idCheck = [&](Id id) {
        if (!id || !ids.insert(id).second)
            throw std::runtime_error("Missing or duplicate ID");
        largest = std::max(id, largest);
    };
    auto checkParams = [&](const auto &ps) {
        for (auto &[n, p] : ps) {
            if (n.empty() || !finite(p.value))
                throw std::runtime_error("Invalid parameter");
            double previous = -std::numeric_limits<double>::infinity();
            for (auto &k : p.keys) {
                if (!finite(k.time) || k.time <= previous || !finite(k.value) || !finite(k.shape))
                    throw std::runtime_error("Invalid animation");
                previous = k.time;
            }
        }
    };
    auto checkMix = [&](const MixChannel &mix, bool masterChannel = false) {
        if (mix.effects.size() > 64 || mix.sends.size() > 64 ||
            (masterChannel && (mix.output || !mix.sends.empty())))
            throw std::runtime_error("Invalid mix channel");
        if (mix.output && !findBus(mix.output))
            throw std::runtime_error("Audio output not found");
        std::set<Id> destinations;
        for (auto &send : mix.sends)
            if (!send.bus || !findBus(send.bus) || !destinations.insert(send.bus).second ||
                !finite(send.gain) || send.gain < -80 || send.gain > 12)
                throw std::runtime_error("Invalid audio send");
        auto rangedParams = [&](const auto &params, auto range) {
            checkParams(params);
            for (auto &[key, parameter] : params) {
                auto [low, high] = range(key);
                auto check = [&](float value) {
                    if (value < low || value > high)
                        throw std::runtime_error("Mix parameter out of range: " + key);
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
            throw std::runtime_error("Unknown channel parameter: " + key);
        });
        for (auto &effect : mix.effects) {
            if (!effect.pluginState.empty()) {
                if (effect.id != "vst3")
                    throw std::runtime_error("Opaque state is incompatible with this effect");
                decodeBase64(effect.pluginState);
            }
            if (effect.parameterNames.size() > 512)
                throw std::runtime_error("Too many plugin parameter names");
            for (auto &[key, value] : effect.parameterNames)
                if (!externalAudio(effect.id) || !externalParameter(key) || value.size() > 256)
                    throw std::runtime_error("Invalid plugin parameter name");
            auto *definition = audioEffect(effect.id);
            if (!definition || (!externalAudio(effect.id) && !effect.code.empty()) ||
                (effect.id != "cydaw.graph" && !effect.graph.IsNull()) ||
                (externalAudio(effect.id) && effect.code.empty()))
                throw std::runtime_error("Unknown or incompatible mixing DSP");
            if (effect.id == "cydaw.graph")
                decodeDspGraph(effect.graph);
            rangedParams(effect.params, [&](const std::string &key) -> std::pair<float, float> {
                if (externalAudio(effect.id) && externalParameter(key))
                    return {0, 1};
                for (auto &parameter : definition->parameters)
                    if (key == parameter.key)
                        return {parameter.minimum, parameter.maximum};
                throw std::runtime_error("Unknown DSP parameter: " + key);
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
            throw std::runtime_error("Invalid audio bus");
        checkMix(bus.mix);
    }
    audioBusOrder();
    if (markers.size() > 4096)
        throw std::runtime_error("4096 markers maximum");
    if (groups.size() > 256)
        throw std::runtime_error("256 track groups maximum");
    if (bin.size() > 4096 || std::any_of(bin.begin(), bin.end(), [](const std::string &path) {
            return path.empty() || path.size() > 32768;
        }))
        throw std::runtime_error("Invalid media bin");
    for (auto &group : groups) {
        idCheck(group.id);
        if (group.name.empty() || group.name.size() > 256 || group.color > 0xffffff)
            throw std::runtime_error("Invalid track group");
    }
    for (auto &marker : markers) {
        idCheck(marker.id);
        if (!finite(marker.time) || marker.time < 0 || marker.time > 86400 || marker.name.size() > 256 ||
            marker.note.size() > 4096 || marker.color > 0xffffff || !finite(marker.duration) ||
            marker.duration < 0 || marker.duration > 86400)
            throw std::runtime_error("Invalid marker");
    }
    if (!finite(workIn) || !finite(workOut) || workIn < 0 || workOut < 0 || workIn > 86400 || workOut > 86400)
        throw std::runtime_error("Invalid work area");
    for (auto &t : tracks) {
        idCheck(t.id);
        checkMix(t.mix);
        if (t.color > 0xffffff || !finite(t.height) || t.height < 0 || t.height > 600)
            throw std::runtime_error("Invalid track appearance");
        if (t.group && !std::any_of(groups.begin(), groups.end(), [&](const TrackGroup &g) { return g.id == t.group; }))
            throw std::runtime_error("Track group not found");
        if (t.kind != TrackKind::Audio &&
            (t.mix.output || !t.mix.sends.empty() || !t.mix.effects.empty() || !t.mix.params.empty()))
            throw std::runtime_error("A video track cannot host an audio channel");
        if (!finite(t.gain) || t.gain < -80 || t.gain > 12 || !finite(t.pan) || std::abs(t.pan) > 1)
            throw std::runtime_error("Invalid mix");
        for (auto &c : t.clips) {
            idCheck(c.id);
            largest = std::max(largest, c.link);
            if (c.color > 0xffffff)
                throw std::runtime_error("Invalid clip color");
            if (!finite(c.start) || c.start < 0 || !finite(c.duration) || c.duration < 1e-5 ||
                !finite(c.in) || c.in < 0 || !finite(c.speed) || c.speed < 0.05 || c.speed > 16 ||
                !finite(c.fadeIn) || c.fadeIn < 0 || c.fadeIn > c.duration || !finite(c.fadeOut) ||
                c.fadeOut < 0 || c.fadeOut > c.duration)
                throw std::runtime_error("Invalid clip time");
            checkParams(c.params);
            if (!c.composition.IsNull()) {
                if (t.kind != TrackKind::Video || c.generator || c.title.enabled || !c.source.empty() ||
                    c.composition.Dump(0).size() > 16 * 1024 * 1024)
                    throw std::runtime_error("Invalid video container");
                Project::fromJson(c.composition);
            }

            if (c.sourceColor != "auto" && c.sourceColor != "logc3-awg3-ei800" && !color(c.sourceColor))
                throw std::runtime_error("Unknown media color space");
            if (!finite(c.transition.duration) || c.transition.duration < 1e-5 ||
                (c.transition.enabled && (t.kind != TrackKind::Video || c.transition.id.empty() ||
                                          c.transition.duration > c.duration)))
                throw std::runtime_error("Invalid video transition");
            if (c.adjustment && (t.kind != TrackKind::Video || c.generator || c.title.enabled || !c.source.empty() ||
                                 !c.composition.IsNull()))
                throw std::runtime_error("Invalid adjustment layer: video clip without media or a generator");
            if (c.title.enabled &&
                (t.kind != TrackKind::Video || c.generator || !c.source.empty() ||
                 c.title.text.size() > 16384 || c.title.font.empty() || c.title.font.size() > 256 ||
                 c.title.alignment < 0 || c.title.alignment > 2))
                throw std::runtime_error("Invalid title");
            if (int(c.mask.shape) < 0 || int(c.mask.shape) > 3 ||
                (c.mask.shape != MaskShape::None && t.kind != TrackKind::Video))
                throw std::runtime_error("Invalid video mask");
            if (c.mask.points.size() > 16 || (c.mask.shape == MaskShape::Bezier && c.mask.points.size() < 3))
                throw std::runtime_error("3 to 16 Bezier points required");
            for (auto &point : c.mask.points)
                for (float value : {point.x, point.y, point.inX, point.inY, point.outX, point.outY})
                    if (!finite(value) || std::abs(value) > 2)
                        throw std::runtime_error("Bezier coordinate out of range");
            auto range = [&](const char *key, float low, float high) {
                auto it = c.params.find(key);
                if (it == c.params.end())
                    return;
                auto check = [&](float v) {
                    if (v < low || v > high)
                        throw std::runtime_error(std::string("Parameter out of range: ") + key);
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
                        throw std::runtime_error("Color grading requires a video clip");
                    validateGrade(e);
                }
                if (e.id == "ofx") {
                    if (t.kind != TrackKind::Video)
                        throw std::runtime_error("OFX requires a video clip");
                    validateOfx(e);
                }
                if (e.id == "cydaw.graph" || externalAudio(e.id)) {
                    if (t.kind != TrackKind::Audio)
                        throw std::runtime_error("A DSP requires an audio clip");
                    MixChannel chain;
                    chain.effects = {e};
                    checkMix(chain);
                }
            }
        }
    }
    if (nextId <= largest)
        throw std::runtime_error("Invalid ID counter");
}
void Project::save(const std::string &path) const {
    validate();
    AtomicFile output(path);
    {
        std::ofstream f(output.path(), std::ios::binary);
        if (!f)
            throw std::runtime_error("Unable to write the project");
        f << json().Dump(2);
        f.flush();
        if (!f)
            throw std::runtime_error("Project write failed");
    }
    output.commit();
}
Project Project::load(const std::string &path) {
    std::string text;
    if (!sx::ReadTextFile(path, text))
        throw std::runtime_error("Project not found");
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
    p.name = "First Light";
    auto &t = p.addTrack(TrackKind::Video);
    t.name = "01 / Procedural light";
    Clip c;
    c.id = p.allocate();
    c.name = "Aurora - HLSL generator";
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
    sound.name = "02  /  Soundtrack";
    return p;
}
void History::checkpoint(const Project &p, std::string label) {
    past.push_back({p, std::move(label)});
    if (past.size() > 64)
        past.erase(past.begin());
    future.clear();
}
bool History::undo(Project &p) {
    if (past.empty())
        return false;
    future.push_back({p, past.back().label});
    p = std::move(past.back().project);
    past.pop_back();
    return true;
}
bool History::redo(Project &p) {
    if (future.empty())
        return false;
    past.push_back({p, future.back().label});
    p = std::move(future.back().project);
    future.pop_back();
    return true;
}
} // namespace sf

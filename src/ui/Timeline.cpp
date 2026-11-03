// Timeline of ShaderixFrame, rebuilt on CyDAW v2's ArrangementView and Cinema timeline:
// toolbar, navigator band, timecode ruler with markers and work area, resizable track headers,
// per-track automation lanes and custom scrollbars. Time is in seconds, snapped to video frames.
#include "ui/Editor.h"
#include "adapters/CyDawParameters.h"
#include <cyui/modules/Audio.h>
#include "ui/MixerStrip.h"
#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <cmath>
#include <imgui_internal.h>
#include <cydaw/engine/EffectCatalogue.h>
#include <imgui_stdlib.h>
#include <set>

namespace sf {
using namespace style;
namespace {
constexpr float groupHeaderHeight = 26, groupPreviewHeight = 30, navigatorHeight = 14, rulerHeight = 28, scrollbarSize = 12, edgeGrab = 6, automationHeight = 76,
                filmstripHeight = 84, maxTimelineZoom = 20000;
constexpr const char *gridNames[] = {"Image", "2 images", "5 images", "10 images", "Second"};
Json clipArgs(Id id) {
    Json a = Json::Object();
    a["clip"] = id;
    return a;
}
ImU32 trackColour(const Track &t) {
    return t.color ? fromRgb(t.color) : t.kind == TrackKind::Audio ? style::audio : accent;
}
ImU32 clipColour(const Track &t, const Clip &c) {
    if (c.color)
        return fromRgb(c.color);
    if (t.color)
        return fromRgb(t.color);
    if (t.kind == TrackKind::Audio)
        return style::audio;
    if (!c.composition.IsNull())
        return container;
    if (c.title.enabled)
        return style::title;
    if (c.generator)
        return generator;
    return accent;
}
const Effect *laneEffect(const Clip &c, const std::string &id) {
    for (auto &effect : c.effects)
        if (effect.id == id)
            return &effect;
    return nullptr;
}
Effect *laneEffect(Clip &c, const std::string &id) {
    return const_cast<Effect *>(laneEffect(const_cast<const Clip &>(c), id));
}
// Clip gain uses CyDAW's volume lane scale, squared over -60..+12 dB; other lanes are linear.
template <class Lane> float lanePart(const Lane &lane, std::pair<float, float> range, float value) {
    if (lane.effect.empty() && lane.key == "gain") {
        float t = (std::clamp(value, -60.f, 12.f) + 60) / 72;
        return t * t;
    }
    return (value - range.first) / (range.second - range.first);
}
template <class Lane> float laneValue(const Lane &lane, std::pair<float, float> range, float part) {
    if (lane.effect.empty() && lane.key == "gain")
        return std::sqrt(std::clamp(part, 0.f, 1.f)) * 72 - 60;
    return range.first + part * (range.second - range.first);
}
// Every keyframe time of a clip, its own parameters and its effects', for the clip's diamonds.
std::vector<double> keyTimes(const Clip &c) {
    std::vector<double> times;
    auto collect = [&](const std::map<std::string, Parameter> &params) {
        for (auto &[key, param] : params)
            for (auto &k : param.keys)
                times.push_back(k.time);
    };
    collect(c.params);
    for (auto &effect : c.effects)
        collect(effect.params);
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }),
                times.end());
    return times;
}
// Clip gain line: CyDAW's squared -60..+12 dB scale over the clip body.
float gainPart(float db) {
    float t = (std::clamp(db, -60.f, 12.f) + 60) / 72;
    return t * t;
}
float gainFromPart(float part) { return std::sqrt(std::clamp(part, 0.f, 1.f)) * 72 - 60; }
std::string clipBadges(const Clip &c) {
    std::string badges;
    if (!c.effects.empty())
        badges += "fx " + std::to_string(c.effects.size()) + "  ";
    if (std::abs(c.speed - 1) > 1e-6) {
        char speed[24];
        snprintf(speed, sizeof(speed), "%.0f %%  ", c.speed * 100);
        badges += speed;
    }
    if (!c.composition.IsNull())
        badges += "CONTAINER  ";
    if (c.hold)
        badges += "FREEZE  ";
    if (c.reverse)
        badges += "REVERSE  ";
    if (c.adjustment)
        badges += "ADJUSTMENT LAYER  ";
    return badges;
}
float fallbackValue(const std::string &key) { return key == "opacity" || key == "scale" ? 1.f : 0.f; }
void drawLink(ImDrawList *list, ImVec2 at, ImU32 colour) {
    list->AddCircle(at, 2.6f, colour, 8, 1.2f);
    list->AddCircle(at + ImVec2(3.6f, 0), 2.6f, colour, 8, 1.2f);
}
} // namespace
// Display name of a lane or curve parameter (also used by the Animations panel).
std::string laneLabel(const std::string &key, const std::string &effect) {
    static const std::map<std::string, std::string> names{
        {"opacity", "Opacity"}, {"x", "Position X"}, {"y", "Position Y"}, {"scale", "Scale"},
        {"rotation", "Rotation"}, {"gain", "Gain"},  {"pan", "Pan"}, {"speed_ramp", "Speed (ramp)"}};
    auto it = names.find(key);
    auto label = it == names.end() ? key : it->second;
    return effect.empty() ? label : effect + " · " + label;
}

std::string timecode(double time, int fps);

double Editor::gridStep() const {
    constexpr int frames[] = {1, 2, 5, 10};
    return gridIndex >= 4 ? 1. : double(frames[std::clamp(gridIndex, 0, 3)]) / s.project.fps;
}
double Editor::snapToGrid(double seconds, bool force) const {
    bool on = snap != ImGui::GetIO().KeyAlt;
    if (!on && !force)
        return seconds;
    double step = gridStep();
    return std::round(seconds / step) * step;
}
std::vector<double> Editor::snapPoints(const std::vector<Id> &ignore) const {
    std::vector<double> points{0, s.time};
    for (auto &track : s.project.tracks)
        for (auto &clip : track.clips)
            if (std::find(ignore.begin(), ignore.end(), clip.id) == ignore.end()) {
                points.push_back(clip.start);
                points.push_back(clip.start + clip.duration);
            }
    for (auto &marker : s.project.markers)
        points.push_back(marker.time);
    if (s.project.hasWorkArea()) {
        points.push_back(s.project.workIn);
        points.push_back(s.project.workOut);
    }
    return points;
}
float Editor::trackHeight(const Track &track) const {
    float base = track.height > 0 ? track.height : track.kind == TrackKind::Audio ? 58.f : 86.f;
    return std::clamp(base * laneZoom, 22.f, 600.f);
}
void Editor::revealTime(double time) {
    float width = std::max(80.f, ImGui::GetContentRegionAvail().x - headerWidth - scrollbarSize);
    if (time < scroll || time > scroll + width / zoom)
        scroll = std::max(0., time - width * .1 / zoom);
}
void Editor::fitTimeline(float width) {
    double length = std::max(1., std::max(s.project.duration, s.project.hasWorkArea() ? s.project.workOut : 0.));
    zoom = std::clamp(std::max(40.f, width) * .95f / float(length), 2.f, maxTimelineZoom);
    scroll = 0;
}
// Z: the selected clips fill the view (the whole sequence when nothing is selected).
void Editor::fitSelection(float width) {
    double first = 1e18, last = -1;
    for (auto id : s.selection)
        if (auto *clip = s.project.findClip(id)) {
            first = std::min(first, clip->start);
            last = std::max(last, clip->start + clip->duration);
        }
    if (last <= first) {
        fitTimeline(width);
        return;
    }
    double span = (last - first) * 1.1;
    zoom = std::clamp(std::max(40.f, width) / float(span), 2.f, 20000.f);
    scroll = std::max(0., first - (last - first) * .05);
}
void Editor::blade(Id clip, double time) {
    auto *c = s.project.findClip(clip);
    if (!c || time <= c->start + 1e-6 || time >= c->start + c->duration - 1e-6)
        return;
    auto args = clipArgs(clip);
    args["time"] = time;
    action("split_clip", args);
}

// Shaderixel effects for a video clip, CyDAW DSP for an audio clip, searchable by name or category.
void Editor::effectMenu(Id clipId) {
    auto *clip = s.project.findClip(clipId);
    if (!clip)
        return;
    bool audio = s.project.owner(clipId)->kind == TrackKind::Audio;
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##effectSearch", "Find an effect...", &effectSearch);
    auto lower = [](std::string text) {
        for (auto &c : text)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };
    auto query = lower(effectSearch);
    auto matches = [&](const std::string &name, const std::string &category) {
        return query.empty() || lower(name).find(query) != std::string::npos ||
               lower(category).find(query) != std::string::npos;
    };
    auto add = [&](const std::string &id) {
        auto args = clipArgs(clipId);
        args["id"] = id;
        action(audio ? "add_audio_effect" : "add_effect", args);
        ImGui::CloseCurrentPopup();
    };
    if (audio) {
        for (auto &d : cydaw::engine::effectCatalogue)
            if (audioInsert(d) && matches(std::string(d.displayName), "CyDAW"))
                if (ImGui::MenuItem(std::string(d.displayName).c_str()))
                    add("cydaw." + std::string(d.typeKey));
        return;
    }
    std::map<std::string, std::vector<const EffectDefinition *>> categories;
    for (auto &d : s.renderer.library.definitions)
        if (d.kind == "effect" && !d.generator && matches(d.name, d.category))
            categories[d.category.empty() ? "Other" : d.category].push_back(&d);
    if (categories.empty())
        ImGui::TextDisabled(cyui::tr("No matching effects.").c_str());
    // A search lists the results flat; otherwise one submenu per category.
    for (auto &[category, list] : categories) {
        bool open = !query.empty() || ImGui::BeginMenu(category.c_str());
        if (!open)
            continue;
        if (!query.empty())
            ImGui::SeparatorText(category.c_str());
        for (auto *d : list) {
            ImGui::PushID(d->id.c_str());
            if (ImGui::MenuItem(d->name.c_str()))
                add(d->id);
            ImGui::PopID();
        }
        if (query.empty())
            ImGui::EndMenu();
    }
}

// Clipboard: clips keep their track, relative timing, links, effects and animations.
void Editor::copyClips(bool cut) {
    if (s.selection.empty())
        return;
    clipboard.clear();
    for (auto id : s.project.selection(s.selection, false))
        if (auto *clip = s.project.findClip(id))
            clipboard.push_back({s.project.owner(id)->id, *clip});
    if (cut)
        action("delete_clips", selectionArgs());
}
// A (Alt: backwards): every clip of the unlocked tracks that reaches past the time (or starts before it).
void Editor::selectFrom(double time, bool forward, Id onlyTrack) {
    std::vector<Id> ids;
    for (auto &t : s.project.tracks)
        if (!t.locked && (!onlyTrack || t.id == onlyTrack))
            for (auto &c : t.clips)
                if (forward ? c.start + c.duration > time + 1e-7 : c.start < time - 1e-7)
                    ids.push_back(c.id);
    s.setSelection(ids);
}
// Shift+D: a crossfade on the video cut nearest the playhead (within half a second), on the selected
// clip's track first; one second at most, shortened to the source handle before the cut.
void Editor::crossfadeAtPlayhead() {
    auto &p = s.project;
    Id outgoing = 0, incoming = 0;
    double distance = .5;
    auto search = [&](const Track &t) {
        if (t.kind != TrackKind::Video || t.locked)
            return;
        for (auto &a : t.clips)
            for (auto &b : t.clips)
                if (a.id != b.id && std::abs(a.start + a.duration - b.start) < 1e-6 && !b.transition.enabled &&
                    std::abs(b.start - s.time) < distance) {
                    distance = std::abs(b.start - s.time);
                    outgoing = a.id;
                    incoming = b.id;
                }
    };
    if (auto *c = p.findClip(s.selected))
        search(*p.owner(c->id));
    for (auto &t : p.tracks)
        if (!outgoing)
            search(t);
    if (!outgoing) {
        error = "No video cut near the playhead";
        return;
    }
    auto &first = *p.findClip(outgoing), &second = *p.findClip(incoming);
    double handle = second.generator || second.title.enabled ? 1e9 : second.in / std::max(1e-6, second.speed);
    double seconds = std::min({1., first.duration * .5, handle});
    seconds = std::floor(seconds * p.fps + 1e-6) / p.fps;
    if (seconds < 1. / p.fps) {
        error = "No source handles before the cut for a crossfade";
        return;
    }
    auto a = clipArgs(outgoing);
    a["next"] = incoming;
    a["duration"] = seconds;
    action("overlap_transition", a);
}
// Durations offered to open the sequence at a point, on every track.
void Editor::insertGapMenu(double at) {
    if (!ImGui::BeginMenu(cyui::trLabel("Insert Gap").c_str()))
        return;
    double frame = 1. / s.project.fps;
    for (auto [label, length] : {std::pair{"1 image", frame}, {"1 second", 1.}, {"2 seconds", 2.},
                                 {"5 seconds", 5.}, {"10 seconds", 10.}})
        if (ImGui::MenuItem(label)) {
            Json a = Json::Object();
            a["time"] = std::clamp(at, 0., s.project.duration);
            a["duration"] = length;
            action("insert_gap", a);
        }
    ImGui::EndMenu();
}
// Shift+F: the video clip under the playhead (the selected one first) holds its frame there.
void Editor::freezeAtPlayhead(double length) {
    auto &p = s.project;
    auto under = [&](const Clip &c) {
        return s.time >= c.start - 1e-7 && s.time < c.start + c.duration - 1e-7 && p.owner(c.id)->kind == TrackKind::Video;
    };
    Id target = 0;
    if (auto *c = p.findClip(s.selected); c && under(*c))
        target = c->id;
    for (auto &t : p.tracks)
        for (auto &c : t.clips)
            if (!target && !t.locked && t.kind == TrackKind::Video && under(c))
                target = c.id;
    if (!target)
        return;
    auto a = clipArgs(target);
    a["time"] = snapTime(s.time, p.fps, true);
    a["duration"] = length;
    if (action("freeze_frame", a))
        s.selectClip(actionResult["clip"].U());
}
// Q / W: the clip under the playhead (the selected one first) loses what lies before / after it.
void Editor::trimToPlayhead(bool head, bool ripple) {
    auto &p = s.project;
    auto inside = [&](const Clip &c) { return s.time > c.start + 1e-7 && s.time < c.start + c.duration - 1e-7; };
    Id target = 0;
    if (auto *c = p.findClip(s.selected); c && inside(*c))
        target = c->id;
    for (auto id : s.selection)
        if (auto *c = p.findClip(id); !target && c && inside(*c))
            target = id;
    for (auto &t : p.tracks)
        for (auto &c : t.clips)
            if (!target && !t.locked && inside(c))
                target = c.id;
    if (!target)
        return;
    auto a = clipArgs(target);
    a["time"] = s.time;
    a["side"] = head ? "start" : "end";
    a["ripple"] = ripple;
    action("trim_clip", a);
}
// Shift+E: the selection is disabled (or enabled again when all of it already is), one undo step.
void Editor::toggleClipsEnabled() {
    auto ids = s.project.selection(s.selection, false);
    if (ids.empty())
        return;
    bool anyEnabled = false;
    for (auto id : ids)
        if (auto *clip = s.project.findClip(id))
            anyEnabled = anyEnabled || clip->enabled;
    s.history.checkpoint(s.project, "Edit");
    for (auto id : ids)
        if (auto *clip = s.project.findClip(id); clip && !s.project.owner(id)->locked)
            clip->enabled = !anyEnabled;
    s.dirty = true;
    s.media.resetAudio();
    s.renderer.clearHistory();
}
void Editor::pasteAttributes() {
    if (clipboard.empty() || s.selection.empty())
        return;
    auto &[track, source] = clipboard.front();
    auto *owner = s.project.findTrack(track);
    try {
        auto candidate = s.project;
        int pasted = candidate.pasteAttributes(source, owner ? owner->kind : TrackKind::Video,
                                               s.project.selection(s.selection, false), attributeChoice);
        s.history.checkpoint(s.project, "Paste Attributes");
        s.project = std::move(candidate);
        s.dirty = true;
        s.renderer.clearHistory();
        s.media.resetAudio();
        say("Attributes from ' " + source.name + " ' pasted onto " + std::to_string(pasted) + " clip(s)");
    } catch (const std::exception &failure) {
        error = failure.what();
    }
}
void Editor::pasteClips(double at) {
    if (clipboard.empty())
        return;
    auto &p = s.project;
    auto before = p;
    try {
        double earliest = clipboard.front().second.start;
        std::map<Id, int> members;
        for (auto &[track, clip] : clipboard) {
            earliest = std::min(earliest, clip.start);
            if (clip.link)
                ++members[clip.link];
        }
        std::map<Id, Id> links;
        std::vector<Id> pasted;
        for (auto &[trackId, original] : clipboard) {
            auto kind = TrackKind::Video;
            if (auto *home = before.findTrack(trackId))
                kind = home->kind;
            else
                kind = original.source.empty() || original.generator || original.title.enabled ? TrackKind::Video
                                                                                                 : TrackKind::Audio;
            Track *track = p.findTrack(trackId);
            if (!track || track->locked) {
                track = nullptr;
                for (auto &candidate : p.tracks)
                    if (candidate.kind == kind && !candidate.locked) {
                        track = &candidate;
                        break;
                    }
            }
            if (!track)
                track = &p.addTrack(kind);
            Clip copy = original;
            copy.id = p.allocate();
            if (copy.link && members[copy.link] > 1) {
                auto [it, added] = links.emplace(copy.link, 0);
                if (added)
                    it->second = p.allocate();
                copy.link = it->second;
            } else
                copy.link = 0;
            track->clips.push_back(std::move(copy));
            pasted.push_back(track->clips.back().id);
        }
        p.moveClips(pasted, std::max(0., at) - earliest);
        for (auto id : pasted)
            if (auto *clip = p.findClip(id))
                p.duration = std::max(p.duration, clip->start + clip->duration);
        p.validate();
        s.history.checkpoint(before, "Edit");
        s.dirty = true;
        s.media.resetAudio();
        s.renderer.clearHistory();
        s.setSelection(pasted);
        error.clear();
    } catch (const std::exception &e) {
        p = std::move(before);
        error = e.what();
    }
}

void Editor::timelineToolbar() {
    float height = ImGui::GetFrameHeight();
    if (iconButton(icon::add, "Add Track or Clip"))
        ImGui::OpenPopup("timelineAdd");
    if (ImGui::BeginPopup("timelineAdd")) {
        if (ImGui::MenuItem(cyui::trLabel("Video Track").c_str())) {
            Json a = Json::Object();
            a["kind"] = "video";
            action("add_track", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Audio Track").c_str())) {
            Json a = Json::Object();
            a["kind"] = "audio";
            action("add_track", a);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(cyui::trLabel("HLSL Generator").c_str()))
            action("add_generator");
        if (ImGui::MenuItem(cyui::trLabel("Title / Text").c_str()))
            action("add_title");
        if (ImGui::MenuItem(cyui::trLabel("Lower Third...").c_str())) {
            lowerStart = s.time;
            openLowerThird = true;
        }
        ImGui::EndPopup();
    }
    toolbarSeparator();
    if (vectorButton("##toolSelect", sx::ui::IconMove, "Select [V]\nMove, edge trim, fades",
                     tool == Tool::Select))
        tool = Tool::Select;
    ImGui::SameLine(0, 2);
    if (iconButton(icon::cut, "Blade [B]\nClick to split a clip; Shift: all tracks", tool == Tool::Blade))
        tool = Tool::Blade;
    ImGui::SameLine(0, 2);
    if (vectorButton("##toolSlip", sx::ui::IconSwap, "Slip [Y]\nOffset the source without moving the clip",
                     tool == Tool::Slip))
        tool = Tool::Slip;
    ImGui::SameLine(0, 2);
    if (iconButton("][", "Roll [N]\nMove the cut between neighboring clips",
                   tool == Tool::Roll))
        tool = Tool::Roll;
    ImGui::SameLine(0, 2);
    if (iconButton("%", "Rate Stretch [R]\nDrag an edge to change clip speed instead of content",
                   tool == Tool::Stretch))
        tool = Tool::Stretch;
    ImGui::SameLine(0, 2);
    if (iconButton("[=]", "Slide [U]\nMove a clip between neighbors, extending or trimming them",
                   tool == Tool::Slide))
        tool = Tool::Slide;
    ImGui::SameLine(0, 2);
    if (vectorButton("##toolHand", sx::ui::IconHand, "Hand [H]\nPan the view", tool == Tool::Hand))
        tool = Tool::Hand;
    toolbarSeparator();
    if (iconButton(icon::grid, "Snap to Grid (Alt Toggles)", snap))
        snap = !snap;
    ImGui::SameLine(0, 2);
    ImGui::SetNextItemWidth(92);
    ImGui::Combo("##grid", &gridIndex, gridNames, IM_ARRAYSIZE(gridNames));
    tooltip("Grid Step");
    ImGui::SameLine(0, 2);
    if (iconButton(icon::link, "Snap to clip edges, markers, work area and playhead", snapEdges))
        snapEdges = !snapEdges;
    ImGui::SameLine(0, 4);
    if (toggle("RIPPLE", rippleEdits, accent, {54, height},
               "Ripple trim: dragging an edge shifts later clips"))
        rippleEdits = !rippleEdits;
    toolbarSeparator();
    if (iconButton(icon::zoomOut, "Zoom Out [-]"))
        zoom = std::clamp(zoom / 1.4f, 2.f, maxTimelineZoom);
    ImGui::SameLine(0, 2);
    if (iconButton(icon::zoomIn, "Zoom In [+]"))
        zoom = std::clamp(zoom * 1.4f, 2.f, maxTimelineZoom);
    ImGui::SameLine(0, 2);
    if (iconButton(icon::fit, "Fit All [\\]"))
        fitTimeline(ImGui::GetWindowWidth() - headerWidth - 40);
    ImGui::SameLine(0, 2);
    if (iconButton(icon::follow, followPaused ? "Follow Playback (Paused: Resume)" : "Follow Playback",
                   follow && !followPaused, followPaused ? textDim : 0)) {
        if (followPaused)
            followPaused = false;
        else
            follow = !follow;
    }
    toolbarSeparator();
    ImGui::BeginDisabled(s.selection.empty());
    if (iconButton(icon::cut, "Split Selection at Playhead [S]") && !s.selection.empty()) {
        auto a = selectionArgs();
        a["time"] = s.time;
        action("split_clips", a);
    }
    ImGui::SameLine(0, 2);
    if (iconButton(icon::copy, "Duplicate after Selection [Ctrl+D]"))
        action("duplicate_clips", selectionArgs());
    ImGui::SameLine(0, 2);
    if (iconButton(icon::trash, "Delete [Delete]"))
        action("delete_clips", selectionArgs());
    ImGui::SameLine(0, 2);
    if (iconButton(icon::close, "Ripple Delete [Shift+Delete]\nClips crossing the "
                                "ranges must be split or selected.")) {
        auto args = selectionArgs();
        args["ripple"] = true;
        action("delete_clips", args);
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, 6);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled(cyui::tr("%zu clip%s").c_str(), s.selection.size(), s.selection.size() > 1 ? "s" : "");
    toolbarSeparator();
    if (iconButton(icon::tag, "Add Marker at Playhead [M]")) {
        Json a = Json::Object();
        a["time"] = s.time;
        action("add_marker", a);
    }
    ImGui::SameLine(0, 2);
    if (iconButton(icon::loop, "Loop Work Area [L]\nDrag the top of the ruler to define it; "
                               "I / O: in / out",
                   loop))
        loop = !loop;
    ImGui::SameLine(0, 2);
    if (iconButton(icon::video, "FRAME Filmstrip: final rendered frames above the tracks, as in "
                                "CyDAW Cinema. Drag to scrub through frames.",
                   showFilmstrip))
        showFilmstrip = !showFilmstrip;
    ImGui::SameLine(0, 2);
    if (iconButton(icon::view, "View: rows, thumbnails, waveforms, groups, lanes and heights"))
        ImGui::OpenPopup("timelineView");
    if (ImGui::BeginPopup("timelineView")) {
        ImGui::SeparatorText(cyui::tr("Show").c_str());
        ImGui::Checkbox(cyui::trLabel("FRAME Row (Final Render)").c_str(), &showFilmstrip);
        ImGui::Checkbox(cyui::trLabel("Video Clip Thumbnails").c_str(), &showClipThumbnails);
        ImGui::Checkbox(cyui::trLabel("Waveforms").c_str(), &showWaveforms);
        ImGui::Checkbox(cyui::trLabel("Clip Keyframes").c_str(), &showKeyframes);
        ImGui::SeparatorText(cyui::tr("Tracks").c_str());
        const std::pair<const char *, float> heights[] = {{"Compact", .6f}, {"Normal", 1.f}, {"Large", 1.6f}};
        for (auto [label, value] : heights) {
            if (ImGui::RadioButton(label, std::abs(laneZoom - value) < 1e-3f))
                laneZoom = value;
            ImGui::SameLine();
        }
        ImGui::NewLine();
        // Folding every group is one undo step.
        auto foldAll = [&](bool collapsed) {
            s.history.checkpoint(s.project, "Edit");
            for (auto &group : s.project.groups)
                group.collapsed = collapsed;
            s.dirty = true;
        };
        ImGui::BeginDisabled(s.project.groups.empty());
        if (ImGui::Button(cyui::trLabel("Collapse groups").c_str()))
            foldAll(true);
        ImGui::SameLine();
        if (ImGui::Button(cyui::trLabel("Expand Groups").c_str()))
            foldAll(false);
        ImGui::EndDisabled();
        bool anyLane = std::any_of(lanes.begin(), lanes.end(), [](auto &entry) { return entry.second.visible; });
        ImGui::BeginDisabled(!anyLane);
        if (ImGui::Button(cyui::trLabel("Hide All Automation Lanes").c_str()))
            for (auto &[track, lane] : lanes)
                lane.visible = false;
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::SameLine(0, 2);
    if (iconButton(icon::edit, "Edit Selected Clip / Container") && s.selected)
        openClipEditor(s.selected);
    ImGui::SameLine(0, 2);
    if (iconButton(icon::more, "Advanced Editing: Numeric Trims, Crossfade, Range Removal"))
        ImGui::OpenPopup("Advanced Editing");
    if (ImGui::BeginPopup("Advanced Editing")) {
        ImGui::TextUnformatted(cyui::tr("Trim Selected Clip").c_str());
        ImGui::InputFloat(cyui::trLabel("Duration Change (s)").c_str(), &trimDelta, .1f, 1.f);
        ImGui::Checkbox(cyui::trLabel("Adjust Source In").c_str(), &leftTrim);
        ImGui::BeginDisabled(!s.selected);
        if (ImGui::Button(cyui::trLabel("Ripple Trim").c_str())) {
            auto args = clipArgs(s.selected);
            args["delta"] = trimDelta;
            args["left"] = leftTrim;
            action("ripple_trim", args);
        }
        Id next = 0;
        if (auto *clip = s.project.findClip(s.selected))
            for (auto &other : s.project.owner(clip->id)->clips)
                if (other.id != clip->id && std::abs(clip->start + clip->duration - other.start) < 1e-6) {
                    next = other.id;
                    break;
                }
        ImGui::BeginDisabled(!next);
        if (ImGui::Button(cyui::trLabel("Roll Trim with Next Clip").c_str())) {
            auto args = clipArgs(s.selected);
            args["next"] = next;
            args["delta"] = trimDelta;
            action("roll_trim", args);
        }
        ImGui::InputFloat(cyui::trLabel("Overlap (s)").c_str(), &transitionOverlap, .1f, 1.f);
        if (ImGui::Button(cyui::trLabel("Create Crossfade with Next Clip").c_str())) {
            auto args = clipArgs(s.selected);
            args["next"] = next;
            args["duration"] = transitionOverlap;
            action("overlap_transition", args);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::SeparatorText(cyui::tr("Remove Range on All Tracks").c_str());
        if (s.project.hasWorkArea() && ImGui::SmallButton(cyui::trLabel("Use Work Area").c_str())) {
            rangeBegin = s.project.workIn;
            rangeEnd = s.project.workOut;
        }
        ImGui::InputDouble(cyui::trLabel("Start").c_str(), &rangeBegin, .1, 1, "%.3f");
        ImGui::InputDouble(cyui::trLabel("End").c_str(), &rangeEnd, .1, 1, "%.3f");
        if (ImGui::Button(cyui::trLabel("Split and Close Gap").c_str())) {
            Json args = Json::Object();
            args["begin"] = rangeBegin;
            args["end"] = rangeEnd;
            action("remove_range", args);
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0, 2);
    if (iconButton("?", "Timeline Shortcuts"))
        ImGui::OpenPopup("timelineShortcuts");
    if (ImGui::BeginPopup("timelineShortcuts")) {
        static const std::pair<const char *, const char *> sections[] = {
            {"TOOLS", nullptr},
            {"V", "Select: move, edge trim, fades"},
            {"B", "Blade (Shift: All Tracks)"},
            {"Y", "Slip: Offset Source"},
            {"N", "Roll: Move Cut"},
            {"R", "Rate Stretch: Change Speed"},
            {"U", "Slide between neighbors"},
            {"H", "Hand: Pan View"},
            {"EDIT", nullptr},
            {"S", "Split Selection at Playhead"},
            {"Delete / Shift+Delete", "Delete / Ripple Delete"},
            {"Ctrl+C / X / V", "Copy, Cut, Paste at Playhead"},
            {"Ctrl+D", "Duplicate after Selection"},
            {"Ctrl+G", "Group Selected Clips' Tracks"},
            {"Ctrl+A / Esc", "Select All / Deselect"},
            {"A  ·  Alt+A", "Select Forward from Playhead / Backward"},
            {"1 … 4", "Multicam: switch to this angle at playhead (top video tracks)"},
            {"F", "Match Frame: Open Clip's Media in Source"},
            {"Z", "Zoom to Selection (Otherwise Entire Sequence)"},
            {"Shift+E", "Disable / Enable Selection (Keep Position)"},
            {"Ctrl+R", "Clip Speed / Duration"},
            {"Ctrl+F", "Find Clip (Name, Track, File)"},
            {"Ctrl+Alt+V", "Paste Copied Clip Attributes (Effects, Transforms...)"},
            {"Input", "Pre-render Work Area (Green Bar: Ready)"},
            {"Shift+F", "2 s Freeze Frame at Playhead"},
            {"Shift+D", "Crossfade at Nearest Video Cut"},
            {"Ctrl + ← →", "Previous / Next Keyframe (Selection)"},
            {"Q  ·  W  (Alt)", "Ripple Trim Start / End to Playhead (Keep Gap)"},
            {"Alt", "Disable Snapping"},
            {"Alt + Left / Right (Shift)", "Nudge Selection One Frame (Ten)"},
            {"TIME", nullptr},
            {"Space", "Play / Pause"},
            {"Left / Right (Shift)", "Previous / Next Frame (Second)"},
            {"↑ ↓", "Previous / Next Edit Point"},
            {"Start / End", "Sequence Start / End"},
            {"M", "Marker at Playhead"},
            {"I  ·  O  ·  L", "Work Area In / Out / Loop"},
            {"Shift+Space", "Play Two Seconds around Playhead, Then Return"},
            {"J / K / Shift+L", "Shuttle: backward, stop, forward (repeat for x2, x4, x8)"},
            {"SOURCE", nullptr},
            {"Space / K / L", "Play / Pause / Stop / Faster (x2, x4)"},
            {"J  ·  ← →", "Back One Second / Frame by Frame"},
            {"I  ·  O", "Media In / Out"},
            {",  ·  .", "Insert / Overwrite at Playhead (C Target Tracks)"},
            {"VIEW", nullptr},
            {"Mouse wheel", "Tracks / Shift: Time / Ctrl: Zoom / Alt: Height"},
            {"+  ·  -  ·  \\", "Zoom In, Zoom Out, Fit All"},
            {"F1 … F6", "Edit through Deliver Workspaces"},
            {"Ctrl+Shift+P", "Command Palette: Find Any Command by Name"},
        };
        if (ImGui::BeginTable("shortcuts", 2, ImGuiTableFlags_SizingFixedFit)) {
            for (auto &[keys, meaning] : sections) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (!meaning) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent), "%s", keys);
                    continue;
                }
                ImGui::TextUnformatted(keys);
                ImGui::TableNextColumn();
                ImGui::TextDisabled(cyui::tr("%s").c_str(), meaning);
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        bool custom = keymap.json().Size() > 0;
        ImGui::TextDisabled(custom ? "Default shortcuts: some have been customized." : "Default shortcuts.");
        ImGui::SameLine();
        if (ImGui::SmallButton(cyui::trLabel("Personnaliser…").c_str())) {
            panels["Keyboard Shortcuts"] = true;
            ImGui::SetWindowFocus("Keyboard Shortcuts");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    // Timecode display, right-aligned as in CyDAW's transport.
    auto code = timecode(s.time, s.project.fps);
    auto total = "/ " + timecode(s.project.duration, s.project.fps);
    ImGui::PushFont(sx::ui::GetFonts().mono, 0);
    float codeWidth = ImGui::CalcTextSize(code.c_str()).x + ImGui::CalcTextSize(total.c_str()).x + 18;
    ImGui::SameLine();
    // Shown only when it fits: the mode bar always carries the timecode.
    if (ImGui::GetContentRegionAvail().x > codeWidth + 8) {
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - codeWidth);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(s.playing ? play : accent), "%s", code.c_str());
        ImGui::SameLine(0, 6);
        ImGui::TextDisabled(cyui::tr("%s").c_str(), total.c_str());
    } else
        ImGui::NewLine();
    ImGui::PopFont();
}

void Editor::timeline() {
    clipPreviews.beginFrame();
    if (!panels["Timeline"] || !beginPanel("Timeline"))
        return;
    auto &io = ImGui::GetIO();
    auto &p = s.project;
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput &&
                   !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    auto selectAll = [&]() {
        std::vector<Id> ids;
        for (auto &track : p.tracks)
            for (auto &clip : track.clips)
                ids.push_back(clip.id);
        s.setSelection(ids);
    };
    if (focused) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
            selectAll();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s.selectClip(0);
            timelineDragging = 0;
            timelineDragBefore.reset();
            marquee = false;
            gesture = Gesture::None;
        }
        // Editable bindings (Raccourcis panel): each chord with exactly its modifiers.
        for (auto [id, chosen] : {std::pair{"tool.select", Tool::Select}, {"tool.blade", Tool::Blade},
                                  {"tool.slip", Tool::Slip}, {"tool.hand", Tool::Hand}, {"tool.roll", Tool::Roll},
                                  {"tool.stretch", Tool::Stretch}, {"tool.slide", Tool::Slide}})
            if (keymap.pressed(id))
                tool = chosen;
        if (keymap.pressed("clip.match"))
            matchFrame();
        if (keymap.pressed("clip.freeze", false))
            freezeAtPlayhead(2);
        if (keymap.pressed("view.fit_selection"))
            fitSelection(ImGui::GetWindowWidth() - headerWidth - 40);
        if (keymap.pressed("clip.toggle", false))
            toggleClipsEnabled();
        if (keymap.pressed("select.forward", false))
            selectFrom(s.time, true);
        if (keymap.pressed("select.backward", false))
            selectFrom(s.time, false);
        if (keymap.pressed("cut.crossfade", false))
            crossfadeAtPlayhead();
        for (auto [id, head, ripple] : {std::tuple{"trim.head", true, true}, {"trim.head_keep", true, false},
                                        {"trim.tail", false, true}, {"trim.tail_keep", false, false}})
            if (keymap.pressed(id, false))
                trimToPlayhead(head, ripple);
        if (!io.KeyCtrl) {
            // Multicam: 1 to 4 (keypad too) cut to that angle at the playhead.
            for (int angle = 1; angle <= 4; ++angle)
                if (ImGui::IsKeyPressed(ImGuiKey(ImGuiKey_0 + angle), false) ||
                    ImGui::IsKeyPressed(ImGuiKey(ImGuiKey_Keypad0 + angle), false)) {
                    Json a = Json::Object();
                    a["angle"] = angle;
                    a["audio"] = angleSound;
                    action("switch_angle", a);
                }
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
                zoom = std::clamp(zoom * 1.4f, 2.f, maxTimelineZoom);
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
                zoom = std::clamp(zoom / 1.4f, 2.f, maxTimelineZoom);
            if (ImGui::IsKeyPressed(ImGuiKey_Backslash))
                fitTimeline(ImGui::GetWindowWidth() - headerWidth - 40);
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !s.selection.empty())
                action("delete_clips", selectionArgs());
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false) && !s.selection.empty()) {
            // CyDAW's Ctrl+G: group the tracks of the selected clips.
            Json a = Json::Object();
            a["tracks"] = Json::Array();
            std::set<Id> owners;
            for (auto id : s.selection)
                if (auto *owner = p.owner(id); owner && owners.insert(owner->id).second)
                    a["tracks"].Push(owner->id);
            action("group_tracks", a);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false))
            openFind = true;
        if (!io.KeyCtrl && !io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            startRenderCache();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false))
            if (auto *c = p.findClip(s.selected)) {
                speedClip = c->id;
                speedDraft = float(c->speed * 100);
                durationDraft = c->duration;
                openSpeed = true;
            }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
            copyClips(false);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X, false))
            copyClips(true);
        if (io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_V, false))
            openAttributes = !clipboard.empty() && !s.selection.empty();
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
            pasteClips(s.time);
    }
    if (pendingReveal >= 0) {
        revealTime(pendingReveal);
        pendingReveal = -1;
    }
    timelineToolbar();

    // A clip whose parameter was keyed or picked in Propriétés shows its lane.
    if (auto *picked = p.findClip(s.selected)) {
        auto *owner = p.owner(picked->id);
        std::string effect = animationEffect >= 0 && animationEffect < int(picked->effects.size())
                                 ? picked->effects[animationEffect].id
                                 : "";
        if (automationReveal) {
            auto &lane = lanes[owner->id];
            lane.visible = true;
            lane.key = animationKey;
            lane.effect = effect;
        }
    }
    automationReveal = false;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg0);
    ImGui::BeginChild("timelineCanvas", {0, 0}, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 260.f);
    size.y = std::max(size.y, 120.f);
    // A narrow panel shrinks the header column for this frame only; the chosen width comes back.
    float preferredHeader = std::clamp(headerWidth, 140.f, 420.f);
    bool headerResized = false;
    headerWidth = std::clamp(headerWidth, 140.f, std::max(140.f, std::min(420.f, size.x - 120)));
    const float left = origin.x + headerWidth, right = origin.x + size.x - scrollbarSize,
                navTop = origin.y, rulerTop = origin.y + navigatorHeight, stripTop = rulerTop + rulerHeight,
                lanesTop = stripTop + (showFilmstrip ? filmstripHeight : 0),
                lanesBottom = origin.y + size.y - scrollbarSize, laneWidth = std::max(40.f, right - left);
    ImVec2 mouse = io.MousePos;
    bool playing = s.playing;
    if (playing && !timelinePlaying)
        followPaused = false;
    timelinePlaying = playing;
    auto x = [&](double t) { return left + float((t - scroll) * zoom); };
    auto rawTime = [&](float px) { return scroll + (px - left) / zoom; };
    auto time = [&](float px) { return std::max(0., snapToGrid(rawTime(px))); };
    double visibleEnd = scroll + laneWidth / zoom;
    // CyDAW Cinema filmstrip: cells sit on multiples of a frame stride, so panning reuses them, and the
    // stride falls to one frame when zoomed in: every frame of the video is then visible.
    auto filmstrip = [&](ImVec2 min, ImVec2 max, double from, double to, auto picture, ImU32 tint) {
        float codeHeight = max.y - min.y >= 46 ? 13.f : 0.f;
        float imageHeight = max.y - min.y - codeHeight;
        if (imageHeight < 8 || to <= from)
            return;
        double fps = p.fps, aspect = double(p.width) / p.height;
        float cellWidth = std::max(20.f, float(imageHeight * aspect));
        long long stride = std::max(1ll, (long long)std::ceil(fps * cellWidth / zoom - 1e-6));
        double visibleFrom = std::max(from, rawTime(min.x)), visibleTo = std::min(to, rawTime(max.x));
        long long firstFrame = (long long)std::ceil(from * fps - 1e-6);
        long long lastFrame = std::max(firstFrame, (long long)std::ceil(to * fps - 1e-6) - 1);
        long long first = (long long)std::floor(visibleFrom * fps / stride) * stride;
        float labelWidth = smallTextSize("00:00:00:00", .7f).x + 6;
        int cells = 0;
        for (long long f = first; double(f) / fps < visibleTo && cells < 192; f += stride, ++cells) {
            float cx = x(double(f) / fps), cw = float(double(stride) * zoom / fps);
            long long sample = std::clamp(f, firstFrame, lastFrame);
            ImVec2 cellMin{cx, min.y}, cellMax{cx + cw, min.y + imageHeight};
            dl->AddRectFilled(cellMin, cellMax, IM_COL32(0, 0, 0, 255));
            float boxW = std::max(1.f, cw - 2), boxH = std::max(1.f, imageHeight - 2);
            float drawW = boxW, drawH = boxH;
            if (boxW / boxH > aspect)
                drawW = float(boxH * aspect);
            else
                drawH = float(boxW / aspect);
            ImVec2 imageMin = cellMin + ImVec2(1 + (boxW - drawW) * .5f, 1 + (boxH - drawH) * .5f);
            if (auto *view = picture(double(sample) / fps))
                dl->AddImage((ImTextureID)view, imageMin, imageMin + ImVec2(drawW, drawH), {0, 0}, {1, 1}, tint);
            else
                dl->AddRectFilled(imageMin, imageMin + ImVec2(drawW, drawH), bg2);
            dl->AddLine({cx, min.y}, {cx, max.y}, line);
            if (codeHeight > 0 && cw >= labelWidth) {
                auto code = timecode(double(f) / fps, p.fps);
                smallText(dl, {cx + 3, cellMax.y}, textDim, code.c_str(), .7f);
            }
        }
    };

    // Vertical layout: clip row then the automation band of every track.
    // Track groups (CyDAW folders) add a header row; a folded group hides its tracks behind a preview.
    std::vector<float> tops(p.tracks.size()), rows(p.tracks.size()), heights(p.tracks.size());
    struct GroupRow {
        Id group;
        float top, height;
        size_t first, last;
    };
    std::vector<GroupRow> groupRows;
    for (size_t i = 0; i < p.tracks.size(); ++i) {
        rows[i] = trackHeight(p.tracks[i]);
        auto it = lanes.find(p.tracks[i].id);
        heights[i] = rows[i] + (it != lanes.end() && it->second.visible ? automationHeight : 0);
    }
    auto layout = [&](float origin) {
        float y = origin;
        groupRows.clear();
        for (size_t i = 0; i < p.tracks.size();) {
            auto *group = p.tracks[i].group ? p.findGroup(p.tracks[i].group) : nullptr;
            if (!group) {
                tops[i] = y;
                y += heights[i++];
                continue;
            }
            size_t last = i;
            while (last + 1 < p.tracks.size() && p.tracks[last + 1].group == group->id)
                ++last;
            float height = groupHeaderHeight + (group->collapsed ? groupPreviewHeight : 0);
            groupRows.push_back({group->id, y, height, i, last});
            y += height;
            for (size_t k = i; k <= last; ++k)
                if (group->collapsed)
                    tops[k] = -1e6f; // folded: off every hit test and drawing range
                else {
                    tops[k] = y;
                    y += heights[k];
                }
            i = last + 1;
        }
        return y - origin;
    };
    float content = layout(0);
    for (size_t i = 0; i < p.tracks.size(); ++i)
        if (tops[i] < -1e5f)
            heights[i] = 0;
    float viewHeight = std::max(10.f, lanesBottom - lanesTop);
    verticalScroll = std::clamp(verticalScroll, 0.f, std::max(0.f, content + 40 - viewHeight));
    layout(lanesTop - verticalScroll);
    auto trackAt = [&](float y) -> int {
        for (size_t i = 0; i < p.tracks.size(); ++i)
            if (y >= tops[i] && y < tops[i] + heights[i])
                return int(i);
        return -1;
    };
    geometry.navigator = {left, navTop};
    geometry.lanes = {left, lanesTop};
    geometry.ruler = rulerTop;
    geometry.tops = tops;
    geometry.rows = rows;

    // ---- Wheel: CyDAW mapping (plain: tracks, Shift: time, Ctrl: zoom, Alt: track height).
    bool canvasHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
                         ImRect(origin, origin + size).Contains(mouse);
    // A header knob under the pointer takes the wheel for itself.
    bool wheelForWidget = headerWheelHovered && mouse.x < left;
    headerWheelHovered = false;
    if (canvasHovered && !wheelForWidget && (io.MouseWheel != 0 || io.MouseWheelH != 0)) {
        if (io.KeyCtrl && io.MouseWheel != 0) {
            float anchorX = std::max(mouse.x, left);
            double anchor = rawTime(anchorX);
            zoom = std::clamp(zoom * std::pow(1.18f, io.MouseWheel), 2.f, maxTimelineZoom);
            scroll = std::max(0., anchor - (anchorX - left) / zoom);
        } else if (io.KeyAlt && io.MouseWheel != 0)
            laneZoom = std::clamp(laneZoom * std::pow(1.12f, io.MouseWheel), .35f, 5.f);
        else if (io.KeyShift || io.MouseWheelH != 0)
            scroll = std::max(0., scroll - (io.MouseWheelH != 0 ? -io.MouseWheelH : io.MouseWheel) * 80 / zoom);
        else
            verticalScroll -= io.MouseWheel * 48;
        if (playing)
            followPaused = true;
    }

    // ---- Lane backgrounds, grid, work area, markers.
    dl->AddRectFilled(origin, origin + size, bg0);
    dl->PushClipRect({left, lanesTop}, {right, lanesBottom}, true);
    Id contextOwner = 0;
    if (auto *selectedClip = p.findClip(s.selected))
        contextOwner = p.owner(selectedClip->id)->id;
    for (size_t i = 0; i < p.tracks.size(); ++i) {
        auto &t = p.tracks[i];
        ImU32 back = t.id == contextOwner ? mix(bg0, bg2, .8f) : i % 2 ? mix(bg0, bg1, .5f) : bg0;
        dl->AddRectFilled({left, tops[i]}, {right, tops[i] + heights[i]}, back);
        dl->AddLine({left, tops[i] + heights[i] - .5f}, {right, tops[i] + heights[i] - .5f}, line);
    }
    for (auto &row : groupRows) {
        auto *group = p.findGroup(row.group);
        ImU32 colour = group->color ? fromRgb(group->color) : accent;
        dl->AddRectFilled({left, row.top}, {right, row.top + row.height}, mix(bg0, colour, .10f));
        dl->AddLine({left, row.top + row.height - .5f}, {right, row.top + row.height - .5f}, line);
        if (!group->collapsed)
            continue;
        // Folded group: each member's clips as a thin translucent row.
        size_t members = row.last - row.first + 1;
        float band = (groupPreviewHeight - 6) / float(members), y = row.top + groupHeaderHeight + 2;
        for (size_t k = row.first; k <= row.last; ++k, y += band)
            for (auto &clip : p.tracks[k].clips) {
                float a = x(clip.start), b = std::max(a + 2, x(clip.start + clip.duration));
                if (b >= left && a <= right)
                    dl->AddRectFilled({a, y}, {b, y + std::max(2.f, band - 1)},
                                      alpha(clipColour(p.tracks[k], clip), .55f), 2);
            }
    }
    // Ruler steps from CyDAW Cinema: frames, then seconds, chosen for readable labels.
    float labelWidth = smallTextSize("00:00:00:00").x + 14;
    double frame = 1. / p.fps, step = 3600;
    for (double candidate : {frame, 2 * frame, 5 * frame, 10 * frame, .5, 1., 2., 5., 10., 30., 60., 120., 300.,
                             600., 1800., 3600.})
        if (candidate * zoom >= labelWidth) {
            step = candidate;
            break;
        }
    int division = 1;
    for (int candidate : {10, 5, 4, 2})
        if (step / candidate >= frame - 1e-9 && step / candidate * zoom >= 7 &&
            std::abs(std::round(step / candidate / frame) * frame - step / candidate) < 1e-6) {
            division = candidate;
            break;
        }
    if (division == 1 && step >= .5)
        for (int candidate : {10, 5, 4, 2})
            if (step / candidate * zoom >= 7) {
                division = candidate;
                break;
            }
    double first = std::floor(scroll / step) * step;
    for (double t = first; t <= visibleEnd + step; t += step) {
        dl->AddLine({x(t), lanesTop}, {x(t), lanesBottom}, gridStrong);
        if (step / division * zoom >= 14)
            for (int d = 1; d < division; ++d)
                dl->AddLine({x(t + step * d / division), lanesTop}, {x(t + step * d / division), lanesBottom}, grid);
    }
    if (p.hasWorkArea()) {
        dl->AddRectFilled({x(p.workIn), lanesTop}, {x(p.workOut), lanesBottom}, alpha(accent, loop ? .06f : .025f));
        for (double edge : {p.workIn, p.workOut})
            dl->AddLine({x(edge), lanesTop}, {x(edge), lanesBottom}, alpha(accent, loop ? .45f : .2f));
    }
    for (auto &marker : p.markers) {
        ImU32 colour = marker.color ? fromRgb(marker.color) : warn;
        dl->AddLine({x(marker.time), lanesTop}, {x(marker.time), lanesBottom}, alpha(colour, .45f));
        // A region tints the lanes over its length and closes with a fainter line.
        if (marker.duration > 0) {
            dl->AddRectFilled({x(marker.time), lanesTop}, {x(marker.end()), lanesBottom}, alpha(colour, .035f));
            dl->AddLine({x(marker.end()), lanesTop}, {x(marker.end()), lanesBottom}, alpha(colour, .25f));
        }
    }
    dl->PopClipRect();

    // ---- Lanes input, before drawing so a drag shows its result this frame.
    ImGui::SetCursorScreenPos({left, lanesTop});
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("timelineInput", {laneWidth, viewHeight},
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::BeginDragDropTarget()) {
        int target = trackAt(mouse.y);
        if (auto *payload = ImGui::AcceptDragDropPayload("MEDIA")) {
            // A medium still loading is placed once known, so its sound track is not forgotten.
            std::string path = static_cast<const char *>(payload->Data);
            double at = snapToGrid(rawTime(mouse.x), true);
            if (s.media.get(path).loading) {
                pendingImports.push_back({path, at});
                say("Placed as soon as the medium has loaded");
            } else {
                Json a = Json::Object();
                a["path"] = path;
                a["start"] = at;
                a["proxy"] = autoProxies;
                action("import_media", a);
            }
        }
        if (auto *payload = ImGui::AcceptDragDropPayload("EFFECT")) {
            if (target >= 0)
                for (auto &clip : p.tracks[target].clips)
                    if (clip.active(time(mouse.x))) {
                        Json a = clipArgs(clip.id);
                        a["id"] = static_cast<const char *>(payload->Data);
                        auto &track = p.tracks[target];
                        if (track.kind == TrackKind::Audio && a["id"].S().starts_with("cydaw.")) {
                            auto id = a["id"];
                            a = Json::Object();
                            a["scope"] = "track";
                            a["target"] = track.id;
                            a["id"] = id;
                            action("add_mix_effect", a);
                        } else
                            action("add_effect", a);
                        break;
                    }
        }
        // A span marked in the source monitor overwrites the track it is dropped on.
        if (auto *payload = ImGui::AcceptDragDropPayload("SOURCE")) {
            Json a = Json::Object();
            a["path"] = static_cast<const char *>(payload->Data);
            a["in"] = sourceIn;
            if (sourceOut > sourceIn)
                a["out"] = sourceOut;
            a["start"] = snapToGrid(rawTime(mouse.x), true);
            a["mode"] = "overwrite";
            a["video"] = sourceVideo;
            a["audio"] = sourceAudio;
            if (target >= 0 && !p.tracks[target].locked)
                a[p.tracks[target].kind == TrackKind::Video ? "video_track" : "audio_track"] = p.tracks[target].id;
            action("edit_source", a);
        }
        if (auto *payload = ImGui::AcceptDragDropPayload("SHADERIXEL_LAYER"))
            addLibraryLayer(static_cast<const char *>(payload->Data));
        if (auto *payload = ImGui::AcceptDragDropPayload("TRANSITION")) {
            if (target >= 0)
                for (auto &clip : p.tracks[target].clips)
                    if (clip.active(time(mouse.x))) {
                        auto args = clipArgs(clip.id);
                        args["id"] = static_cast<const char *>(payload->Data);
                        action("set_transition", args);
                        break;
                    }
        }
        ImGui::EndDragDropTarget();
    }
    // Pan with the middle button or the hand tool.
    if (hovered && (ImGui::IsMouseClicked(2) || (tool == Tool::Hand && ImGui::IsMouseClicked(0))))
        gesture = Gesture::Pan;
    if (gesture == Gesture::Pan) {
        scroll = std::max(0., scroll - io.MouseDelta.x / zoom);
        verticalScroll -= io.MouseDelta.y;
        if (!ImGui::IsMouseDown(2) && !ImGui::IsMouseDown(0))
            gesture = Gesture::None;
        if (playing)
            followPaused = true;
    }

    struct Hit {
        Id clip = 0;
        int track = -1, zone = 0; // zone: 0 body, -1/1 edges, 2/3 fade handles, 4 keyframe, 5 gain line
        float a = 0, b = 0, top = 0, bottom = 0;
        double key = 0;
    } hit;
    auto gainLine = [&](const Clip &c, float top, float bottom) {
        float headerBottom = top + std::min(15.f, (bottom - top) * .35f);
        return bottom - 3 - gainPart(c.param("gain", s.time, 0)) * (bottom - headerBottom - 6);
    };
    auto gainAnimated = [](const Clip &c) {
        auto it = c.params.find("gain");
        return it != c.params.end() && !it->second.keys.empty();
    };
    auto clipRect = [&](size_t ti, const Clip &c, float &a, float &b, float &top, float &bottom) {
        a = x(c.start);
        b = std::max(a + 3, x(c.start + c.duration));
        top = tops[ti] + 2;
        bottom = tops[ti] + rows[ti] - 2;
    };
    float header = 15;
    if (hovered && gesture == Gesture::None && !timelineDragging)
        for (size_t ti = 0; ti < p.tracks.size() && !hit.clip; ++ti)
            for (auto &c : p.tracks[ti].clips) {
                float a, b, top, bottom;
                clipRect(ti, c, a, b, top, bottom);
                if (mouse.x < a || mouse.x > b || mouse.y < top || mouse.y > bottom)
                    continue;
                hit = {c.id, int(ti), 0, a, b, top, bottom};
                float headerBottom = top + std::min(header, (bottom - top) * .35f);
                bool chosen = std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
                float fadeIn = x(c.start + c.fadeIn), fadeOut = x(c.start + c.duration - c.fadeOut);
                if (chosen && std::abs(mouse.y - headerBottom) < edgeGrab && std::abs(mouse.x - fadeIn) < edgeGrab)
                    hit.zone = 2;
                else if (chosen && std::abs(mouse.y - headerBottom) < edgeGrab &&
                         std::abs(mouse.x - fadeOut) < edgeGrab)
                    hit.zone = 3;
                else if (!c.effects.empty() && mouse.y < headerBottom && b - a >= 90 &&
                         mouse.x >= std::min(b, right) - smallTextSize(clipBadges(c).c_str(), .74f).x - 4)
                    hit.zone = 6;
                else if (c.transition.enabled && mouse.y > bottom - 11 &&
                         std::abs(mouse.x - x(c.start + c.transition.duration)) < 5)
                    hit.zone = 7;
                else if (mouse.y > bottom - 12) {
                    for (double k : keyTimes(c))
                        if (std::abs(mouse.x - x(k)) < 5) {
                            hit.zone = 4;
                            hit.key = k;
                        }
                }
                if (hit.zone == 0 && p.tracks[ti].kind == TrackKind::Audio && !gainAnimated(c) &&
                    std::abs(mouse.y - gainLine(c, top, bottom)) < 4 && mouse.x - a >= edgeGrab && b - mouse.x >= edgeGrab)
                    hit.zone = 5;
                if (hit.zone == 0 && mouse.x - a < edgeGrab && b - a > 3 * edgeGrab)
                    hit.zone = -1;
                else if (hit.zone == 0 && b - mouse.x < edgeGrab && b - a > 3 * edgeGrab)
                    hit.zone = 1;
                break;
            }
    // Roll: the cut between two adjacent clips of the hovered track.
    Id rollOut = 0, rollIn = 0;
    if (tool == Tool::Roll && hovered && gesture == Gesture::None)
        if (int target = trackAt(mouse.y); target >= 0 && mouse.y < tops[target] + rows[target])
            for (auto &c : p.tracks[target].clips) {
                double end = c.start + c.duration;
                if (std::abs(mouse.x - x(end)) >= edgeGrab)
                    continue;
                for (auto &other : p.tracks[target].clips)
                    if (other.id != c.id && std::abs(other.start - end) < 1e-6) {
                        rollOut = c.id;
                        rollIn = other.id;
                    }
            }
    if (rollOut)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (rollOut && ImGui::IsMouseClicked(0) && !p.owner(rollOut)->locked) {
        gesture = Gesture::Roll;
        gestureId = rollOut;
        gestureOther = rollIn;
        gestureMouse = mouse.x;
        gestureBefore = p;
        s.playing = false;
    }
    if (hit.clip && tool == Tool::Stretch && (hit.zone == -1 || hit.zone == 1))
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (hit.clip && tool == Tool::Select)
        ImGui::SetMouseCursor(hit.zone == -1 || hit.zone == 1 ? ImGuiMouseCursor_ResizeEW
                              : hit.zone == 5                 ? ImGuiMouseCursor_ResizeNS
                              : hit.zone == 7                 ? ImGuiMouseCursor_ResizeEW
                              : hit.zone >= 2                 ? ImGuiMouseCursor_Hand
                                                              : ImGuiMouseCursor_Arrow);
    if (hit.clip && tool == Tool::Select && hit.zone == 4)
        ImGui::SetTooltip(cyui::tr("Keyframe - %s\nClick: move playhead here").c_str(), timecode(hit.key, p.fps).c_str());
    if (hovered && tool == Tool::Hand)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    bool clickedClip = false;
    if (hit.clip && gesture == Gesture::None) {
        auto &t = p.tracks[hit.track];
        auto &c = *p.findClip(hit.clip);
        if (tool == Tool::Blade && ImGui::IsMouseClicked(0)) {
            clickedClip = true;
            double at = snapToGrid(rawTime(mouse.x));
            if (io.KeyShift) {
                std::vector<Id> crossing;
                for (auto &track : p.tracks)
                    for (auto &clip : track.clips)
                        if (at > clip.start + 1e-6 && at < clip.start + clip.duration - 1e-6)
                            crossing.push_back(clip.id);
                Json a = Json::Object();
                a["clips"] = Json::Array();
                for (auto id : crossing)
                    a["clips"].Push(id);
                a["time"] = at;
                if (!crossing.empty())
                    action("split_clips", a);
            } else
                blade(c.id, at);
        } else if (tool == Tool::Stretch && ImGui::IsMouseClicked(0) && !t.locked && (hit.zone == -1 || hit.zone == 1)) {
            clickedClip = true;
            s.selectClip(c.id, false, false);
            gesture = Gesture::Stretch;
            gestureId = c.id;
            gestureIndex = hit.zone;
            gestureMouse = mouse.x;
            gestureBefore = p;
            s.playing = false;
        } else if (tool == Tool::Slide && ImGui::IsMouseClicked(0) && !t.locked) {
            clickedClip = true;
            s.selectClip(c.id, false, false);
            gesture = Gesture::Slide;
            gestureId = c.id;
            gestureMouse = mouse.x;
            gestureBefore = p;
            s.playing = false;
        } else if (tool == Tool::Slip && ImGui::IsMouseClicked(0) && !t.locked) {
            clickedClip = true;
            s.selectClip(c.id, false, false);
            gesture = Gesture::Slip;
            gestureId = c.id;
            gestureValue = c.in;
            gestureMouse = mouse.x;
            gestureBefore = p;
            s.playing = false;
        } else if (tool == Tool::Select && ImGui::IsMouseDoubleClicked(0) && t.kind == TrackKind::Video) {
            openClipEditor(c.id);
            clickedClip = true;
        } else if (tool == Tool::Select && ImGui::IsMouseClicked(0)) {
            clickedClip = true;
            bool additive = io.KeyCtrl || io.KeyShift;
            if (hit.zone == 6) {
                if (std::find(s.selection.begin(), s.selection.end(), c.id) == s.selection.end())
                    s.selectClip(c.id, false, true);
                effectClip = c.id;
                effectSearch.clear();
                ImGui::OpenPopup("effectStack");
            } else if (hit.zone == 7 && !t.locked) {
                gesture = Gesture::Transition;
                gestureId = c.id;
                gestureBefore = p;
                s.playing = false;
            } else if (hit.zone == 4) {
                s.time = std::clamp(hit.key, 0., p.duration);
                if (std::find(s.selection.begin(), s.selection.end(), c.id) == s.selection.end())
                    s.selectClip(c.id, false, true);
            } else if (hit.zone == 5 && !t.locked) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    s.history.checkpoint(p, "Edit");
                    c.params.try_emplace("gain", Parameter{0}).first->second.value = 0;
                    s.dirty = true;
                    s.media.resetAudio();
                } else {
                    gesture = Gesture::Gain;
                    gestureId = c.id;
                    gestureBefore = p;
                    s.playing = false;
                }
            } else if (hit.zone >= 2 && hit.zone <= 3 && !t.locked) {
                gesture = Gesture::Fade;
                gestureId = c.id;
                gestureIndex = hit.zone - 2;
                gestureBefore = p;
                s.playing = false;
            } else {
                bool chosen = std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
                if (!chosen || additive)
                    s.selectClip(c.id, additive, !additive);
                else
                    s.selected = c.id;
                animationEffect = -1;
                animationKey = t.kind == TrackKind::Audio ? "gain" : "opacity";
                draftIdentity.clear();
                if (!t.locked && !additive && hit.zone != 0 && rippleEdits) {
                    gesture = Gesture::Ripple;
                    gestureId = c.id;
                    gestureIndex = hit.zone;
                    gestureMouse = mouse.x;
                    gestureBefore = p;
                    s.playing = false;
                } else if (!t.locked && !additive) {
                    try {
                        timelineTrim = hit.zone;
                        timelineDragClips =
                            p.selection(timelineTrim == 0 ? s.selection : std::vector<Id>{c.id});
                        timelineDragBefore = p;
                        timelineDragging = c.id;
                        timelineStart = c.start;
                        timelineLength = c.duration;
                        timelineIn = c.in;
                        timelineDragX = mouse.x;
                        timelineMoved = 0;
                        timelineMinimum = c.start;
                        timelineSequenceLength = p.duration;
                        for (auto id : timelineDragClips)
                            timelineMinimum = std::min(timelineMinimum, p.findClip(id)->start);
                        s.playing = false;
                    } catch (const std::exception &e) {
                        error = e.what();
                    }
                }
            }
        }
        if (ImGui::IsMouseClicked(1)) {
            clickedClip = true;
            bool chosen = std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
            if (!chosen)
                s.selectClip(c.id, false, true);
            s.selected = c.id;
            contextTime = snapToGrid(rawTime(mouse.x));
            ImGui::OpenPopup("clipContext");
        }
    }

    // Move and trim (the original ShaderixFrame gestures), with CyDAW-style edge magnetism.
    double snapLine = -1;
    if (timelineDragging && ImGui::IsMouseDown(0)) {
        auto *c = p.findClip(timelineDragging);
        if (c) {
            double delta = (mouse.x - timelineDragX) / zoom;
            bool nudging = std::abs(mouse.x - timelineDragX) < 4;
            double threshold = 8 / zoom;
            auto magnet = [&](double value, const std::vector<double> &ends) {
                if (!snapEdges || nudging || io.KeyAlt)
                    return value;
                double best = value, distance = threshold;
                for (double point : snapPoints(timelineDragClips))
                    for (double end : ends)
                        if (std::abs(value + end - point) < distance) {
                            distance = std::abs(value + end - point);
                            best = point - end;
                            snapLine = point;
                        }
                return best;
            };
            double start = c->start, length = c->duration, in = c->in;
            if (timelineTrim == 0) {
                start = snapToGrid(timelineStart + delta);
                double magnetic = magnet(timelineStart + delta, {0, timelineLength});
                if (magnetic != timelineStart + delta)
                    start = magnetic;
            }
            if (timelineTrim == 1) {
                double end = magnet(snapToGrid(timelineStart + timelineLength + delta), {0});
                length = std::max(1. / p.fps, end - timelineStart);
            }
            if (timelineTrim == -1) {
                double pos = magnet(snapToGrid(timelineStart + delta), {0});
                pos = std::clamp(pos, std::max(0., timelineStart - timelineIn / c->speed),
                                 timelineStart + timelineLength - 1. / p.fps);
                length = timelineLength - (pos - timelineStart);
                in = timelineIn + (pos - timelineStart) * c->speed;
                start = pos;
            }
            try {
                bool edited = false;
                if (timelineTrim == 0) {
                    double moved = std::max(-timelineMinimum, start - timelineStart);
                    double change = moved - timelineMoved;
                    if (change != 0) {
                        p.moveClips(timelineDragClips, change);
                        edited = true;
                    }
                    timelineMoved = moved;
                    p.duration = timelineSequenceLength;
                    for (auto id : timelineDragClips) {
                        auto &clip = *p.findClip(id);
                        p.duration = std::max(p.duration, clip.start + clip.duration);
                    }
                } else if (start != c->start || length != c->duration || in != c->in) {
                    p.setTiming(timelineDragging, start, length, in, c->speed, false);
                    edited = true;
                }
                if (edited) {
                    if (timelineDragBefore) {
                        s.history.checkpoint(*timelineDragBefore, "Edit");
                        timelineDragBefore.reset();
                    }
                    s.dirty = true;
                }
            } catch (const std::exception &e) {
                error = e.what();
            }
            if (!nudging) {
                auto *moved = p.findClip(timelineDragging);
                if (moved && timelineTrim == 0)
                    ImGui::SetTooltip(cyui::tr("%s  (%+.2f s)").c_str(), timecode(moved->start, p.fps).c_str(), moved->start - timelineStart);
                else if (moved)
                    ImGui::SetTooltip(cyui::tr("Duration %s - %s -> %s").c_str(), timecode(moved->duration, p.fps).c_str(),
                                      timecode(moved->start, p.fps).c_str(),
                                      timecode(moved->start + moved->duration, p.fps).c_str());
            }
        }
    }
    int dropTrack = -1;
    if (timelineDragging && timelineTrim == 0) {
        auto *owner = p.owner(timelineDragging);
        int target = trackAt(mouse.y);
        if (owner && target >= 0 && &p.tracks[target] != owner && p.tracks[target].kind == owner->kind &&
            !p.tracks[target].locked && timelineDragClips.size() == p.linked(timelineDragging, false).size())
            dropTrack = target;
    }
    if (timelineDragging && ImGui::IsMouseReleased(0)) {
        auto *owner = p.owner(timelineDragging);
        if (dropTrack >= 0 && owner && !owner->locked) {
            auto &dest = p.tracks[dropTrack];
            if (timelineDragBefore) {
                s.history.checkpoint(*timelineDragBefore, "Edit");
                timelineDragBefore.reset();
            }
            Clip moved = *p.findClip(timelineDragging);
            std::erase_if(owner->clips, [&](const Clip &clip) { return clip.id == timelineDragging; });
            dest.clips.push_back(moved);
            s.dirty = true;
        }
        timelineDragging = 0;
        timelineDragBefore.reset();
    }
    // Slip: the source moves under a fixed clip, for the whole linked group.
    if (gesture == Gesture::Slip) {
        if (auto *c = p.findClip(gestureId); c && gestureBefore) {
            double delta = -(mouse.x - gestureMouse) / zoom * c->speed;
            double in = std::max(0., gestureValue + delta);
            if (!c->source.empty())
                try {
                    auto &asset = s.media.get(c->source);
                    if (asset.duration > 0)
                        in = std::min(in, std::max(0., asset.duration - c->duration * c->speed));
                } catch (...) {
                }
            in = snapToGrid(in);
            double change = in - gestureValue;
            try {
                auto changed = *gestureBefore;
                for (auto id : changed.linked(gestureId)) {
                    auto &member = *changed.findClip(id);
                    member.in = std::max(0., member.in + change * member.speed / c->speed);
                }
                changed.validate();
                p = std::move(changed);
                if (std::abs(change) > 1e-9)
                    s.dirty = true;
            } catch (const std::exception &e) {
                error = e.what();
            }
            ImGui::SetTooltip(cyui::tr("Source : %s").c_str(), timecode(in, p.fps).c_str());
        }
        if (!ImGui::IsMouseDown(0)) {
            if (gestureBefore && p.json().Dump(0) != gestureBefore->json().Dump(0))
                s.history.checkpoint(*gestureBefore, "Edit");
            gestureBefore.reset();
            gesture = Gesture::None;
        }
    }
    // Ripple trim: replay the operation from the gesture's snapshot every frame.
    if (gesture == Gesture::Ripple) {
        if (gestureBefore) {
            auto *original = gestureBefore->findClip(gestureId);
            double delta = snapToGrid((mouse.x - gestureMouse) / zoom, false);
            if (original) {
                double change = gestureIndex == 1 ? delta : -delta;
                change = std::max(change, 1. / p.fps - original->duration);
                if (gestureIndex == -1)
                    change = std::min(change, original->in / original->speed);
                try {
                    auto changed = *gestureBefore;
                    if (std::abs(change) > 1e-9)
                        changed.rippleTrim(gestureId, change, gestureIndex == -1);
                    p = std::move(changed);
                    s.dirty = s.dirty || std::abs(change) > 1e-9;
                    error.clear();
                } catch (const std::exception &e) {
                    error = e.what();
                }
                ImGui::SetTooltip(cyui::tr("Ripple %+.2f s").c_str(), change);
            }
        }
        if (!ImGui::IsMouseDown(0)) {
            if (gestureBefore && p.json().Dump(0) != gestureBefore->json().Dump(0))
                s.history.checkpoint(*gestureBefore, "Edit");
            gestureBefore.reset();
            gesture = Gesture::None;
        }
    }
    // Fade handles.
    if (gesture == Gesture::Fade) {
        if (auto *c = p.findClip(gestureId)) {
            double at = snapToGrid(rawTime(mouse.x));
            double other = gestureIndex == 0 ? c->fadeOut : c->fadeIn;
            double value = gestureIndex == 0 ? at - c->start : c->start + c->duration - at;
            value = std::clamp(value, 0., std::max(0., c->duration - other));
            double &target = gestureIndex == 0 ? c->fadeIn : c->fadeOut;
            if (std::abs(value - target) > 1e-9) {
                if (gestureBefore) {
                    s.history.checkpoint(*gestureBefore, "Edit");
                    gestureBefore.reset();
                }
                target = value;
                s.dirty = true;
                s.media.resetAudio();
            }
            ImGui::SetTooltip(cyui::tr("%s fade: %.2f s").c_str(), gestureIndex == 0 ? "incoming" : "outgoing", value);
        }
        if (!ImGui::IsMouseDown(0)) {
            gestureBefore.reset();
            gesture = Gesture::None;
        }
    }

    // Transition length, from its handle on the clip.
    if (gesture == Gesture::Transition) {
        if (auto *c = p.findClip(gestureId); c && c->transition.enabled) {
            double length = std::clamp(snapToGrid(rawTime(mouse.x)) - c->start, 1. / p.fps, c->duration);
            if (std::abs(length - c->transition.duration) > 1e-9) {
                if (gestureBefore) {
                    s.history.checkpoint(*gestureBefore, "Edit");
                    gestureBefore.reset();
                }
                c->transition.duration = length;
                s.dirty = true;
                s.renderer.clearHistory();
            }
            ImGui::SetTooltip(cyui::tr("Transition %s").c_str(), timecode(length, p.fps).c_str());
        }
        if (!ImGui::IsMouseDown(0)) {
            gestureBefore.reset();
            gesture = Gesture::None;
        }
    }
    // Clip gain line, dragged vertically.
    if (gesture == Gesture::Gain) {
        if (auto *c = p.findClip(gestureId)) {
            int ti = -1;
            for (size_t i = 0; i < p.tracks.size(); ++i)
                if (p.owner(c->id) == &p.tracks[i])
                    ti = int(i);
            if (ti >= 0) {
                float top = tops[ti] + 2, bottom = tops[ti] + rows[ti] - 2;
                float headerBottom = top + std::min(15.f, (bottom - top) * .35f);
                float db = std::round(gainFromPart((bottom - 3 - mouse.y) / std::max(1.f, bottom - headerBottom - 6)) * 10) / 10;
                auto &gain = c->params.try_emplace("gain", Parameter{0}).first->second;
                if (std::abs(gain.value - db) > 1e-4f) {
                    if (gestureBefore) {
                        s.history.checkpoint(*gestureBefore, "Edit");
                        gestureBefore.reset();
                    }
                    gain.value = db;
                    s.dirty = true;
                    s.media.resetAudio();
                }
                ImGui::SetTooltip(cyui::tr("Clip Gain %+.1f dB - double-click: 0 dB").c_str(), gain.value);
            }
        }
        if (!ImGui::IsMouseDown(0)) {
            gestureBefore.reset();
            gesture = Gesture::None;
        }
    }
    // Rate stretch and slide, replayed from the gesture's snapshot every frame.
    auto replay = [&](auto apply, const char *label) {
        if (gestureBefore) {
            try {
                auto changed = *gestureBefore;
                std::string tip = apply(changed);
                changed.validate();
                p = std::move(changed);
                error.clear();
                ImGui::SetTooltip(cyui::tr("%s").c_str(), tip.c_str());
            } catch (const std::exception &e) {
                error = e.what();
            }
        }
        if (!ImGui::IsMouseDown(0)) {
            if (gestureBefore && p.json().Dump(0) != gestureBefore->json().Dump(0)) {
                s.history.checkpoint(*gestureBefore, "Edit");
                s.dirty = true;
            }
            gestureBefore.reset();
            gesture = Gesture::None;
        }
        (void)label;
    };
    if (gesture == Gesture::Stretch)
        replay(
            [&](Project &changed) {
                auto original = *gestureBefore->findClip(gestureId);
                double delta = (mouse.x - gestureMouse) / zoom, end = original.start + original.duration;
                double duration = gestureIndex == 1 ? snapToGrid(end + delta) - original.start
                                                    : end - snapToGrid(original.start + delta);
                duration = std::max(duration, 1. / p.fps);
                double speed = std::clamp(original.speed * original.duration / duration, .05, 16.);
                duration = original.duration * original.speed / speed;
                // setTiming carries the linked group (picture and its sound); the source entry stays.
                if (gestureIndex == 1)
                    changed.setTiming(gestureId, original.start, duration, original.in, speed, false);
                else
                    changed.setTiming(gestureId, std::max(0., end - duration), duration, original.in, speed, true);
                char tip[64];
                snprintf(tip, sizeof(tip), "Speed %.0f %% - duration %s", speed * 100, timecode(duration, p.fps).c_str());
                return std::string(tip);
            },
            "stretch");
    if (gesture == Gesture::Slide)
        replay(
            [&](Project &changed) {
                double wanted = snapToGrid((mouse.x - gestureMouse) / zoom, false), frame = 1. / p.fps;
                // The slide is limited by the neighbours of every member of the linked group.
                double low = -1e9, high = 1e9;
                struct Edit {
                    Id clip, before, after;
                };
                std::vector<Edit> edits;
                for (auto id : gestureBefore->linked(gestureId)) {
                    auto &clip = *gestureBefore->findClip(id);
                    Edit e{id, 0, 0};
                    low = std::max(low, -clip.start);
                    for (auto &other : gestureBefore->owner(id)->clips) {
                        if (std::abs(other.start + other.duration - clip.start) < 1e-6) {
                            e.before = other.id;
                            low = std::max(low, frame - other.duration);
                        }
                        if (std::abs(other.start - (clip.start + clip.duration)) < 1e-6) {
                            e.after = other.id;
                            high = std::min(high, other.duration - frame);
                            low = std::max(low, -other.in / other.speed);
                        }
                    }
                    edits.push_back(e);
                }
                double delta = std::clamp(wanted, low, std::max(low, high));
                // Neighbours change one by one (their own groups are edited track by track),
                // then the slid group moves with its animations.
                for (auto &e : edits) {
                    if (e.before) {
                        auto &before = *changed.findClip(e.before);
                        before.duration += delta;
                        before.fadeOut = std::min(before.fadeOut, before.duration);
                    }
                    if (e.after) {
                        auto &after = *changed.findClip(e.after);
                        after.start += delta;
                        after.duration -= delta;
                        after.in += delta * after.speed;
                        after.fadeIn = std::min(after.fadeIn, after.duration);
                    }
                }
                if (std::abs(delta) > 1e-9)
                    changed.moveClips({gestureId}, delta);
                char tip[64];
                snprintf(tip, sizeof(tip), "Slide %+.2f s", delta);
                return std::string(tip);
            },
            "slide");
    // Roll: replayed from the snapshot so the two clips always share their cut.
    if (gesture == Gesture::Roll) {
        if (gestureBefore) {
            auto *outgoing = gestureBefore->findClip(gestureId);
            double delta = snapToGrid((mouse.x - gestureMouse) / zoom, false);
            if (outgoing) {
                double cut = snapToGrid(outgoing->start + outgoing->duration + delta);
                double change = cut - (outgoing->start + outgoing->duration);
                try {
                    auto changed = *gestureBefore;
                    if (std::abs(change) > 1e-9)
                        changed.rollTrim(gestureId, gestureOther, change);
                    p = std::move(changed);
                    s.dirty = s.dirty || std::abs(change) > 1e-9;
                    error.clear();
                } catch (const std::exception &e) {
                    error = e.what();
                }
                ImGui::SetTooltip(cyui::tr("Roll %+.2f s - cut at %s").c_str(), change, timecode(cut, p.fps).c_str());
            }
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (!ImGui::IsMouseDown(0)) {
            if (gestureBefore && p.json().Dump(0) != gestureBefore->json().Dump(0))
                s.history.checkpoint(*gestureBefore, "Edit");
            gestureBefore.reset();
            gesture = Gesture::None;
        }
    }
    // Automation bands: hit-test their label and points before the rubber band claims the click.
    bool clickedAutomation = false;
    struct LanePoint {
        Id clip;
        int index;
        ImVec2 at;
    };
    auto laneRange = [&](const AutomationLane &lane, const Track &track) {
        auto known = parameterRanges.find(lane.key);
        std::pair<float, float> range{0.f, 1.f};
        if (known != parameterRanges.end())
            range = known->second;
        else if (lane.key == "gain")
            range = {-60.f, 12.f};
        else if (lane.key == "pan" || lane.key == "x" || lane.key == "y")
            range = {-1.f, 1.f};
        else if (lane.key == "rotation")
            range = {-180.f, 180.f};
        else if (lane.key == "scale")
            range = {.05f, 4.f};
        for (auto &clip : track.clips) {
            const Parameter *param = nullptr;
            if (lane.effect.empty()) {
                auto it = clip.params.find(lane.key);
                param = it == clip.params.end() ? nullptr : &it->second;
            } else if (auto *effect = laneEffect(clip, lane.effect)) {
                auto it = effect->params.find(lane.key);
                param = it == effect->params.end() ? nullptr : &it->second;
            }
            if (param) {
                range.first = std::min(range.first, param->value);
                range.second = std::max(range.second, param->value);
                for (auto &k : param->keys) {
                    range.first = std::min(range.first, k.value);
                    range.second = std::max(range.second, k.value);
                }
            }
        }
        if (range.second - range.first < 1e-4f)
            range.second = range.first + 1;
        return range;
    };
    auto laneParameter = [&](Clip &clip, const AutomationLane &lane, bool create) -> Parameter * {
        std::map<std::string, Parameter> *params = &clip.params;
        if (!lane.effect.empty()) {
            auto *effect = laneEffect(clip, lane.effect);
            if (!effect)
                return nullptr;
            params = &effect->params;
        }
        auto it = params->find(lane.key);
        if (it != params->end())
            return &it->second;
        if (!create)
            return nullptr;
        return &params->try_emplace(lane.key, Parameter{fallbackValue(lane.key)}).first->second;
    };
    for (size_t ti = 0; ti < p.tracks.size(); ++ti) {
        auto &t = p.tracks[ti];
        auto it = lanes.find(t.id);
        if (it == lanes.end() || !it->second.visible)
            continue;
        auto &lane = it->second;
        float top = tops[ti] + rows[ti], bottom = top + automationHeight;
        if (bottom < lanesTop || top > lanesBottom)
            continue;
        auto range = laneRange(lane, t);
        auto valueY = [&](float v) { return bottom - 8 - lanePart(lane, range, v) * (automationHeight - 26); };
        auto yValue = [&](float y) {
            return laneValue(lane, range, std::clamp((bottom - 8 - y) / (automationHeight - 26), 0.f, 1.f));
        };
        auto label = laneLabel(lane.key, lane.effect) + "  " + icon::chevronDown;
        ImVec2 labelMin{left + 6, top + 4}, labelMax = labelMin + smallTextSize(label.c_str(), .78f) + ImVec2(12, 4);
        bool overLabel = hovered && ImRect(labelMin, labelMax).Contains(mouse);
        if (overLabel && ImGui::IsMouseClicked(0)) {
            contextTrack = t.id;
            clickedAutomation = true;
            ImGui::OpenPopup("laneChooser");
        }
        // Points of every clip of the track.
        std::vector<LanePoint> points;
        for (auto &clip : t.clips)
            if (auto *param = laneParameter(clip, lane, false))
                for (int k = 0; k < int(param->keys.size()); ++k)
                    points.push_back({clip.id, k, {x(param->keys[k].time), valueY(param->keys[k].value)}});
        bool inBand = hovered && mouse.y >= top && mouse.y < bottom && !overLabel;
        if (inBand && !t.locked && gesture == Gesture::None) {
            const LanePoint *closest = nullptr;
            for (auto &point : points)
                if (std::abs(mouse.x - point.at.x) < 7 && std::abs(mouse.y - point.at.y) < 7)
                    closest = &point;
            if (closest && ImGui::IsMouseClicked(0)) {
                clickedAutomation = true;
                gesture = Gesture::Key;
                gestureId = closest->clip;
                gestureIndex = closest->index;
                contextTrack = t.id;
                gestureBefore = p;
            } else if (closest && ImGui::IsMouseClicked(1)) {
                clickedAutomation = true;
                auto *clip = p.findClip(closest->clip);
                if (auto *param = laneParameter(*clip, lane, false)) {
                    s.history.checkpoint(p, "Edit");
                    param->value = param->at(param->keys[closest->index].time);
                    param->keys.erase(param->keys.begin() + closest->index);
                    s.dirty = true;
                }
            } else if (!closest && ImGui::IsMouseDoubleClicked(0)) {
                clickedAutomation = true;
                double at = snapToGrid(rawTime(mouse.x));
                for (auto &clip : t.clips)
                    if (at >= clip.start && at <= clip.start + clip.duration) {
                        auto before = p;
                        if (auto *param = laneParameter(clip, lane, true)) {
                            s.history.checkpoint(before, "Edit");
                            param->setKey({at, yValue(mouse.y), Curve::Smooth, 0});
                            s.dirty = true;
                        }
                        break;
                    }
            } else if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))
                clickedAutomation = true;
        }
        if (gesture == Gesture::Key && contextTrack == t.id) {
            if (auto *clip = p.findClip(gestureId))
                if (auto *param = laneParameter(*clip, lane, false);
                    param && gestureIndex >= 0 && gestureIndex < int(param->keys.size())) {
                    auto &k = param->keys[gestureIndex];
                    double low = gestureIndex ? param->keys[gestureIndex - 1].time + 1. / p.fps : clip->start,
                           high = gestureIndex + 1 < int(param->keys.size())
                                      ? param->keys[gestureIndex + 1].time - 1. / p.fps
                                      : clip->start + clip->duration;
                    double at = low <= high ? std::clamp(snapToGrid(rawTime(mouse.x)), low, high) : k.time;
                    float value = yValue(mouse.y);
                    if (at != k.time || value != k.value) {
                        if (gestureBefore) {
                            s.history.checkpoint(*gestureBefore, "Edit");
                            gestureBefore.reset();
                        }
                        k.time = at;
                        k.value = value;
                        s.dirty = true;
                    }
                    ImGui::SetTooltip(cyui::tr("%s  ·  %.3f").c_str(), timecode(k.time, p.fps).c_str(), k.value);
                }
            if (!ImGui::IsMouseDown(0)) {
                gestureBefore.reset();
                gesture = Gesture::None;
            }
        }
    }

    // Rubber band in empty space.
    if (hovered && !clickedClip && !clickedAutomation && tool == Tool::Select && gesture == Gesture::None &&
        ImGui::IsMouseClicked(0) && trackAt(mouse.y) >= 0) {
        marquee = true;
        marqueeAnchor = mouse;
        marqueeAdd = io.KeyCtrl || io.KeyShift;
        marqueeBase = marqueeAdd ? s.selection : std::vector<Id>{};
        if (!marqueeAdd)
            s.selectClip(0);
    }
    if (hovered && !clickedClip && !clickedAutomation && ImGui::IsMouseClicked(1) && trackAt(mouse.y) >= 0) {
        contextTrack = p.tracks[trackAt(mouse.y)].id;
        contextTime = snapToGrid(rawTime(mouse.x));
        ImGui::OpenPopup("laneContext");
    }

    // ---- Clips.
    dl->PushClipRect({left, lanesTop}, {right, lanesBottom}, true);
    for (size_t ti = 0; ti < p.tracks.size(); ++ti) {
        auto &t = p.tracks[ti];
        if (tops[ti] + rows[ti] < lanesTop || tops[ti] > lanesBottom)
            continue;
        for (auto &c : t.clips) {
            float a, b, top, bottom;
            clipRect(ti, c, a, b, top, bottom);
            if (b < left || a > right)
                continue;
            ImU32 colour = clipColour(t, c);
            bool hidden = !p.audible(t) || !c.enabled;
            if (hidden)
                colour = mix(colour, textFaint, .6f);
            bool chosen = std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
            bool over = hit.clip == c.id;
            float headerHeight = std::min(header, (bottom - top) * .35f);
            dl->AddRectFilled({a, top}, {b, bottom}, darker(colour, over ? .62f : .70f), 3);
            dl->AddRectFilled({a, top}, {b, top + headerHeight}, darker(colour, .15f), 3, ImDrawFlags_RoundCornersTop);
            float visibleA = std::max(a, left), visibleB = std::min(b, right);
            // Contents: filmstrip from the renderer or the audio peaks.
            dl->PushClipRect({std::max(a + 1, left), top + headerHeight}, {std::min(b - 1, right), bottom - 1}, true);
            if (t.kind == TrackKind::Audio && !c.source.empty() && showWaveforms) {
                try {
                    auto &asset = s.media.get(c.source);
                    float mid = (top + headerHeight + bottom) * .5f, amplitude = (bottom - top - headerHeight) * .45f;
                    ImU32 wave = brighter(colour, .25f);
                    double samplesPerPixel = 48000. * c.speed / zoom;
                    size_t frames = asset.samples() / 2;
                    float ceiling = std::max(asset.peakCeiling, 1e-6f);
                    auto sampleAt = [&](size_t n) {
                        return std::make_pair(asset.sample(n * 2), asset.sample(n * 2 + 1));
                    };
                    auto sourceFrame = [&](float xx) { return (c.in + (rawTime(xx) - c.start) * c.speed) * 48000.; };
                    if (frames && samplesPerPixel < 1) {
                        // Closer than one sample per pixel: the samples themselves, as in CyDAW's Foundry.
                        auto first = size_t(std::max(0., std::floor(sourceFrame(visibleA))));
                        auto last = std::min(frames - 1, size_t(std::max(0., std::ceil(sourceFrame(visibleB)))));
                        ImVec2 previous{};
                        for (size_t n = first; n <= last; ++n) {
                            double at = c.start + (double(n) / 48000. - c.in) / c.speed;
                            auto [l, r] = sampleAt(n);
                            float value = (l + r) * .5f / ceiling * c.envelope(at) * std::pow(10.f, c.param("gain", at, 0) / 20);
                            ImVec2 point{x(at), mid - std::clamp(value, -1.f, 1.f) * amplitude};
                            if (n > first)
                                dl->AddLine(previous, point, wave, 1.2f);
                            if (1 / samplesPerPixel > 6)
                                dl->AddCircleFilled(point, 2, wave);
                            previous = point;
                        }
                    } else if (frames && samplesPerPixel < 480) {
                        // Signed minimum and maximum of the PCM under each pixel.
                        for (float xx = visibleA; xx < visibleB; xx += 1) {
                            double s0 = sourceFrame(xx);
                            if (s0 < 0 || s0 >= double(frames))
                                continue;
                            size_t i0 = size_t(s0), i1 = std::min(frames, size_t(s0 + samplesPerPixel) + 1);
                            float low = 1e9f, high = -1e9f;
                            for (size_t i = i0; i < i1; ++i) {
                                auto [l, r] = sampleAt(i);
                                low = std::min({low, l, r});
                                high = std::max({high, l, r});
                            }
                            float gain = c.envelope(rawTime(xx)) * std::pow(10.f, c.param("gain", rawTime(xx), 0) / 20) / ceiling;
                            dl->AddLine({xx, mid - std::clamp(high * gain, -1.f, 1.f) * amplitude},
                                        {xx, mid - std::clamp(low * gain, -1.f, 1.f) * amplitude + 1}, wave);
                        }
                    } else
                    for (float xx = visibleA; xx < visibleB; xx += 1) {
                        double t0 = c.in + (rawTime(xx) - c.start) * c.speed, t1 = t0 + c.speed / zoom;
                        size_t i0 = size_t(std::max(0., t0) * 100), i1 = std::max(i0 + 1, size_t(std::max(0., t1) * 100));
                        float peak = 0;
                        for (size_t i = i0; i < i1 && i < asset.peaks.size(); ++i)
                            peak = std::max(peak, asset.peaks[i]);
                        peak = asset.peakCeiling > 0 ? std::min(1.f, peak / asset.peakCeiling) : 0;
                        float gain = c.envelope(rawTime(xx)) * std::pow(10.f, c.param("gain", rawTime(xx), 0) / 20);
                        if (peak * gain > .002f)
                            dl->AddLine({xx, mid - peak * gain * amplitude}, {xx, mid + peak * gain * amplitude}, wave);
                    }
                    dl->AddLine({visibleA, mid}, {visibleB, mid}, alpha(wave, .3f));
                } catch (...) {
                }
            } else if (t.kind == TrackKind::Video && bottom - top - headerHeight > 14 && showClipThumbnails) {
                auto picture = [&](double at) { return clipPreviews.get(p, c, at, s.renderer); };
                filmstrip({a, top + headerHeight}, {b, bottom}, c.start, c.start + c.duration, picture,
                          hidden ? IM_COL32(255, 255, 255, 90) : IM_COL32_WHITE);
            }
            dl->PopClipRect();
            // Header: name kept readable when the clip starts off-screen, then badges.
            ImU32 ink = light(darker(colour, .15f)) ? bg0 : text;
            std::string badges = clipBadges(c);
            float badgeWidth = badges.empty() || b - a < 90 ? 0 : smallTextSize(badges.c_str(), .74f).x;
            if (badgeWidth > 0) {
                if (over && hit.zone == 6)
                    dl->AddRectFilled({visibleB - badgeWidth - 4, top + 1}, {visibleB - 1, top + headerHeight - 1},
                                      alpha(bg0, .35f), 3);
                smallText(dl, {visibleB - badgeWidth - 2, top + 1}, alpha(ink, over && hit.zone == 6 ? 1.f : .75f),
                          badges.c_str(), .74f);
            }
            float nameX = visibleA + 5 + (c.link ? 10 : 0);
            if (c.link)
                drawLink(dl, {visibleA + 6, top + headerHeight * .5f}, alpha(ink, .8f));
            if (headerHeight >= 9)
                clippedText(dl, {nameX, top}, visibleB - nameX - badgeWidth - 6, ink, c.name.c_str(), .8f);
            // Fades, transition and handles.
            float fadeIn = x(c.start + c.fadeIn), fadeOut = x(c.start + c.duration - c.fadeOut);
            if (c.fadeIn > 0)
                dl->AddLine({a, bottom}, {fadeIn, top + headerHeight}, alpha(text, .6f), 1.2f);
            if (c.fadeOut > 0)
                dl->AddLine({fadeOut, top + headerHeight}, {b, bottom}, alpha(text, .6f), 1.2f);
            if (c.transition.enabled) {
                float end = x(c.start + c.transition.duration);
                dl->AddRectFilled({a, bottom - 7}, {end, bottom}, alpha(style::audio, .65f));
                dl->AddLine({a, bottom}, {end, bottom - 7}, alpha(text, .8f));
                if (chosen || over)
                    dl->AddRectFilled({end - 2, bottom - 10}, {end + 2, bottom}, over && hit.zone == 7 ? text : style::audio, 1);
            }
            // Keyframes of the clip and its effects, as diamonds along the bottom edge.
            for (double k : showKeyframes ? keyTimes(c) : std::vector<double>{}) {
                float kx = x(k);
                if (kx < visibleA - 4 || kx > visibleB + 4)
                    continue;
                bool hot = over && hit.zone == 4 && std::abs(hit.key - k) < 1e-6;
                float r = hot ? 4.5f : 3.5f, ky = bottom - 5;
                dl->AddQuadFilled({kx, ky - r}, {kx + r, ky}, {kx, ky + r}, {kx - r, ky}, hot ? text : control);
                dl->AddQuad({kx, ky - r}, {kx + r, ky}, {kx, ky + r}, {kx - r, ky}, alpha(bg0, .8f));
            }
            // Clip gain: a line to drag on audio clips, the curve when it is animated.
            if (t.kind == TrackKind::Audio && bottom - top > 30) {
                float headerBottom = top + headerHeight;
                auto gainY = [&](double at) {
                    return bottom - 3 - gainPart(c.param("gain", at, 0)) * (bottom - headerBottom - 6);
                };
                ImU32 gainColour = (over && hit.zone == 5) || (gesture == Gesture::Gain && gestureId == c.id) ? text : alpha(control, .85f);
                if (gainAnimated(c)) {
                    ImVec2 previous{visibleA, gainY(rawTime(visibleA))};
                    for (float xx = visibleA + 3; xx <= visibleB; xx += 3) {
                        ImVec2 point{xx, gainY(rawTime(xx))};
                        dl->AddLine(previous, point, alpha(control, .6f), 1.2f);
                        previous = point;
                    }
                } else
                    dl->AddLine({visibleA + 1, gainY(s.time)}, {visibleB - 1, gainY(s.time)}, gainColour,
                                over && hit.zone == 5 ? 2.f : 1.2f);
            }
            if (chosen || over) {
                for (float fx : {fadeIn, fadeOut})
                    dl->AddRectFilled({fx - 3.5f, top + headerHeight - 3.5f}, {fx + 3.5f, top + headerHeight + 3.5f}, text);
            }
            if (t.locked)
                smallText(dl, {visibleB - 14, bottom - 16}, alpha(text, .6f), icon::lock, .8f);
            if (!c.enabled) {
                // Hatched like a muted region: the clip stays in place without being rendered or mixed.
                dl->PushClipRect({visibleA, top + headerHeight}, {visibleB, bottom}, true);
                for (float hx = std::floor((visibleA - (bottom - top)) / 10) * 10; hx < visibleB; hx += 10)
                    dl->AddLine({hx, bottom}, {hx + (bottom - top - headerHeight), top + headerHeight},
                                IM_COL32(0, 0, 0, 90), 2);
                dl->PopClipRect();
                smallText(dl, {visibleA + 6, top + headerHeight + 3}, alpha(text, .85f), "DISABLED", .72f);
            }
            if (chosen)
                dl->AddRect({a, top}, {b, bottom}, text, 3, 0, 1.5f);
            else if (over)
                dl->AddRect({a, top}, {b, bottom}, alpha(text, .35f), 3);
            else
                dl->AddRect({a, top}, {b, bottom}, alpha(colour, .45f), 3);
        }
    }
    if (dropTrack >= 0)
        if (auto *c = p.findClip(timelineDragging))
            dl->AddRect({x(c->start), tops[dropTrack] + 2}, {x(c->start + c->duration), tops[dropTrack] + rows[dropTrack] - 2},
                        accent, 3, 0, 1.5f);
    // Automation bands.
    for (size_t ti = 0; ti < p.tracks.size(); ++ti) {
        auto &t = p.tracks[ti];
        auto it = lanes.find(t.id);
        if (it == lanes.end() || !it->second.visible)
            continue;
        auto &lane = it->second;
        float top = tops[ti] + rows[ti], bottom = top + automationHeight;
        if (bottom < lanesTop || top > lanesBottom)
            continue;
        dl->AddRectFilled({left, top}, {right, bottom}, IM_COL32(0, 0, 0, 56));
        dl->AddLine({left, top}, {right, top}, gridStrong);
        auto range = laneRange(lane, t);
        auto valueY = [&](float v) { return bottom - 8 - lanePart(lane, range, v) * (automationHeight - 26); };
        if (lane.effect.empty() && lane.key == "gain") {
            // CyDAW's dashed 0 dB reference.
            float zero = valueY(0);
            for (float xx = left; xx < right; xx += 8)
                dl->AddLine({xx, zero}, {std::min(xx + 4, right), zero}, alpha(textFaint, .35f));
        }
        ImU32 colour = lane.key == "gain" || lane.key == "opacity" ? control : modulation;
        bool any = false;
        for (auto &clip : t.clips) {
            float a = std::max(left, x(clip.start)), b = std::min(right, x(clip.start + clip.duration));
            if (b <= a)
                continue;
            auto *param = laneParameter(clip, lane, false);
            if (!param) {
                dl->AddLine({a, valueY(fallbackValue(lane.key))}, {b, valueY(fallbackValue(lane.key))},
                            alpha(colour, .18f));
                continue;
            }
            any = any || !param->keys.empty();
            ImVec2 previous{a, valueY(param->at(rawTime(a)))};
            std::vector<ImVec2> fill{{a, bottom}};
            fill.push_back(previous);
            for (float xx = a + 2; xx <= b + 1; xx += 2) {
                ImVec2 point{std::min(xx, b), valueY(param->at(rawTime(std::min(xx, b))))};
                dl->AddLine(previous, point, param->keys.empty() ? alpha(colour, .45f) : colour, 1.5f);
                fill.push_back(point);
                previous = point;
            }
            for (int k = 0; k < int(param->keys.size()); ++k) {
                ImVec2 at{x(param->keys[k].time), valueY(param->keys[k].value)};
                bool active = gesture == Gesture::Key && gestureId == clip.id && gestureIndex == k;
                bool closest = hovered && std::abs(mouse.x - at.x) < 7 && std::abs(mouse.y - at.y) < 7;
                if (active)
                    dl->AddCircleFilled(at, 5, text), dl->AddCircle(at, 5, colour, 12, 1.5f);
                else
                    dl->AddCircleFilled(at, closest ? 4.5f : 3.5f, colour);
            }
        }
        auto label = laneLabel(lane.key, lane.effect) + "  " + icon::chevronDown;
        ImVec2 labelMin{left + 6, top + 4}, labelMax = labelMin + smallTextSize(label.c_str(), .78f) + ImVec2(12, 4);
        bool overLabel = hovered && ImRect(labelMin, labelMax).Contains(mouse);
        dl->AddRectFilled(labelMin, labelMax, overLabel ? mix(bg2, colour, .35f) : alpha(bg1, .92f), 3);
        smallText(dl, labelMin + ImVec2(6, 2), text, label.c_str(), .78f);
        if (!any)
            smallText(dl, {labelMax.x + 10, top + 6}, textFaint, "double-click: add key - right-click: remove", .74f);
    }
    // Roll: brackets on the cut being hovered or dragged.
    {
        Id out = gesture == Gesture::Roll ? gestureId : rollOut;
        if (auto *c = out ? p.findClip(out) : nullptr)
            for (size_t ti = 0; ti < p.tracks.size(); ++ti)
                if (p.owner(c->id) == &p.tracks[ti]) {
                    float cx = x(c->start + c->duration), y0 = tops[ti] + 2, y1 = tops[ti] + rows[ti] - 2;
                    dl->AddLine({cx, y0}, {cx, y1}, accent, 2);
                    dl->AddLine({cx - 6, y0}, {cx + 6, y0}, accent, 2);
                    dl->AddLine({cx - 6, y1}, {cx + 6, y1}, accent, 2);
                }
    }
    // Blade preview.
    if (tool == Tool::Blade && hovered) {
        double at = snapToGrid(rawTime(mouse.x));
        int target = trackAt(mouse.y);
        float y0 = io.KeyShift || target < 0 ? lanesTop : tops[target], y1 = io.KeyShift || target < 0 ? lanesBottom : tops[target] + rows[target];
        dl->AddLine({x(at), y0}, {x(at), y1}, text, 1.2f);
    }
    // Rubber band.
    if (marquee) {
        ImVec2 low{std::max(left, std::min(marqueeAnchor.x, mouse.x)), std::min(marqueeAnchor.y, mouse.y)};
        ImVec2 high{std::min(right, std::max(marqueeAnchor.x, mouse.x)), std::max(marqueeAnchor.y, mouse.y)};
        auto ids = marqueeBase;
        if (std::abs(mouse.x - marqueeAnchor.x) > 3 || std::abs(mouse.y - marqueeAnchor.y) > 3)
            for (size_t ti = 0; ti < p.tracks.size(); ++ti)
                for (auto &clip : p.tracks[ti].clips)
                    if (x(clip.start) <= high.x && x(clip.start + clip.duration) >= low.x && tops[ti] + 2 <= high.y &&
                        tops[ti] + rows[ti] - 2 >= low.y)
                        ids.push_back(clip.id);
        s.setSelection(ids);
        dl->AddRectFilled(low, high, alpha(accent, .08f));
        dl->AddRect(low, high, alpha(accent, .7f));
        if (ImGui::IsMouseReleased(0) || !ImGui::IsMouseDown(0))
            marquee = false;
    }
    float play = x(s.time);
    dl->AddLine({play, lanesTop}, {play, lanesBottom}, playing ? style::play : accent, 1.5f);
    if (snapLine >= 0) {
        dl->AddLine({x(snapLine), lanesTop}, {x(snapLine), lanesBottom}, alpha(text, .85f), 1);
        dl->AddTriangleFilled({x(snapLine) - 4, lanesTop}, {x(snapLine) + 4, lanesTop}, {x(snapLine), lanesTop + 6}, text);
    }
    dl->PopClipRect();

    // Follow playback: jump when the playhead leaves 88 % of the width.
    if (playing && follow && !followPaused && gesture == Gesture::None && !timelineDragging &&
        (s.time > scroll + laneWidth * .88 / zoom || s.time < scroll))
        scroll = std::max(0., s.time - laneWidth * .1 / zoom);

    // ---- Navigator band (drag: sideways to scroll, vertically to zoom; double click: fit).
    ImGui::SetCursorScreenPos({left, navTop});
    ImGui::InvisibleButton("CyDAWNavigator", {laneWidth, navigatorHeight});
    if (ImGui::IsItemActivated()) {
        navigateAnchor = rawTime(mouse.x);
        gesture = Gesture::Navigator;
    }
    if (ImGui::IsItemActive()) {
        zoom = std::clamp(zoom * std::exp(io.MouseDelta.y * .012f), 2.f, maxTimelineZoom);
        scroll = std::max(0., navigateAnchor - (mouse.x - left) / zoom);
        if (playing)
            followPaused = true;
    } else if (gesture == Gesture::Navigator)
        gesture = Gesture::None;
    bool navHovered = ImGui::IsItemHovered();
    if (navHovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        tooltip("Drag left/right to pan, down to zoom in, up to zoom out. Double-click: fit all.");
        if (ImGui::IsMouseDoubleClicked(0))
            fitTimeline(laneWidth);
    }
    dl->AddRectFilled({left, navTop}, {right, rulerTop}, ImGui::IsItemActive() ? bg3 : navHovered ? mix(bg2, bg3, .5f) : bg2);
    {
        double tick = 1;
        while (tick * zoom < 8)
            tick *= 2;
        for (double t = std::floor(scroll / tick) * tick; t <= visibleEnd; t += tick) {
            bool strong = std::fmod(t + 1e-6, tick * 4) < 1e-3;
            dl->AddLine({x(t), strong ? navTop + 3 : navTop + navigatorHeight * .5f}, {x(t), rulerTop},
                        alpha(textFaint, strong ? .55f : .25f));
        }
        // Whole-sequence overview: proportional thumb of the visible span.
        double total = std::max(p.duration, visibleEnd);
        float thumbA = left + float(scroll / total) * laneWidth, thumbB = left + float(visibleEnd / total) * laneWidth;
        if (scroll > 1e-6 || visibleEnd < p.duration)
            dl->AddRectFilled({thumbA, rulerTop - 3}, {std::max(thumbA + 6, thumbB), rulerTop - 1}, alpha(accent, .6f), 1);
        dl->AddLine({left, rulerTop - .5f}, {right, rulerTop - .5f}, line);
    }

    // ---- Ruler: timecode, work area strip, markers and playhead handle.
    ImGui::SetCursorScreenPos({left, rulerTop});
    ImGui::InvisibleButton("timelineRuler", {laneWidth, rulerHeight}, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool rulerHovered = ImGui::IsItemHovered();
    float workTop = rulerTop + 1, workBottom = rulerTop + 8;
    float markerY = rulerTop + 9;
    Marker *overMarker = nullptr;
    for (auto &marker : p.markers)
        if (rulerHovered && std::abs(mouse.x - x(marker.time)) < 6 && mouse.y >= markerY - 1 && mouse.y <= markerY + 11)
            overMarker = &marker;
    // The end of a region is a handle on its band, under the marker row.
    Marker *overRegionEnd = nullptr;
    if (!overMarker)
        for (auto &marker : p.markers)
            if (rulerHovered && marker.duration > 0 && std::abs(mouse.x - x(marker.end())) < 5 &&
                mouse.y >= markerY + 9 && mouse.y <= markerY + 19)
                overRegionEnd = &marker;
    if (overRegionEnd || gesture == Gesture::RegionEnd)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    int workZone = 0; // 1 in, 2 out, 3 body
    if (p.hasWorkArea() && rulerHovered && mouse.y <= workBottom + 2) {
        if (std::abs(mouse.x - x(p.workIn)) < 5)
            workZone = 1;
        else if (std::abs(mouse.x - x(p.workOut)) < 5)
            workZone = 2;
        else if (mouse.x > x(p.workIn) && mouse.x < x(p.workOut))
            workZone = 3;
    }
    if (workZone == 1 || workZone == 2)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (rulerHovered && ImGui::IsMouseClicked(0)) {
        gestureBefore = p;
        gestureTime = rawTime(mouse.x);
        if (overMarker) {
            gesture = Gesture::Marker;
            gestureId = overMarker->id;
            s.time = std::clamp(overMarker->time, 0., p.duration);
            if (ImGui::IsMouseDoubleClicked(0)) {
                renamingMarker = overMarker->id;
                renameDraft = overMarker->name;
                renameFocus = true;
                ImGui::OpenPopup("markerRename");
                gesture = Gesture::None;
            }
        } else if (overRegionEnd) {
            gesture = Gesture::RegionEnd;
            gestureId = overRegionEnd->id;
        } else if (workZone) {
            gesture = workZone == 1 ? Gesture::WorkIn : workZone == 2 ? Gesture::WorkOut : Gesture::WorkMove;
            gestureValue = p.workIn;
        } else if (io.KeyShift || mouse.y < rulerTop + rulerHeight * .4f) {
            gesture = Gesture::WorkDraw;
            gestureTime = snapToGrid(rawTime(mouse.x), true);
        } else
            gesture = Gesture::Seek;
    }
    if (rulerHovered && ImGui::IsMouseClicked(1)) {
        contextTime = std::clamp(snapToGrid(rawTime(mouse.x)), 0., p.duration);
        contextMarker = overMarker ? overMarker->id : 0;
        ImGui::OpenPopup("rulerContext");
    }
    auto commitWork = [&](double a, double b) {
        a = std::clamp(a, 0., p.duration);
        b = std::clamp(b, 0., p.duration);
        if (b < a)
            std::swap(a, b);
        if (b - a < 1. / p.fps)
            return;
        if (a != p.workIn || b != p.workOut) {
            if (gestureBefore) {
                s.history.checkpoint(*gestureBefore, "Edit");
                gestureBefore.reset();
            }
            p.workIn = a;
            p.workOut = b;
            s.dirty = true;
        }
    };
    switch (gesture) {
    case Gesture::Seek:
        s.time = std::clamp(time(mouse.x), 0., p.duration);
        break;
    case Gesture::WorkDraw:
        commitWork(gestureTime, snapToGrid(rawTime(mouse.x), true));
        break;
    case Gesture::WorkIn:
        commitWork(std::min(snapToGrid(rawTime(mouse.x)), p.workOut - 1. / p.fps), p.workOut);
        break;
    case Gesture::WorkOut:
        commitWork(p.workIn, std::max(snapToGrid(rawTime(mouse.x)), p.workIn + 1. / p.fps));
        break;
    case Gesture::WorkMove: {
        double length = p.workOut - p.workIn;
        double start = std::clamp(snapToGrid(gestureValue + rawTime(mouse.x) - gestureTime), 0., p.duration - length);
        commitWork(start, start + length);
        break;
    }
    case Gesture::RegionEnd:
        if (auto *marker = p.findMarker(gestureId)) {
            double end = std::clamp(snapToGrid(rawTime(mouse.x)), marker->time + 1. / p.fps, p.duration);
            if (std::abs(end - marker->end()) > 1e-9) {
                if (gestureBefore) {
                    s.history.checkpoint(*gestureBefore, "Region Duration");
                    gestureBefore.reset();
                }
                marker->duration = end - marker->time;
                s.dirty = true;
            }
        }
        break;
    case Gesture::Marker:
        if (auto *marker = p.findMarker(gestureId)) {
            // A region moves whole and stays inside the sequence.
            double at = std::clamp(snapToGrid(rawTime(mouse.x)), 0., std::max(0., p.duration - marker->duration));
            if (std::abs(mouse.x - x(gestureTime)) > 3 && at != marker->time) {
                if (gestureBefore) {
                    s.history.checkpoint(*gestureBefore, "Edit");
                    gestureBefore.reset();
                }
                marker->time = at;
                std::stable_sort(p.markers.begin(), p.markers.end(),
                                 [](const Marker &l, const Marker &r) { return l.time < r.time; });
                s.dirty = true;
                s.time = at;
            }
        }
        break;
    default:
        break;
    }
    if ((gesture == Gesture::Seek || gesture == Gesture::WorkDraw || gesture == Gesture::WorkIn ||
         gesture == Gesture::WorkOut || gesture == Gesture::WorkMove || gesture == Gesture::Marker ||
         gesture == Gesture::RegionEnd) &&
        !ImGui::IsMouseDown(0)) {
        gesture = Gesture::None;
        gestureBefore.reset();
    }
    dl->AddRectFilled({left, rulerTop}, {right, lanesTop}, bg1);
    dl->AddLine({left, lanesTop - .5f}, {right, lanesTop - .5f}, lineStrong);
    dl->PushClipRect({left, rulerTop}, {right, lanesTop}, true);
    for (double t = first; t <= visibleEnd + step; t += step) {
        float xx = x(t);
        dl->AddLine({xx, rulerTop + rulerHeight * .42f}, {xx, lanesTop}, textFaint);
        auto code = timecode(t, p.fps);
        smallText(dl, {xx + 3, rulerTop + rulerHeight * .38f}, textDim, code.c_str(), .78f);
        if (step / division * zoom >= 5)
            for (int d = 1; d < division; ++d) {
                float minor = x(t + step * d / division);
                dl->AddLine({minor, lanesTop - (d * 2 == division ? 7.f : 4.f)}, {minor, lanesTop}, alpha(textFaint, .7f));
            }
    }
    if (p.hasWorkArea()) {
        float a = x(p.workIn), b = x(p.workOut);
        ImU32 strip = loop ? alpha(accent, .85f) : alpha(textFaint, .7f);
        dl->AddRectFilled({a, workTop}, {b, workBottom}, strip, 2);
        dl->AddRectFilled({a - 1, workTop}, {a + 2, workBottom + 3}, workZone == 1 ? text : strip);
        dl->AddRectFilled({b - 2, workTop}, {b + 1, workBottom + 3}, workZone == 2 ? text : strip);
    } else if (rulerHovered && mouse.y < rulerTop + rulerHeight * .4f)
        dl->AddRectFilled({left, workTop}, {right, workBottom}, alpha(textFaint, .12f), 2);
    for (size_t i = 0; i < p.markers.size(); ++i) {
        auto &marker = p.markers[i];
        float mx = x(marker.time);
        if (mx < left - 120 || mx > right + 6)
            continue;
        ImU32 colour = marker.color ? fromRgb(marker.color) : warn;
        bool over = overMarker == &marker;
        if (marker.duration > 0)
            dl->AddRectFilled({mx, markerY + 14}, {std::max(mx + 2, x(marker.end())), markerY + 17},
                              alpha(colour, over ? .9f : .55f), 1);
        dl->AddTriangleFilled({mx - 5, markerY}, {mx + 5, markerY}, {mx, markerY + 8}, over ? text : colour);
        float room = i + 1 < p.markers.size() ? x(p.markers[i + 1].time) - mx - 14 : right - mx;
        // The name sits on a ruler-coloured plate so the timecodes under it do not show through.
        float labelWidth = std::min({room, 140.f, ImGui::CalcTextSize(marker.name.c_str()).x * .72f + 4});
        if (labelWidth > 4 && !marker.name.empty())
            dl->AddRectFilled({mx + 5, markerY - 2}, {mx + 7 + labelWidth, markerY + ImGui::GetFontSize() * .72f},
                              bg1, 2);
        clippedText(dl, {mx + 7, markerY - 2}, std::min(room, 140.f), alpha(colour, .95f), marker.name.c_str(), .72f);
    }
    if (!renderCache.empty()) {
        // Render bar, as in Premiere: green where frames are pre-rendered, grey where they are still coming.
        float barTop = stripTop - 3, barBottom = stripTop - 1;
        float a = std::max(left, x(renderCache.begin())), b = std::min(right, x(renderCache.end()));
        dl->AddRectFilled({a, barTop}, {b, barBottom}, bg5);
        double frameWidth = zoom / renderCache.frameRate();
        int step = std::max(1, int(std::ceil(2 / std::max(1e-6, frameWidth))));
        int firstFrame = int(std::round(renderCache.begin() * renderCache.frameRate()));
        for (int index = std::max(0, int((rawTime(a) - renderCache.begin()) * renderCache.frameRate()));
             index < renderCache.frameCount(); index += step) {
            float fx = x(double(firstFrame + index) / renderCache.frameRate());
            if (fx > b)
                break;
            if (renderCache.ready(index))
                dl->AddRectFilled({fx, barTop}, {std::min(b, fx + float(frameWidth * step) + .5f), barBottom},
                                  style::play);
        }
    }
    dl->AddTriangleFilled({play - 5, lanesTop - 9}, {play + 5, lanesTop - 9}, {play, lanesTop - 1}, playing ? style::play : accent);
    dl->AddLine({play, rulerTop + rulerHeight * .4f}, {play, lanesTop}, playing ? style::play : accent);
    dl->PopClipRect();

    // ---- IMAGE row: frames of the final composition, as CyDAW Cinema's video filmstrip.
    if (showFilmstrip) {
        float stripBottom = lanesTop;
        ImGui::SetCursorScreenPos({left, stripTop});
        ImGui::InvisibleButton("timelineFilmstrip", {laneWidth, filmstripHeight});
        bool stripHovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive()) {
            double frameTime = std::round(std::max(0., rawTime(mouse.x)) * p.fps) / p.fps;
            s.time = std::clamp(frameTime, 0., p.duration);
        }
        if (stripHovered)
            tooltip("Final Render Frame by Frame: Drag to Scrub");
        dl->AddRectFilled({left, stripTop}, {right, stripBottom}, bg0);
        dl->PushClipRect({left, stripTop}, {right, stripBottom}, true);
        auto picture = [&](double at) { return clipPreviews.getSequence(p, at, s.renderer); };
        filmstrip({left, stripTop + 3}, {right, stripBottom - 2}, 0, p.duration, picture, IM_COL32_WHITE);
        if (p.hasWorkArea()) {
            dl->AddRectFilled({left, stripTop}, {x(p.workIn), stripBottom}, IM_COL32(0, 0, 0, loop ? 110 : 60));
            dl->AddRectFilled({x(p.workOut), stripTop}, {right, stripBottom}, IM_COL32(0, 0, 0, loop ? 110 : 60));
        }
        dl->AddLine({play, stripTop}, {play, stripBottom}, playing ? style::play : accent, 2);
        dl->PopClipRect();
        dl->AddLine({origin.x, stripBottom - .5f}, {right, stripBottom - .5f}, lineStrong);
        dl->AddRectFilled({origin.x, stripTop}, {left, stripBottom}, bg1);
        dl->AddRectFilled({origin.x, stripTop}, {origin.x + 4, stripBottom}, accent);
        ImGui::PushFont(boldFont(), 0);
        clippedText(dl, {origin.x + 12, stripTop + 6}, headerWidth - 20, text, "IMAGE");
        ImGui::PopFont();
        smallText(dl, {origin.x + 12, stripTop + 26}, textFaint, "Final render", .74f);
        auto code = timecode(s.time, p.fps);
        ImGui::PushFont(sx::ui::GetFonts().mono, 0);
        clippedText(dl, {origin.x + 12, stripTop + 44}, headerWidth - 20, playing ? style::play : accent, code.c_str(), .9f);
        ImGui::PopFont();
    }

    // ---- Track headers (clipped to their column).
    dl->AddRectFilled({origin.x, lanesTop}, {left, lanesBottom}, bg1);
    ImGui::PushClipRect({origin.x, lanesTop}, {left, lanesBottom}, true);
    int moveTarget = -1;
    for (size_t i = 0; i < p.tracks.size(); ++i) {
        auto &t = p.tracks[i];
        float top = tops[i], h = heights[i], bottom = top + h;
        if (bottom < lanesTop || top > lanesBottom)
            continue;
        ImGui::PushID(int(t.id));
        bool isContext = t.id == contextOwner;
        ImU32 colour = trackColour(t);
        auto *memberOf = t.group ? p.findGroup(t.group) : nullptr;
        float indent = memberOf ? 7 : 0;
        dl->AddRectFilled({origin.x, top}, {left, bottom}, isContext ? bg3 : bg2);
        if (memberOf) {
            ImU32 groupColour = memberOf->color ? fromRgb(memberOf->color) : accent;
            dl->AddRectFilled({origin.x, top}, {left, bottom}, alpha(groupColour, .06f));
            dl->AddRectFilled({origin.x, top}, {origin.x + 4, bottom}, groupColour);
            dl->AddRectFilled({origin.x + 8, top + 3}, {origin.x + 11, bottom - 3}, colour, 1.5f);
        } else
            dl->AddRectFilled({origin.x, top}, {origin.x + 4, bottom}, colour);
        dl->AddLine({origin.x, bottom - .5f}, {left, bottom - .5f}, line);
        if (t.id == targetVideo || t.id == targetAudio)
            dl->AddTriangleFilled({left - 1, top + 4}, {left - 1, top + 16}, {left - 8, top + 10}, accent);
        ImGui::SetCursorScreenPos({origin.x, top});
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##headerSelect", {headerWidth, std::max(1.f, h - 5)},
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool headerHovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActivated()) {
            gestureMouse = mouse.y;
            gestureId = t.id;
        }
        if (ImGui::IsItemActive() && gesture == Gesture::None && std::abs(mouse.y - gestureMouse) > 5 && !t.locked)
            gesture = Gesture::TrackMove;
        if (headerHovered && ImGui::IsMouseDoubleClicked(0) && mouse.y < top + 24) {
            renamingTrack = t.id;
            renameDraft = t.name;
            renameFocus = true;
        }
        if (headerHovered && ImGui::IsMouseClicked(1)) {
            contextTrack = t.id;
            ImGui::OpenPopup("trackContext");
        }
        // Name, or the inline rename field.
        if (renamingTrack == t.id) {
            ImGui::SetCursorScreenPos({origin.x + 10, top + 3});
            ImGui::SetNextItemWidth(headerWidth - 20);
            if (renameFocus) {
                ImGui::SetKeyboardFocusHere();
                renameFocus = false;
            }
            bool done = ImGui::InputText("##rename", &renameDraft,
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (done && !renameDraft.empty() && renameDraft != t.name) {
                Json a = Json::Object();
                a["track"] = t.id;
                a["name"] = renameDraft;
                action("set_track", a);
            }
            if (done || (!ImGui::IsItemActive() && !ImGui::IsItemActivated() && !renameFocus &&
                         (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Escape))))
                renamingTrack = 0;
        } else {
            const char *tag = t.kind == TrackKind::Audio ? "AUDIO" : "VIDEO";
            float tagWidth = smallTextSize(tag, .74f).x;
            ImGui::PushFont(boldFont(), 0);
            clippedText(dl, {origin.x + 12 + indent, top + 4}, headerWidth - tagWidth - 26 - indent, text, t.name.c_str());
            ImGui::PopFont();
            smallText(dl, {left - tagWidth - 8, top + 7}, t.kind == TrackKind::Audio ? style::audio : accent, tag, .74f);
        }
        // Toggles: visibility or M, S, lock, automation, then a meter for audio.
        if (rows[i] >= 40 && renamingTrack != t.id) {
            float buttonY = top + std::min(rows[i] - 21, 26.f);
            ImGui::SetCursorScreenPos({origin.x + 12 + indent, buttonY});
            ImVec2 toggleSize{22, 17};
            auto setTrack = [&](const char *field, bool value) {
                Json a = Json::Object();
                a["track"] = t.id;
                a[field] = value;
                action("set_track", a);
            };
            if (t.kind == TrackKind::Video) {
                if (toggle(t.muted ? "—" : "V", !t.muted, accent, toggleSize,
                           t.muted ? "Hidden Track: Click to Show" : "Hide Track"))
                    setTrack("muted", !t.muted);
            } else if (toggle("M", t.muted, mute, toggleSize, "Mute"))
                setTrack("muted", !t.muted);
            ImGui::SameLine(0, 3);
            if (toggle("S", t.solo, solo, toggleSize, "Solo"))
                setTrack("solo", !t.solo);
            ImGui::SameLine(0, 3);
            if (toggle(icon::lock, t.locked, record, toggleSize, "Lock Track"))
                setTrack("locked", !t.locked);
            ImGui::SameLine(0, 3);
            auto &lane = lanes[t.id];
            if (toggle("A", lane.visible, control, toggleSize, "Automation Lane Below Track")) {
                lane.visible = !lane.visible;
                if (lane.visible && lane.key == "opacity" && t.kind == TrackKind::Audio)
                    lane.key = "gain";
            }
            ImGui::SameLine(0, 3);
            Id &target = t.kind == TrackKind::Video ? targetVideo : targetAudio;
            if (toggle("C", target == t.id, accent, toggleSize,
                       "Target Track for Source Monitor Edits (Insert / Overwrite)"))
                target = target == t.id ? 0 : t.id;
            if (t.kind == TrackKind::Video) {
                // FX: the track's effect graph; lit when it has one. Effects dropped on it join the chain.
                ImGui::SameLine(0, 3);
                if (toggle("FX", !t.graph.IsNull(), accent, {26, 17},
                           "Track effects graph (drop an effect here to add it to the whole track)"))
                    openTrackGraph(t.id);
                if (ImGui::BeginDragDropTarget()) {
                    if (auto *payload = ImGui::AcceptDragDropPayload("EFFECT")) {
                        Json a = Json::Object();
                        a["track"] = t.id;
                        a["id"] = static_cast<const char *>(payload->Data);
                        if (action("add_track_effect", a))
                            openTrackGraph(t.id);
                    }
                    // A blend card dropped here sets how the track lies over the tracks below.
                    if (auto *payload = ImGui::AcceptDragDropPayload("BLEND")) {
                        Json a = Json::Object();
                        a["track"] = t.id;
                        a["blend_effect"] = static_cast<const char *>(payload->Data);
                        action("set_track", a);
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            if (t.kind == TrackKind::Audio) {
                // FX: the track's insert chain in the Track Effects panel; lit when it has devices.
                ImGui::SameLine(0, 3);
                auto fxTip = std::to_string(t.mix.effects.size()) + " insert effect(s): open Track Effects";
                if (toggle("FX", !t.mix.effects.empty(), style::audio, {26, 17}, fxTip.c_str())) {
                    mixScope = "track";
                    mixTarget = t.id;
                    openPanel("Track Effects");
                }
                ImGui::SameLine(0, 6);
                auto at = ImGui::GetCursorScreenPos();
                float width = left - at.x - 10;
                if (width > 16) {
                    auto peaks = s.media.mixPeaks.find(t.id);
                    float level = playing && peaks != s.media.mixPeaks.end() ? std::max(peaks->second[0], peaks->second[1]) : 0;
                    float db = level > 0 ? 20 * std::log10(level) : -100;
                    float position = std::clamp(std::exp((db - 6) / 26), 0.f, 1.f);
                    ImVec2 meterMin{at.x, at.y + 5}, meterMax{at.x + width, at.y + 12};
                    dl->AddRectFilled(meterMin, meterMax, bg0, 2);
                    if (position > 0)
                        dl->AddRectFilled(meterMin, {meterMin.x + width * position, meterMax.y},
                                          db > -3 ? record : db > -12 ? control : style::play, 2);
                }
                ImGui::NewLine();
            }
        }
        // Tall audio tracks get CyDAW's gain and pan knobs, as the Foundry track headers.
        if (t.kind == TrackKind::Audio && rows[i] >= 96 && renamingTrack != t.id) {
            auto knob = [&](const char *id, bool gain, float low, float high, float fallback, const char *label,
                            const char *format, int flags) {
                float &value = gain ? t.gain : t.pan;
                float old = value;
                cyui::KnobStyle look;
                look.diameter = 22;
                look.label = label;
                look.format = format;
                look.flags = flags;
                look.colour = style::audio;
                bool changed = cyui::knob(id, &value, low, high, fallback, look);
                headerWheelHovered = headerWheelHovered || ImGui::IsItemHovered();
                bool started = ImGui::IsItemActivated() || (changed && !ImGui::IsItemActive());
                if (started && !t.locked) {
                    auto before = p;
                    (gain ? before.findTrack(t.id)->gain : before.findTrack(t.id)->pan) = old;
                    s.history.checkpoint(before, "Edit");
                }
                if (changed && t.locked)
                    value = old;
                else if (changed) {
                    s.dirty = true;
                    s.media.resetAudio();
                }
            };
            ImGui::SetCursorScreenPos({origin.x + 12 + indent, top + 50});
            knob("##gain", true, -60, 12, 0, "Gain", "%.1f dB", 0);
            ImGui::SameLine(0, 10);
            knob("##pan", false, -1, 1, 0, "Pan", "%.2f", cyui::knobBipolar);
        }
        if (lanes.count(t.id) && lanes[t.id].visible) {
            float bandTop = top + rows[i];
            smallText(dl, {origin.x + 12, bandTop + 6}, textFaint, "AUTOMATION", .72f);
        }
        // Height handle on the bottom edge.
        ImGui::SetCursorScreenPos({origin.x, bottom - 5});
        ImGui::InvisibleButton("##height", {headerWidth, 5});
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            Json a = Json::Object();
            a["track"] = t.id;
            a["height"] = 0;
            action("set_track", a);
        } else if (ImGui::IsItemActivated()) {
            gesture = Gesture::TrackHeight;
            gestureId = t.id;
            gestureBefore = p;
        }
        if (gesture == Gesture::TrackHeight && gestureId == t.id) {
            float wanted = std::clamp((mouse.y - top - (h - rows[i])) / laneZoom, 22.f, 600.f);
            if (std::abs(wanted - trackHeight(t) / laneZoom) > .5f) {
                if (gestureBefore) {
                    s.history.checkpoint(*gestureBefore, "Edit");
                    gestureBefore.reset();
                }
                t.height = std::round(wanted);
                s.dirty = true;
            }
            if (!ImGui::IsMouseDown(0)) {
                gesture = Gesture::None;
                gestureBefore.reset();
            }
        }
        if (gesture == Gesture::TrackMove && mouse.y >= top && mouse.y < bottom)
            moveTarget = int(i) + (mouse.y > top + h * .5f ? 1 : 0);
        ImGui::PopID();
    }
    for (auto &row : groupRows) {
        if (row.top + row.height < lanesTop || row.top > lanesBottom)
            continue;
        auto *group = p.findGroup(row.group);
        ImU32 colour = group->color ? fromRgb(group->color) : accent;
        float top = row.top, bottom = row.top + row.height;
        ImGui::PushID(int(group->id) + 0x40000000);
        dl->AddRectFilled({origin.x, top}, {left, bottom}, mix(bg2, colour, .26f));
        dl->AddRectFilled({origin.x, top}, {origin.x + 4, bottom}, colour);
        dl->AddLine({origin.x, bottom - .5f}, {left, bottom - .5f}, line);
        // Fold triangle.
        ImGui::SetCursorScreenPos({origin.x + 6, top + 3});
        if (ImGui::InvisibleButton("##fold", {20, 20})) {
            Json a = Json::Object();
            a["group"] = group->id;
            a["collapsed"] = !group->collapsed;
            action("set_group", a);
        }
        group = p.findGroup(row.group); // the action may have replaced the project
        if (group)
            style::tooltip(group->collapsed ? "Expand Group" : "Collapse Group");
        {
            ImVec2 c{origin.x + 16, top + 13};
            ImU32 ink = ImGui::IsItemHovered() ? accent : text;
            if (group && group->collapsed)
                dl->AddTriangleFilled({c.x - 3, c.y - 5}, {c.x - 3, c.y + 5}, {c.x + 4, c.y}, ink);
            else if (group)
                dl->AddTriangleFilled({c.x - 5, c.y - 3}, {c.x + 5, c.y - 3}, {c.x, c.y + 4}, ink);
        }
        if (!group) {
            ImGui::PopID();
            continue;
        }
        bool roomy = headerWidth >= 190;
        float nameX = origin.x + (roomy ? 46 : 28), nameWidth = left - 52 - 32 - nameX;
        if (roomy)
            smallText(dl, {origin.x + 28, top + 5}, alpha(text, .8f), "\xee\xa2\xb7", 1);
        ImGui::SetCursorScreenPos({nameX, top});
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##groupName", {std::max(10.f, nameWidth), groupHeaderHeight},
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            renamingGroup = group->id;
            renameDraft = group->name;
            renameFocus = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
            contextGroup = group->id;
            ImGui::OpenPopup("groupContext");
        }
        if (renamingGroup == group->id) {
            ImGui::SetCursorScreenPos({nameX, top + 2});
            ImGui::SetNextItemWidth(std::max(40.f, nameWidth));
            if (renameFocus) {
                ImGui::SetKeyboardFocusHere();
                renameFocus = false;
            }
            bool done = ImGui::InputText("##groupRename", &renameDraft,
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (done && !renameDraft.empty() && renameDraft != group->name) {
                Json a = Json::Object();
                a["group"] = group->id;
                a["name"] = renameDraft;
                action("set_group", a);
            }
            if (done || (!ImGui::IsItemActive() && !ImGui::IsItemActivated() && !renameFocus &&
                         (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Escape))))
                renamingGroup = 0;
        } else {
            ImGui::PushFont(boldFont(), 0);
            clippedText(dl, {nameX, top + 4}, nameWidth, text, group->name.c_str());
            ImGui::PopFont();
        }
        // Member count badge, then M and S for the whole group (one undo step).
        if (group = p.findGroup(row.group); !group) {
            ImGui::PopID();
            continue;
        }
        char count[16];
        snprintf(count, sizeof(count), "%zu", row.last - row.first + 1);
        auto countSize = smallTextSize(count, .74f);
        ImVec2 badge{left - 60 - countSize.x, top + 6};
        dl->AddRectFilled(badge, badge + countSize + ImVec2(8, 2), colour, 3);
        smallText(dl, badge + ImVec2(4, 1), bg0, count, .74f);
        bool allMuted = true, allSolo = true;
        for (size_t k = row.first; k <= row.last; ++k) {
            allMuted = allMuted && p.tracks[k].muted;
            allSolo = allSolo && p.tracks[k].solo;
        }
        auto setAll = [&](bool solo, bool value) {
            s.history.checkpoint(p, "Edit");
            for (size_t k = row.first; k <= row.last; ++k)
                if (!p.tracks[k].locked)
                    (solo ? p.tracks[k].solo : p.tracks[k].muted) = value;
            s.dirty = true;
            s.media.resetAudio();
            s.renderer.clearHistory();
        };
        ImGui::SetCursorScreenPos({left - 52, top + 4});
        if (toggle("M", allMuted, mute, {22, 17}, "Group Mute / Hide"))
            setAll(false, !allMuted);
        ImGui::SameLine(0, 3);
        if (toggle("S", allSolo, solo, {22, 17}, "Group Solo"))
            setAll(true, !allSolo);
        ImGui::PopID();
    }
    if (gesture == Gesture::TrackMove) {
        if (moveTarget < 0)
            moveTarget = mouse.y < lanesTop ? 0 : int(p.tracks.size());
        float y = moveTarget < int(tops.size()) ? tops[moveTarget] : (tops.empty() ? lanesTop : tops.back() + heights.back());
        dl->AddLine({origin.x, y}, {right, y}, accent, 2);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (!ImGui::IsMouseDown(0)) {
            auto current = std::find_if(p.tracks.begin(), p.tracks.end(), [&](const Track &t) { return t.id == gestureId; });
            if (current != p.tracks.end()) {
                int from = int(current - p.tracks.begin());
                int to = moveTarget > from ? moveTarget - 1 : moveTarget;
                if (to != from) {
                    Json a = Json::Object();
                    a["track"] = gestureId;
                    a["index"] = to;
                    action("move_track", a);
                }
            }
            gesture = Gesture::None;
        }
    }
    // Quick add under the last track, as CyDAW's empty arrangement invites.
    {
        float y = (tops.empty() ? lanesTop : tops.back() + heights.back()) + 8;
        if (y < lanesBottom - 20) {
            ImGui::SetCursorScreenPos({origin.x + 10, y});
            ImGui::PushStyleColor(ImGuiCol_Button, bg2);
            if (ImGui::SmallButton(cyui::trLabel("+ Video").c_str())) {
                Json a = Json::Object();
                a["kind"] = "video";
                action("add_track", a);
            }
            ImGui::SameLine(0, 4);
            if (ImGui::SmallButton(cyui::trLabel("+ Audio").c_str())) {
                Json a = Json::Object();
                a["kind"] = "audio";
                action("add_track", a);
            }
            ImGui::PopStyleColor();
        }
        if (p.tracks.empty())
            smallText(dl, {left + 20, lanesTop + 20}, textFaint,
                      "Empty sequence: import media (Ctrl+I) or drop a file here.", .9f);
    }
    ImGui::PopClipRect();
    // Header corner and column splitter.
    dl->AddRectFilled(origin, {left, stripTop}, bg1);
    dl->AddLine({origin.x, stripTop - .5f}, {left, stripTop - .5f}, lineStrong);
    {
        char corner[64];
        snprintf(corner, sizeof(corner), "%zu TRACKS / %d FPS", p.tracks.size(), p.fps);
        smallText(dl, {origin.x + 12, rulerTop + 6}, textFaint, corner, .76f);
    }
    ImGui::SetCursorScreenPos({left - 3, origin.y});
    ImGui::InvisibleButton("##headerWidth", {6, lanesBottom - origin.y});
    bool splitter = ImGui::IsItemHovered() || ImGui::IsItemActive();
    if (splitter)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive()) {
        headerWidth = std::clamp(headerWidth + io.MouseDelta.x, 140.f, std::max(140.f, std::min(420.f, size.x - 120)));
        headerResized = true;
    }
    dl->AddLine({left - .5f, origin.y}, {left - .5f, lanesBottom}, splitter ? accent : lineStrong, splitter ? 2.f : 1.f);

    // ---- Scrollbars and the fit corner.
    {
        double total = std::max({p.duration * 1.05, visibleEnd, scroll + laneWidth / zoom});
        ImVec2 barMin{left, lanesBottom}, barMax{right, origin.y + size.y};
        ImGui::SetCursorScreenPos(barMin);
        ImGui::InvisibleButton("##hscroll", barMax - barMin);
        float thumbA = left + float(scroll / total) * laneWidth;
        float thumbW = std::max(24.f, float((laneWidth / zoom) / total) * laneWidth);
        bool barHovered = ImGui::IsItemHovered(), barActive = ImGui::IsItemActive();
        if (ImGui::IsItemActivated() && (mouse.x < thumbA || mouse.x > thumbA + thumbW))
            scroll = std::max(0., (mouse.x - left - thumbW * .5f) / laneWidth * total);
        if (barActive) {
            scroll = std::max(0., scroll + io.MouseDelta.x / laneWidth * total);
            if (playing)
                followPaused = true;
        }
        dl->AddRectFilled(barMin, barMax, bg1);
        dl->AddRectFilled({thumbA, barMin.y + 2}, {std::min(right, thumbA + thumbW), barMax.y - 2},
                          barHovered || barActive ? accentDim : mix(bg5, textDim, .35f), 4);
        float span = content + 40;
        ImVec2 vMin{right, lanesTop}, vMax{origin.x + size.x, lanesBottom};
        ImGui::SetCursorScreenPos(vMin);
        ImGui::InvisibleButton("##vscroll", vMax - vMin);
        float trackLength = vMax.y - vMin.y;
        float vThumb = std::max(32.f, std::min(1.f, viewHeight / span) * trackLength);
        float vA = vMin.y + (span > viewHeight ? verticalScroll / (span - viewHeight) * (trackLength - vThumb) : 0);
        bool vHovered = ImGui::IsItemHovered(), vActive = ImGui::IsItemActive();
        if (ImGui::IsItemActivated() && (mouse.y < vA || mouse.y > vA + vThumb) && span > viewHeight)
            verticalScroll = (mouse.y - vMin.y - vThumb * .5f) / (trackLength - vThumb) * (span - viewHeight);
        if (vActive && span > viewHeight)
            verticalScroll += io.MouseDelta.y / std::max(1.f, trackLength - vThumb) * (span - viewHeight);
        dl->AddRectFilled(vMin, vMax, bg1);
        if (span > viewHeight)
            dl->AddRectFilled({vMin.x + 2, vA}, {vMax.x - 2, vA + vThumb}, vHovered || vActive ? accentDim : mix(bg5, textDim, .35f), 4);
        ImVec2 cornerMin{right, lanesBottom}, cornerMax = origin + size;
        ImGui::SetCursorScreenPos(cornerMin);
        ImGui::InvisibleButton("##fitBoth", cornerMax - cornerMin);
        if (ImGui::IsItemClicked()) {
            fitTimeline(laneWidth);
            float natural = 0;
            for (auto &t : p.tracks)
                natural += trackHeight(t) / laneZoom;
            if (natural > 0)
                laneZoom = std::clamp((viewHeight - 30) / natural, .35f, 2.f);
            verticalScroll = 0;
        }
        tooltip("Fit All Tracks and Time");
        dl->AddRectFilled(cornerMin, cornerMax, ImGui::IsItemHovered() ? bg3 : bg2);
        sx::ui::DrawIcon(dl, sx::ui::IconZoom, (cornerMin + cornerMax) * .5f, 9, textDim);
        dl->AddRectFilled({origin.x, lanesBottom}, {left, origin.y + size.y}, bg1);
        char scale[48];
        snprintf(scale, sizeof(scale), "HEIGHT %.2f×  ·  ALT + WHEEL", laneZoom);
        smallText(dl, {origin.x + 10, lanesBottom}, textFaint, scale, .68f);
    }

    // ---- Popups.
    auto colourMenu = [&](std::uint32_t current, auto apply) {
        for (int i = 0; i < 12; ++i) {
            if (i % 6)
                ImGui::SameLine(0, 4);
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, fromRgb(trackColours[i]));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brighter(fromRgb(trackColours[i]), .2f));
            if (ImGui::Button(current == trackColours[i] ? "•" : "", {18, 18})) {
                apply(trackColours[i]);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }
        if (ImGui::MenuItem(cyui::trLabel("Default Color").c_str(), nullptr, current == 0))
            apply(0u);
    };
    if (openLowerThird) {
        ImGui::OpenPopup("lowerThird");
        openLowerThird = false;
    }
    if (ImGui::BeginPopup("lowerThird")) {
        ImGui::TextDisabled(cyui::tr("Lower Third at %s").c_str(), timecode(lowerStart, p.fps).c_str());
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(240);
        bool entered = ImGui::InputTextWithHint("Name", "First Last", &lowerName, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetNextItemWidth(240);
        entered |= ImGui::InputTextWithHint("Function", "facultative", &lowerRole, ImGuiInputTextFlags_EnterReturnsTrue);
        if ((ImGui::Button(cyui::trLabel("Add").c_str()) || entered) && !lowerName.empty()) {
            Json a = Json::Object();
            a["start"] = lowerStart;
            a["name"] = lowerName;
            a["role"] = lowerRole;
            action("add_lower_third", a);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cyui::trLabel("Undo").c_str()))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (openAttributes) {
        ImGui::OpenPopup("pasteAttributes");
        openAttributes = false;
    }
    if (ImGui::BeginPopup("pasteAttributes")) {
        if (!clipboard.empty())
            ImGui::TextDisabled(cyui::tr("From '%s' to %zu clip(s)").c_str(), clipboard.front().second.name.c_str(), s.selection.size());
        ImGui::Checkbox(cyui::trLabel("Effects").c_str(), &attributeChoice.effects);
        ImGui::SameLine();
        ImGui::BeginDisabled(!attributeChoice.effects);
        ImGui::Checkbox(cyui::trLabel("replace existing ones").c_str(), &attributeChoice.replaceEffects);
        ImGui::EndDisabled();
        ImGui::Checkbox(cyui::trLabel("Transforms (Position, Scale, Rotation)").c_str(), &attributeChoice.transform);
        ImGui::Checkbox(cyui::trLabel("Opacity").c_str(), &attributeChoice.opacity);
        ImGui::Checkbox(cyui::trLabel("Audio (Gain, Pan)").c_str(), &attributeChoice.audio);
        ImGui::Checkbox(cyui::trLabel("Mask").c_str(), &attributeChoice.mask);
        ImGui::TextDisabled(cyui::tr("Animation follows each clip's start.").c_str());
        if (ImGui::Button(cyui::trLabel("Paste").c_str())) {
            pasteAttributes();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cyui::trLabel("Undo").c_str()))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (openRename) {
        ImGui::OpenPopup("clipRename");
        openRename = false;
    }
    if (ImGui::BeginPopup("clipRename")) {
        ImGui::TextDisabled(cyui::tr("%zu clip%s, in timeline order").c_str(), s.selection.size(),
                            s.selection.size() > 1 ? "s" : "");
        ImGui::SetNextItemWidth(220);
        ImGui::InputText(cyui::trLabel("Template").c_str(), &renamePattern);
        ImGui::SetNextItemWidth(220);
        ImGui::InputInt(cyui::trLabel("First Number").c_str(), &renameFirst);
        ImGui::TextDisabled(cyui::tr("#: number (##: 01, 02...) / {nom}: current name").c_str());
        // Preview of the first names on a copy of the document.
        try {
            auto preview = p;
            auto names = preview.renameClips(s.selection, renamePattern, renameFirst);
            for (size_t i = 0; i < names.size() && i < 3; ++i)
                ImGui::BulletText("%s", names[i].c_str());
            if (names.size() > 3)
                ImGui::TextDisabled(cyui::tr("…").c_str());
        } catch (const std::exception &failure) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(warn), "%s", failure.what());
        }
        if (ImGui::Button(cyui::trLabel("Rename").c_str())) {
            auto a = selectionArgs();
            a["pattern"] = renamePattern;
            a["first"] = renameFirst;
            action("rename_clips", a);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cyui::trLabel("Undo").c_str()))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (openFind) {
        ImGui::OpenPopup("clipFind");
        openFind = false;
        findFocus = true;
    }
    if (ImGui::BeginPopup("clipFind")) {
        if (findFocus) {
            ImGui::SetKeyboardFocusHere();
            findFocus = false;
        }
        ImGui::SetNextItemWidth(420);
        bool enter = ImGui::InputTextWithHint("##find", "Clip, Track or File Name", &findText,
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        auto lower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            return text;
        };
        auto query = lower(findText);
        std::vector<std::pair<const Clip *, const Track *>> found;
        for (auto &t : p.tracks)
            for (auto &c : t.clips) {
                auto file = sx::Narrow(sx::Utf8Path(c.source).filename().wstring());
                if (query.empty() || lower(c.name).find(query) != std::string::npos ||
                    lower(t.name).find(query) != std::string::npos || lower(file).find(query) != std::string::npos)
                    found.push_back({&c, &t});
            }
        std::sort(found.begin(), found.end(), [](auto &a, auto &b) { return a.first->start < b.first->start; });
        Id chosen = enter && !found.empty() ? found.front().first->id : 0;
        ImGui::TextDisabled(cyui::tr("%zu clip%s").c_str(), found.size(), found.size() > 1 ? "s" : "");
        float rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::BeginChild("findResults", {420, std::clamp(rowHeight * float(found.size()) + 8, rowHeight + 8, 320.f)});
        for (auto &[c, t] : found) {
            ImGui::PushID(int(c->id));
            auto label = timecode(c->start, p.fps) + "   " + c->name;
            if (ImGui::Selectable(label.c_str(), c->id == s.selected))
                chosen = c->id;
            ImGui::SameLine(300);
            ImGui::TextDisabled(cyui::tr("%s").c_str(), t->name.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (chosen) {
            auto *c = p.findClip(chosen);
            s.selectClip(chosen);
            s.time = std::clamp(c->start, 0., p.duration);
            pendingReveal = c->start;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (openSpeed) {
        ImGui::OpenPopup("clipSpeed");
        openSpeed = false;
    }
    if (ImGui::BeginPopup("clipSpeed")) {
        // The source span stays: a faster clip is shorter, a slower one longer.
        if (auto *c = p.findClip(speedClip)) {
            double span = c->duration * c->speed;
            ImGui::TextDisabled(cyui::tr("%s").c_str(), c->name.c_str());
            ImGui::SetNextItemWidth(140);
            if (ImGui::DragFloat(cyui::trLabel("Speed").c_str(), &speedDraft, .5f, 5, 1600, "%.1f %%", ImGuiSliderFlags_AlwaysClamp))
                durationDraft = span / (speedDraft / 100);
            ImGui::SetNextItemWidth(140);
            double low = 1. / p.fps, high = span / .05;
            if (ImGui::DragScalar(cyui::trLabel("Duration").c_str(), ImGuiDataType_Double, &durationDraft, .01f, &low, &high, "%.3f s",
                                  ImGuiSliderFlags_AlwaysClamp))
                speedDraft = float(std::clamp(span / durationDraft * 100, 5., 1600.));
            ImGui::TextDisabled(cyui::tr("%s  ·  source %s").c_str(), timecode(durationDraft, p.fps).c_str(), timecode(span, p.fps).c_str());
            for (float preset : {25.f, 50.f, 100.f, 200.f, 400.f}) {
                char label[16];
                snprintf(label, sizeof(label), "%.0f %%", preset);
                bool pressed = ImGui::SmallButton(label);
                if (preset == 200)
                    speedButtons[0] = (ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) * .5f;
                if (pressed) {
                    speedDraft = preset;
                    durationDraft = span / (preset / 100);
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            bool apply = ImGui::Button(cyui::trLabel("Apply").c_str());
            speedButtons[1] = (ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) * .5f;
            if (apply) {
                auto a = clipArgs(c->id);
                a["speed"] = double(speedDraft) / 100;
                a["duration"] = std::max(1. / p.fps, std::round(durationDraft * p.fps) / p.fps);
                action("set_clip", a);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(cyui::trLabel("Undo").c_str()))
                ImGui::CloseCurrentPopup();
        } else
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("effectStack")) {
        if (auto *c = p.findClip(effectClip)) {
            bool locked = p.owner(c->id)->locked;
            ImGui::TextDisabled(cyui::tr("Effects  ·  %s").c_str(), c->name.c_str());
            ImGui::Separator();
            for (int i = 0; i < int(c->effects.size()); ++i) {
                ImGui::PushID(i);
                auto &effect = c->effects[i];
                auto *d = s.renderer.library.find(effect.id);
                std::string name = effect.id == "graph"     ? "Shaderixel Graph"
                                   : effect.id == "grading" ? "Color Grading"
                                   : d                      ? d->name
                                                            : effect.id;
                ImGui::BeginDisabled(locked);
                bool enabled = effect.enabled;
                if (ImGui::Checkbox("##on", &enabled)) {
                    auto a = clipArgs(c->id);
                    a["effect"] = i;
                    a["enabled"] = enabled;
                    action("set_effect", a);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Selectable(name.c_str(), selectedEffect == i, 0, {170, 0})) {
                    selectedEffect = i;
                    openPanel(effect.id == "grading"   ? "Color Grading"
                              : !effect.graph.IsNull() ? "Shader Graph"
                                                       : "Properties");
                }
                style::tooltip("Open Settings");
                ImGui::SameLine();
                ImGui::BeginDisabled(locked);
                auto move = [&](int position) {
                    auto a = clipArgs(c->id);
                    a["effect"] = i;
                    a["position"] = position;
                    action("set_effect", a);
                };
                ImGui::BeginDisabled(i == 0);
                if (ImGui::SmallButton(cyui::trLabel("\xee\x9c\x8e").c_str()))
                    move(i - 1);
                ImGui::EndDisabled();
                ImGui::SameLine(0, 2);
                ImGui::BeginDisabled(i + 1 >= int(c->effects.size()));
                if (ImGui::SmallButton(cyui::trLabel("\xee\x9c\x8d").c_str()))
                    move(i + 1);
                ImGui::EndDisabled();
                ImGui::SameLine(0, 2);
                bool remove = ImGui::SmallButton(cyui::trLabel("\xee\x9c\x91").c_str());
                style::tooltip("Remove Effect and Its Animation");
                ImGui::EndDisabled();
                ImGui::PopID();
                if (remove) {
                    auto a = clipArgs(c->id);
                    a["effect"] = i;
                    action("remove_effect", a);
                    break;
                }
            }
            if (!locked) {
                ImGui::Separator();
                if (ImGui::BeginMenu(cyui::trLabel("Add Effect").c_str())) {
                    effectMenu(c->id);
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndPopup();
    }
    // Measured here: inside the menu, the window is the menu.
    float clipMenuWidth = ImGui::GetWindowWidth() - headerWidth - 40;
    if (clipMenuRequest) {
        if (auto *c = p.findClip(s.selected))
            contextTime = c->start + c->duration / 2;
        ImGui::OpenPopup("clipContext");
        clipMenuRequest = false;
    }
    if (ImGui::BeginPopup("clipContext")) {
        // The Audio menu: on a sound, or on a picture whose linked group holds one.
        bool sounding = false;
        if (auto *c = p.findClip(s.selected))
            for (auto member : p.linked(c->id))
                sounding = sounding || (p.owner(member)->kind == TrackKind::Audio && !p.findClip(member)->source.empty());
        if (ImGui::MenuItem(cyui::trLabel("Split at Playhead").c_str(), "S")) {
            auto a = selectionArgs();
            a["time"] = s.time;
            action("split_clips", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Split Here").c_str()))
            blade(s.selected, contextTime);
        if (ImGui::MenuItem(cyui::trLabel("Copy").c_str(), "Ctrl+C"))
            copyClips(false);
        if (ImGui::MenuItem(cyui::trLabel("Cut").c_str(), "Ctrl+X"))
            copyClips(true);
        if (ImGui::MenuItem(cyui::trLabel("Paste at Playhead").c_str(), "Ctrl+V", false, !clipboard.empty()))
            pasteClips(s.time);
        if (ImGui::MenuItem(cyui::trLabel("Duplicate").c_str(), "Ctrl+D"))
            action("duplicate_clips", selectionArgs());
        if (auto *c = p.findClip(s.selected); c && ImGui::MenuItem(c->enabled ? "Disable" : "Enable", "Shift+E"))
            toggleClipsEnabled();
        ImGui::Separator();
        if (ImGui::BeginMenu(cyui::trLabel("Edit").c_str())) {
            if (ImGui::MenuItem(cyui::trLabel("Trim Start to Playhead").c_str(), "Q"))
                trimToPlayhead(true, true);
            if (ImGui::MenuItem(cyui::trLabel("Trim End to Playhead").c_str(), "W"))
                trimToPlayhead(false, true);
            if (ImGui::MenuItem(cyui::trLabel("Split at Markers").c_str(), nullptr, false, !p.markers.empty()))
                action("split_at_markers", selectionArgs());
            if (ImGui::MenuItem(cyui::trLabel("Align to Markers").c_str(), nullptr, false, !p.markers.empty()))
                action("fit_to_markers", selectionArgs());
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Arrange selected clips from one marker to the next (use audio "
                                  "transient markers to edit to the beat)");
            if (ImGui::MenuItem(cyui::trLabel("Crossfade at Nearest Cut").c_str(), "Shift+D"))
                crossfadeAtPlayhead();
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Video &&
                                                      ImGui::BeginMenu(cyui::trLabel("Freeze Frame at Playhead").c_str())) {
                for (auto [label, length] : {std::pair{"1 second", 1.}, {"2 Seconds (Shift+F)", 2.}, {"5 seconds", 5.}})
                    if (ImGui::MenuItem(label))
                        freezeAtPlayhead(length);
                ImGui::EndMenu();
            }
            if (auto *c = p.findClip(s.selected); c && ImGui::MenuItem(cyui::trLabel("Speed / Duration...").c_str(), "Ctrl+R")) {
                speedClip = c->id;
                speedDraft = float(c->speed * 100);
                durationDraft = c->duration;
                openSpeed = true;
            }
            if (auto *c = p.findClip(s.selected); c && !c->source.empty() && !c->hold &&
                                                      ImGui::MenuItem(cyui::trLabel("Play Backward").c_str(), nullptr, c->reverse)) {
                auto a = clipArgs(c->id);
                a["reverse"] = !c->reverse;
                action("set_clip", a);
            }
            // Two clips selected (outside one linked group): the one right-clicked moves onto the other's sound.
            Id syncReference = 0;
            if (p.findClip(s.selected)) {
                auto group = p.linked(s.selected);
                for (auto id : s.selection)
                    if (id != s.selected && std::find(group.begin(), group.end(), id) == group.end())
                        syncReference = id;
            }
            if (syncReference && ImGui::MenuItem(cyui::trLabel("Synchronize to Other Clip by Audio").c_str())) {
                auto a = clipArgs(s.selected);
                a["reference"] = syncReference;
                if (action("sync_audio", a)) {
                    char text[120];
                    snprintf(text, sizeof(text), "Clip moved %+.2f s - match %.0f %%", actionResult["offset"].Num(),
                             actionResult["confidence"].Num() * 100);
                    say(text);
                }
            }
            ImGui::EndMenu();
        }
        if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Video &&
                                                  ImGui::BeginMenu(cyui::trLabel("Image").c_str())) {
            // Two video clips selected: the right-clicked one takes the other's colours.
            Id colourReference = 0;
            for (auto id : s.selection)
                if (auto *other = p.findClip(id); other && id != c->id && p.owner(id)->kind == TrackKind::Video &&
                                                  !other->title.enabled && !other->adjustment)
                    colourReference = id;
            if (colourReference && !c->title.enabled && !c->adjustment &&
                ImGui::MenuItem(cyui::trLabel("Match Color to Other Clip").c_str())) {
                auto a = clipArgs(c->id);
                a["reference"] = colourReference;
                a["time"] = s.time >= c->start && s.time < c->start + c->duration ? s.time : c->start + c->duration / 2;
                if (action("match_grade", a)) {
                    char text[120];
                    snprintf(text, sizeof(text), "Matched colors: exposure %+.2f EV%s", actionResult["exposure"].Num(),
                             actionResult["clamped"].B() ? "  - difference too large; partial match" : "");
                    say(text);
                }
            }
            // Three clips or more: all the others take the colours of the right-clicked one.
            int others = 0;
            for (auto id : s.selection)
                if (auto *other = p.findClip(id); other && id != c->id && p.owner(id)->kind == TrackKind::Video &&
                                                  !other->title.enabled && !other->adjustment)
                    ++others;
            if (others >= 2 && !c->title.enabled && !c->adjustment &&
                ImGui::MenuItem(cyui::trLabel("Match Entire Selection to This Clip").c_str())) {
                auto a = selectionArgs();
                a["reference"] = c->id;
                if (action("match_grade", a))
                    say(std::to_string(actionResult["clips"].I()) + " clip(s) matched to ' " + c->name + " »" +
                        (actionResult["clamped"].B() ? "  - difference too large for some clips" : ""));
            }
            if (!c->title.enabled && !c->adjustment && ImGui::MenuItem(cyui::trLabel("Auto Color (White Balance, Exposure)").c_str())) {
                if (action("auto_grade", clipArgs(c->id))) {
                    char text[120];
                    snprintf(text, sizeof(text), "Auto color: exposure %+.2f EV, white balance adjusted",
                             actionResult["exposure"].Num());
                    say(text);
                }
            }
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Video && !c->source.empty() &&
                                                      ImGui::BeginMenu(cyui::trLabel("Framing").c_str())) {
                if (ImGui::MenuItem(cyui::trLabel("Fill Frame (Crop)").c_str())) {
                    auto a = selectionArgs();
                    a["mode"] = "fill";
                    action("fit_clips", a);
                }
                if (ImGui::MenuItem(cyui::trLabel("Fit Frame (Letterbox)").c_str())) {
                    auto a = selectionArgs();
                    a["mode"] = "fit";
                    action("fit_clips", a);
                }
                ImGui::EndMenu();
            }
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Video &&
                                                      ImGui::BeginMenu(cyui::trLabel("Keying").c_str())) {
                for (auto [label, position] :
                     {std::pair{"Top Left", "top_left"}, {"Top Right", "top_right"},
                      {"Bottom Left", "bottom_left"}, {"Bottom Right", "bottom_right"}, {"Center", "center"},
                      {"Full frame", "full"}})
                    if (ImGui::MenuItem(label)) {
                        auto a = selectionArgs();
                        a["position"] = position;
                        action("place_clip", a);
                    }
                ImGui::Separator();
                ImGui::BeginDisabled(s.selection.size() < 2 || s.selection.size() > 4);
                for (auto [label, layout] : {std::pair{"Split Screen: Side by Side", "side_by_side"},
                                             {"Split Screen: Stacked", "stacked"},
                                             {"Split Screen: Four Quadrants", "grid"}})
                    if (ImGui::MenuItem(label)) {
                        auto a = selectionArgs();
                        a["layout"] = layout;
                        action("split_screen", a);
                    }
                ImGui::EndDisabled();
                ImGui::Separator();
                for (auto [label, colour] : {std::pair{"Key Green Screen", "green"}, {"Key Blue Screen", "blue"}})
                    if (ImGui::MenuItem(label)) {
                        auto a = selectionArgs();
                        a["color"] = colour;
                        action("key_background", a);
                    }
                ImGui::EndMenu();
            }
            if (auto *c = p.findClip(s.selected); c && c->title.enabled && ImGui::BeginMenu(cyui::trLabel("Title Style").c_str())) {
                for (auto [label, style] : {std::pair{"Classic (white, outline, shadow)", "classic"},
                                            {"Yellow subtitle", "subtitle_yellow"}, {"Black Box", "boxed"},
                                            {"Cinema (Serif, Soft Shadow)", "cinema"}, {"Neon (Cyan Glow)", "neon"}})
                    if (ImGui::MenuItem(label)) {
                        auto a = selectionArgs();
                        a["style"] = style;
                        action("apply_title_style", a);
                    }
                ImGui::EndMenu();
            }
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Video && !c->source.empty() &&
                                                      !c->title.enabled && ImGui::BeginMenu(cyui::trLabel("Detect Scenes").c_str())) {
                for (auto [label, split] : {std::pair{"Split at Each Scene Change", true}, {"Marker at Each Scene", false}})
                    if (ImGui::MenuItem(label, nullptr, false, !analysis))
                        detectScenesInBackground(c->id, split);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (sounding && ImGui::BeginMenu(cyui::trLabel("Audio").c_str())) {
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Audio &&
                                                      ImGui::MenuItem(cyui::trLabel("Normalize (-1 dBFS)").c_str()))
                action("normalize_audio", selectionArgs());
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Audio &&
                                                      ImGui::MenuItem(cyui::trLabel("Enhance Voice (Gate, EQ, Compressor)").c_str())) {
                if (action("enhance_voice", selectionArgs()))
                    say("Voice enhanced on " + std::to_string(actionResult["clips"].I()) + " clip(s): effects added");
            }
            if (sounding && ImGui::BeginMenu(cyui::trLabel("Remove Silence").c_str())) {
                for (auto [label, threshold, length] :
                     {std::tuple{"Prudent  ·  -45 dB, 0,8 s", -45., .8}, {"Normal  ·  -40 dB, 0,5 s", -40., .5},
                      {"Tight / -35 dB, 0.3 s", -35., .3}})
                    if (ImGui::MenuItem(label, nullptr, false, !analysis))
                        removeSilencesInBackground(s.selected, threshold, length);
                ImGui::TextDisabled(cyui::tr("Close the gaps across all tracks").c_str());
                ImGui::EndMenu();
            }
            if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Audio &&
                                                      ImGui::BeginMenu(cyui::trLabel("Transient Markers").c_str())) {
                for (auto [label, sensitivity] :
                     {std::pair{"Few: strong transients", .2}, {"Normal", .5}, {"Many: every note", .85}})
                    if (ImGui::MenuItem(label)) {
                        auto a = clipArgs(c->id);
                        a["sensitivity"] = sensitivity;
                        action("detect_beats", a);
                    }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (auto *c = p.findClip(s.selected); c && ImGui::BeginMenu(cyui::trLabel("Animation Preset").c_str())) {
            bool video = p.owner(c->id)->kind == TrackKind::Video;
            auto preset = [&](const char *label, const char *id, bool enabled = true) {
                if (ImGui::MenuItem(label, nullptr, false, enabled)) {
                    auto a = selectionArgs();
                    a["preset"] = id;
                    if (action("apply_animation", a)) {
                        animationEffect = -1;
                        animationKey = std::string(id).starts_with("ramp_") ? "speed_ramp"
                                       : std::string(id).starts_with("fade") ? "opacity"
                                                                               : "scale";
                    }
                }
            };
            preset("Fade In", "fade_in", video);
            preset("Fade Out", "fade_out", video);
            preset("Fade In and Out", "fade_in_out", video);
            preset("Slide Up", "slide_up", video);
            preset("Slow zoom in", "zoom_in", video);
            preset("Ken Burns (Zoom and Pan)", "ken_burns", video);
            preset("Typewriter (Titles)", "typewriter", c->title.enabled);
            ImGui::Separator();
            preset("Speed: Accelerate (100 -> 300%)", "ramp_up");
            preset("Speed: slow down (100 → 25 %)", "ramp_down");
            preset("Speed: Slow in Middle", "ramp_middle");
            ImGui::TextDisabled(cyui::tr("Edit Keyframes in Animation").c_str());
            ImGui::EndMenu();
        }
        if (auto *c = p.findClip(s.selected); c && ImGui::BeginMenu(cyui::trLabel("Effects").c_str())) {
            if (!p.owner(c->id)->locked) {
                if (ImGui::BeginMenu(cyui::trLabel("Add Effect").c_str())) {
                    effectMenu(c->id);
                    ImGui::EndMenu();
                }
                if (!c->effects.empty() && ImGui::BeginMenu(cyui::trLabel("Clip Effects").c_str())) {
                    for (int i = 0; i < int(c->effects.size()); ++i) {
                        ImGui::PushID(i);
                        auto &effect = c->effects[i];
                        auto *d = s.renderer.library.find(effect.id);
                        if (ImGui::MenuItem(d ? d->name.c_str() : effect.id.c_str(), nullptr, effect.enabled)) {
                            auto a = clipArgs(c->id);
                            a["effect"] = i;
                            a["enabled"] = !effect.enabled;
                            action("set_effect", a);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndMenu();
                }
            }
            if (ImGui::MenuItem(cyui::trLabel("Paste Attributes...").c_str(), "Ctrl+Alt+V", false, !clipboard.empty()))
                openAttributes = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(cyui::trLabel("Clip").c_str())) {
            if (ImGui::MenuItem(cyui::trLabel("Batch Rename Selection...").c_str()))
                openRename = true;
            if (auto *c = p.findClip(s.selected); c && ImGui::MenuItem(cyui::trLabel("Select Same Color").c_str())) {
                std::vector<Id> same;
                for (auto &t : p.tracks)
                    for (auto &other : t.clips)
                        if (other.color == c->color && !t.locked)
                            same.push_back(other.id);
                s.setSelection(same);
            }
            if (auto *c = p.findClip(s.selected)) {
                if (!c->source.empty() && ImGui::MenuItem(cyui::trLabel("Match Frame in Source").c_str(), "F"))
                    matchFrame();
                if (p.owner(c->id)->kind == TrackKind::Video && ImGui::MenuItem(cyui::trLabel("Edit Content").c_str()))
                    openClipEditor(c->id);
                if (c->link && ImGui::MenuItem(cyui::trLabel("Unlink Group").c_str()))
                    action("unlink_clip", clipArgs(s.selected));
                if (ImGui::BeginMenu(cyui::trLabel("Link with...").c_str())) {
                    Id other = 0;
                    for (auto &track : p.tracks)
                        for (auto &candidate : track.clips) {
                            if (candidate.id == s.selected || (c->link && candidate.link == c->link))
                                continue;
                            ImGui::PushID(int(candidate.id));
                            if (ImGui::MenuItem((track.name + " / " + candidate.name).c_str(), nullptr, false, !track.locked))
                                other = candidate.id;
                            ImGui::PopID();
                        }
                    if (other) {
                        auto a = clipArgs(s.selected);
                        a["other"] = other;
                        action("link_clips", a);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(cyui::trLabel("Color").c_str())) {
                    colourMenu(c->color, [&](std::uint32_t colour) {
                        s.history.checkpoint(p, "Edit");
                        for (auto id : s.selection)
                            if (auto *chosen = p.findClip(id); chosen && !p.owner(id)->locked)
                                chosen->color = colour;
                        s.dirty = true;
                    });
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(s.selection.size() > 1 ? "Set Work Area to Selection" : "Set Work Area to Clip")) {
                    double begin = c->start, end = c->start + c->duration;
                    for (auto id : s.selection)
                        if (auto *chosen = p.findClip(id)) {
                            begin = std::min(begin, chosen->start);
                            end = std::max(end, chosen->start + chosen->duration);
                        }
                    Json a = Json::Object();
                    a["begin"] = begin;
                    a["end"] = end;
                    action("set_work_area", a);
                }
                if (ImGui::MenuItem(cyui::trLabel("Zoom to Selection").c_str(), "Z"))
                    fitSelection(clipMenuWidth);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(cyui::trLabel("Delete").c_str(), "Delete"))
            action("delete_clips", selectionArgs());
        if (ImGui::MenuItem(cyui::trLabel("Delete and Close Gap").c_str(), "Shift+Delete")) {
            auto args = selectionArgs();
            args["ripple"] = true;
            action("delete_clips", args);
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("trackContext")) {
        if (auto *t = p.findTrack(contextTrack)) {
            ImGui::TextDisabled(cyui::tr("%s").c_str(), t->name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("Rename").c_str())) {
                renamingTrack = t->id;
                renameDraft = t->name;
                renameFocus = true;
            }
            if (ImGui::BeginMenu(cyui::trLabel("Color").c_str())) {
                colourMenu(t->color, [&](std::uint32_t colour) {
                    Json a = Json::Object();
                    a["track"] = contextTrack;
                    a["color"] = colour;
                    action("set_track", a);
                });
                ImGui::EndMenu();
            }
            if (t->kind == TrackKind::Video) {
                if (ImGui::MenuItem(cyui::trLabel("Track Effects Graph...").c_str()))
                    openTrackGraph(t->id);
                // How the track lies over the tracks below: Shaderixel's modes, library blends, opacity.
                std::string current = t->blendEffect.id.empty()
                                          ? sx::BlendModeName(sx::BlendMode(t->blend))
                                          : (s.renderer.library.find(t->blendEffect.id)
                                                 ? s.renderer.library.find(t->blendEffect.id)->name
                                                 : t->blendEffect.id);
                if (ImGui::BeginMenu(("Blend: " + current).c_str())) {
                    for (int k = 0; k < sx::kBlendMenuCount; ++k) {
                        int mode = sx::kBlendMenuOrder[k];
                        if (mode < 0) {
                            ImGui::Separator();
                            continue;
                        }
                        if (ImGui::MenuItem(sx::BlendModeName(sx::BlendMode(mode)), nullptr,
                                            t->blendEffect.id.empty() && t->blend == mode)) {
                            Json a = Json::Object();
                            a["track"] = contextTrack;
                            a["blend"] = mode;
                            action("set_track", a);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::BeginMenu(cyui::trLabel("Blend Effects").c_str())) {
                        for (auto &d : s.renderer.library.definitions)
                            if (d.kind == "effect" && d.code.find("//@blend") != std::string::npos &&
                                ImGui::MenuItem(d.name.c_str(), nullptr, t->blendEffect.id == d.id)) {
                                Json a = Json::Object();
                                a["track"] = contextTrack;
                                a["blend_effect"] = d.id;
                                action("set_track", a);
                            }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                if (trackOpacityFor != t->id) {
                    trackOpacityFor = t->id;
                    trackOpacityDraft = t->opacity * 100;
                }
                ImGui::SetNextItemWidth(160);
                ImGui::SliderFloat(cyui::trLabel("Opacity").c_str(), &trackOpacityDraft, 0, 100, "%.0f %%");
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    Json a = Json::Object();
                    a["track"] = contextTrack;
                    a["opacity"] = trackOpacityDraft / 100;
                    action("set_track", a);
                }
            } else if (ImGui::MenuItem(cyui::trLabel("Track Effects...").c_str())) {
                mixScope = "track";
                mixTarget = t->id;
                openPanel("Track Effects");
            }
            int index = int(t - p.tracks.data());
            if (ImGui::MenuItem(cyui::trLabel("Move Up").c_str(), nullptr, false, index > 0)) {
                Json a = Json::Object();
                a["track"] = contextTrack;
                a["index"] = index - 1;
                action("move_track", a);
            }
            if (ImGui::MenuItem(cyui::trLabel("Move Down").c_str(), nullptr, false, index + 1 < int(p.tracks.size()))) {
                Json a = Json::Object();
                a["track"] = contextTrack;
                a["index"] = index + 1;
                action("move_track", a);
            }
            if (ImGui::BeginMenu(cyui::trLabel("Height").c_str())) {
                const std::pair<const char *, float> sizes[] = {
                    {"Compact", 34.f}, {"Normal", 0.f}, {"Large", 120.f}, {"Extra Large", 200.f}};
                for (auto [label, value] : sizes)
                    if (ImGui::MenuItem(label)) {
                        Json a = Json::Object();
                        a["track"] = contextTrack;
                        a["height"] = value;
                        action("set_track", a);
                    }
                ImGui::EndMenu();
            }
            auto &lane = lanes[t->id];
            if (ImGui::MenuItem(cyui::trLabel("Lane d'automation").c_str(), nullptr, lane.visible))
                lane.visible = !lane.visible;
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("New Group with This Track").c_str(), "Ctrl+G")) {
                Json a = Json::Object();
                a["tracks"] = Json::Array();
                a["tracks"].Push(t->id);
                action("group_tracks", a);
            }
            if (!p.groups.empty() && ImGui::BeginMenu(cyui::trLabel("Add to Group").c_str())) {
                for (auto &group : p.groups) {
                    ImGui::PushID(int(group.id));
                    if (ImGui::MenuItem(group.name.c_str(), nullptr, t->group == group.id)) {
                        Json a = Json::Object();
                        a["track"] = contextTrack;
                        a["group"] = group.id;
                        action("set_track", a);
                    }
                    ImGui::PopID();
                }
                ImGui::EndMenu();
            }
            if (t && t->group && ImGui::MenuItem(cyui::trLabel("Remove from Group").c_str())) {
                Json a = Json::Object();
                a["track"] = contextTrack;
                a["group"] = 0;
                action("set_track", a);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("New Video Track").c_str())) {
                Json a = Json::Object();
                a["kind"] = "video";
                action("add_track", a);
            }
            if (ImGui::MenuItem(cyui::trLabel("New Audio Track").c_str())) {
                Json a = Json::Object();
                a["kind"] = "audio";
                action("add_track", a);
            }
            ImGui::Separator();
            if (t->kind == TrackKind::Audio && ImGui::MenuItem(cyui::trLabel("Anti-click Fades at Cuts").c_str())) {
                Json a = Json::Object();
                a["track"] = t->id;
                if (action("audio_cut_fades", a))
                    say(std::to_string(actionResult["cuts"].I()) + " cut(s) smoothed");
            }
            if (t->kind == TrackKind::Audio && ImGui::BeginMenu(cyui::trLabel("Duck under Voice").c_str())) {
                for (auto &other : p.tracks)
                    if (other.kind == TrackKind::Audio && other.id != t->id) {
                        auto label = "Voice: " + other.name + "##duck" + std::to_string(other.id);
                        if (ImGui::MenuItem(label.c_str())) {
                            Json a = Json::Object();
                            a["track"] = t->id;
                            a["voice"] = other.id;
                            if (action("auto_duck", a))
                                say(std::to_string(actionResult["spans"].I()) + " voice range(s): music at -12 dB");
                        }
                    }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(cyui::trLabel("Duplicate Track").c_str())) {
                Json a = Json::Object();
                a["track"] = contextTrack;
                action("duplicate_track", a);
            }
            if (ImGui::MenuItem(cyui::trLabel("Delete Track").c_str(), nullptr, false, !t->locked)) {
                Json a = Json::Object();
                a["track"] = contextTrack;
                action("remove_track", a);
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("groupContext")) {
        if (auto *group = p.findGroup(contextGroup)) {
            ImGui::TextDisabled(cyui::tr("%s").c_str(), group->name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("Rename").c_str())) {
                renamingGroup = group->id;
                renameDraft = group->name;
                renameFocus = true;
            }
            if (ImGui::BeginMenu(cyui::trLabel("Color").c_str())) {
                colourMenu(group->color, [&](std::uint32_t colour) {
                    Json a = Json::Object();
                    a["group"] = contextGroup;
                    a["color"] = colour;
                    action("set_group", a);
                });
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(group->collapsed ? "Expand" : "Collapse")) {
                Json a = Json::Object();
                a["group"] = contextGroup;
                a["collapsed"] = !group->collapsed;
                action("set_group", a);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("Ungroup").c_str())) {
                Json a = Json::Object();
                a["group"] = contextGroup;
                action("ungroup", a);
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("laneContext")) {
        if (auto *t = p.findTrack(contextTrack)) {
            ImGui::TextDisabled(cyui::tr("%s  ·  %s").c_str(), t->name.c_str(), timecode(contextTime, p.fps).c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("Move Playhead Here").c_str()))
                s.time = std::clamp(contextTime, 0., p.duration);
            // The empty span around the click on this track, closed on every track when they are all empty there.
            double gapBegin = 0, gapEnd = -1;
            if (p.gapAt(t->id, contextTime, gapBegin, gapEnd)) {
                bool clear = p.spanEmpty(gapBegin, gapEnd);
                char label[64];
                snprintf(label, sizeof(label), "Close Gap (%s)", timecode(gapEnd - gapBegin, p.fps).c_str());
                if (ImGui::MenuItem(label, nullptr, false, clear)) {
                    Json a = Json::Object();
                    a["begin"] = gapBegin;
                    a["end"] = gapEnd;
                    action("remove_range", a);
                }
                if (!clear && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip(cyui::tr("Other tracks contain clips in this gap").c_str());
            }
            if (ImGui::MenuItem(cyui::trLabel("Close All Track Gaps").c_str(), nullptr, false, !t->clips.empty() && !t->locked)) {
                Json a = Json::Object();
                a["track"] = t->id;
                action("close_gaps", a);
            }
            if (ImGui::MenuItem(cyui::trLabel("Paste Here").c_str(), "Ctrl+V", false, !clipboard.empty()))
                pasteClips(contextTime);
            insertGapMenu(contextTime);
            if (ImGui::MenuItem(cyui::trLabel("Add Marker Here").c_str())) {
                Json a = Json::Object();
                a["time"] = std::clamp(contextTime, 0., p.duration);
                action("add_marker", a);
            }
            if (t->kind == TrackKind::Video) {
                if (ImGui::MenuItem(cyui::trLabel("Adjustment Layer Here").c_str())) {
                    Json a = Json::Object();
                    a["start"] = std::max(0., contextTime);
                    action("add_adjustment_layer", a);
                }
                if (ImGui::MenuItem(cyui::trLabel("HLSL Generator").c_str()))
                    action("add_generator");
                if (ImGui::MenuItem(cyui::trLabel("Title / Text").c_str()))
                    action("add_title");
                if (ImGui::MenuItem(cyui::trLabel("Lower Third Here...").c_str())) {
                    lowerStart = std::max(0., contextTime);
                    openLowerThird = true;
                }
            }
            auto &lane = lanes[t->id];
            if (ImGui::MenuItem(cyui::trLabel("Lane d'automation").c_str(), nullptr, lane.visible))
                lane.visible = !lane.visible;
            if (ImGui::MenuItem(cyui::trLabel("Select All").c_str(), "Ctrl+A"))
                selectAll();
            if (ImGui::MenuItem(cyui::trLabel("Select Forward on All Tracks").c_str(), "A"))
                selectFrom(contextTime, true);
            if (ImGui::MenuItem(cyui::trLabel("Select Forward on This Track").c_str()))
                selectFrom(contextTime, true, t->id);
            if (ImGui::MenuItem(cyui::trLabel("Select Backward").c_str(), "Alt+A"))
                selectFrom(contextTime, false);
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("laneChooser")) {
        if (auto *t = p.findTrack(contextTrack)) {
            auto &lane = lanes[t->id];
            std::set<std::string> clipKeys;
            std::map<std::string, std::set<std::string>> effectKeys;
            std::set<std::string> animated;
            if (t->kind == TrackKind::Video)
                clipKeys = {"opacity", "x", "y", "scale", "rotation"};
            else
                clipKeys = {"gain", "pan"};
            for (auto &clip : t->clips) {
                for (auto &[key, param] : clip.params) {
                    clipKeys.insert(key);
                    if (!param.keys.empty())
                        animated.insert("/" + key);
                }
                for (auto &effect : clip.effects)
                    for (auto &[key, param] : effect.params) {
                        effectKeys[effect.id].insert(key);
                        if (!param.keys.empty())
                            animated.insert(effect.id + "/" + key);
                    }
            }
            ImGui::TextDisabled(cyui::tr("Lane Parameter").c_str());
            for (auto &key : clipKeys) {
                auto label = laneLabel(key, "") + (animated.count("/" + key) ? "  ●" : "");
                if (ImGui::MenuItem(label.c_str(), nullptr, lane.effect.empty() && lane.key == key)) {
                    lane.key = key;
                    lane.effect.clear();
                }
            }
            if (!effectKeys.empty())
                ImGui::SeparatorText(cyui::tr("Effects").c_str());
            for (auto &[effect, keys] : effectKeys)
                if (ImGui::BeginMenu(effect.c_str())) {
                    for (auto &key : keys) {
                        auto label = key + (animated.count(effect + "/" + key) ? "  ●" : "");
                        if (ImGui::MenuItem(label.c_str(), nullptr, lane.effect == effect && lane.key == key)) {
                            lane.key = key;
                            lane.effect = effect;
                        }
                    }
                    ImGui::EndMenu();
                }
            ImGui::Separator();
            if (ImGui::MenuItem(cyui::trLabel("Hide Lane").c_str()))
                lane.visible = false;
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("rulerContext")) {
        ImGui::TextDisabled(cyui::tr("%s").c_str(), timecode(contextTime, p.fps).c_str());
        ImGui::Separator();
        if (auto *marker = p.findMarker(contextMarker)) {
            if (ImGui::MenuItem(cyui::trLabel("Rename Marker").c_str())) {
                renamingMarker = marker->id;
                renameDraft = marker->name;
                renameFocus = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::BeginMenu(cyui::trLabel("Marker Color").c_str())) {
                colourMenu(marker->color, [&](std::uint32_t colour) {
                    Json a = Json::Object();
                    a["marker"] = contextMarker;
                    a["color"] = colour;
                    action("set_marker", a);
                });
                ImGui::EndMenu();
            }
            // Regions: a marker with a length, set from the playhead or the work area.
            if (ImGui::MenuItem(cyui::trLabel("Region to Playhead").c_str(), nullptr, false, s.time > marker->time + 1e-6)) {
                Json a = Json::Object();
                a["marker"] = contextMarker;
                a["duration"] = s.time - marker->time;
                action("set_marker", a);
            }
            if (ImGui::MenuItem(cyui::trLabel("Region over Work Area").c_str(), nullptr, false, p.hasWorkArea())) {
                Json a = Json::Object();
                a["marker"] = contextMarker;
                a["time"] = p.workIn;
                a["duration"] = p.workOut - p.workIn;
                action("set_marker", a);
            }
            if (marker->duration > 0) {
                if (ImGui::MenuItem(cyui::trLabel("Set Work Area to Region").c_str())) {
                    Json a = Json::Object();
                    a["begin"] = marker->time;
                    a["end"] = std::min(marker->end(), p.duration);
                    action("set_work_area", a);
                }
                if (ImGui::MenuItem(cyui::trLabel("Point Marker (No Duration)").c_str())) {
                    Json a = Json::Object();
                    a["marker"] = contextMarker;
                    a["duration"] = 0;
                    action("set_marker", a);
                }
            }
            if (ImGui::MenuItem(cyui::trLabel("Delete Marker").c_str())) {
                Json a = Json::Object();
                a["marker"] = contextMarker;
                action("remove_marker", a);
            }
            ImGui::Separator();
        }
        if (ImGui::MenuItem(cyui::trLabel("Move Playhead Here").c_str()))
            s.time = contextTime;
        if (ImGui::MenuItem(cyui::trLabel("Add Marker Here").c_str(), "M")) {
            Json a = Json::Object();
            a["time"] = contextTime;
            action("add_marker", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Add Region over Work Area").c_str(), nullptr, false, p.hasWorkArea())) {
            Json a = Json::Object();
            a["time"] = p.workIn;
            a["duration"] = p.workOut - p.workIn;
            a["name"] = "Region";
            action("add_marker", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Work Area In Here").c_str(), "I")) {
            Json a = Json::Object();
            a["begin"] = contextTime;
            a["end"] = p.hasWorkArea() && p.workOut > contextTime ? p.workOut : p.duration;
            action("set_work_area", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Work Area Out Here").c_str(), "O")) {
            Json a = Json::Object();
            a["begin"] = p.hasWorkArea() && p.workIn < contextTime ? p.workIn : 0;
            a["end"] = contextTime;
            action("set_work_area", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Lift Work Area (Leave Gap)").c_str(), nullptr, false, p.hasWorkArea())) {
            Json a = Json::Object();
            a["begin"] = p.workIn;
            a["end"] = p.workOut;
            action("lift_range", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Extract Work Area (Close Gap)").c_str(), nullptr, false, p.hasWorkArea())) {
            Json a = Json::Object();
            a["begin"] = p.workIn;
            a["end"] = p.workOut;
            action("remove_range", a);
        }
        if (ImGui::MenuItem(cyui::trLabel("Clear Work Area").c_str(), nullptr, false, p.hasWorkArea())) {
            Json a = Json::Object();
            a["begin"] = 0;
            a["end"] = 0;
            action("set_work_area", a);
        }
        if (!p.markers.empty() && ImGui::BeginMenu(cyui::trLabel("Go to Marker").c_str())) {
            for (auto &marker : p.markers) {
                ImGui::PushID(int(marker.id));
                auto label = timecode(marker.time, p.fps) + "   " + marker.name;
                if (ImGui::MenuItem(label.c_str()))
                    s.time = std::clamp(marker.time, 0., p.duration);
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        insertGapMenu(s.time);
        if (ImGui::MenuItem(cyui::trLabel("Split All Tracks at Markers").c_str(), nullptr, false, !p.markers.empty()))
            action("split_at_markers");
        if (ImGui::MenuItem(cyui::trLabel("Marker List...").c_str()))
            openPanel("Markers");
        if (ImGui::MenuItem(cyui::trLabel("Loop Work Area").c_str(), "L", loop))
            loop = !loop;
        ImGui::EndPopup();
    }
    if (renamingMarker && !ImGui::IsPopupOpen("markerRename") && renameFocus)
        ImGui::OpenPopup("markerRename");
    if (ImGui::BeginPopup("markerRename")) {
        if (renameFocus) {
            ImGui::SetKeyboardFocusHere();
            renameFocus = false;
        }
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##markerName", &renameDraft,
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            Json a = Json::Object();
            a["marker"] = renamingMarker;
            a["name"] = renameDraft;
            action("set_marker", a);
            renamingMarker = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else if (renamingMarker && !renameFocus)
        renamingMarker = 0;

    if (!headerResized)
        headerWidth = preferredHeader;
    ImGui::SetCursorScreenPos(origin + size);
    ImGui::Dummy({0, 0});
    ImGui::EndChild();
    ImGui::End();
}
} // namespace sf

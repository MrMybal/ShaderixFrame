// Command palette (Ctrl+Maj+P): the editor's commands by name, filtered as one types, and the file and
// work-area commands its keyboard shortcuts share.
#include "ui/Editor.h"
#include <algorithm>
#include <cstring>
#include <imgui_stdlib.h>

namespace sf {
namespace {
// Lower case without French accents, so « decoup » finds « Découper ».
std::string folded(const std::string &text) {
    static const std::pair<const char *, char> accents[] = {
        {"à", 'a'}, {"â", 'a'}, {"ä", 'a'}, {"ç", 'c'}, {"é", 'e'}, {"è", 'e'}, {"ê", 'e'}, {"ë", 'e'},
        {"î", 'i'}, {"ï", 'i'}, {"ô", 'o'}, {"ö", 'o'}, {"ù", 'u'}, {"û", 'u'}, {"ü", 'u'}, {"À", 'a'},
        {"Â", 'a'}, {"Ç", 'c'}, {"É", 'e'}, {"È", 'e'}, {"Ê", 'e'}, {"Î", 'i'}, {"Ô", 'o'}, {"Û", 'u'}};
    std::string out;
    for (size_t i = 0; i < text.size();) {
        bool replaced = false;
        for (auto [from, to] : accents) {
            size_t length = strlen(from);
            if (text.compare(i, length, from) == 0) {
                out += to;
                i += length;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            char c = text[i++];
            out += c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
        }
    }
    return out;
}
// Every word of the query must appear in the label or the shortcut; a label starting with the first word
// comes first, then the earliest match. Negative: no match.
int rank(const std::string &query, const std::string &label, const std::string &shortcut) {
    auto haystack = folded(label) + "  " + folded(shortcut);
    auto words = folded(query);
    int score = 0;
    bool first = true;
    size_t at = 0;
    while (at < words.size()) {
        while (at < words.size() && words[at] == ' ')
            ++at;
        size_t end = words.find(' ', at);
        if (end == std::string::npos)
            end = words.size();
        if (end > at) {
            auto found = haystack.find(words.substr(at, end - at));
            if (found == std::string::npos)
                return -1;
            if (first)
                score = found == 0 ? 0 : 1 + int(found);
            first = false;
        }
        at = end;
    }
    return score;
}
// The commands matching a query, best first.
template <class List> std::vector<size_t> matching(const List &list, const std::string &query) {
    std::vector<std::pair<int, size_t>> order;
    for (size_t i = 0; i < list.size(); ++i)
        if (int score = rank(query, list[i].label, list[i].shortcut); score >= 0)
            order.push_back({score, i});
    std::stable_sort(order.begin(), order.end(), [](auto &a, auto &b) { return a.first < b.first; });
    std::vector<size_t> indices;
    for (auto &entry : order)
        indices.push_back(entry.second);
    return indices;
}
} // namespace

void Editor::saveCommand() {
    auto path = s.projectPath.empty() ? dialog(true, L"ShaderixFrame\0*.sframe\0\0", L"sframe") : s.projectPath;
    if (!path.empty()) {
        Json a = Json::Object();
        a["path"] = path;
        action("save_project", a);
    }
}
void Editor::openCommand() {
    auto path = dialog(false, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
    if (!path.empty()) {
        Json a = Json::Object();
        a["path"] = path;
        action("open_project", a);
    }
}
void Editor::importCommand() {
    auto path = dialog(false, L"Media\0*.*\0\0", nullptr);
    if (!path.empty())
        drop(path);
}
void Editor::workAreaEdge(bool in) {
    // The other edge stays when it is still on the right side of the playhead.
    Json a = Json::Object();
    double other = in ? (s.project.hasWorkArea() && s.project.workOut > s.time ? s.project.workOut : s.project.duration)
                      : (s.project.hasWorkArea() && s.project.workIn < s.time ? s.project.workIn : 0);
    a["begin"] = in ? s.time : other;
    a["end"] = in ? other : s.time;
    action("set_work_area", a);
}
std::vector<Editor::Command> Editor::commands() {
    std::vector<Command> list;
    auto add = [&](std::string label, std::string shortcut, std::function<void()> run, bool enabled = true) {
        list.push_back({std::move(label), std::move(shortcut), std::move(run), enabled});
    };
    auto bound = [&](const std::string &id, std::function<void()> run, bool enabled = true) {
        add(keymap.get(id).label, keymap.label(id), std::move(run), enabled);
    };
    bool selected = s.selected != 0;
    for (auto [id, chosen] : {std::pair{"tool.select", Tool::Select}, {"tool.blade", Tool::Blade},
                              {"tool.slip", Tool::Slip}, {"tool.roll", Tool::Roll}, {"tool.stretch", Tool::Stretch},
                              {"tool.slide", Tool::Slide}, {"tool.hand", Tool::Hand}})
        bound(id, [this, chosen] { tool = chosen; });
    bound(
        "edit.split",
        [this] {
            auto a = selectionArgs();
            a["time"] = s.time;
            action("split_clips", a);
        },
        selected);
    add("Delete Selection", "Delete", [this] { action("delete_clips", selectionArgs()); }, selected);
    add(
        "Ripple Delete Selection", "Shift+Delete",
        [this] {
            auto a = selectionArgs();
            a["ripple"] = true;
            action("delete_clips", a);
        },
        selected);
    add("Duplicate Selection", "Ctrl+D", [this] { action("duplicate_clips", selectionArgs()); }, selected);
    for (auto [id, head, ripple] : {std::tuple{"trim.head", true, true}, {"trim.head_keep", true, false},
                                    {"trim.tail", false, true}, {"trim.tail_keep", false, false}})
        bound(id, [this, head, ripple] { trimToPlayhead(head, ripple); }, selected);
    bound("cut.crossfade", [this] { crossfadeAtPlayhead(); });
    bound("clip.freeze", [this] { freezeAtPlayhead(2); }, selected);
    bound("clip.toggle", [this] { toggleClipsEnabled(); }, selected);
    bound("clip.match", [this] { matchFrame(); }, selected);
    // The selected clip's own commands, from its context menu.
    auto *clip = s.project.findClip(s.selected);
    auto *owner = clip ? s.project.owner(clip->id) : nullptr;
    bool picture = clip && owner->kind == TrackKind::Video && !clip->title.enabled && !clip->adjustment;
    bool sound = clip && owner->kind == TrackKind::Audio, sounding = sound;
    if (clip)
        for (auto member : s.project.linked(clip->id, false))
            sounding = sounding || s.project.owner(member)->kind == TrackKind::Audio;
    add(
        "Clip Speed / Duration...", "Ctrl+R",
        [this] {
            if (auto *c = s.project.findClip(s.selected)) {
                speedClip = c->id;
                speedDraft = float(c->speed * 100);
                durationDraft = c->duration;
                openSpeed = true;
            }
        },
        selected);
    add(clip && clip->reverse ? "Play Forward" : "Play Backward", "",
        [this] {
            if (auto *c = s.project.findClip(s.selected)) {
                Json a = Json::Object();
                a["clip"] = c->id;
                a["reverse"] = !c->reverse;
                action("set_clip", a);
            }
        },
        clip && !clip->source.empty() && !clip->hold);
    add("Auto Color (White Balance, Exposure)", "",
        [this] {
            Json a = Json::Object();
            a["clip"] = s.selected;
            if (action("auto_grade", a))
                say("Auto color applied");
        },
        picture);
    add("Normalize Clip Audio (-1 dBFS)", "", [this] { action("normalize_audio", selectionArgs()); }, sound);
    add("Enhance Voice (Gate, EQ, Compressor)", "", [this] { action("enhance_voice", selectionArgs()); },
        sound);
    add("Remove Silence (-40 dB, 0.5 s)", "", [this] { removeSilencesInBackground(s.selected, -40, .5); },
        sounding && !analysis);
    add("Add Audio Transient Markers", "",
        [this] {
            Json a = Json::Object();
            a["clip"] = s.selected;
            a["sensitivity"] = .5;
            action("detect_beats", a);
        },
        sound);
    add("Batch Rename Selection...", "", [this] { openRename = true; }, selected);
    add("Paste Attributes...", "Ctrl+Alt+V", [this] { openAttributes = true; }, selected && !clipboard.empty());
    add("Adjustment Layer at Playhead", "", [this] {
        Json a = Json::Object();
        a["start"] = s.time;
        action("add_adjustment_layer", a);
    });
    add("Add Title", "", [this] { action("add_title"); });
    add("Add Lower Third (Name and Role)...", "", [this] { openLowerThirdDialog(s.time); });
    bool marked = !s.project.markers.empty();
    add("Split at Markers", "", [this] { action("split_at_markers", s.selection.empty() ? Json::Object() : selectionArgs()); },
        marked);
    add("Align Selection to Markers", "", [this] { action("fit_to_markers", selectionArgs()); },
        marked && selected);
    add("Close All Sequence Gaps", "", [this] { action("close_gaps"); });
    bound("select.forward", [this] { selectFrom(s.time, true); });
    bound("select.backward", [this] { selectFrom(s.time, false); });
    bound("marker.add", [this] {
        Json a = Json::Object();
        a["time"] = s.time;
        action("add_marker", a);
    });
    bound("area.in", [this] { workAreaEdge(true); });
    bound("area.out", [this] { workAreaEdge(false); });
    add("Play / Pause", "Space", [this] { s.playing = !s.playing; });
    add("Play Around Playhead", "Shift+Space", [this] { playAround(); });
    bound("loop.toggle", [this] { loop = !loop; });
    add("Go to Start", "Start", [this] { s.time = 0; });
    add("Go to End", "End", [this] { s.time = s.project.duration; });
    add("Undo", "Ctrl+Z", [this] { action("undo"); });
    add("Redo", "Ctrl+Y", [this] { action("redo"); });
    add("Save Project", "Ctrl+S", [this] { saveCommand(); });
    add("Open Project...", "Ctrl+O", [this] { openCommand(); });
    add("Import Media...", "Ctrl+I", [this] { importCommand(); });
    add("New project", "Ctrl+N", [this] { action("new_project"); });
    add(s.project.hasWorkArea() ? "Pre-render Work Area" : "Pre-render Sequence", "Input",
        [this] { startRenderCache(); });
    for (auto [label, target] : {std::pair{"Normalize Loudness to -14 LUFS (YouTube)", -14.},
                                 {"Normalize Loudness to -16 LUFS (Podcast)", -16.},
                                 {"Normalize Loudness to -23 LUFS (EBU R128)", -23.}})
        add(label, "", [this, target] { startLoudness(target); }, !loudnessJob);
    if (panelSuffix.empty()) {
        const char *names[] = {"Edit", "Effects", "Motion", "Color Grading", "Audio", "Deliver"};
        for (int m = 0; m < modeCount; ++m)
            add(std::string("Mode ") + names[m], "F" + std::to_string(m + 1), [this, m] { setMode(Mode(m)); });
    }
    for (auto &[name, open] : panels)
        add("Panel: " + name, "", [this, panel = name] { openPanel(panel); });
    return list;
}
std::vector<std::string> Editor::paletteMatches(const std::string &query) {
    auto list = commands();
    std::vector<std::string> labels;
    for (auto i : matching(list, query))
        labels.push_back(list[i].label);
    return labels;
}
void Editor::commandPalette() {
    if (paletteFocus)
        ImGui::OpenPopup("##palette");
    auto *vp = ImGui::GetMainViewport();
    float width = std::min(560.f, vp->WorkSize.x - 32);
    ImGui::SetNextWindowPos({vp->WorkPos.x + (vp->WorkSize.x - width) / 2, vp->WorkPos.y + 60});
    ImGui::SetNextWindowSize({width, 0});
    if (!ImGui::BeginPopup("##palette")) {
        paletteOpen = false;
        return;
    }
    std::function<void()> chosen;
    auto list = commands();
    auto order = matching(list, paletteQuery);
    if (paletteFocus) {
        ImGui::SetKeyboardFocusHere();
        paletteFocus = false;
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##query", "Command, tool, panel, workspace...", &paletteQuery))
        paletteIndex = 0;
    int count = int(order.size());
    bool moved = false;
    if (count) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            paletteIndex = (paletteIndex + 1) % count;
            moved = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            paletteIndex = (paletteIndex + count - 1) % count;
            moved = true;
        }
        paletteIndex = std::clamp(paletteIndex, 0, count - 1);
    }
    float rows = std::min(12, std::max(1, count)) * ImGui::GetTextLineHeightWithSpacing() + 6;
    if (ImGui::BeginChild("##commands", {0, rows})) {
        for (int n = 0; n < count; ++n) {
            auto &command = list[order[n]];
            ImGui::PushID(n);
            ImGui::BeginDisabled(!command.enabled);
            if (ImGui::Selectable(command.label.c_str(), n == paletteIndex) && command.enabled)
                chosen = command.run;
            ImGui::EndDisabled();
            if (n == paletteIndex && moved)
                ImGui::SetScrollHereY();
            if (!command.shortcut.empty()) {
                ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(command.shortcut.c_str()).x);
                ImGui::TextDisabled("%s", command.shortcut.c_str());
            }
            ImGui::PopID();
        }
        if (!count)
            ImGui::TextDisabled("No matching commands.");
    }
    ImGui::EndChild();
    if (count && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
        auto &command = list[order[paletteIndex]];
        if (command.enabled)
            chosen = command.run;
    }
    if (chosen || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
        paletteOpen = false;
    }
    ImGui::EndPopup();
    if (chosen)
        chosen();
}
} // namespace sf

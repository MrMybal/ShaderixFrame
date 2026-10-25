// Sequence panels: the markers list (as CyDAW's marker list, with notes and chapter export).
#include "ui/Editor.h"
#include "core/AtomicFile.h"
#include "ui/FrameStyle.h"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <fstream>

namespace sf {
using namespace style;
std::string timecode(double time, int fps);

namespace {
std::string lowered(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
} // namespace

void Editor::markersPanel() {
    if (!panels["Markers"] || !beginPanel("Markers"))
        return;
    auto &p = s.project;
    if (ImGui::Button("+ Marker")) {
        Json a = Json::Object();
        a["time"] = s.time;
        action("add_marker", a);
    }
    tooltip("Marker at Playhead [M in Timeline]");
    ImGui::SameLine();
    ImGui::BeginDisabled(p.markers.empty());
    if (ImGui::Button("Chapters"))
        ImGui::SetClipboardText(p.markerChapters().c_str());
    tooltip("Copy Markers as YouTube Chapters (0:00 Title)");
    ImGui::SameLine();
    if (ImGui::Button("CSV…")) {
        auto path = dialog(true, L"CSV\0*.csv\0\0", L"csv");
        if (!path.empty()) {
            Json a = Json::Object();
            a["format"] = "csv";
            a["path"] = path;
            action("export_markers", a);
        }
    }
    tooltip("Export each marker's name, timecode, color and note");
    ImGui::SameLine();
    if (ImGui::Button("Images…")) {
        auto path = dialog(true, L"PNG image\0*.png\0Half-float EXR 16-bit\0*.exr\0\0", L"png");
        if (!path.empty()) {
            Json a = Json::Object();
            a["path"] = path;
            action("export_marker_stills", a);
        }
    }
    tooltip("One final rendered frame per marker: chosen name + number + marker name");
    ImGui::SameLine();
    ImGui::Checkbox("Images", &markerPictures);
    tooltip("Thumbnail of the final render at each marker");
    ImGui::SameLine();
    if (ImGui::Button("Cut"))
        action("split_at_markers", s.selection.empty() ? Json::Object() : selectionArgs());
    tooltip("Split selection (or all tracks) at each marker");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##markerFilter", "Search names or notes", &markerFilter);
    ImGui::EndDisabled();
    if (p.markers.empty()) {
        ImGui::TextDisabled("No markers: press M in the timeline or + Marker.");
        ImGui::End();
        return;
    }
    // The marker the playhead has passed last is highlighted, as the current chapter.
    Id current = 0;
    for (auto &marker : p.markers)
        if (marker.time <= s.time + 1e-6)
            current = marker.id;
    auto filter = lowered(markerFilter);
    Id removed = 0, recoloured = 0;
    std::uint32_t colour = 0;
    // A text edit is one undo step: the project before the first keystroke is kept until it ends.
    auto textEdit = [&] {
        if (ImGui::IsItemActivated())
            markerEditBefore = p;
        if (ImGui::IsItemEdited())
            s.dirty = true;
        if (ImGui::IsItemDeactivatedAfterEdit() && markerEditBefore) {
            s.history.checkpoint(*markerEditBefore, "Marker");
            markerEditBefore.reset();
        }
    };
    int columns = markerPictures ? 4 : 3;
    if (ImGui::BeginTable(markerPictures ? "markerListPictures" : "markerList", columns,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        float pictureWidth = 72, pictureHeight = pictureWidth * float(p.height) / float(std::max(1, p.width));
        ImGui::TableSetupColumn("##colour", ImGuiTableColumnFlags_WidthFixed, 22);
        if (markerPictures)
            ImGui::TableSetupColumn("##picture", ImGuiTableColumnFlags_WidthFixed, pictureWidth);
        // Time and name on the first line, the note under them: readable in a narrow side panel.
        ImGui::TableSetupColumn("Marker", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 22);
        for (auto &marker : p.markers) {
            if (!filter.empty() && lowered(marker.name).find(filter) == std::string::npos &&
                lowered(marker.note).find(filter) == std::string::npos)
                continue;
            ImGui::PushID(int(marker.id));
            ImGui::TableNextRow();
            if (marker.id == current)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, alpha(accent, .16f));
            ImGui::TableNextColumn();
            ImU32 swatch = marker.color ? fromRgb(marker.color) : accent;
            ImGui::PushStyleColor(ImGuiCol_Button, swatch);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brighter(swatch, .2f));
            if (ImGui::Button("##swatch", {16, 16}))
                ImGui::OpenPopup("markerColour");
            ImGui::PopStyleColor(2);
            if (ImGui::BeginPopup("markerColour")) {
                for (int i = 0; i < 12; ++i) {
                    if (i % 6)
                        ImGui::SameLine(0, 4);
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_Button, fromRgb(trackColours[i]));
                    if (ImGui::Button(marker.color == trackColours[i] ? "•" : "", {18, 18})) {
                        recoloured = marker.id;
                        colour = trackColours[i];
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                if (ImGui::MenuItem("Default Color", nullptr, marker.color == 0)) {
                    recoloured = marker.id;
                    colour = 0;
                }
                ImGui::EndPopup();
            }
            if (markerPictures) {
                ImGui::TableNextColumn();
                if (auto *view = clipPreviews.getSequence(p, marker.time, s.renderer))
                    ImGui::Image((ImTextureID)(intptr_t)view, {pictureWidth, pictureHeight});
                else
                    ImGui::Dummy({pictureWidth, pictureHeight});
                if (ImGui::IsItemClicked())
                    s.time = std::min(marker.time, p.duration);
            }
            ImGui::TableNextColumn();
            auto code = timecode(marker.time, p.fps);
            ImGui::AlignTextToFramePadding();
            if (ImGui::Selectable(code.c_str(), false, 0, {ImGui::CalcTextSize(code.c_str()).x, 0}))
                s.time = std::min(marker.time, p.duration);
            tooltip("Go to Marker");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##name", &marker.name);
            textEdit();
            if (marker.duration > 0) {
                auto length = "duration " + timecode(marker.duration, p.fps);
                if (ImGui::Button(length.c_str())) {
                    Json a = Json::Object();
                    a["begin"] = marker.time;
                    a["end"] = std::min(marker.end(), p.duration);
                    action("set_work_area", a);
                }
                tooltip("Region: Set Work Area to Its Duration");
                ImGui::SameLine();
            }
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##note", "Note", &marker.note);
            textEdit();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("×"))
                removed = marker.id;
            tooltip("Delete Marker");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (recoloured) {
        Json a = Json::Object();
        a["marker"] = recoloured;
        a["color"] = colour;
        action("set_marker", a);
    }
    if (removed) {
        Json a = Json::Object();
        a["marker"] = removed;
        action("remove_marker", a);
    }
    ImGui::End();
}
} // namespace sf

namespace sf {
using namespace style;
// History: every undoable step by name, as Photoshop's history list; a click returns to that state.
void Editor::historyPanel() {
    if (!panels["History"] || !beginPanel("History"))
        return;
    auto &past = s.history.undoSteps();
    auto &future = s.history.redoSteps();
    int current = int(past.size()), total = current + int(future.size()), target = -1;
    ImGui::BeginDisabled(!current);
    if (ImGui::Button("Undo"))
        target = current - 1;
    ImGui::EndDisabled();
    tooltip("Undo Last Step [Ctrl+Z]");
    ImGui::SameLine();
    ImGui::BeginDisabled(future.empty());
    if (ImGui::Button("Redo"))
        target = current + 1;
    ImGui::EndDisabled();
    tooltip("Redo Step [Ctrl+Y]");
    ImGui::SameLine();
    ImGui::TextDisabled("%d step%s", total, total > 1 ? "s" : "");
    ImGui::Separator();
    ImGui::BeginChild("historySteps");
    bool follow = historySeen != total * 1000 + current;
    historySeen = total * 1000 + current;
    for (int row = 0; row <= total; ++row) {
        std::string label = row == 0           ? "History Start"
                            : row <= current   ? past[size_t(row - 1)].label
                                               : future[future.size() - size_t(row - current)].label;
        if (label.empty())
            label = "Edit";
        bool undone = row > current;
        if (undone)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(textFaint));
        char text[320];
        snprintf(text, sizeof(text), "%3d   %s##step%d", row, label.c_str(), row);
        if (ImGui::Selectable(text, row == current))
            target = row;
        if (undone)
            ImGui::PopStyleColor();
        if (row == current && follow)
            ImGui::SetScrollHereY(.5f);
    }
    ImGui::EndChild();
    if (target >= 0)
        goToHistoryStep(target);
    ImGui::End();
}
// Returning to a step goes through undo/redo, so previews, audio and clip workspaces follow.
void Editor::goToHistoryStep(int target) {
    for (int step = int(s.history.undoSteps().size()); step > target; --step)
        if (!action("undo"))
            return;
    for (int step = int(s.history.undoSteps().size()); step < target; ++step)
        if (!action("redo"))
            return;
}
} // namespace sf

namespace sf {
using namespace style;
// Subtitles: the cues of a track as an editable list, with a style shared by the whole track.
void Editor::subtitlesPanel() {
    if (!panels["Subtitles"] || !beginPanel("Subtitles"))
        return;
    auto &p = s.project;
    auto titles = [](const Track &t) {
        int count = 0;
        for (auto &clip : t.clips)
            count += clip.title.enabled;
        return count;
    };
    // The chosen track, else the first video track holding title clips.
    auto *track = p.findTrack(subtitleTrack);
    if (!track || track->kind != TrackKind::Video) {
        track = nullptr;
        for (auto &t : p.tracks)
            if (t.kind == TrackKind::Video && titles(t) && !track)
                track = &t;
    }
    ImGui::SetNextItemWidth(std::max(120.f, ImGui::GetContentRegionAvail().x - 170));
    if (ImGui::BeginCombo("##subtitleTrack", track ? track->name.c_str() : "Choose a Track")) {
        for (auto &t : p.tracks)
            if (t.kind == TrackKind::Video) {
                auto label = t.name + "  ·  " + std::to_string(titles(t)) + " cue(s)##" + std::to_string(t.id);
                if (ImGui::Selectable(label.c_str(), track == &t))
                    subtitleTrack = t.id;
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Import…")) {
        auto path = dialog(false, L"SubRip subtitles\0*.srt\0\0", L"srt");
        if (!path.empty()) {
            Json a = Json::Object();
            a["path"] = path;
            if (action("import_subtitles", a))
                subtitleTrack = actionResult["track"].U();
        }
    }
    tooltip("SubRip file (.srt): a new track, one title clip per cue");
    ImGui::SameLine();
    ImGui::BeginDisabled(!track || !titles(*track));
    if (ImGui::Button("Export…")) {
        auto path = dialog(true, L"SubRip subtitles\0*.srt\0\0", L"srt");
        if (!path.empty()) {
            Json a = Json::Object();
            a["path"] = path;
            a["track"] = track->id;
            action("export_subtitles", a);
        }
    }
    ImGui::EndDisabled();
    if (!track) {
        ImGui::TextDisabled("Import an SRT file or choose a video track to write subtitle cues.");
        ImGui::End();
        return;
    }
    subtitleTrack = track->id;
    ImGui::BeginDisabled(track->locked);
    if (ImGui::Button("+ Cue at Playhead")) {
        Json a = Json::Object();
        a["track"] = track->id;
        a["start"] = s.time;
        action("add_subtitle", a);
    }
    ImGui::EndDisabled();
    tooltip("Two seconds using the previous cue's style, stopping at the next clip");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##subtitleFilter", "Search cues", &subtitleFilter);
    std::vector<Clip *> cues;
    for (auto &clip : track->clips)
        if (clip.title.enabled)
            cues.push_back(&clip);
    std::sort(cues.begin(), cues.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
    // Style of the whole track: drafts follow the first cue until one of them is being dragged.
    if (!cues.empty()) {
        static const char *keys[] = {"font_size", "y", "text_outline", "text_box"};
        static const char *names[] = {"font_size", "y", "outline", "box"};
        static const float defaults[] = {40, .38f, 2, 0};
        if (!subtitleStyleEditing)
            for (int i = 0; i < 4; ++i)
                subtitleStyle[i] = cues.front()->param(keys[i], 0, defaults[i]);
        subtitleStyleEditing = false;
        // Three fields share the width left by their labels, so the row fits a narrow side panel.
        float spacing = ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().ItemSpacing.x;
        float fieldWidth = std::max(40.f, (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("SizeHeightOutlineBackground").x -
                                           spacing * 4) / 4);
        auto styleField = [&](int i, const char *label, float speed, float low, float high, const char *format) {
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::DragFloat(label, &subtitleStyle[i], speed, low, high, format, ImGuiSliderFlags_AlwaysClamp);
            subtitleStyleEditing = subtitleStyleEditing || ImGui::IsItemActive();
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Json a = Json::Object();
                a["track"] = track->id;
                a[names[i]] = subtitleStyle[i];
                action("set_subtitle_style", a);
            }
        };
        styleField(0, "Size", .5f, 8, 200, "%.0f px");
        ImGui::SameLine();
        styleField(1, "Height", .005f, -.5f, .5f, "%.2f");
        ImGui::SameLine();
        styleField(2, "Outline", .1f, 0, 16, "%.1f");
        ImGui::SameLine();
        styleField(3, "Background", .01f, 0, 1, "%.2f");
        tooltip("Applied to All Cues on the Track");
    }
    auto filter = lowered(subtitleFilter);
    Id removed = 0;
    if (ImGui::BeginTable("subtitleList", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("##time", ImGuiTableColumnFlags_WidthFixed, 92);
        ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##length", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 22);
        float textHeight = ImGui::GetTextLineHeight() * 2 + ImGui::GetStyle().FramePadding.y * 2 + 2;
        for (auto *clip : cues) {
            if (!filter.empty() && lowered(clip->title.text).find(filter) == std::string::npos)
                continue;
            ImGui::PushID(int(clip->id));
            ImGui::TableNextRow();
            if (s.time >= clip->start && s.time < clip->start + clip->duration)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, alpha(accent, .16f));
            ImGui::TableNextColumn();
            if (ImGui::Selectable(timecode(clip->start, p.fps).c_str(), clip->id == s.selected)) {
                s.time = std::min(clip->start, p.duration);
                s.selectClip(clip->id);
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(track->locked);
            ImGui::InputTextMultiline("##text", &clip->title.text, {-1, textHeight});
            if (ImGui::IsItemActivated())
                markerEditBefore = p;
            if (ImGui::IsItemEdited()) {
                clip->name = clip->title.text.substr(0, clip->title.text.find('\n')).substr(0, 48);
                s.dirty = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit() && markerEditBefore) {
                s.history.checkpoint(*markerEditBefore, "Subtitle Text");
                markerEditBefore.reset();
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%.1f s", clip->duration);
            ImGui::TableNextColumn();
            if (!track->locked && ImGui::SmallButton("×"))
                removed = clip->id;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removed) {
        Json a = Json::Object();
        a["clips"] = Json::Array();
        a["clips"].Push(removed);
        action("delete_clips", a);
    }
    ImGui::End();
}
} // namespace sf

namespace sf {
using namespace style;
// Multicam angles: each angle's own picture at the playhead (isolated render), the live one framed;
// a click cuts to it there, as the 1 to 4 keys in the timeline.
void Editor::anglesPanel() {
    if (!panels["Camera Angles"] || !beginPanel("Camera Angles"))
        return;
    auto &p = s.project;
    auto angles = p.anglesAt(s.time);
    if (angles.size() < 2) {
        ImGui::TextWrapped("Overlap at least two takes on video tracks (for example, synchronized by audio): "
                           "their images appear here; keys 1 to 4 switch to an angle at the playhead.");
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Audio Follows Angle", &angleSound);
    tooltip("Split and enable each take's linked audio with its video");
    int chosen = 0;
    float cell = std::max(80.f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2);
    float height = cell * float(p.height) / float(std::max(1, p.width));
    for (int index = 0; index < int(angles.size()); ++index) {
        auto *clip = p.findClip(angles[size_t(index)]);
        if (index % 2)
            ImGui::SameLine();
        ImGui::PushID(index);
        auto at = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##angle", {cell, height + 20});
        if (ImGui::IsItemClicked())
            chosen = index + 1;
        auto *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(at, at + ImVec2(cell, height), IM_COL32(0, 0, 0, 255));
        if (auto *view = clipPreviews.get(p, *clip, s.time, s.renderer))
            dl->AddImage((ImTextureID)(intptr_t)view, at, at + ImVec2(cell, height));
        bool live = clip->enabled;
        dl->AddRect(at, at + ImVec2(cell, height), live ? accent : (ImGui::IsItemHovered() ? textDim : bg4), 3, 0,
                    live ? 3.f : 1.f);
        char label[96];
        snprintf(label, sizeof(label), "%d  ·  %s", index + 1, p.owner(clip->id)->name.c_str());
        clippedText(dl, at + ImVec2(4, height + 3), cell - 8, live ? accent : textDim, label, .8f);
        ImGui::PopID();
    }
    if (chosen) {
        Json a = Json::Object();
        a["angle"] = chosen;
        a["audio"] = angleSound;
        action("switch_angle", a);
    }
    ImGui::End();
}
} // namespace sf

namespace sf {
using namespace style;
void Editor::saveKeymap() {
    // Written like the panel settings: not by previews or tests without an ini file.
    if (!panelSuffix.empty() || !ImGui::GetIO().IniFilename)
        return;
    try {
        AtomicFile output(sx::Narrow((root / "keymap.json").wstring()));
        {
            std::ofstream file(output.path(), std::ios::binary);
            file << keymap.json().Dump(2);
        }
        output.commit();
    } catch (const std::exception &) {
        // Bindings that cannot be written still apply for this session.
    }
}
// Shortcuts: each editable action with its chord; a click waits for the next key, with its modifiers.
void Editor::shortcutsPanel() {
    if (!keymapRead) {
        keymapRead = true;
        std::string text;
        Json document;
        if (sx::ReadTextFile(sx::Narrow((root / "keymap.json").wstring()), text) && Json::Parse(text, document))
            keymap.load(document);
    }
    if (!keymapCapture.empty()) {
        // The capture runs even when the panel is hidden, so a key pressed elsewhere still lands.
        auto &io = ImGui::GetIO();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            keymapCapture.clear();
        else
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                auto key = ImGuiKey(k);
                bool modifier = key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl || key == ImGuiKey_LeftShift ||
                                key == ImGuiKey_RightShift || key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
                                key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper || key == ImGuiKey_ReservedForModCtrl ||
                                key == ImGuiKey_ReservedForModShift || key == ImGuiKey_ReservedForModAlt ||
                                key == ImGuiKey_ReservedForModSuper || (key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY);
                if (!modifier && ImGui::IsKeyPressed(key, false)) {
                    keymap.set(keymapCapture, key, io.KeyCtrl, io.KeyShift, io.KeyAlt);
                    keymapCapture.clear();
                    saveKeymap();
                    break;
                }
            }
    }
    if (!panels["Keyboard Shortcuts"] || !beginPanel("Keyboard Shortcuts"))
        return;
    if (ImGui::Button("Restore Default Shortcuts")) {
        keymap.reset();
        keymapCapture.clear();
        saveKeymap();
    }
    tooltip("ShaderixFrame's Default Shortcuts");
    ImGui::SameLine();
    ImGui::TextDisabled("Click a shortcut, then press the new key (Esc: cancel)");
    std::string group;
    if (ImGui::BeginTable("shortcutList", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 130);
        for (auto &original : Keymap::defaults()) {
            if (original.group != group) {
                group = original.group;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent), "%s", group.c_str());
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(original.label.c_str());
            auto clashes = keymap.conflicts(original.id);
            if (!clashes.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(warn), "conflit");
                if (ImGui::IsItemHovered()) {
                    std::string names;
                    for (auto &other : clashes)
                        names += "\n" + keymap.get(other).label;
                    ImGui::SetTooltip("Same Shortcut as:%s", names.c_str());
                }
            }
            ImGui::TableNextColumn();
            bool waiting = keymapCapture == original.id;
            auto label = (waiting ? std::string("Key…") : keymap.label(original.id)) + "##" + original.id;
            if (ImGui::Button(label.c_str(), {-1, 0}))
                keymapCapture = waiting ? std::string() : original.id;
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
} // namespace sf

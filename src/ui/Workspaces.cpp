// Workspaces as CyDAW v2's application modes: one bar with the mode buttons and the transport,
// and a dock layout per mode. Each mode remembers which panels the user keeps open.
#include "ui/Editor.h"
#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <algorithm>
#include <functional>
#include <map>
#include <imgui_internal.h>
#include <set>

namespace sf {
using namespace style;
std::string timecode(double time, int fps);
namespace {
struct ModeInfo {
    const char *label, *tip, *focus;
};
constexpr ModeInfo modes[Editor::modeCount] = {
    {"Edit", "Edit: media, composition and large timeline [F1]", "Timeline"},
    {"Effects", "Shaderixel effects: library, VFX and layer graphs, HLSL [F2]", "Shader Graph"},
    {"Motion", "Motion design and animation curves [F3]", "Motion Design"},
    {"Color Grading", "Color grading, scopes and comparison [F4]", "Color Grading"},
    {"Audio", "Mixer, CyDAW DSP graph and audio tracks [F5]", "Mixer & Audio Graph"},
    {"Deliver", "Exports, plugins and MCP [F6]", "Exports"},
};
// Each mode's arrangement is kept in a hidden dockspace, its windows renamed to placeholders,
// with ImGui's DockBuilderCopyDockSpace. Those nodes are saved in the ini with the live layout.
ImGuiID layoutStore(int mode) { return ImHashStr(("ShaderixFrame/ModeLayout/" + std::to_string(mode)).c_str()); }
std::string placeholderSuffix(int mode) { return "##sfmode" + std::to_string(mode); }
ImGuiID dockRoot(ImGuiID id) {
    auto *node = id ? ImGui::DockBuilderGetNode(id) : nullptr;
    while (node && node->ParentNode)
        node = node->ParentNode;
    return node ? node->ID : 0;
}
// Panels open by default in each mode; the others stay docked at a sensible place, closed.
std::set<std::string> defaultOpen(Editor::Mode mode) {
    switch (mode) {
    case Editor::Mode::Edit:
        return {"Media & Effects", "Effects Browser", "Shaderixel Library", "Composition", "Layers", "Properties",
                "Timeline", "Animation"};
    case Editor::Mode::Effects:
        return {"Effects Browser", "Shaderixel Library", "Media & Effects", "Composition", "Layers", "Properties",
                "Shader Graph", "Layer Graph", "Track Graph", "HLSL", "Timeline"};
    case Editor::Mode::Motion:
        return {"Composition", "Layers", "Properties", "Motion Design", "Timeline", "Animation"};
    case Editor::Mode::Color:
        return {"Composition", "Video Scopes", "Color Grading", "Timeline", "Layers"};
    case Editor::Mode::Audio:
        return {"Media & Effects", "Effects Browser", "Properties", "Mixer & Audio Graph", "DSP Graph", "Timeline",
                "Track Effects"};
    case Editor::Mode::Deliver:
        return {"Composition", "Plugins & MCP", "Exports", "Timeline", "Video Scopes"};
    }
    return {};
}
} // namespace

void Editor::setMode(Mode next) {
    if (!panelSuffix.empty())
        return;
    if (!modesReady) {
        for (int m = 0; m < modeCount; ++m) {
            auto open = defaultOpen(Mode(m));
            modePanels[m] = panels;
            for (auto &[name, visible] : modePanels[m])
                visible = open.count(name) > 0;
        }
        modesReady = true;
        mode = next;
        panels = modePanels[int(mode)];
        return;
    }
    if (next == mode)
        return;
    saveModeLayout();
    modePanels[int(mode)] = panels;
    mode = next;
    panels = modePanels[int(mode)];
    layoutReady = false;
    auto *stored = ImGui::DockBuilderGetNode(layoutStore(int(mode)));
    layoutRestore = stored && stored->IsSplitNode();
    if (!layoutRestore)
        if (auto *workspace = ImGui::FindWindowByName("Workspace"))
            ImGui::DockBuilderRemoveNode(workspace->GetID("FrameDock"));
    pendingPanelFocus = panelTitle(modes[int(mode)].focus);
}

void Editor::saveModeLayout() {
    auto *workspace = ImGui::FindWindowByName("Workspace");
    if (!panelSuffix.empty() || !workspace)
        return;
    ImGuiID dock = workspace->GetID("FrameDock");
    auto *node = ImGui::DockBuilderGetNode(dock);
    if (!node || !node->IsSplitNode())
        return;
    // Every window docked under the workspace, open or closed, gets a placeholder in the store.
    std::vector<std::string> names;
    for (auto *window : GImGui->Windows)
        if (window->DockId && dockRoot(window->DockId) == dock)
            names.push_back(window->Name);
    for (auto &[name, open] : panels) {
        auto title = panelTitle(name.c_str());
        if (std::find(names.begin(), names.end(), title) != names.end() || ImGui::FindWindowByName(title.c_str()))
            continue;
        if (auto *settings = ImGui::FindWindowSettingsByID(ImHashStr(title.c_str())))
            if (dockRoot(settings->DockId) == dock)
                names.push_back(title);
    }
    std::vector<std::string> storage;
    storage.reserve(names.size() * 2);
    ImVector<const char *> pairs;
    for (auto &name : names) {
        storage.push_back(name);
        storage.push_back(name + placeholderSuffix(int(mode)));
    }
    for (auto &item : storage)
        pairs.push_back(item.c_str());
    ImGui::DockBuilderCopyDockSpace(dock, layoutStore(int(mode)), &pairs);
}
bool Editor::restoreModeLayout(ImGuiID dock, ImVec2 size) {
    auto store = layoutStore(int(mode));
    auto *stored = ImGui::DockBuilderGetNode(store);
    if (!stored || !stored->IsSplitNode())
        return false;
    auto suffix = placeholderSuffix(int(mode));
    std::vector<std::pair<std::string, ImGuiID>> windows; // real name, stored leaf
    auto &settings = GImGui->SettingsWindows;
    for (auto *item = settings.begin(); item; item = settings.next_chunk(item)) {
        std::string name = item->GetName();
        if (name.size() > suffix.size() && name.ends_with(suffix) && dockRoot(item->DockId) == store)
            windows.push_back({name.substr(0, name.size() - suffix.size()), item->DockId});
    }
    if (windows.empty())
        return false;
    // Rebuild the stored tree with the same builder calls as the default layouts, keeping proportions.
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, size);
    std::map<ImGuiID, ImGuiID> leaves;
    std::function<void(ImGuiDockNode *, ImGuiID)> rebuild = [&](ImGuiDockNode *source, ImGuiID target) {
        if (!source->IsSplitNode()) {
            leaves[source->ID] = target;
            return;
        }
        auto *first = source->ChildNodes[0], *second = source->ChildNodes[1];
        int axis = source->SplitAxis == ImGuiAxis_X ? 0 : 1;
        float a = first->Size[axis], b = second->Size[axis];
        float ratio = std::clamp(a + b > 0 ? a / (a + b) : .5f, .05f, .95f);
        ImGuiID one = 0, two = 0;
        ImGui::DockBuilderSplitNode(target, axis == 0 ? ImGuiDir_Left : ImGuiDir_Up, ratio, &one, &two);
        rebuild(first, one);
        rebuild(second, two);
    };
    rebuild(stored, dock);
    for (auto &[name, leaf] : windows)
        if (auto it = leaves.find(leaf); it != leaves.end())
            ImGui::DockBuilderDockWindow(name.c_str(), it->second);
    ImGui::DockBuilderFinish(dock);
    return true;
}
void Editor::trackDetachedPanels() {
    auto *mainViewport = ImGui::GetMainViewport();
    for (auto &[name, open] : panels)
        if (auto *window = ImGui::FindWindowByName(panelTitle(name.c_str()).c_str()); window && window->Active) {
            if (!window->DockIsActive && window->Viewport && window->Viewport->ID != mainViewport->ID)
                detachedPanels.insert(name);
            else
                detachedPanels.erase(name);
        }
}
int Editor::storedModeLayouts() const {
    int count = 0;
    for (int m = 0; m < modeCount; ++m)
        if (auto *node = ImGui::DockBuilderGetNode(layoutStore(m)); node && node->IsSplitNode())
            ++count;
    return count;
}
void Editor::resetModeLayout() {
    ImGui::DockBuilderRemoveNode(layoutStore(int(mode)));
    layoutRestore = false;
}

void Editor::buildLayout(ImGuiID dock, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, size);
    ImGuiID centre = dock, left = 0, right = 0, upperRight = 0, bottom = 0, side = 0;
    std::set<std::string> placed;
    auto place = [&](const char *name, ImGuiID node) {
        if (!node || placed.count(name))
            return;
        // A panel detached to its own native window (another screen) stays where the user put it,
        // even when it is closed in the mode being left.
        if (detachedPanels.count(name)) {
            placed.insert(name);
            return;
        }
        ImGui::DockBuilderDockWindow(panelTitle(name).c_str(), node);
        placed.insert(name);
    };
    switch (mode) {
    case Mode::Edit:
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, .18f, &left, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .27f, &right, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .48f, &bottom, &centre);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .32f, &upperRight, &right);
        for (auto name : {"Media & Effects", "Effects Browser", "Shaderixel Library", "Plugins & MCP"})
            place(name, left);
        place("Composition", centre);
        place("Source", centre);
        place("Layers", upperRight);
        for (auto name : {"Properties", "Video Scopes", "Markers", "History", "Subtitles", "Camera Angles", "Keyboard Shortcuts"})
            place(name, right);
        for (auto name : {"Timeline", "Animation"})
            place(name, bottom);
        break;
    case Mode::Effects:
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .30f, &bottom, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, .18f, &left, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .25f, &right, &centre);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .35f, &upperRight, &right);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .52f, &side, &centre);
        for (auto name : {"Effects Browser", "Shaderixel Library", "Media & Effects", "Plugins & MCP"})
            place(name, left);
        place("Composition", centre);
        for (auto name : {"Track Graph", "Shader Graph", "Layer Graph", "HLSL"})
            place(name, side);
        place("Layers", upperRight);
        place("Properties", right);
        for (auto name : {"Timeline", "Animation"})
            place(name, bottom);
        break;
    case Mode::Motion: {
        // The curve editor sits above the timeline, both visible.
        ImGuiID curves = 0;
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .55f, &bottom, &centre);
        ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Up, .5f, &curves, &bottom);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .25f, &right, &centre);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .35f, &upperRight, &right);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .48f, &side, &centre);
        place("Composition", centre);
        place("Motion Design", side);
        place("Layers", upperRight);
        place("Properties", right);
        place("Animation", curves);
        place("Timeline", bottom);
        break;
    }
    case Mode::Color:
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .22f, &bottom, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .46f, &side, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .32f, &right, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, .2f, &left, &centre);
        place("Composition", centre);
        place("Layers", left);
        place("Video Scopes", right);
        place("Color Grading", side);
        place("Timeline", bottom);
        break;
    case Mode::Audio:
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .46f, &bottom, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, .18f, &left, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .27f, &right, &centre);
        place("Media & Effects", left);
        place("Effects Browser", left);
        for (auto name : {"Mixer & Audio Graph", "DSP Graph"})
            place(name, centre);
        place("Properties", right);
        {
            // The insert chain under the timeline, as CyDAW's device panel.
            ImGuiID devices = 0;
            ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Down, .42f, &devices, &bottom);
            place("Track Effects", devices);
        }
        place("Timeline", bottom);
        break;
    case Mode::Deliver:
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .36f, &bottom, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, .22f, &left, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .28f, &right, &centre);
        place("Plugins & MCP", left);
        place("Composition", centre);
        place("Video Scopes", right);
        for (auto name : {"Timeline", "Exports"})
            place(name, bottom);
        break;
    }
    place("Clip Editor", centre);
    // Panels this mode does not show by default still open in a sensible node.
    for (auto &[name, open] : panels)
        place(name.c_str(), bottom ? bottom : centre);
    ImGui::DockBuilderFinish(dock);
}

void Editor::modeBar() {
    auto *viewport = ImGui::GetMainViewport();
    float height = ImGui::GetFrameHeight() + 12;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 6});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    if (ImGui::BeginViewportSideBar("##modeBar", viewport, ImGuiDir_Up, height,
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        for (int m = 0; m < modeCount; ++m) {
            if (m)
                ImGui::SameLine(0, 2);
            if (modeButton(modes[m].label, int(mode) == m, modes[m].tip))
                setMode(Mode(m));
        }
        toolbarSeparator();
        // Transport, as in CyDAW's top bar.
        auto jump = [&](bool forward) {
            std::vector<double> points{0, s.project.duration};
            for (auto &marker : s.project.markers)
                points.push_back(marker.time);
            if (s.project.hasWorkArea()) {
                points.push_back(s.project.workIn);
                points.push_back(s.project.workOut);
            }
            double best = forward ? s.project.duration : 0;
            for (double point : points)
                if (forward ? point > s.time + 1e-6 && point < best : point < s.time - 1e-6 && point > best)
                    best = point;
            s.time = best;
        };
        if (iconButton(icon::previous, "Previous Marker / Start"))
            jump(false);
        ImGui::SameLine(0, 2);
        if (iconButton(s.playing ? icon::pause : icon::play, "Play / Pause [Space]", s.playing, play))
            s.playing = !s.playing;
        ImGui::SameLine(0, 2);
        if (iconButton(icon::stop, "Stop and Return to Work Area Start")) {
            s.playing = false;
            s.time = loop && s.project.hasWorkArea() ? s.project.workIn : 0;
        }
        ImGui::SameLine(0, 2);
        if (iconButton(icon::next, "Next Marker / End"))
            jump(true);
        ImGui::SameLine(0, 2);
        if (iconButton(icon::loop, "Loop [L]: work area or entire sequence", loop))
            loop = !loop;
        ImGui::SameLine(0, 10);
        ImGui::PushFont(sx::ui::GetFonts().mono, ImGui::GetStyle().FontSizeBase * 1.15f);
        auto code = timecode(s.time, s.project.fps);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(s.playing ? play : accent), "%s", code.c_str());
        ImGui::PopFont();
        if (offlineMedia > 0) {
            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(record));
            char label[48];
            snprintf(label, sizeof(label), "%d media file%s offline", offlineMedia, offlineMedia > 1 ? "s" : "");
            if (ImGui::SmallButton(label))
                relinkDialog();
            ImGui::PopStyleColor();
            tooltip("Files not found: choose a file from their folder to relink them");
        }
        if (renderCache.rendering()) {
            ImGui::SameLine(0, 8);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(play), "Pre-render %d %%",
                               renderCache.framesDone() * 100 / std::max(1, renderCache.frameCount()));
            tooltip("Pre-rendered frames play without recalculating effects; editing invalidates them");
        }
        if (double rate = playbackRate(); rate != 0 && rate != 1) {
            ImGui::SameLine(0, 6);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(warn), "%s ×%.0f", rate < 0 ? "<<" : ">>", std::abs(rate));
            tooltip("Shuttle: J backward, Shift+L forward, K stop");
        }
        ImGui::SameLine(0, 10);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled(cyui::tr("%d × %d  ·  %d fps").c_str(), s.project.width, s.project.height, s.project.fps);
        if (s.project.hasWorkArea()) {
            ImGui::SameLine(0, 10);
            ImGui::TextDisabled(cyui::tr("zone %s → %s").c_str(), timecode(s.project.workIn, s.project.fps).c_str(),
                                timecode(s.project.workOut, s.project.fps).c_str());
        }
        auto title = s.project.name + (s.dirty ? "  •" : "");
        float width = ImGui::CalcTextSize(title.c_str()).x;
        ImGui::SameLine();
        if (ImGui::GetContentRegionAvail().x > width + 12) {
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - width);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(title.c_str());
        } else
            ImGui::NewLine();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}
} // namespace sf

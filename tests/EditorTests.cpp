#include "ui/Editor.h"
#include <imgui_internal.h>
#include <iostream>
#include <mfapi.h>

static int checks = 0;
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
    ++checks;
    std::cout << message << std::endl;
}
static ImGuiWindow *canvas(const char *parent, const char *name) {
    for (auto *window : GImGui->Windows)
        if (window->ParentWindow && window->ParentWindow->Name == std::string(parent) &&
            std::string(window->Name).find(name) != std::string::npos)
            return window;
    throw std::runtime_error("Canvas not found");
}
int main(int argc, char **argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    ImGui::CreateContext();
    int result = 0;
    try {
        auto root = sx::Utf8Path(argv[1]);
        sf::Session s;
        s.renderer.initialize();
        s.renderer.library.scan(root);
        s.project = sf::Project::load(sx::Narrow((root / "examples/CutStudy.sframe").wstring()));
        s.selectClip(s.project.tracks[0].clips[0].id);
        s.time = 1;
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.DisplaySize = {1600, 980};
        sf::applyFrameTheme();
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        sf::Editor editor(s, root);
        auto frame = [&](const char *focus = nullptr) {
            io.DeltaTime = 1.f / 60;
            ImGui::NewFrame();
            if (focus)
                ImGui::SetWindowFocus(focus);
            editor.draw();
            ImGui::Render();
        };
        for (int i = 0; i < 4; ++i)
            frame("Timeline");
        auto origin = canvas("Timeline", "timelineCanvas")->DC.CursorStartPos;
        auto point = [&](float seconds, int track) {
            return origin + ImVec2(180 + seconds * 55, 32.f + float(track) * 68 + 25);
        };
        auto click = [&](ImVec2 at, bool ctrl = false) {
            io.AddMousePosEvent(at.x, at.y);
            io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame();
        };
        auto original = s.project.json().Dump(0);
        auto second = s.project.tracks[0].clips[1].id;
        auto first = s.project.tracks[0].clips[0].id;
        click(point(4, 0));
        require(s.selection.size() == 3 && s.selected == second && s.project.json().Dump(0) == original &&
                    !s.history.undo(s.project) && !s.dirty,
                "A timeline click selects its linked group without dirtying or checkpointing");
        click(point(1, 1), true);
        require(s.selection.size() == 6, "Ctrl-click adds a second linked group");
        auto at = point(4, 0);
        io.AddMousePosEvent(at.x, at.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(at.x + 55, at.y);
        frame();
        io.AddMousePosEvent(at.x + 110, at.y);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        require(s.project.findClip(first)->start == 2 && s.project.findClip(second)->start == 5 &&
                    s.project.tracks[2].clips[0].start == 2 && s.dirty && !editor.hasError(),
                "Dragging a selected body moves both groups with the audio");
        require(s.history.undo(s.project) && s.project.json().Dump(0) == original &&
                    !s.history.undo(s.project),
                "A multi-frame group drag is one undo step");
        s.dirty = false;
        // One mouse pixel can already cross the snap threshold at the default zoom.
        s.selectClip(first);
        at = point(1, 0);
        io.AddMousePosEvent(at.x, at.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(at.x + 1, at.y);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        require(std::abs(s.project.findClip(first)->start - 1. / 30) < 1e-7 && s.dirty,
                "One-pixel snapped movement is marked dirty and recoverable");
        s.history.undo(s.project);
        s.history.clear();
        s.dirty = false;
        // A rectangle drawn across two title clips also selects their video/audio partners.
        ImVec2 low = origin + ImVec2(180 + .3f * 55, 32 + 2);
        ImVec2 high = origin + ImVec2(180 + 5.8f * 55, 32 + 60);
        io.AddMousePosEvent(low.x, low.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(high.x, high.y);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        require(s.selection.size() == 6 && !s.dirty && !s.history.undo(s.project),
                "Marquee selection expands linked groups without modifying the document");
        s.project = sf::Project::load(sx::Narrow((root / "examples/GraphStudy.sframe").wstring()));
        auto video = s.project.tracks[0].clips[0].id;
        s.selectClip(video);
        s.history.clear();
        for (int i = 0; i < 4; ++i)
            frame("Graphe VFX");
        auto *graphCanvas = canvas("Graphe VFX", "graphCanvas");
        auto graphOrigin = graphCanvas->DC.CursorStartPos;
        auto available = graphCanvas->InnerRect.Max - graphOrigin - graphCanvas->WindowPadding;
        sf::CanvasView view;
        view.fit({-100, -80}, {565, 163}, {available.x, available.y});
        auto center = graphOrigin + available * .5f;
        original = s.project.json().Dump(0);
        io.AddMousePosEvent(center.x, center.y);
        frame();
        io.AddMouseWheelEvent(0, 1);
        frame();
        view.zoomAt({center.x - graphOrigin.x, center.y - graphOrigin.y}, view.zoom * 1.15f);
        io.AddMouseButtonEvent(2, true);
        frame();
        io.AddMousePosEvent(center.x + 30, center.y + 20);
        frame();
        io.AddMouseButtonEvent(2, false);
        frame();
        view.pan = view.pan + sx::float2{30, 20};
        require(s.project.json().Dump(0) == original && !s.dirty && !s.history.undo(s.project),
                "Graph wheel zoom and middle pan preserve the upstream graph and history");
        auto header = view.screen({-90, -70});
        at = graphOrigin + ImVec2(header.x, header.y);
        io.AddMousePosEvent(at.x, at.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMousePosEvent(at.x + 46, at.y + 23);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        sx::EffectGraph graph;
        sx::EffectGraph::FromJson(s.project.findClip(video)->effects[0].graph, graph);
        require(std::abs(graph.Find(1)->pos.x - (-100 + 46 / view.zoom)) < .01 &&
                    std::abs(graph.Find(1)->pos.y - (-80 + 23 / view.zoom)) < .01 && s.dirty,
                "Graph header drag converts screen pixels through the current zoom");
        require(s.history.undo(s.project) && s.project.json().Dump(0) == original &&
                    !s.history.undo(s.project),
                "Graph drag is a single undo step after navigation");
        s.dirty = false;
        auto snapshot = s.project;
        snapshot.width = 320;
        snapshot.height = 180;
        snapshot.duration = 600;
        auto outputs = std::filesystem::current_path() / "test-output/editor";
        std::filesystem::create_directories(outputs);
        auto job = s.exports.start(snapshot, s.renderer.library, sf::ExportKind::Video,
                                   sx::Narrow((outputs / "background.mp4").wstring()));
        for (int i = 0; i < 3; ++i)
            frame("Exports");
        auto *panel = ImGui::FindWindowByName("Exports");
        require(panel && !panel->Hidden && panel->DockId == ImGui::FindWindowByName("Timeline")->DockId &&
                    s.project.json().Dump(0) == original && !s.dirty && !editor.hasError(),
                "Export panel docks with the timeline while the preview continues rendering");
        auto *status = ImGui::FindWindowByName("status");
        auto cancelPoint = status->DC.CursorStartPos + ImVec2(205, 7);
        click(cancelPoint);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (s.exports.status(job).active() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(s.exports.status(job).state == sf::ExportState::Cancelled && !editor.hasError() &&
                    s.project.json().Dump(0) == original && !s.dirty,
                "The status bar cancel button stops the worker without changing the project");
        s.project = sf::Project::load(sx::Narrow((root / "examples/MixerStudy.sframe").wstring()));
        s.history.clear();
        s.dirty = false;
        original = s.project.json().Dump(0);
        for (int i = 0; i < 4; ++i)
            frame("Mixeur & graphe audio");
        auto *details = canvas("Mixeur & graphe audio", "CyDAWMixerStrips");
        require(
            details && !details->Hidden && s.project.json().Dump(0) == original &&
                !s.history.undo(s.project) && !s.dirty && !editor.hasError(),
            "Viewing native CyDAW mixer strips and DSP values leaves the document and automation unchanged");
        sf::DspCanvas dsp;
        sf::Json draft;
        sf::Json::Parse(
            R"({"schema":2,"nodes":[{"id":1,"processor":"cydaw.eq-three","pos":[430,200],"params":{}}],"connections":[{"source":0,"destination":0}]})",
            draft);
        dsp.open(draft);
        auto dspFrame = [&]() {
            io.DeltaTime = 1.f / 60;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({30, 30});
            ImGui::SetNextWindowSize({1100, 540});
            ImGui::SetNextWindowFocus();
            ImGui::Begin("DSP test");
            dsp.draw();
            ImGui::End();
            ImGui::Render();
        };
        for (int i = 0; i < 4; ++i)
            dspFrame();
        auto pointDsp = [&](sx::float2 p) { return dsp.screenPoint(p); };
        auto from = pointDsp({232, 242}), to = pointDsp({430, 242});
        io.AddMousePosEvent(from.x, from.y);
        dspFrame();
        io.AddMouseButtonEvent(0, true);
        dspFrame();
        io.AddMousePosEvent(to.x, to.y);
        dspFrame();
        io.AddMouseButtonEvent(0, false);
        dspFrame();
        require(dsp.document()["connections"].Size() == 2,
                "DSP canvas mouse gesture connects a real stereo output and input");
        auto dspHeader = pointDsp({460, 212});
        io.AddMousePosEvent(dspHeader.x, dspHeader.y);
        dspFrame();
        io.AddMouseButtonEvent(0, true);
        dspFrame();
        float dspZoom = dsp.navigation().zoom;
        io.AddMousePosEvent(dspHeader.x + 60, dspHeader.y + 20);
        dspFrame();
        io.AddMouseButtonEvent(0, false);
        dspFrame();
        require(std::abs(dsp.document()["nodes"].At(0)["pos"].At(0).F() - 430 - 60 / dspZoom) < .01f,
                "DSP canvas card drag respects zoom and stores editable layout");
        require(dsp.undo() && dsp.document()["nodes"].At(0)["pos"].At(0).F() == 430,
                "A DSP mouse drag is one local undo step");
        s.project = sf::Project{};
        s.project.width = 320;
        s.project.height = 180;
        s.project.duration = 1;
        sf::Clip gradeClip;
        gradeClip.id = s.project.allocate();
        gradeClip.generator = true;
        gradeClip.duration = 1;
        gradeClip.effects = {sf::makeGrade("Grade test")};
        s.project.addTrack(sf::TrackKind::Video).clips = {gradeClip};
        s.selectClip(gradeClip.id);
        s.time = .25;
        s.history.clear();
        s.dirty = false;
        editor.showGrading();
        original = s.project.json().Dump(0);
        for (int i = 0; i < 4; ++i)
            frame("Étalonnage");
        auto *gradeCanvas = canvas("Étalonnage", "Grade graph");
        require(gradeCanvas && !gradeCanvas->Hidden && s.project.json().Dump(0) == original && !s.dirty &&
                    !editor.hasError(),
                "Opening grading tools and the actual correction graph leaves the project unchanged");
        click(gradeCanvas->DC.CursorStartPos + ImVec2(127 + 75, 22));
        require(s.project.json().Dump(0) == original && !s.history.undo(s.project) && !editor.hasError(),
                "Selecting a grading node does not create an undo entry or mutate its parameters");
        for (int i = 0; i < 4; ++i)
            frame("Scopes vidéo");
        require(s.project.json().Dump(0) == original && !s.dirty && !editor.hasError(),
                "Video scopes analyze a real GPU frame without changing the document");
        s.project = sf::Project::load(sx::Narrow((root / "examples/LayerGraphStudy.sframe").wstring()));
        s.time = 4;
        s.history.clear();
        s.dirty = false;
        original = s.project.json().Dump(0);
        for (int i = 0; i < 4; ++i)
            frame("Graphe des calques");
        auto *layers = canvas("Graphe des calques", "ShaderixelComposition");
        require(layers && !layers->Hidden && s.project.json().Dump(0) == original &&
                    !s.history.undo(s.project) && !s.dirty && !editor.hasError(),
                "Opening the independent Shaderixel layer graph renders its sources without changing the "
                "document");
        {
            auto at = layers->DC.CursorStartPos + ImVec2(50, 50);
            io.AddMousePosEvent(at.x, at.y);
            frame("Graphe des calques");
            auto old = editor.layerGraphZoom();
            io.AddMouseWheelEvent(0, 1);
            frame("Graphe des calques");
            require(editor.layerGraphZoom() > old && s.project.json().Dump(0) == original,
                    "Wheel zoom on the native layer graph changes the view without editing the project");
            at = layers->InnerRect.Max - ImVec2(20, 20);
            io.AddMousePosEvent(at.x, at.y);
            frame();
            io.AddMouseButtonEvent(1, true);
            frame();
            io.AddMouseButtonEvent(1, false);
            frame();
            require(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId),
                    "Right click opens the layer graph context menu");
            io.AddKeyEvent(ImGuiKey_Escape, true);
            frame();
            io.AddKeyEvent(ImGuiKey_Escape, false);
            frame();
            for (int i = 0; i < 3; ++i)
                frame("Composition");
            auto *composition = canvas("Composition", "CompositionViewport");
            at = composition->DC.CursorStartPos + ImVec2(100, 80);
            io.AddMousePosEvent(at.x, at.y);
            frame("Composition");
            old = editor.previewNavigation().zoom;
            io.AddMouseWheelEvent(0, 1);
            frame("Composition");
            require(editor.previewNavigation().zoom > old && s.project.json().Dump(0) == original,
                    "Composition wheel zoom enlarges the video view without changing its render");
            for (int i = 0; i < 3; ++i)
                frame("Timeline");
            auto *timeline = canvas("Timeline", "timelineCanvas");
            at = timeline->DC.CursorStartPos + ImVec2(350, 20);
            io.AddMousePosEvent(at.x, at.y);
            frame("Timeline");
            old = editor.timelinePixelsPerSecond();
            io.AddMouseWheelEvent(0, 1);
            frame("Timeline");
            require(editor.timelinePixelsPerSecond() > old && s.project.json().Dump(0) == original,
                    "Timeline wheel zoom changes time scale without creating a project edit");
            at = timeline->Pos + ImVec2(350, -10);
            io.AddMousePosEvent(at.x, at.y);
            frame("Timeline");
            old = editor.timelinePixelsPerSecond();
            io.AddMouseButtonEvent(0, true);
            frame("Timeline");
            io.AddMousePosEvent(at.x - 35, at.y + 30);
            frame("Timeline");
            io.AddMouseButtonEvent(0, false);
            frame("Timeline");
            require(editor.timelinePixelsPerSecond() > old,
                    "CyDAW navigator strip responds to vertical drag by zooming the timeline");
        }
        s.project = sf::Project::load(sx::Narrow((root / "examples/MotionStudy.sframe").wstring()));
        // Optional parent fields must stay absent during read-only hierarchy traversal.
        for (auto &track : s.project.tracks)
            for (auto &clip : track.clips) {
                const auto layout = clip.widget;
                if (!layout.IsNull() && !layout["parent"].U()) {
                    clip.widget = sf::Json::Object();
                    for (auto &[key, value] : layout.Items())
                        if (key != "parent")
                            clip.widget[key] = value;
                }
            }
        s.selectClip(6);
        s.time = 4;
        s.dirty = false;
        s.history.clear();
        original = s.project.json().Dump(0);
        editor.openPanel("Motion design");
        for (int i = 0; i < 4; ++i)
            frame("Motion design");
        require(s.project.json().Dump(0) == original && !s.dirty && !editor.hasError(),
                "Opening the widget hierarchy and rendered canvas does not edit the motion design document");
        s.project = sf::Project::load(sx::Narrow((root / "examples/ClipStudy.sframe").wstring()));
        s.selectClip(2);
        s.time = 4;
        s.dirty = false;
        s.history.clear();
        original = s.project.json().Dump(0);
        editor.showClipEditor();
        for (int i = 0; i < 5; ++i)
            frame("Éditeur de clip");
        require(s.project.json().Dump(0) == original && !s.dirty && !editor.hasError(),
                "The embedded clip workspace opens its local timeline without committing a parent edit");
        for (int i = 0; i < 3; ++i)
            frame("Timeline##clip:2");
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_D, true);
        frame();
        io.AddKeyEvent(ImGuiKey_D, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        frame();
        require(
            editor.hasClipDrafts() && s.project.json().Dump(0) == original,
            "Keyboard duplicate in a clip workspace edits its local draft and leaves the parent untouched");
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_Z, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Z, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        frame();
        require(s.project.json().Dump(0) == original,
                "Keyboard undo in a clip workspace does not undo the parent project");
        require(editor.applyClipDrafts() && !editor.hasClipDrafts() && !editor.hasError(),
                "Clip drafts apply through validated parent project mutations");
        std::cout << checks << " editor interaction checks passed.\n";
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << e.what() << std::endl;
        result = 1;
    }
    ImGui::DestroyContext();
    MFShutdown();
    CoUninitialize();
    return result;
}

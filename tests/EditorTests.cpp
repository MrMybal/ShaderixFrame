#include "ui/EnglishPanels.h"
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
        sf::Json legacy = sf::Json::Object();
        legacy["Graphe des calques"] = false;
        legacy["Propriétés"] = true;
        auto english = sf::englishPanelSettings(legacy);
        require(english.Has("Layer Graph") && !english["Layer Graph"].B() && english["Properties"].B(),
                "English panel migration preserves saved visibility settings");
        auto oldIni = std::string("[Window][Graphe des calques##sfmode2]\nDockId=0x12345678\n");
        auto newIni = sf::englishWindowSettings(oldIni);
        require(newIni.find("[Window][Layer Graph##sfmode2]") != std::string::npos &&
                    newIni.find("DockId=0x12345678") != std::string::npos,
                "English window titles preserve the user's saved docking layout");
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
        require(canvas("Timeline", "timelineCanvas") && editor.currentMode() == sf::Editor::Mode::Edit,
                "The editing mode opens with the CyDAW-style timeline");
        {
            // A copied frame goes to the clipboard as a bottom-up BGRA DIB.
            sf::Image still;
            still.width = 2;
            still.height = 2;
            still.pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 10, 20, 30, 255};
            auto dib = sf::clipboardDib(still);
            auto *header = reinterpret_cast<const BITMAPINFOHEADER *>(dib.data());
            auto *rows = dib.data() + sizeof(BITMAPINFOHEADER);
            require(dib.size() == sizeof(BITMAPINFOHEADER) + 16 && header->biWidth == 2 && header->biHeight == 2 &&
                        header->biBitCount == 32 && rows[0] == 255 && rows[2] == 0 && rows[4] == 30 &&
                        rows[6] == 10 && rows[8] == 0 && rows[10] == 255,
                    "Copy image builds a bottom-up BGRA clipboard bitmap");
        }
        {
            // Recent projects: newest first, each once, ten at most.
            for (int i = 0; i < 12; ++i)
                editor.remember("C:/projets/film" + std::to_string(i) + ".sframe");
            editor.remember("C:/projets/film5.sframe");
            auto &recent = editor.recentList();
            require(recent.size() == 10 && recent[0] == "C:/projets/film5.sframe" &&
                        recent[1] == "C:/projets/film11.sframe" &&
                        std::count(recent.begin(), recent.end(), "C:/projets/film5.sframe") == 1,
                    "Recent projects keep the newest first, each once, ten at most");
            // An empty sequence shows the welcome card; a clip hides it.
            auto full = s.project;
            s.project = sf::Project{};
            for (int i = 0; i < 3; ++i)
                frame("Composition");
            bool welcomed = editor.welcomeVisible();
            s.project = full;
            for (int i = 0; i < 2; ++i)
                frame("Composition");
            require(welcomed && !editor.welcomeVisible(), "An empty sequence shows the welcome card, a clip hides it");
            for (int i = 0; i < 2; ++i)
                frame("Timeline");
        }
        {
            // Editable shortcuts: a captured chord rebinds an action, conflicts show, and the defaults come back.
            auto &keys = editor.shortcuts();
            editor.captureShortcut("marker.add");
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_G, true);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_G, false);
            frame("Timeline");
            bool rebound = keys.label("marker.add") == "G";
            size_t markersBefore = s.project.markers.size();
            io.AddKeyEvent(ImGuiKey_G, true);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_G, false);
            frame("Timeline");
            bool placed = s.project.markers.size() == markersBefore + 1;
            io.AddKeyEvent(ImGuiKey_M, true);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_M, false);
            frame("Timeline");
            bool oldKeyFree = s.project.markers.size() == markersBefore + 1;
            keys.set("tool.slip", ImGuiKey_G, false, false, false);
            bool clash = keys.conflicts("marker.add").size() == 1 && keys.conflicts("marker.add")[0] == "tool.slip";
            sf::Keymap reloaded;
            reloaded.load(keys.json());
            bool saved = reloaded.label("marker.add") == "G" && keys.json().Size() == 2;
            keys.reset();
            require(rebound && placed && oldKeyFree && clash && saved && keys.label("marker.add") == "M" &&
                        keys.conflicts("marker.add").empty(),
                    "A captured chord rebinds an action, frees the old key, shows conflicts, saves, and resets");
            if (s.history.undoSteps().size())
                s.history.undo(s.project);
            s.history.clear();
            s.dirty = false;
        }
        {
            // Command palette: accents ignored, best match first; typing then Enter runs the command and closes.
            auto split = editor.paletteMatches("split sélection");
            auto panel = editor.paletteMatches("panel markers");
            auto none = editor.paletteMatches("zzqx");
            auto run = [&](const char *query) {
                editor.openPalette();
                frame();
                frame();
                io.AddInputCharactersUTF8(query);
                frame();
                bool shown = editor.paletteVisible();
                io.AddKeyEvent(ImGuiKey_Enter, true);
                frame();
                io.AddKeyEvent(ImGuiKey_Enter, false);
                frame();
                return shown && !editor.paletteVisible();
            };
            bool before = editor.looping();
            bool ran = run("loop");
            bool toggled = editor.looping() != before;
            run("loop");
            require(!split.empty() && split[0] == "Split Selection at Playhead" && !panel.empty() &&
                        panel[0] == "Panel: Markers" && none.empty() && ran && toggled && editor.looping() == before,
                    "The command palette finds commands without accents and runs the chosen one with Enter");
            frame("Timeline");
        }
        {
            // A region's end is a handle on the ruler: dragging it changes the length in one undo step.
            auto saved = s.project;
            auto &region = s.project.addMarker(1, "Region");
            region.duration = 1;
            sf::Id regionId = region.id;
            s.history.clear();
            float zoomBefore = editor.timelinePixelsPerSecond();
            double scrollBefore = editor.timelineFirstVisibleTime();
            editor.setTimelineView(100, 0);
            for (int i = 0; i < 2; ++i)
                frame("Timeline");
            auto grab = editor.rulerPoint(2, 23), drop = editor.rulerPoint(4, 23);
            io.AddMousePosEvent(grab.x, grab.y);
            frame("Timeline");
            io.AddMouseButtonEvent(0, true);
            frame("Timeline");
            io.AddMousePosEvent(drop.x, drop.y);
            frame("Timeline");
            io.AddMouseButtonEvent(0, false);
            frame("Timeline");
            auto *resized = s.project.findMarker(regionId);
            bool longer = resized && std::abs(resized->duration - 3) < .1 && std::abs(resized->time - 1) < 1e-9;
            bool undone = s.history.undo(s.project) && s.project.findMarker(regionId) &&
                          std::abs(s.project.findMarker(regionId)->duration - 1) < 1e-9;
            require(longer && undone, "Dragging a region's end on the ruler changes its length, undone in one step");
            s.project = saved;
            s.history.clear();
            s.dirty = false;
            editor.setTimelineView(zoomBefore, scrollBefore);
            for (int i = 0; i < 2; ++i)
                frame("Timeline");
        }
        {
            // Track Effects follows the selected clip's sound and shows the chain without editing it.
            auto saved = s.project;
            auto &voice = s.project.addTrack(sf::TrackKind::Audio);
            sf::Clip line;
            line.id = s.project.allocate();
            line.source = "voice.wav";
            line.duration = 2;
            voice.clips.push_back(line);
            sf::Id voiceTrack = voice.id;
            sf::Json add = sf::Json::Object();
            add["scope"] = "track";
            add["target"] = voiceTrack;
            add["id"] = "cydaw.comp-pro";
            sf::Protocol(s).execute("add_mix_effect", add);
            add["id"] = "cydaw.limiter";
            sf::Protocol(s).execute("add_mix_effect", add);
            s.selectClip(line.id);
            editor.openPanel("Track Effects");
            for (int i = 0; i < 3; ++i)
                frame("Track Effects");
            auto shown = s.project.json().Dump(0);
            frame("Track Effects");
            auto channel = editor.chainChannel();
            require(channel.first == "track" && channel.second == voiceTrack &&
                        s.project.findTrack(voiceTrack)->mix.effects.size() == 2 && s.project.json().Dump(0) == shown &&
                        !editor.hasError(),
                    "Track Effects follows the selected clip's audio track and shows its chain without editing it");
            s.project = saved;
            s.history.clear();
            s.dirty = false;
            s.selectClip(0);
            for (int i = 0; i < 2; ++i)
                frame("Timeline");
        }
        {
            // Track Graph: a library effect added to a whole video track shows in its graph, undone in one step.
            auto saved = s.project;
            sf::Id videoTrack = 0;
            for (auto &t : s.project.tracks)
                if (t.kind == sf::TrackKind::Video) {
                    videoTrack = t.id;
                    break;
                }
            std::string effectId;
            for (auto &d : s.renderer.library.definitions)
                if (d.kind == "effect" && !d.generator && d.id.starts_with("shaderixel.")) {
                    effectId = d.id;
                    break;
                }
            editor.openTrackGraph(videoTrack);
            sf::Json add = sf::Json::Object();
            add["track"] = videoTrack;
            add["id"] = effectId;
            bool added = editor.runAction("add_track_effect", add);
            for (int i = 0; i < 3; ++i)
                frame("Track Graph");
            auto *t = s.project.findTrack(videoTrack);
            bool shown = editor.trackGraphShown() == videoTrack && t && !t->graph.IsNull() &&
                         t->graph["nodes"].Size() == 3 && !editor.hasError();
            bool undone = s.history.undo(s.project) && s.project.findTrack(videoTrack)->graph.IsNull();
            require(added && shown && undone,
                    "A library effect added to a video track appears in its Track Graph and undoes in one step");
            s.project = saved;
            s.history.clear();
            s.dirty = false;
            for (int i = 0; i < 2; ++i)
                frame("Timeline");
        }
        {
            // The Effects Browser shows each family of cards (previews come from its own worker) without editing.
            auto shown = s.project.json().Dump(0);
            editor.openPanel("Effects Browser");
            for (int kind = 0; kind < 5; ++kind) {
                editor.setBrowserKind(kind);
                for (int i = 0; i < 3; ++i)
                    frame("Effects Browser");
            }
            require(s.project.json().Dump(0) == shown && !editor.hasError(),
                    "The Effects Browser shows effects, blends, generators, transitions and audio without editing");
            editor.setBrowserKind(0);
            frame("Timeline");
        }
        auto point = [&](float seconds, int track) { return editor.timelinePoint(seconds, track); };
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
        ImVec2 low{point(.3f, 0).x, editor.timelineTrackTop(0) + 1};
        ImVec2 high{point(5.8f, 0).x, point(5.8f, 0).y + 10};
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
        // Timeline markers, work area and track operations go through undoable protocol commands.
        {
            auto before = s.project.json().Dump(0);
            s.time = 2;
            io.AddKeyEvent(ImGuiKey_M, true);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_M, false);
            frame("Timeline");
            require(s.project.markers.size() == 1 && std::abs(s.project.markers[0].time - 2) < 1e-9 && s.dirty,
                    "M adds a ruler marker at the playhead");
            auto strip = editor.timelineNavigatorPoint(3.37) + ImVec2(0, 14 + 28 + 30);
            io.AddMousePosEvent(strip.x, strip.y);
            frame("Timeline");
            io.AddMouseButtonEvent(0, true);
            frame("Timeline");
            io.AddMouseButtonEvent(0, false);
            frame("Timeline");
            require(std::abs(s.time * 30 - std::round(s.time * 30)) < 1e-6 && std::abs(s.time - 3.37) < .05 &&
                        s.project.markers.size() == 1,
                    "Scrubbing the IMAGE filmstrip lands on a whole video frame");
            s.time = 2;
            auto ruler = editor.timelineNavigatorPoint(5) + ImVec2(0, 14 + 4);
            io.AddMousePosEvent(ruler.x, ruler.y);
            frame("Timeline");
            io.AddMouseButtonEvent(0, true);
            frame("Timeline");
            io.AddMousePosEvent(ruler.x + 110, ruler.y);
            frame("Timeline");
            io.AddMouseButtonEvent(0, false);
            frame("Timeline");
            require(s.project.hasWorkArea() && std::abs(s.project.workIn - 5) < 1e-6 &&
                        std::abs(s.project.workOut - 7) < 1e-6,
                    "Dragging the top of the ruler draws the work area on frames");
            auto lane = point(6.5f, 1);
            io.AddMousePosEvent(lane.x, lane.y);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_B, true);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_B, false);
            frame("Timeline");
            io.AddMouseButtonEvent(0, true);
            frame("Timeline");
            io.AddMouseButtonEvent(0, false);
            frame("Timeline");
            require(s.project.tracks[1].clips.size() == 4 && !editor.hasError(),
                    "The blade tool splits the clicked clip at the snapped pointer time");
            io.AddKeyEvent(ImGuiKey_V, true);
            frame("Timeline");
            io.AddKeyEvent(ImGuiKey_V, false);
            frame("Timeline");
            sf::Json move = sf::Json::Object();
            move["track"] = s.project.tracks[2].id;
            move["index"] = 0;
            sf::Protocol(s).execute("move_track", move);
            require(s.project.tracks[0].kind == sf::TrackKind::Audio, "move_track reorders the timeline");
            sf::Json tint = sf::Json::Object();
            tint["clip"] = s.project.tracks[1].clips[0].id;
            tint["color"] = 0x5ab0f0;
            sf::Protocol(s).execute("set_clip", tint);
            require(sf::Project::fromJson(s.project.json()).tracks[1].clips[0].color == 0x5ab0f0,
                    "A clip colour survives the project round trip");
            {
                auto count = [&] {
                    size_t n = 0;
                    for (auto &t : s.project.tracks)
                        n += t.clips.size();
                    return n;
                };
                s.selectClip(s.project.tracks[1].clips[0].id);
                auto clipsBefore = count();
                s.time = 9;
                for (auto key : {ImGuiKey_C, ImGuiKey_V}) {
                    io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    io.AddKeyEvent(key, true);
                    frame("Timeline");
                    io.AddKeyEvent(key, false);
                    io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    frame("Timeline");
                }
                require(count() == clipsBefore + 3 && s.selection.size() == 3 &&
                            std::abs(s.project.findClip(s.selection[0])->start - 9) < 1e-9 && !editor.hasError(),
                        "Ctrl+C / Ctrl+V pastes a linked group at the playhead as one new group");
            }
            {
                auto drag = [&](ImVec2 from, ImVec2 to) {
                    io.AddMousePosEvent(from.x, from.y);
                    frame("Timeline");
                    io.AddMouseButtonEvent(0, true);
                    frame("Timeline");
                    io.AddMousePosEvent((from.x + to.x) * .5f, (from.y + to.y) * .5f);
                    frame("Timeline");
                    io.AddMousePosEvent(to.x, to.y);
                    frame("Timeline");
                    io.AddMouseButtonEvent(0, false);
                    frame("Timeline");
                };
                auto key = [&](ImGuiKey k) {
                    io.AddKeyEvent(k, true);
                    frame("Timeline");
                    io.AddKeyEvent(k, false);
                    frame("Timeline");
                };
                auto outgoing = s.project.tracks[0].clips[0].id, incoming = s.project.tracks[0].clips[1].id;
                key(ImGuiKey_N);
                auto cut = point(3, 0);
                drag(cut, cut + ImVec2(55, 0));
                require(std::abs(s.project.findClip(outgoing)->duration - 4) < 1e-6 &&
                            std::abs(s.project.findClip(incoming)->start - 4) < 1e-6 &&
                            std::abs(s.project.findClip(incoming)->duration - 2) < 1e-6 && !editor.hasError(),
                        "The roll tool moves the cut shared by two adjacent clips");
                key(ImGuiKey_V);
                // The audio clip's gain line rises with the pointer.
                int audio = 0;
                while (s.project.tracks[audio].kind != sf::TrackKind::Audio)
                    ++audio;
                float top = editor.timelineTrackTop(audio), row = (point(1.5f, audio).y - top) / .6f;
                float clipTop = top + 2, clipBottom = top + row - 2;
                float body = clipBottom - (clipTop + std::min(15.f, (clipBottom - clipTop) * .35f)) - 6;
                float line = clipBottom - 3 - std::pow(60.f / 72, 2.f) * body;
                auto audioClip = s.project.tracks[audio].clips[0].id;
                drag({point(1.5f, audio).x, line}, {point(1.5f, audio).x, line - 12});
                auto gain = s.project.findClip(audioClip)->params.find("gain");
                require(gain != s.project.findClip(audioClip)->params.end() && gain->second.value > 0.5f &&
                            gain->second.value <= 12 && !editor.hasError(),
                        "Dragging an audio clip's gain line raises its gain in decibels");
                // Track groups, as CyDAW's folders: grouping, folding and Ctrl+G.
                sf::Protocol protocol(s);
                sf::Json grouping = sf::Json::Object();
                grouping["tracks"] = sf::Json::Array();
                grouping["tracks"].Push(s.project.tracks[0].id);
                grouping["tracks"].Push(s.project.tracks[2].id);
                auto groupId = protocol.execute("group_tracks", grouping)["group"].U();
                require(s.project.groups.size() == 1 && s.project.tracks[0].group == groupId &&
                            s.project.tracks[1].group == groupId && s.project.tracks[2].group == 0,
                        "Grouping tracks gathers them under the first one");
                sf::Json fold = sf::Json::Object();
                fold["group"] = groupId;
                fold["collapsed"] = true;
                protocol.execute("set_group", fold);
                for (int i = 0; i < 2; ++i)
                    frame("Timeline");
                require(editor.timelineTrackTop(0) < -1e5f && editor.timelineTrackTop(1) < -1e5f &&
                            editor.timelineTrackTop(2) > 0 && !editor.hasError(),
                        "A folded group hides its tracks from the timeline");
                sf::Json dissolve = sf::Json::Object();
                dissolve["group"] = groupId;
                protocol.execute("ungroup", dissolve);
                for (int i = 0; i < 2; ++i)
                    frame("Timeline");
                s.selectClip(s.project.tracks[2].clips[0].id);
                io.AddKeyEvent(ImGuiMod_Ctrl, true);
                io.AddKeyEvent(ImGuiKey_G, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_G, false);
                io.AddKeyEvent(ImGuiMod_Ctrl, false);
                frame("Timeline");
                require(s.project.groups.size() == 1, "Ctrl+G groups the tracks of the selected clips");
                protocol.execute("undo", sf::Json::Object());
                for (int i = 0; i < 2; ++i)
                    frame("Timeline");
                // Rate stretch on the last video clip, slide on a middle one.
                int video = 0;
                while (s.project.tracks[video].kind != sf::TrackKind::Video)
                    ++video;
                auto sorted = [&] {
                    std::vector<sf::Clip> clips = s.project.tracks[video].clips;
                    std::sort(clips.begin(), clips.end(), [](auto &l, auto &r) { return l.start < r.start; });
                    return clips;
                };
                auto last = sorted().back();
                key(ImGuiKey_R);
                auto edge = point(float(last.start + last.duration), video) - ImVec2(2, 0);
                drag(edge, edge + ImVec2(55 * float(last.duration) / 3, 0));
                auto stretched = *s.project.findClip(last.id);
                require(std::abs(stretched.speed - last.speed * .75) < .02 &&
                            std::abs(stretched.duration - last.duration * 4 / 3) < .05 && !editor.hasError(),
                        "The stretch tool changes a clip's speed from its edge");
                auto middle = sorted()[1];
                key(ImGuiKey_U);
                auto inside = point(float(middle.start + middle.duration * .5), video);
                drag(inside, inside - ImVec2(55, 0));
                auto clips = sorted();
                require(std::abs(clips[1].start - (middle.start - 1)) < 1e-6 &&
                            std::abs(clips[0].start + clips[0].duration - clips[1].start) < 1e-6 &&
                            std::abs(clips[1].start + clips[1].duration - clips[2].start) < 1e-6 && !editor.hasError(),
                        "The slide tool moves a clip between neighbours that follow its edges");
                key(ImGuiKey_V);
                // The fx badge opens the clip's effect stack; a transition handle sets its length.
                int stacked = -1;
                for (int i = 0; i < int(s.project.tracks.size()) && stacked < 0; ++i)
                    for (auto &clip : s.project.tracks[i].clips)
                        if (clip.effects.size() >= 2 && clip.duration >= 2) {
                            stacked = i;
                            break;
                        }
                require(stacked >= 0, "The study has a clip with an effect stack");
                sf::Clip withEffects;
                withEffects.start = 1e9;
                for (auto &clip : s.project.tracks[stacked].clips)
                    if (clip.effects.size() >= 2 && clip.duration >= 2 &&
                        clip.start + clip.duration < withEffects.start + withEffects.duration)
                        withEffects = clip;
                float badgeY = editor.timelineTrackTop(stacked) + 2 + 7;
                float badgeX = point(float(withEffects.start + withEffects.duration), stacked).x - 8;
                io.AddMousePosEvent(badgeX, badgeY);
                frame("Timeline");
                // Frames without a forced focus: focusing a window closes popups.
                io.AddMouseButtonEvent(0, true);
                frame();
                io.AddMouseButtonEvent(0, false);
                frame();
                require(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !editor.hasError(),
                        "Clicking the fx badge opens the clip's effect stack");
                io.AddKeyEvent(ImGuiKey_Escape, true);
                frame();
                io.AddKeyEvent(ImGuiKey_Escape, false);
                frame();
                sf::Json fade = sf::Json::Object();
                fade["clip"] = withEffects.id;
                fade["id"] = "transition.crossfade";
                fade["duration"] = 1;
                protocol.execute("set_transition", fade);
                frame("Timeline");
                float stackedRow = (point(1, stacked).y - editor.timelineTrackTop(stacked)) / .6f;
                ImVec2 handle{point(float(withEffects.start + 1), stacked).x,
                              editor.timelineTrackTop(stacked) + stackedRow - 2 - 4};
                drag(handle, handle + ImVec2(27.5f, 0));
                require(std::abs(s.project.findClip(withEffects.id)->transition.duration - 1.5) < 1e-6 && !editor.hasError(),
                        "Dragging the transition handle sets its length on frames");
            }
            {
                auto layered = sf::Project::load(sx::Narrow((root / "examples/LayerGraphStudy.sframe").wstring()));
                auto victim = std::find_if(layered.tracks.begin(), layered.tracks.end(),
                                           [](auto &t) { return t.kind == sf::TrackKind::Video; });
                bool explicitGraph = !layered.layerGraph.IsNull();
                auto count = layered.tracks.size();
                layered.removeTrack(victim->id);
                layered.validate();
                require(layered.tracks.size() == count - 1 && (!explicitGraph || !layered.layerGraph.IsNull()),
                        "Removing a video track keeps a valid explicit Shaderixel layer graph");
            }
            while (s.history.undo(s.project))
                ;
            require(s.project.json().Dump(0) == before, "Marker, work area, blade and track edits all undo");
            s.history.clear();
            s.dirty = false;
            for (int i = 0; i < 2; ++i)
                frame("Timeline");
            {
                // A tall audio track shows CyDAW's gain knob; dragging it up raises the track gain.
                int audio = 0;
                while (s.project.tracks[audio].kind != sf::TrackKind::Audio)
                    ++audio;
                s.project.tracks[audio].height = 120;
                for (int i = 0; i < 3; ++i)
                    frame("Timeline");
                ImVec2 knob{editor.timelineHeaderLeft() + 12 + 17, editor.timelineTrackTop(audio) + 50 + 11};
                io.AddMousePosEvent(knob.x, knob.y);
                frame("Timeline");
                io.AddMouseButtonEvent(0, true);
                frame("Timeline");
                io.AddMousePosEvent(knob.x, knob.y - 10);
                frame("Timeline");
                io.AddMousePosEvent(knob.x, knob.y - 20);
                frame("Timeline");
                io.AddMouseButtonEvent(0, false);
                frame("Timeline");
                float raised = s.project.tracks[audio].gain;
                require(raised > 2 && s.history.undo(s.project) && s.project.tracks[audio].gain == 0 &&
                            !s.history.undo(s.project),
                        "Dragging a track's gain knob raises it in one undo step");
                s.project.tracks[audio].height = 0;
                s.history.clear();
                s.dirty = false;
                for (int i = 0; i < 2; ++i)
                    frame("Timeline");
                // Z frames the selection: the selected clip fills the view, a little margin before it.
                auto zoomBefore = editor.timelinePixelsPerSecond();
                s.selectClip(s.project.tracks[0].clips[1].id);
                auto &chosen = *s.project.findClip(s.selected);
                io.AddKeyEvent(ImGuiKey_Z, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_Z, false);
                frame("Timeline");
                require(editor.timelinePixelsPerSecond() > zoomBefore &&
                            editor.timelineFirstVisibleTime() < chosen.start &&
                            editor.timelineFirstVisibleTime() > chosen.start - chosen.duration * .1 &&
                            s.project.json().Dump(0) == before,
                        "Z frames the selected clips without editing the sequence");
                // Alt + arrows nudge the selection by one frame, ten with Shift.
                double startBefore = chosen.start, timeBefore = s.time;
                sf::Id nudged = chosen.id;
                io.AddKeyEvent(ImGuiMod_Alt, true);
                io.AddKeyEvent(ImGuiKey_RightArrow, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_RightArrow, false);
                frame("Timeline");
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(ImGuiKey_LeftArrow, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_LeftArrow, false);
                io.AddKeyEvent(ImGuiMod_Shift, false);
                io.AddKeyEvent(ImGuiMod_Alt, false);
                frame("Timeline");
                require(std::abs(s.project.findClip(nudged)->start - (startBefore - 9. / 30)) < 1e-9 && s.time == timeBefore &&
                            s.history.undo(s.project) && s.history.undo(s.project) &&
                            s.project.json().Dump(0) == before,
                        "Alt + arrows nudge the selected clips by frames without moving the playhead");
                s.history.clear();
                s.dirty = false;
                // Shift+E disables the selection with its linked group, one undo step.
                s.selectClip(nudged);
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(ImGuiKey_E, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_E, false);
                io.AddKeyEvent(ImGuiMod_Shift, false);
                frame("Timeline");
                bool allOff = true;
                for (auto id : s.project.linked(nudged, false))
                    allOff = allOff && !s.project.findClip(id)->enabled;
                require(allOff && s.history.undo(s.project) && s.project.json().Dump(0) == before,
                        "Shift+E disables the selected clips with their linked group in one undo step");
                // Ctrl+R: speed / duration dialog, 200 % halves the clip and keeps its source span.
                s.selectClip(nudged);
                auto *speedy = s.project.findClip(nudged);
                double span = speedy->duration * speedy->speed, oldSpeed = speedy->speed;
                io.AddKeyEvent(ImGuiMod_Ctrl, true);
                io.AddKeyEvent(ImGuiKey_R, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_R, false);
                io.AddKeyEvent(ImGuiMod_Ctrl, false);
                frame();
                frame();
                click(editor.speedButton(0));
                click(editor.speedButton(1));
                speedy = s.project.findClip(nudged);
                double frameLength = 1. / s.project.fps;
                require(std::abs(speedy->speed - 2) < 1e-6 && std::abs(speedy->duration - span / 2) <= frameLength &&
                            s.history.undo(s.project) && s.project.findClip(nudged)->speed == oldSpeed &&
                            s.project.json().Dump(0) == before,
                        "Ctrl+R sets a clip to 200 % speed from the dialog, halving it, in one undo step");
                // Alt+W cuts the selected clip's tail at the playhead without closing the gap.
                s.selectClip(nudged);
                auto trimmed = *s.project.findClip(nudged);
                double playheadBefore = s.time;
                s.time = std::round((trimmed.start + trimmed.duration * .5) * s.project.fps) / s.project.fps;
                io.AddKeyEvent(ImGuiMod_Alt, true);
                io.AddKeyEvent(ImGuiKey_W, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_W, false);
                io.AddKeyEvent(ImGuiMod_Alt, false);
                frame("Timeline");
                auto *tail = s.project.findClip(nudged);
                require(tail->start == trimmed.start && std::abs(tail->start + tail->duration - s.time) < 1e-6 &&
                            s.history.undo(s.project) && s.project.json().Dump(0) == before,
                        "Alt+W trims the selected clip's tail to the playhead in one undo step");
                // Shift+F inserts a two-second freeze frame of the selected clip and selects it.
                s.selectClip(nudged);
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(ImGuiKey_F, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_F, false);
                io.AddKeyEvent(ImGuiMod_Shift, false);
                frame("Timeline");
                auto *held = s.project.findClip(s.selected);
                require(held && held->hold && std::abs(held->start - s.time) < 1e-6 && held->duration == 2 &&
                            s.history.undo(s.project) && s.project.json().Dump(0) == before,
                        "Shift+F inserts a selected two-second freeze frame at the playhead in one undo step");
                // J shuttles backwards, faster when pressed again; K stops at ×1.
                auto press = [&](ImGuiKey keyName) {
                    io.AddKeyEvent(keyName, true);
                    frame("Timeline");
                    io.AddKeyEvent(keyName, false);
                    frame("Timeline");
                };
                double shuttleFrom = s.time = 3;
                press(ImGuiKey_J);
                press(ImGuiKey_J);
                for (int i = 0; i < 10; ++i)
                    frame("Timeline");
                bool reversing = s.playing && editor.playbackRate() == -2 && s.time < shuttleFrom - .1;
                press(ImGuiKey_K);
                require(reversing && !s.playing && editor.playbackRate() == 0 && s.project.json().Dump(0) == before,
                        "J plays backwards at twice the speed on a second press, K stops");
                // A selects everything from the playhead on, on every unlocked track.
                s.time = 3;
                press(ImGuiKey_A);
                std::set<sf::Id> expectedForward;
                for (auto &t : s.project.tracks)
                    for (auto &c : t.clips)
                        if (!t.locked && c.start + c.duration > 3 + 1e-7)
                            expectedForward.insert(c.id);
                require(!expectedForward.empty() &&
                            std::set<sf::Id>(s.selection.begin(), s.selection.end()) == expectedForward &&
                            s.project.json().Dump(0) == before,
                        "A selects every clip reaching past the playhead without editing");
                // Ctrl+F finds a clip by part of its name; Enter selects it and moves the playhead to it.
                auto unfound = s.project;
                auto sought = s.project.tracks[0].clips.back();
                s.project.findClip(sought.id)->name = "Plan Zébulon";
                s.selectClip(0);
                s.time = 0;
                io.AddKeyEvent(ImGuiMod_Ctrl, true);
                io.AddKeyEvent(ImGuiKey_F, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_F, false);
                io.AddKeyEvent(ImGuiMod_Ctrl, false);
                frame();
                frame();
                io.AddInputCharactersUTF8("zébu");
                frame();
                io.AddKeyEvent(ImGuiKey_Enter, true);
                frame();
                io.AddKeyEvent(ImGuiKey_Enter, false);
                frame();
                bool foundIt = s.selected == sought.id && std::abs(s.time - sought.start) < 1e-9;
                s.project = unfound;
                require(foundIt, "Ctrl+F finds a clip by name; Enter selects it and moves the playhead there");
                {
                    // A small sequence of two adjacent generators: key navigation and the crossfade at the cut.
                    auto kept = s.project;
                    sf::Project pair;
                    pair.duration = 6;
                    sf::Clip one, two;
                    one.id = pair.allocate();
                    one.generator = true;
                    one.duration = 3;
                    one.params["opacity"].setKey({.5, 1});
                    one.params["opacity"].setKey({2.5, .5f});
                    two = one;
                    two.id = pair.allocate();
                    two.start = 3;
                    two.params.clear();
                    pair.addTrack(sf::TrackKind::Video).clips = {one, two};
                    s.project = pair;
                    s.history.clear();
                    s.selectClip(one.id);
                    s.time = 1;
                    for (int i = 0; i < 2; ++i)
                        frame("Timeline");
                    auto chordKey = [&](ImGuiKey modifier, ImGuiKey keyName) {
                        io.AddKeyEvent(modifier, true);
                        io.AddKeyEvent(keyName, true);
                        frame("Timeline");
                        io.AddKeyEvent(keyName, false);
                        io.AddKeyEvent(modifier, false);
                        frame("Timeline");
                    };
                    chordKey(ImGuiMod_Ctrl, ImGuiKey_RightArrow);
                    double nextKey = s.time;
                    chordKey(ImGuiMod_Ctrl, ImGuiKey_LeftArrow);
                    double previousKey = s.time;
                    require(std::abs(nextKey - 2.5) < 1e-9 && std::abs(previousKey - .5) < 1e-9,
                            "Ctrl + arrows jump to the next and previous animation keys of the selection");
                    s.time = 3.1;
                    chordKey(ImGuiMod_Shift, ImGuiKey_D);
                    auto *incoming = s.project.findClip(two.id);
                    require(incoming->transition.enabled && std::abs(incoming->transition.duration - 1) < 1e-9 &&
                                std::abs(incoming->start - 2) < 1e-9 && s.history.undo(s.project) &&
                                !s.project.findClip(two.id)->transition.enabled,
                            "Shift+D crossfades the cut nearest the playhead for one second, in one undo step");
                    // A right click on a clip opens a compact menu: the less common actions live in submenus.
                    editor.setTimelineView(100, 0);
                    for (int i = 0; i < 2; ++i)
                        frame("Timeline");
                    auto onClip = editor.timelinePoint(1, 0);
                    io.AddMousePosEvent(onClip.x, onClip.y);
                    frame("Timeline");
                    io.AddMouseButtonEvent(1, true);
                    frame();
                    io.AddMouseButtonEvent(1, false);
                    frame();
                    frame();
                    auto *menu = GImGui->OpenPopupStack.empty() ? nullptr : GImGui->OpenPopupStack.back().Window;
                    float menuHeight = menu ? menu->Size.y : 0;
                    io.AddKeyEvent(ImGuiKey_Escape, true);
                    frame();
                    io.AddKeyEvent(ImGuiKey_Escape, false);
                    frame();
                    require(menu && menuHeight > 100 && menuHeight < 22 * ImGui::GetFrameHeight(),
                            "A right click on a clip opens a compact menu with grouped submenus");
                    // Multicam: the 2 key cuts to the second angle at the playhead; the Angles panel shows the takes.
                    auto takes = s.project;
                    sf::Clip lower = takes.tracks[0].clips[0];
                    lower.id = takes.allocate();
                    lower.params.clear();
                    takes.addTrack(sf::TrackKind::Video).clips = {lower};
                    s.project = takes;
                    s.history.clear();
                    s.time = 1;
                    frame("Timeline");
                    io.AddKeyEvent(ImGuiKey_2, true);
                    frame("Timeline");
                    io.AddKeyEvent(ImGuiKey_2, false);
                    frame("Timeline");
                    bool upperOff = false, lowerOn = false;
                    for (auto &clip : s.project.tracks[0].clips)
                        upperOff = upperOff || (std::abs(clip.start - 1) < 1e-9 && !clip.enabled);
                    for (auto &clip : s.project.tracks[1].clips)
                        lowerOn = lowerOn || (std::abs(clip.start - 1) < 1e-9 && clip.enabled);
                    editor.openPanel("Camera Angles");
                    for (int i = 0; i < 3; ++i)
                        frame("Camera Angles");
                    require(upperOff && lowerOn && ImGui::FindWindowByName("Camera Angles")->Active && !editor.hasError() &&
                                s.history.undo(s.project),
                            "The 2 key cuts to the second angle at the playhead, and the Angles panel shows the takes");
                    // Ctrl+C then Ctrl+Alt+V on another clip pastes its attributes, keys moved with the clip.
                    s.selectClip(one.id);
                    chordKey(ImGuiMod_Ctrl, ImGuiKey_C);
                    s.selectClip(two.id);
                    io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    io.AddKeyEvent(ImGuiMod_Alt, true);
                    io.AddKeyEvent(ImGuiKey_V, true);
                    frame("Timeline");
                    io.AddKeyEvent(ImGuiKey_V, false);
                    io.AddKeyEvent(ImGuiMod_Alt, false);
                    io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    frame();
                    bool dialog = !GImGui->OpenPopupStack.empty();
                    sf::AttributeChoice opacityOnly;
                    opacityOnly.effects = opacityOnly.transform = opacityOnly.audio = false;
                    editor.setAttributeChoice(opacityOnly);
                    editor.pasteCopiedAttributes();
                    auto *styled = s.project.findClip(two.id);
                    bool carried = styled->params.contains("opacity") && styled->params["opacity"].keys.size() == 2 &&
                                   std::abs(styled->params["opacity"].keys[0].time - 3.5) < 1e-9;
                    io.AddKeyEvent(ImGuiKey_Escape, true);
                    frame();
                    io.AddKeyEvent(ImGuiKey_Escape, false);
                    frame();
                    require(dialog && carried && s.history.undo(s.project) &&
                                !s.project.findClip(two.id)->params.contains("opacity"),
                            "Ctrl+Alt+V pastes the copied clip's opacity animation, moved to the target, in one undo step");
                    s.project = kept;
                    s.history.clear();
                }
                // Shift+Space plays around the playhead and comes back to it at the end.
                s.time = 3;
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(ImGuiKey_Space, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_Space, false);
                io.AddKeyEvent(ImGuiMod_Shift, false);
                bool around = s.playing && s.time < 1.5;
                s.time = 5.2; // as if the two seconds after the playhead had played
                frame("Timeline");
                require(around && !s.playing && std::abs(s.time - 3) < 1e-9,
                        "Shift+Space plays from two seconds before the playhead and returns to it");
                // The A/B snapshot keeps the current view and compares it through a wipe.
                frame();
                bool snapped = editor.takeSnapshot();
                editor.setWipe(.3f);
                for (int i = 0; i < 2; ++i)
                    frame();
                require(snapped && editor.comparing() && !editor.hasError(),
                        "A snapshot of the view turns the A/B wipe comparison on");
                s.time = playheadBefore;
                s.time = playheadBefore;
                s.history.clear();
                s.dirty = false;
                editor.setTimelineView(55, 0);
                for (int i = 0; i < 2; ++i)
                    frame("Timeline");
            }
        }
        {
            // Each mode keeps the arrangement the user gave it.
            auto dockOf = [](const char *name) { return ImGui::FindWindowByName(name)->DockId; };
            ImGui::DockBuilderDockWindow("Layers", dockOf("Timeline"));
            for (int i = 0; i < 3; ++i)
                frame("Timeline");
            require(dockOf("Layers") && dockOf("Layers") == dockOf("Timeline"), "A panel can be docked with the timeline");
            float timelineHeight = ImGui::FindWindowByName("Timeline")->Size.y;
            editor.setMode(sf::Editor::Mode::Effects);
            for (int i = 0; i < 3; ++i)
                frame("Timeline");
            require(dockOf("Layers") != dockOf("Timeline"), "The effects mode has its own arrangement");
            editor.setMode(sf::Editor::Mode::Edit);
            for (int i = 0; i < 3; ++i)
                frame("Timeline");
            require(dockOf("Layers") && dockOf("Layers") == dockOf("Timeline") && dockOf("Composition") &&
                        std::abs(ImGui::FindWindowByName("Timeline")->Size.y - timelineHeight) < 4 && !editor.hasError(),
                    "Returning to the editing mode restores the arrangement the user made");
        }
        s.project = sf::Project::load(sx::Narrow((root / "examples/GraphStudy.sframe").wstring()));
        auto video = s.project.tracks[0].clips[0].id;
        s.selectClip(video);
        s.history.clear();
        editor.setMode(sf::Editor::Mode::Effects);
        for (int i = 0; i < 4; ++i)
            frame("Shader Graph");
        auto *graphCanvas = canvas("Shader Graph", "graphCanvas");
        auto graphOrigin = graphCanvas->DC.CursorStartPos;
        auto available = graphCanvas->InnerRect.Max - graphOrigin - graphCanvas->WindowPadding;
        sf::CanvasView view=editor.shaderNavigation();
        auto center = graphOrigin + available * .5f;
        original = s.project.json().Dump(0);
        io.AddMousePosEvent(center.x, center.y);
        frame();
        io.AddMouseWheelEvent(0, 1);
        frame();
        view.zoomAt({center.x - graphOrigin.x, center.y - graphOrigin.y}, view.zoom * std::exp(.15f));
        io.AddMouseButtonEvent(2, true);
        frame();
        io.AddMousePosEvent(center.x + 30, center.y + 20);
        frame();
        io.AddMouseButtonEvent(2, false);
        frame();
        view=editor.shaderNavigation();
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
        {
            auto restored=s.project.json().Dump(0);auto view=editor.shaderNavigation();auto header=view.screen({-90,-70});
            ImVec2 grab=graphOrigin+ImVec2(header.x,header.y);
            io.AddMousePosEvent(grab.x,grab.y);frame();io.AddMouseButtonEvent(0,true);frame();
            io.AddMousePosEvent(grab.x+30,grab.y+15);frame();io.AddKeyEvent(ImGuiKey_Escape,true);frame();
            io.AddKeyEvent(ImGuiKey_Escape,false);io.AddMouseButtonEvent(0,false);frame();
            require(s.project.json().Dump(0)==restored&&!s.history.undo(s.project),"Escape cancels a shared shader-node drag without a history entry");
        }
        {
            auto document=s.project.json().Dump(0);auto selected=s.selected;bool playing=s.playing;
            editor.openPanel("HLSL");for(int i=0;i<4;++i)frame("HLSL");
            auto* text=canvas("HLSL","##code");auto click=text->DC.CursorStartPos+ImVec2(65,35);
            io.AddMousePosEvent(click.x,click.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
            io.AddKeyEvent(ImGuiKey_S,true);io.AddInputCharactersUTF8("s");frame();io.AddKeyEvent(ImGuiKey_S,false);frame();
            io.AddKeyEvent(ImGuiKey_Space,true);frame();io.AddKeyEvent(ImGuiKey_Space,false);frame();
            io.AddKeyEvent(ImGuiKey_Delete,true);frame();io.AddKeyEvent(ImGuiKey_Delete,false);frame();
            io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Z,true);frame();io.AddKeyEvent(ImGuiKey_Z,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
            require(s.project.json().Dump(0)==document&&s.selected==selected&&s.playing==playing&&!s.history.undo(s.project),"Typing, Delete, Space and local code undo do not edit or play the sequence");
            frame("Shader Graph");
        }
        {
            // The Animations curve editor drags a key on whole frames, as one undo step.
            s.dirty = false;
            s.history.clear();
            auto curves = s.project.json().Dump(0);
            editor.openPanel("Animation");
            for (int i = 0; i < 4; ++i)
                frame("Animation");
            auto key = editor.curvePoint(-1, "opacity", 1);
            require(key.x > 0 && s.project.json().Dump(0) == curves && !editor.hasError(),
                    "The curve editor draws the clip's animated opacity without editing it");
            io.AddMousePosEvent(key.x, key.y);
            frame("Animation");
            io.AddMouseButtonEvent(0, true);
            frame("Animation");
            io.AddMousePosEvent(key.x + 40, key.y + 15);
            frame("Animation");
            io.AddMouseButtonEvent(0, false);
            frame("Animation");
            auto &moved = s.project.findClip(video)->params["opacity"].keys[1];
            require(moved.time > 6 && std::abs(moved.time * 30 - std::round(moved.time * 30)) < 1e-6 &&
                        moved.value < .65f && s.dirty,
                    "Dragging a key moves it later on a whole frame and lowers its value");
            require(s.history.undo(s.project) && s.project.json().Dump(0) == curves && !s.history.undo(s.project),
                    "A curve key drag is one undo step");
            // Ctrl+A, Ctrl+C then Ctrl+V pastes the keys again from the playhead, keeping their spacing.
            auto chord = [&](ImGuiKey keyName) {
                io.AddKeyEvent(ImGuiMod_Ctrl, true);
                io.AddKeyEvent(keyName, true);
                frame("Animation");
                io.AddKeyEvent(keyName, false);
                io.AddKeyEvent(ImGuiMod_Ctrl, false);
                frame("Animation");
            };
            auto copied = s.project.findClip(video)->params["opacity"].keys;
            chord(ImGuiKey_A);
            chord(ImGuiKey_C);
            double timeBefore = s.time;
            s.time = copied.back().time + 1;
            chord(ImGuiKey_V);
            auto &pastedKeys = s.project.findClip(video)->params["opacity"].keys;
            size_t n = copied.size();
            bool spaced = pastedKeys.size() == 2 * n;
            for (size_t i = 0; spaced && i < n; ++i)
                spaced = std::abs(pastedKeys[n + i].time - (s.time + copied[i].time - copied[0].time)) < 1. / 30 + 1e-6 &&
                         pastedKeys[n + i].value == copied[i].value;
            s.time = timeBefore;
            require(spaced && s.dirty && s.history.undo(s.project) && s.project.json().Dump(0) == curves,
                    "Copied curve keys paste at the playhead with their spacing, in one undo step");
            s.dirty = false;
            // The markers panel lists the sequence's markers without editing them.
            auto unmarked = s.project;
            sf::Json mark = sf::Json::Object();
            mark["time"] = 1.5;
            mark["name"] = "Chapitre";
            sf::Protocol(s).execute("add_marker", mark);
            auto marked = s.project.json().Dump(0);
            editor.openPanel("Markers");
            for (int i = 0; i < 4; ++i)
                frame("Markers");
            auto *markerWindow = ImGui::FindWindowByName("Markers");
            require(markerWindow && markerWindow->Active && s.project.json().Dump(0) == marked && !editor.hasError(),
                    "The markers panel opens on the sequence's markers without editing them");
            // The History panel names each step and returns to any of them.
            mark["time"] = 2.5;
            sf::Protocol(s).execute("add_marker", mark);
            auto twice = s.project.json().Dump(0);
            auto steps = sf::Protocol(s).execute("get_history", sf::Json::Object());
            editor.openPanel("History");
            for (int i = 0; i < 4; ++i)
                frame("History");
            int depth = int(s.history.undoSteps().size());
            editor.goToHistoryStep(depth - 2);
            bool back = s.project.json().Dump(0) == unmarked.json().Dump(0);
            editor.goToHistoryStep(depth);
            require(steps["past"].Size() >= 2 && steps["past"].At(steps["past"].Size() - 1).S() == "New Marker" &&
                        back && s.project.json().Dump(0) == twice && ImGui::FindWindowByName("History")->Active &&
                        !editor.hasError(),
                    "The History panel names the steps and returns to an earlier one and back");
            {
                // The Subtitles panel lists the cues of the track it finds, without editing them.
                auto unsubtitled = s.project;
                s.project.importSubtitles(sf::parseSrt("1\n00:00:00,500 --> 00:00:02,000\nUne réplique\n"), "Subtitles");
                auto subtitledJson = s.project.json().Dump(0);
                editor.openPanel("Subtitles");
                for (int i = 0; i < 4; ++i)
                    frame("Subtitles");
                auto *subtitleWindow = ImGui::FindWindowByName("Subtitles");
                require(subtitleWindow && subtitleWindow->Active && s.project.json().Dump(0) == subtitledJson &&
                            !editor.hasError(),
                        "The Subtitles panel opens on the subtitle track without editing it");
                s.project = unsubtitled;
            }
            {
                // Loudness is measured on a worker thread while the editor keeps drawing, then applied to the master.
                auto unmeasured = s.project;
                sf::Project song;
                song.duration = 3;
                sf::Clip music;
                music.id = song.allocate();
                music.source = sx::Narrow(std::filesystem::absolute(root / "examples/firstlight.wav").wstring());
                music.duration = 3;
                song.addTrack(sf::TrackKind::Audio).clips = {music};
                s.project = song;
                s.history.clear();
                editor.measureLoudness(-16);
                int drawn = 0;
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
                while (editor.measuringLoudness() && std::chrono::steady_clock::now() < deadline) {
                    frame();
                    ++drawn;
                }
                require(!editor.measuringLoudness() && drawn > 0 && s.project.master != 0 && !editor.hasError() &&
                            s.history.undo(s.project) && s.project.master == 0,
                        "Loudness is measured in the background while drawing, then applied in one undo step");
                s.project = unmeasured;
                s.history.clear();
            }
            {
                // Silences and shots are analysed on a worker thread too, then applied as one command each.
                auto unanalysed = s.project;
                auto waitAnalysis = [&] {
                    int frames = 0;
                    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
                    while (editor.analysing() && std::chrono::steady_clock::now() < until) {
                        frame();
                        ++frames;
                    }
                    return frames;
                };
                sf::Project song;
                song.duration = 3;
                sf::Clip music;
                music.id = song.allocate();
                music.source = sx::Narrow(std::filesystem::absolute(root / "examples/firstlight.wav").wstring());
                music.duration = 3;
                song.addTrack(sf::TrackKind::Audio).clips = {music};
                s.project = song;
                s.history.clear();
                auto quietBefore = s.project.json().Dump(0);
                editor.removeSilences(music.id, -30, .3);
                int silenceFrames = waitAnalysis();
                bool silenceStep = s.history.undoSteps().size() == 1;
                if (silenceStep)
                    s.history.undo(s.project);
                require(!editor.analysing() && silenceFrames > 0 && !editor.hasError() &&
                            s.project.json().Dump(0) == quietBefore,
                        "Silence removal is analysed in the background while drawing, one undo step at most");
                sf::Project shots;
                shots.duration = 2;
                sf::Clip footage;
                footage.id = shots.allocate();
                footage.source = sx::Narrow(std::filesystem::absolute(root / "test-output/demo.mp4").wstring());
                footage.duration = 2;
                shots.addTrack(sf::TrackKind::Video).clips = {footage};
                s.project = shots;
                s.history.clear();
                // The lower third dialog: a name typed then Enter adds the band at the chosen time (from the timeline,
                // brought to the front first: in this mode the Exports tab covers it).
                frame("Timeline");
                editor.openLowerThirdDialog(.5);
                frame();
                frame();
                io.AddInputCharactersUTF8("Alex Durand");
                frame();
                io.AddKeyEvent(ImGuiKey_Enter, true);
                frame();
                io.AddKeyEvent(ImGuiKey_Enter, false);
                frame();
                bool banded = false;
                for (auto &track : s.project.tracks)
                    for (auto &clip : track.clips)
                        banded = banded || (clip.title.enabled && clip.title.text == "Alex Durand" &&
                                            std::abs(clip.start - .5) < 1e-9);
                require(banded, "The lower third dialog adds a band with the typed name on Enter");
                // Skimming a bin card decodes the frame under the pointer, throttled between moves.
                bool skimmed = editor.skim(footage.source, .5);
                double halfway = editor.skimmedTime();
                for (int i = 0; i < 4; ++i)
                    frame();
                editor.skim(footage.source, .9);
                double further = editor.skimmedTime();
                require(skimmed && halfway > 0 && further > halfway, "Skimming a video card decodes the frame under the pointer");
                // The GIF of the menu renders on the editor's worker, then reports in the status bar.
                auto gifPath = std::filesystem::current_path() / "test-output/editor/menu.gif";
                std::filesystem::create_directories(gifPath.parent_path());
                std::filesystem::remove(gifPath);
                s.project.workIn = 0;
                s.project.workOut = .5;
                editor.exportGif(sx::Narrow(gifPath.wstring()));
                int gifFrames = waitAnalysis();
                require(gifFrames > 0 && !editor.hasError() && std::filesystem::exists(gifPath) &&
                            editor.lastNotice().starts_with("GIF exported"),
                        "The menu's GIF export renders in the background and reports when the file is written");
                s.project.workOut = 0;
                editor.detectScenes(footage.id, false);
                int sceneFrames = waitAnalysis();
                require(!editor.analysing() && sceneFrames > 0 && !editor.hasError(),
                        "Shot detection reads the video in the background while the editor keeps drawing");
                s.project = unanalysed;
                s.history.clear();
            }
            s.project = unmarked;
            s.history.clear();
            s.dirty = false;
            {
                // Pre-render: the work area is rendered in memory, played from it, and dropped after an edit.
                auto unrendered = s.project;
                s.project.workIn = 0;
                s.project.workOut = .5;
                editor.startRenderCache();
                auto &cache = editor.renderCacheState();
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
                while (cache.rendering() && std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                bool complete = cache.frameCount() == 15 && cache.framesDone() == 15;
                s.time = .2;
                s.playing = true;
                frame();
                bool fromCache = editor.showingCachedFrame();
                s.playing = false;
                frame();
                sf::Json renamed = sf::Json::Object();
                renamed["clip"] = s.project.tracks[0].clips[0].id;
                renamed["name"] = "Renommé";
                sf::Protocol(s).execute("set_clip", renamed);
                frame();
                require(complete && fromCache && cache.empty() && !editor.hasError(),
                        "The work area pre-renders in memory, plays from it, and is dropped after an edit");
                s.project = unrendered;
                s.history.clear();
                s.dirty = false;
            }
        }
        {
            // Source monitor: a medium opened from the bin overwrites the sequence at the playhead.
            auto wav = sx::Narrow(std::filesystem::absolute(root / "examples/firstlight.wav").wstring());
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (s.media.get(wav).loading && std::chrono::steady_clock::now() < deadline) {
                s.media.poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            auto restore = s.project;
            s.time = 1;
            // A second audio track becomes the target of source edits from its header button.
            sf::Json audioTrack = sf::Json::Object();
            audioTrack["kind"] = "audio";
            auto targetId = sf::Protocol(s).execute("add_track", audioTrack)["track"].U();
            sf::Json top = sf::Json::Object();
            top["track"] = targetId;
            top["index"] = 0;
            sf::Protocol(s).execute("move_track", top); // visible at the top of the timeline
            int targetIndex = 0;
            for (int i = 0; i < 3; ++i)
                frame("Timeline");
            ImVec2 targetButton{editor.timelineHeaderLeft() + 12 + 4 * 25 + 11,
                                editor.timelineTrackTop(targetIndex) +
                                    std::min((editor.timelinePoint(0, targetIndex).y - editor.timelineTrackTop(targetIndex)) / .6f - 21, 26.f) + 8};
            io.AddMousePosEvent(targetButton.x, targetButton.y);
            frame("Timeline");
            io.AddMouseButtonEvent(0, true);
            frame("Timeline");
            io.AddMouseButtonEvent(0, false);
            frame("Timeline");
            s.history.clear();
            auto before = s.project.json().Dump(0);
            editor.openSource(wav);
            for (int i = 0; i < 4; ++i)
                frame("Source");
            require(ImGui::FindWindowByName("Source") && ImGui::FindWindowByName("Source")->Active &&
                        s.project.json().Dump(0) == before && !editor.hasError(),
                    "Opening a medium in the source monitor leaves the sequence untouched");
            io.AddKeyEvent(ImGuiKey_Period, true);
            frame("Source");
            io.AddKeyEvent(ImGuiKey_Period, false);
            frame("Source");
            const sf::Clip *placed = nullptr;
            for (auto &track : s.project.tracks)
                for (auto &clip : track.clips)
                    if (clip.source == wav && std::abs(clip.start - 1) < 1e-9)
                        placed = &clip;
            require(placed && placed->in == 0 && std::abs(placed->duration - s.media.get(wav).duration) < 1e-6 &&
                        s.project.owner(placed->id)->id == targetId && !editor.hasError(),
                    "The period key overwrites the marked medium at the playhead, on the target track");
            {
                // MCP replace_source with a real medium: refused when too short, then applied from a later entry.
                auto placedId = placed->id;
                auto edited = s.project.json().Dump(0);
                double length = s.media.get(wav).duration;
                sf::Json swap = sf::Json::Object();
                swap["clip"] = placedId;
                swap["path"] = wav;
                swap["in"] = length * .5;
                bool refused = false;
                try {
                    sf::Protocol(s).execute("replace_source", swap);
                } catch (const std::exception &) {
                    refused = true;
                }
                sf::Json shorter = sf::Json::Object();
                shorter["clip"] = placedId;
                shorter["duration"] = length * .25;
                sf::Protocol(s).execute("set_clip", shorter);
                sf::Protocol(s).execute("replace_source", swap);
                auto *swapped = s.project.findClip(placedId);
                bool applied = swapped->source == wav && std::abs(swapped->in - length * .5) < 1e-9 &&
                               std::abs(swapped->duration - length * .25) < 1e-9;
                require(refused && applied && s.history.undo(s.project) && s.history.undo(s.project) &&
                            s.project.json().Dump(0) == edited,
                        "MCP replace_source refuses a medium too short and replaces from a later entry");
                placed = s.project.findClip(placedId); // undo replaced the project the pointer pointed into
            }
            {
                // Match frame (F) reopens a clip's medium in Source with the clip's span marked.
                auto placedId = placed->id;
                double clipIn = placed->in, clipEnd = placed->in + placed->duration * placed->speed;
                editor.openSource(sx::Narrow(std::filesystem::absolute(root / "test-output/demo.mp4").wstring()));
                s.selectClip(placedId);
                s.time = 3;
                for (int i = 0; i < 2; ++i)
                    frame("Timeline");
                io.AddKeyEvent(ImGuiKey_F, true);
                frame("Timeline");
                io.AddKeyEvent(ImGuiKey_F, false);
                frame("Timeline");
                require(editor.sourceMedia() == wav && std::abs(editor.sourceMarks().first - clipIn) < 1e-9 &&
                            std::abs(editor.sourceMarks().second - clipEnd) < 1e-6,
                        "Match frame opens the selected clip's medium in Source with its span marked");
                // The out mark is a handle on the scrub bar.
                for (int i = 0; i < 2; ++i)
                    frame("Source");
                auto [barMin, barSize] = editor.sourceScrubBar();
                double length = s.media.get(wav).duration;
                ImVec2 handle{barMin.x + float(clipEnd / length) * barSize.x - 2, barMin.y + barSize.y * .5f};
                io.AddMousePosEvent(handle.x, handle.y);
                frame("Source");
                io.AddMouseButtonEvent(0, true);
                frame("Source");
                io.AddMousePosEvent(barMin.x + barSize.x * .5f, handle.y);
                frame("Source");
                io.AddMouseButtonEvent(0, false);
                frame("Source");
                require(std::abs(editor.sourceMarks().second - length * .5) < length / barSize.x * 2 + 1. / 30 &&
                            std::abs(editor.sourceMarks().first - clipIn) < 1e-9,
                        "Dragging the out handle on the Source scrub bar moves the out point");
            }
            require(s.history.undo(s.project) && s.project.json().Dump(0) == before,
                    "A source edit is one undo step");
            {
                // Fit to fill (Maj+. in Source): the marked span fills the work area, its speed changed.
                s.project.workIn = 1;
                s.project.workOut = 2;
                auto [markIn, markOut] = editor.sourceMarks();
                for (int i = 0; i < 2; ++i)
                    frame("Source");
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(ImGuiKey_Period, true);
                frame("Source");
                io.AddKeyEvent(ImGuiKey_Period, false);
                io.AddKeyEvent(ImGuiMod_Shift, false);
                frame("Source");
                const sf::Clip *fitted = nullptr;
                for (auto &track : s.project.tracks)
                    for (auto &clip : track.clips)
                        if (clip.source == wav && std::abs(clip.start - 1) < 1e-9 && std::abs(clip.duration - 1) < 1e-9)
                            fitted = &clip;
                require(fitted && std::abs(fitted->speed - (markOut - markIn)) < 1e-6 && std::abs(fitted->in - markIn) < 1e-9,
                        "Fit to fill from Source covers the work area with the marked span at the matching speed");
            }
            s.project = restore;
            s.history.clear();
            s.dirty = false;
        }
        s.dirty = false;
        auto snapshot = s.project;
        snapshot.width = 320;
        snapshot.height = 180;
        snapshot.duration = 600;
        auto outputs = std::filesystem::current_path() / "test-output/editor";
        std::filesystem::create_directories(outputs);
        auto job = s.exports.start(snapshot, s.renderer.library, sf::ExportKind::Video,
                                   sx::Narrow((outputs / "background.mp4").wstring()));
        editor.setMode(sf::Editor::Mode::Deliver);
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
        editor.setMode(sf::Editor::Mode::Audio);
        for (int i = 0; i < 4; ++i)
            frame("Mixer & Audio Graph");
        auto *details = canvas("Mixer & Audio Graph", "CyDAWMixerStrips");
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
        editor.setMode(sf::Editor::Mode::Color);
        original = s.project.json().Dump(0);
        for (int i = 0; i < 4; ++i)
            frame("Color Grading");
        auto *gradeCanvas = canvas("Color Grading", "Grade graph");
        require(gradeCanvas && !gradeCanvas->Hidden && s.project.json().Dump(0) == original && !s.dirty &&
                    !editor.hasError(),
                "Opening grading tools and the actual correction graph leaves the project unchanged");
        click(gradeCanvas->DC.CursorStartPos + ImVec2(127 + 75, 22));
        require(s.project.json().Dump(0) == original && !s.history.undo(s.project) && !editor.hasError(),
                "Selecting a grading node does not create an undo entry or mutate its parameters");
        for (int i = 0; i < 4; ++i)
            frame("Video Scopes");
        require(s.project.json().Dump(0) == original && !s.dirty && !editor.hasError(),
                "Video scopes analyze a real GPU frame without changing the document");
        s.project = sf::Project::load(sx::Narrow((root / "examples/LayerGraphStudy.sframe").wstring()));
        s.time = 4;
        s.history.clear();
        s.dirty = false;
        original = s.project.json().Dump(0);
        editor.setMode(sf::Editor::Mode::Effects);
        for (int i = 0; i < 4; ++i)
            frame("Layer Graph");
        auto *layers = canvas("Layer Graph", "ShaderixelComposition");
        require(layers && !layers->Hidden && s.project.json().Dump(0) == original &&
                    !s.history.undo(s.project) && !s.dirty && !editor.hasError(),
                "Opening the independent Shaderixel layer graph renders its sources without changing the "
                "document");
        {
            auto at = layers->DC.CursorStartPos + ImVec2(50, 50);
            io.AddMousePosEvent(at.x, at.y);
            frame("Layer Graph");
            auto old = editor.layerGraphZoom();
            io.AddMouseWheelEvent(0, 1);
            frame("Layer Graph");
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
            {
                // Once zoomed, the view moves: Shift + wheel, and a left drag with the Hand tool (H).
                for (int i = 0; i < 3; ++i) {
                    io.AddMouseWheelEvent(0, 1);
                    frame("Composition");
                }
                auto panBefore = editor.previewNavigation().pan;
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddMouseWheelEvent(0, 1);
                frame("Composition");
                io.AddKeyEvent(ImGuiMod_Shift, false);
                frame("Composition");
                auto panWheel = editor.previewNavigation().pan;
                io.AddKeyEvent(ImGuiKey_H, true);
                frame("Composition");
                io.AddKeyEvent(ImGuiKey_H, false);
                frame("Composition");
                io.AddMouseButtonEvent(0, true);
                frame("Composition");
                io.AddMousePosEvent(at.x - 50, at.y - 30);
                frame("Composition");
                io.AddMouseButtonEvent(0, false);
                frame("Composition");
                auto panDrag = editor.previewNavigation().pan;
                io.AddKeyEvent(ImGuiKey_H, true);
                frame("Composition");
                io.AddKeyEvent(ImGuiKey_H, false);
                frame("Composition");
                io.AddMousePosEvent(at.x, at.y);
                frame("Composition");
                require(std::abs(panWheel.x - panBefore.x - 60) < 1 && std::abs(panDrag.x - panWheel.x + 50) < 1 &&
                            std::abs(panDrag.y - panWheel.y + 30) < 1 && s.project.json().Dump(0) == original,
                        "A zoomed final image moves with Shift + wheel and with a Hand tool drag, without editing");
            }
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            io.AddKeyEvent(ImGuiKey_1, true);
            frame("Composition");
            io.AddKeyEvent(ImGuiKey_1, false);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame("Composition");
            auto pixel = editor.previewNavigation().zoom;
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            io.AddKeyEvent(ImGuiKey_0, true);
            frame("Composition");
            io.AddKeyEvent(ImGuiKey_0, false);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame("Composition");
            require(pixel != old && std::abs(editor.previewNavigation().zoom - 1) < 1e-6 &&
                        s.project.json().Dump(0) == original,
                    "Ctrl+1 shows the final image at 100 % and Ctrl+0 fits it again, without editing");
            for (int i = 0; i < 3; ++i)
                frame("Timeline");
            auto *timeline = canvas("Timeline", "timelineCanvas");
            require(timeline != nullptr, "The timeline stays available in the effects mode");
            at = editor.timelineNavigatorPoint(2) + ImVec2(0, 30);
            io.AddMousePosEvent(at.x, at.y);
            frame("Timeline");
            old = editor.timelinePixelsPerSecond();
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            io.AddMouseWheelEvent(0, 1);
            frame("Timeline");
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame("Timeline");
            require(editor.timelinePixelsPerSecond() > old && s.project.json().Dump(0) == original,
                    "Ctrl + wheel zooms the timeline without creating a project edit");
            at = editor.timelineNavigatorPoint(3);
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
        editor.openPanel("Motion Design");
        for (int i = 0; i < 4; ++i)
            frame("Motion Design");
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
            frame("Clip Editor");
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

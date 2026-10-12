#include "ui/Editor.h"
#include <imgui_internal.h>
namespace sf {
Project Editor::projectWithClipDrafts() const {
    auto project = s.project;
    if (clipWorkspace) {
        if (auto *c = project.findClip(editedContainer); c && !c->composition.IsNull())
            c->composition = clipWorkspace->projectWithClipDrafts().json();
    }
    return project;
}
bool Editor::applyClipDrafts() {
    if (!clipWorkspace)
        return true;
    if (!clipWorkspace->applyClipDrafts())
        return false;
    if (!clipSession->dirty)
        return true;
    Json args = Json::Object();
    args["clip"] = editedContainer;
    args["project"] = clipSession->project.json();
    if (!action("set_clip_composition", args))
        return false;
    clipSession->dirty = false;
    return true;
}
void Editor::openClipEditor(Id id, bool discard) {
    if (!discard && hasClipDrafts() && !applyClipDrafts())
        return;
    auto *clip = s.project.findClip(id);
    if (!clip || s.project.owner(id)->kind != TrackKind::Video) {
        error = "L'éditeur de clip exige un clip vidéo";
        return;
    }
    Json args = Json::Object();
    args["clip"] = id;
    if (clip->composition.IsNull() && !action("make_clip_container", args))
        return;
    clip = s.project.findClip(id);
    clipWorkspace.reset();
    clipSession = std::make_unique<Session>();
    clipSession->project = Project::fromJson(clip->composition);
    clipSession->renderer.initialize(s.renderer.gpu(), s.renderer.gpuContext());
    clipSession->renderer.library = s.renderer.library;
    clipSession->time = std::clamp(clip->sourceTime(s.time), 0., clipSession->project.duration);
    editedContainer = id;
    for (auto &t : clipSession->project.tracks)
        if (t.kind == TrackKind::Video && !t.clips.empty()) {
            clipSession->selectClip(t.clips.front().id);
            break;
        }
    clipWorkspace = std::make_unique<Editor>(*clipSession, root);
    clipWorkspace->panelSuffix = panelSuffix + "##clip:" + std::to_string(id);
    for (auto &[name, open] : clipWorkspace->panels)
        open = name == "Composition" || name == "Timeline" || name == "Graphe des calques" ||
               name == "Graphe VFX" || name == "Propriétés" || name == "Médias & effets" ||
               name == "Animations";
    s.playing = false;
    clipDetached = true;
    clipDetachRequested = true;
    openPanel("Éditeur de clip");
}
void Editor::drawEmbedded(ImGuiID dock, ImVec2 size) {
    bool first = !layoutReady;
    if (first) {
        layoutReady = true;
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, {std::max(300.f, size.x), std::max(200.f, size.y)});
        ImGuiID centre = dock, left, right, bottom;
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, .2f, &left, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, .28f, &right, &centre);
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, .45f, &bottom, &centre);
        for (auto name : {"Médias & effets", "Bibliothèque Shaderixel"})
            ImGui::DockBuilderDockWindow(panelTitle(name).c_str(), left);
        ImGui::DockBuilderDockWindow(panelTitle("Composition").c_str(), centre);
        for (auto name : {"Propriétés", "Calques", "Étalonnage", "Scopes vidéo"})
            ImGui::DockBuilderDockWindow(panelTitle(name).c_str(), right);
        for (auto name : {"Timeline", "Graphe des calques", "Graphe VFX", "HLSL", "Animations", "Graphe DSP",
                          "Motion design"})
            ImGui::DockBuilderDockWindow(panelTitle(name).c_str(), bottom);
        ImGui::DockBuilderFinish(dock);
    }
    if (s.playing) {
        if (!wasPlaying) {
            audio.start(s.time);
            wasPlaying = true;
        }
        audio.pump(s.project, s.media);
        s.time = audio.playing() ? audio.time() : s.time + ImGui::GetIO().DeltaTime;
        if (s.time >= s.project.duration) {
            s.time = 0;
            s.playing = loop;
            audio.stop();
            wasPlaying = false;
        }
    } else {
        audio.stop();
        wasPlaying = false;
    }
    try {
        s.renderer.render(s.project, s.media, s.time, std::min(1280, s.project.width),
                          std::max(1, s.project.height * std::min(1280, s.project.width) / s.project.width));
    } catch (const std::exception &e) {
        error = e.what();
    }
    clipEditor();
    motionPanel();
    shaderixelLibrary();
    library();
    preview();
    layers();
    inspector();
    timeline();
    animations();
    shaderEditor();
    graphEditor();
    layerGraphEditor();
    gradingPanel();
    scopesPanel();
    mixer();
    dspPanel();
    integrations();
    exportPanel();
    if (first)
        ImGui::SetWindowFocus(panelTitle("Timeline").c_str());
    if (!pendingPanelFocus.empty()) {
        ImGui::SetWindowFocus(pendingPanelFocus.c_str());
        pendingPanelFocus.clear();
    }
    s.pollJobs();
    try {
        s.syncPluginEditors();
    } catch (const std::exception &e) {
        error = e.what();
    }
}
void Editor::clipEditor() {
    if (!panels["Éditeur de clip"])
        return;
    auto *clip = s.project.findClip(editedContainer);
    if (clipWorkspace && (!clip || clip->composition.IsNull())) {
        clipWorkspace.reset();
        clipSession.reset();
        editedContainer = 0;
    }
    if (clipDetached) {
        ImGuiWindowClass windowClass;
        windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&windowClass);
    }
    if (clipDetachRequested) {
        auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            {std::min(1200.f, viewport->WorkSize.x - 60), std::min(850.f, viewport->WorkSize.y - 60)},
            ImGuiCond_Always);
        ImGui::SetNextWindowPos(viewport->WorkPos + ImVec2(30, 30), ImGuiCond_FirstUseEver);
        clipDetachRequested = false;
    }
    bool visible = ImGui::Begin(panelTitle("Éditeur de clip").c_str(), &panels["Éditeur de clip"]);
    ImGuiID dock = ImGui::GetID("ClipDocumentDock");
    ImVec2 dockSize = ImGui::GetContentRegionAvail();
    if (visible) {
        if (ImGui::Button("Ouvrir le clip sélectionné") && s.selected)
            openClipEditor(s.selected);
        if (clipWorkspace) {
            ImGui::SameLine();
            if (ImGui::Button("Fenêtre séparée")) {
                clipDetached = true;
                clipDetachRequested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Appliquer au conteneur")) {
                applyClipDrafts();
            }
            ImGui::SameLine();
            if (ImGui::Button("Recharger"))
                openClipEditor(editedContainer, true);
            ImGui::SameLine();
            if (ImGui::BeginCombo("Fenêtres du clip", "Afficher…")) {
                for (auto &[name, open] : clipWorkspace->panels)
                    if (ImGui::Selectable(name.c_str(), open))
                        clipWorkspace->openPanel(name);
                ImGui::EndCombo();
            }
            if (ImGui::SmallButton("Temps shader local depuis zéro")) {
                clipSession->history.checkpoint(clipSession->project);
                clipSession->project.timeOrigin = 0;
                clipSession->dirty = true;
            }
            ImGui::Text("Clip %u · temps local · %s", editedContainer,
                        clipSession->dirty ? "brouillon modifié, appliquer pour le rendu final"
                                           : "contenu enregistré");
            if (clipWorkspace->hasError())
                ImGui::TextWrapped("%s", clipWorkspace->error.c_str());
            dockSize = ImGui::GetContentRegionAvail();
            ImGui::DockSpace(dock, {0, 0});
        } else
            ImGui::TextWrapped(
                "Un clip conteneur possède sa timeline vidéo/audio, ses effets et son graphe de calques. "
                "Ouvrez un clip par double clic, puis appliquez ses modifications.");
    } else if (clipWorkspace)
        ImGui::DockSpace(dock, {0, 0}, ImGuiDockNodeFlags_KeepAliveOnly);
    ImGui::End();
    if (clipWorkspace && panels["Éditeur de clip"])
        clipWorkspace->drawEmbedded(dock, dockSize);
}
void Editor::dspPanel() {
    if (!panels["Graphe DSP"] || !beginPanel("Graphe DSP"))
        return;
    if (ImGui::BeginCombo("Canal", mixScope == "master" ? "MASTER" : mixScope.c_str())) {
        if (ImGui::Selectable("MASTER", mixScope == "master")) {
            mixScope = "master";
            mixTarget = 0;
        }
        for (auto &t : s.project.tracks)
            if (t.kind == TrackKind::Audio &&
                ImGui::Selectable(t.name.c_str(), mixScope == "track" && mixTarget == t.id)) {
                mixScope = "track";
                mixTarget = t.id;
            }
        for (auto &b : s.project.buses)
            if (ImGui::Selectable(b.name.c_str(), mixScope == "bus" && mixTarget == b.id)) {
                mixScope = "bus";
                mixTarget = b.id;
            }
        if (auto *c = s.project.findClip(s.selected);
            c && s.project.owner(c->id)->kind == TrackKind::Audio &&
            ImGui::Selectable(("Clip / " + c->name).c_str(), mixScope == "clip")) {
            mixScope = "clip";
            mixTarget = c->id;
        }
        ImGui::EndCombo();
    }
    MixChannel *mix = &s.project.masterMix;
    if (mixScope == "track") {
        auto *t = s.project.findTrack(mixTarget);
        if (t && t->kind == TrackKind::Audio)
            mix = &t->mix;
        else {
            mixScope = "master";
            mixTarget = 0;
        }
    }
    if (mixScope == "bus") {
        auto *b = s.project.findBus(mixTarget);
        if (b)
            mix = &b->mix;
        else {
            mixScope = "master";
            mixTarget = 0;
        }
    }
    auto *chain = &mix->effects;
    if (mixScope == "clip") {
        auto *c = s.project.findClip(mixTarget);
        if (c && s.project.owner(c->id)->kind == TrackKind::Audio)
            chain = &c->effects;
        else {
            mixScope = "master";
            mixTarget = 0;
        }
    }
    if (channelDspIndex >= int(chain->size()) ||
        (channelDspIndex >= 0 && (*chain)[channelDspIndex].id != "cydaw.graph"))
        channelDspIndex = -1;
    if (channelDspIndex < 0)
        for (int i = 0; i < int(chain->size()); ++i)
            if ((*chain)[i].id == "cydaw.graph") {
                channelDspIndex = i;
                break;
            }
    if (ImGui::BeginCombo("Instance", channelDspIndex < 0
                                          ? "Nouveau graphe"
                                          : ("DSP #" + std::to_string(channelDspIndex + 1)).c_str())) {
        for (int i = 0; i < int(chain->size()); ++i)
            if ((*chain)[i].id == "cydaw.graph" &&
                ImGui::Selectable(("DSP #" + std::to_string(i + 1)).c_str(), channelDspIndex == i))
                channelDspIndex = i;
        ImGui::EndCombo();
    }
    int index = channelDspIndex;
    Json graph;
    if (index >= 0)
        graph = (*chain)[index].graph;
    else
        Json::Parse(R"({"schema":2,"nodes":[],"connections":[{"source":0,"destination":0}]})", graph);
    auto identity = mixScope + std::to_string(mixTarget) + graph.Dump(0);
    if (identity != channelDspIdentity) {
        channelDsp.open(graph);
        channelDspIdentity = identity;
        channelDspDirty = false;
    }
    if (ImGui::Button(index < 0 ? "Ajouter ce graphe à la chaîne" : "Compiler et appliquer")) {
        Json args = Json::Object();
        args["scope"] = mixScope;
        if (mixScope == "clip")
            args["clip"] = mixTarget;
        else if (mixScope != "master")
            args["target"] = mixTarget;
        if (index >= 0)
            args["effect"] = index;
        args["graph"] = channelDsp.document();
        if (action("set_dsp_graph", args)) {
            channelDspIdentity.clear();
            channelDspDirty = false;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("CyDAW v2 · audio stéréo · %s",
                        channelDspDirty ? "brouillon à appliquer" : "signal enregistré");
    channelDspDirty |= channelDsp.draw(s.time);
    ImGui::End();
}
} // namespace sf

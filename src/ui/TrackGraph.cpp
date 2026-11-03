// Track Graph: the effect graph of one video track, as Shaderixel's composition view. The track's
// clips come in as a surface, effect nodes process it, the output goes on to the track blending.
#include "render/Grading.h"
#include "ui/Editor.h"
#include "ui/FrameStyle.h"
#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sf {
using namespace style;

void Editor::openTrackGraph(Id track) {
    graphTrack = track;
    openPanel("Track Graph");
}

void Editor::trackGraphEditor() {
    if (!panels["Track Graph"] || !beginPanel("Track Graph"))
        return;
    auto &p = s.project;
    // The graph follows the selected clip's video track until one is picked.
    if (s.selected != graphFollowed) {
        graphFollowed = s.selected;
        if (auto *owner = p.findClip(s.selected) ? p.owner(s.selected) : nullptr; owner && owner->kind == TrackKind::Video)
            graphTrack = owner->id;
    }
    auto *track = p.findTrack(graphTrack);
    if (!track || track->kind != TrackKind::Video) {
        track = nullptr;
        for (auto &t : p.tracks)
            if (t.kind == TrackKind::Video) {
                track = &t;
                graphTrack = t.id;
                break;
            }
    }
    if (!track) {
        ImGui::TextDisabled(cyui::tr("Add a video track to give it effects.").c_str());
        ImGui::End();
        return;
    }
    try {
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##graphTrack", track->name.c_str())) {
            for (auto &t : p.tracks)
                if (t.kind == TrackKind::Video &&
                    ImGui::Selectable((t.name + "##graphTrack" + std::to_string(t.id)).c_str(), t.id == graphTrack))
                    graphTrack = t.id;
            ImGui::EndCombo();
        }
        tooltip("Video track whose effects are shown; selecting a clip shows its track");
        ImGui::SameLine();
        std::string added;
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("##trackEffectAdd", "+ Add Effect to Track")) {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##trackEffectSearch", "Search", &trackEffectSearch);
            for (auto &def : s.renderer.library.definitions) {
                if (def.kind != "effect" || def.generator)
                    continue;
                auto label = def.category + " / " + def.name;
                if (!trackEffectSearch.empty() && label.find(trackEffectSearch) == std::string::npos)
                    continue;
                if (ImGui::Selectable((label + "##" + def.id).c_str()))
                    added = def.id;
            }
            ImGui::EndCombo();
        }
        tooltip("Append an effect at the end of the track's chain (or drop one from the library on the graph)");
        ImGui::SameLine();
        ImGui::BeginDisabled(track->graph.IsNull());
        if (ImGui::Button(cyui::trLabel("Remove Track Effects").c_str())) {
            Json a = Json::Object();
            a["track"] = track->id;
            a["graph"] = Json();
            action("set_track_graph", a);
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        ImGui::EndDisabled();
        if (track->graph.IsNull()) {
            ImGui::SameLine();
            ImGui::TextDisabled(cyui::tr("No effect yet: the track goes straight to the blending.").c_str());
        }
        if (!added.empty()) {
            Json a = Json::Object();
            a["track"] = track->id;
            a["id"] = added;
            action("add_track_effect", a);
            ImGui::End();
            return;
        }
        auto draft = track->graph.IsNull() ? defaultTrackGraph(p, track->id) : track->graph;
        trackCanvas.track = track->id;
        Id shown = track->id;
        bool edited = trackCanvas.draw(draft, p, s.renderer, s.time);
        // Effects dropped from the library anywhere on the panel land at the end of the chain.
        if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->InnerRect, ImGui::GetID("trackGraphDrop"))) {
            if (auto *payload = ImGui::AcceptDragDropPayload("EFFECT")) {
                Json a = Json::Object();
                a["track"] = shown;
                a["id"] = static_cast<const char *>(payload->Data);
                action("add_track_effect", a);
                edited = false;
            }
            ImGui::EndDragDropTarget();
        }
        if (edited) {
            Json a = Json::Object();
            a["track"] = shown;
            a["graph"] = draft;
            action("set_track_graph", a);
        }
        if (auto node = trackCanvas.takeLutRequest()) {
            auto path = dialog(false, L"1D / 2D / 3D LUT\0*.cube;*.png;*.tif;*.exr\0All\0*.*\0\0", nullptr);
            if (auto *t = p.findTrack(shown); t && !path.empty() && !t->graph.IsNull()) {
                auto lut = loadLut(sx::Utf8Path(path), trackCanvas.lutLayoutName());
                auto graph = t->graph;
                auto nodes = Json::Array();
                for (auto item : graph["nodes"].Elements()) {
                    if (item["id"].U() == node)
                        item["effect"]["grading"]["lut"] = lut;
                    nodes.Push(item);
                }
                graph["nodes"] = nodes;
                Json a = Json::Object();
                a["track"] = shown;
                a["graph"] = graph;
                action("set_track_graph", a);
            }
        }
        if (auto node = trackCanvas.takeShaderRequest()) {
            layerShaderNode = node;
            layerShaderTrack = shown;
            openPanel("Shader Graph");
        }
    } catch (const std::exception &e) {
        error = e.what();
        ImGui::TextWrapped(cyui::tr("%s").c_str(), error.c_str());
    }
    ImGui::End();
}
} // namespace sf

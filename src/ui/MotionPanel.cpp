#include "core/Motion.h"
#include "ui/Editor.h"
#include <imgui_stdlib.h>
namespace sf {
void Editor::motionPanel() {
    if (!panels["Motion Design"] || !beginPanel("Motion Design"))
        return;
    auto commit = [&](const Project &before) {
        try {
            s.project.validate();
            s.history.checkpoint(before, "Motion Design");
            s.dirty = true;
            s.renderer.clearHistory();
            error.clear();
        } catch (const std::exception &e) {
            s.project = before;
            error = e.what();
        }
    };
    auto add = [&](const std::string &type) {
        auto before = s.project;
        auto parent = s.project.findClip(s.selected);
        Id parentId = parent && !parent->widget.IsNull() ? parent->id : 0;
        std::string image;
        if (type == "image") {
            image = dialog(false, L"Image / Video\0*.png;*.jpg;*.tif;*.exr;*.mp4;*.mov\0\0", nullptr);
            if (image.empty())
                return;
        }
        auto &track = s.project.addTrack(TrackKind::Video);
        track.name = "Widget " + type;
        Clip c;
        c.id = s.project.allocate();
        c.name = track.name;
        c.start = 0;
        c.duration = s.project.duration;
        c.widget = Json::Object();
        c.widget["type"] = type;
        c.widget["parent"] = parentId;
        c.widget["anchorX"] = .5;
        c.widget["anchorY"] = .5;
        c.widget["pivotX"] = .5;
        c.widget["pivotY"] = .5;
        c.params["widget_width"] = {type == "group" ? 640.f : 320.f};
        c.params["widget_height"] = {type == "group" ? 360.f : 120.f};
        if (type == "text") {
            c.title.enabled = true;
            c.title.text = "Your Text";
            c.params["font_size"] = {48};
        } else if (type == "image")
            c.source = image;
        else {
            c.generator = true;
            Effect e;
            e.id = "custom";
            e.code = widgetShader(type);
            e.params["fill.0"] = {.14f};
            e.params["fill.1"] = {.45f};
            e.params["fill.2"] = {.9f};
            e.params["fill.3"] = {1};
            c.effects = {e};
        }
        track.clips = {c};
        std::rotate(s.project.tracks.begin(), s.project.tracks.end() - 1, s.project.tracks.end());
        appendLayerSurface(s.project, s.project.tracks.front().id, c.name);
        s.selectClip(c.id);
        commit(before);
    };
    for (auto type : {"rectangle", "ellipse", "text", "image", "group"}) {
        if (ImGui::Button(("+ " + std::string(std::string(type) == "text"    ? "text"
                                              : std::string(type) == "group" ? "group"
                                                                             : type))
                              .c_str()))
            add(type);
        ImGui::SameLine();
    }
    if (ImGui::Button(cyui::trLabel("Fit").c_str()))
        motionView = {};
    ImGui::TextDisabled(cyui::tr("Hierarchy - anchors - pixels - K: key at current time - wheel / middle: view").c_str());
    ImGui::BeginChild("WidgetHierarchy", {230, 0}, true);
    auto tree = [&](auto &&self, Id parent, int depth) -> void {
        if (depth > 64)
            return;
        for (const auto &track : s.project.tracks)
            for (const auto &c : track.clips)
                if (!c.widget.IsNull() && c.widget["parent"].U() == parent) {
                    ImGui::PushID(int(c.id));
                    ImGui::Indent(depth * 8.f);
                    if (ImGui::Selectable(c.name.c_str(), s.selected == c.id))
                        s.selectClip(c.id);
                    ImGui::Unindent(depth * 8.f);
                    ImGui::PopID();
                    self(self, c.id, depth + 1);
                }
    };
    tree(tree, 0, 0);
    ImGui::Separator();
    if (auto *c = s.project.findClip(s.selected); c && !c->widget.IsNull()) {
        const Json layout = c->widget;
        auto before = s.project;
        ImGui::InputText(cyui::trLabel("Name").c_str(), &c->name);
        editCheckpoint(before);
        if (ImGui::BeginCombo(cyui::trLabel("Parent").c_str(), layout["parent"].U() ? "Widget" : "Canvas")) {
            if (ImGui::Selectable(cyui::trLabel("Canvas").c_str())) {
                before = s.project;
                c->widget["parent"] = 0;
                commit(before);
            }
            std::vector<std::pair<Id, std::string>> parents;
            for (auto &t : s.project.tracks)
                for (auto &other : t.clips)
                    if (other.id != c->id && !other.widget.IsNull())
                        parents.emplace_back(other.id, other.name);
            for (auto &[parentId, name] : parents)
                if (ImGui::Selectable(name.c_str(), parentId == layout["parent"].U())) {
                    before = s.project;
                    c->widget["parent"] = parentId;
                    commit(before);
                    c = s.project.findClip(s.selected);
                }
            ImGui::EndCombo();
        }
        float anchor[2] = {layout["anchorX"].F(), layout["anchorY"].F()};
        before = s.project;
        if (ImGui::SliderFloat2(cyui::trLabel("Anchor").c_str(), anchor, 0, 1)) {
            c->widget["anchorX"] = anchor[0];
            c->widget["anchorY"] = anchor[1];
        }
        editCheckpoint(before);
        float pivot[2] = {layout["pivotX"].F(), layout["pivotY"].F()};
        before = s.project;
        if (ImGui::SliderFloat2(cyui::trLabel("Pivot").c_str(), pivot, 0, 1)) {
            c->widget["pivotX"] = pivot[0];
            c->widget["pivotY"] = pivot[1];
        }
        editCheckpoint(before);
        for (auto key : {"widget_x", "widget_y", "widget_width", "widget_height"}) {
            ImGui::PushID(key);
            float fallback = std::string(key) == "widget_width"    ? 320
                             : std::string(key) == "widget_height" ? 120
                                                                   : 0;
            float value = c->param(key, s.time, fallback);
            before = s.project;
            const char *label = std::string(key) == "widget_x"       ? "Position X"
                                : std::string(key) == "widget_y"     ? "Position Y"
                                : std::string(key) == "widget_width" ? "Width"
                                                                     : "Height";
            if (ImGui::DragFloat(label, &value, 1,
                                 std::string(key).find("width") != std::string::npos ||
                                         std::string(key).find("height") != std::string::npos
                                     ? 1.f
                                     : -65536.f,
                                 65536.f, "%.1f px")) {
                auto &p = c->params[key];
                if (p.keys.empty())
                    p.value = value;
                else
                    p.setKey({s.time, value, Curve::Smooth, 0});
            }
            editCheckpoint(before);
            ImGui::SameLine();
            if (ImGui::SmallButton(cyui::trLabel("K").c_str())) {
                before = s.project;
                c->params[key].setKey({s.time, value, Curve::Smooth, 0});
                commit(before);
            }
            ImGui::PopID();
        }
        for (auto key : {"hidden", "clipChildren"}) {
            bool flag = layout[key].B();
            before = s.project;
            if (ImGui::Checkbox(std::string(key) == "hidden" ? "Hide" : "Clip Children", &flag)) {
                c->widget[key] = flag;
                commit(before);
            }
        }
        if (c->title.enabled) {
            before = s.project;
            ImGui::InputTextMultiline(cyui::trLabel("Text").c_str(), &c->title.text, {200, 60});
            editCheckpoint(before);
            parameter("Text Size", "font_size", 4, 1024, 48);
        }
        if (c->generator && !c->effects.empty()) {
            float color[4];
            for (int k = 0; k < 4; ++k) {
                auto it = c->effects[0].params.find("fill." + std::to_string(k));
                color[k] = it == c->effects[0].params.end() ? 1 : it->second.at(s.time);
            }
            before = s.project;
            if (ImGui::ColorEdit4(cyui::trLabel("Color").c_str(), color)) {
                for (int k = 0; k < 4; ++k) {
                    auto &p = c->effects[0].params["fill." + std::to_string(k)];
                    if (p.keys.empty())
                        p.value = color[k];
                    else
                        p.setKey({s.time, color[k], Curve::Smooth, 0});
                }
            }
            editCheckpoint(before);
        }
        if (ImGui::Button(cyui::trLabel("Widget Animation").c_str())) {
            panels["Animation"] = true;
            animationKey = "widget_x";
            openPanel("Animation");
        }
        if (ImGui::Button(cyui::trLabel("Edit Content / VFX").c_str()))
            openClipEditor(c->id);
    } else
        ImGui::TextWrapped(cyui::tr("Add a widget or select one in the hierarchy.").c_str());
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("WidgetCanvas", {0, 0}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    size.x = std::max(1.f, size.x);
    size.y = std::max(1.f, size.y);
    {
    auto &io=ImGui::GetIO();
    float fit=std::min(size.x/s.project.width,size.y/s.project.height);
    ImVec2 margin{(size.x-s.project.width*fit)*.5f,(size.y-s.project.height*fit)*.5f};
    motionCanvas.view.zoom=fit*motionView.zoom;
    motionCanvas.view.origin={-(margin.x+motionView.pan.x)/motionCanvas.view.zoom,-(margin.y+motionView.pan.y)/motionCanvas.view.zoom};
    cyui::CanvasOptions options;options.grid=false;options.minZoom=fit*.125f;options.maxZoom=fit*32;
    cyui::CanvasScope scope("MotionViewport",motionCanvas,size,options);
    bool hover=scope.frame().hovered;
    motionView.zoom=float(motionCanvas.view.zoom/fit);
    motionView.pan={float(-motionCanvas.view.origin.x*motionCanvas.view.zoom-margin.x),float(-motionCanvas.view.origin.y*motionCanvas.view.zoom-margin.y)};
    float scale=float(motionCanvas.view.zoom);
    auto top=motionCanvas.view.screen({0,0},origin);
    auto *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, origin + size, cyui::palette().bg0);
    dl->AddImage((ImTextureID)(intptr_t)s.renderer.view(), top,
                 top + ImVec2(s.project.width * scale, s.project.height * scale));
    Id hit = 0, resizeHit = 0;
    for (auto it = s.project.tracks.rbegin(); it != s.project.tracks.rend(); ++it)
        for (auto &c : it->clips)
            if (!c.widget.IsNull()) {
                auto r = widgetRect(s.project, c, s.time);
                if (!r.visible)
                    continue;
                auto a = top + ImVec2(r.x * scale, r.y * scale),
                     b = a + ImVec2(r.width * scale, r.height * scale);
                if (hover && io.MousePos.x >= a.x && io.MousePos.x <= b.x && io.MousePos.y >= a.y &&
                    io.MousePos.y <= b.y)
                    hit = c.id;
                if (c.id == s.selected) {
                    dl->AddRect(a, b, cyui::palette().accent, 0.f, 2.f, ImDrawFlags_None);
                    dl->AddCircleFilled(a, 4, cyui::palette().accent);
                    dl->AddRectFilled(b - ImVec2(5, 5), b + ImVec2(5, 5), cyui::palette().accent);
                    if (hover && std::abs(io.MousePos.x - b.x) < 10 && std::abs(io.MousePos.y - b.y) < 10) {
                        resizeHit = c.id;
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                    }
                }
            }
    if (resizeHit)
        hit = resizeHit;
    if (hover && hit && !motionCanvas.panning && ImGui::IsMouseClicked(0)) {
        motionResizing = resizeHit != 0;
        s.selectClip(hit);
        motionDragging = hit;
        motionDragOrigin = io.MousePos;
        auto *c = s.project.findClip(hit);
        motionStartX = c->param("widget_x", s.time, 0);
        motionStartY = c->param("widget_y", s.time, 0);
        motionStartWidth = c->param("widget_width", s.time, 320);
        motionStartHeight = c->param("widget_height", s.time, 120);
        motionBefore = s.project;
    }
    if (motionDragging && ImGui::IsMouseDragging(0, 1)) {
        if (auto *c = s.project.findClip(motionDragging)) {
            auto delta = (io.MousePos - motionDragOrigin) / scale;
            for (auto pair : {std::pair{motionResizing ? "widget_width" : "widget_x",
                                        (motionResizing ? motionStartWidth : motionStartX) + delta.x},
                              std::pair{motionResizing ? "widget_height" : "widget_y",
                                        (motionResizing ? motionStartHeight : motionStartY) + delta.y}}) {
                auto &p = c->params[pair.first];
                if (p.keys.empty())
                    p.value = std::clamp(pair.second, motionResizing ? 1.f : -65536.f, 65536.f);
                else
                    p.setKey({s.time, std::clamp(pair.second, motionResizing ? 1.f : -65536.f, 65536.f),
                              Curve::Smooth, 0});
            }
        }
    }
    if (motionDragging && ImGui::IsMouseReleased(0)) {
        if (motionBefore && motionBefore->json().Dump(0) != s.project.json().Dump(0))
            commit(*motionBefore);
        motionBefore.reset();
        motionDragging = 0;
    }
    }
    ImGui::EndChild();
    ImGui::End();
}
} // namespace sf

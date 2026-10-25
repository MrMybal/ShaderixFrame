#include "ui/LayerCanvas.h"
#include "adapters/ShaderixelNodeVisuals.h"
#include "render/Grading.h"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
namespace sf {
bool LayerCanvas::draw(Json &document, const Project &project, Renderer &renderer, double time) {
    using namespace sx;
    using namespace shaderixel_ui;
    const auto before = document.Dump(0);
    auto decoded = decodeLayerGraph(document, project);
    auto &g = decoded.graph;
    auto mutate = [&](Id id, auto fn) {
        auto nodes = Json::Array();
        for (auto n : document["nodes"].Elements()) {
            if (n["id"].U() == id)
                fn(n);
            nodes.Push(n);
        }
        document["nodes"] = nodes;
    };
    auto add = [&](NodeKind kind, const std::string &name, const Effect *effect = nullptr, Id track = 0) {
        Id id = 1;
        for (auto &n : g.nodes)
            id = std::max(id, n.id + 1);
        Json n = Json::Object();
        n["id"] = id;
        n["kind"] = layerNodeKinds[int(kind)];
        n["name"] = name;
        n["surface"] = track;
        n["pos"] = Json::Array();
        n["pos"].Push(60 - ed.pan.x);
        n["pos"].Push(80 - ed.pan.y);
        n["inputs"] = Json::Array();
        for (int i = 0; i < (kind == NodeKind::Blend     ? 3
                             : kind == NodeKind::Surface ? 0
                             : effect                    ? int(effectInputs(effectCode(*effect)))
                                                         : 1);
             ++i)
            n["inputs"].Push(0);
        if (effect) {
            n["effect"] = Json::Object();
            n["effect"]["id"] = effect->id;
            n["effect"]["code"] = effect->code;
            n["effect"]["graph"] = effect->graph;
            n["effect"]["grading"] = effect->grading;
            n["params"] = Json::Object();
            for (auto &[key, p] : effect->params)
                n["params"][key] = parameterJson(p);
        }
        document["nodes"].Push(n);
        selected = id;
    };
    if (ImGui::Button("+ Blend"))
        add(NodeKind::Blend, "Blend");
    ImGui::SameLine();
    if (ImGui::Button("+ Correction")) {
        auto e = makeGrade();
        add(NodeKind::Effect, "Color Correction", &e);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Shader Graph")) {
        Effect e;
        e.id = "graph";
        e.graph = EffectGraph::Default().ToJson();
        e.code = effectCode(e);
        add(NodeKind::Effect, "Shader Graph", &e);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ LUT 2D / 3D")) {
        auto e = makeGrade("LUT 2D / 3D");
        add(NodeKind::Effect, "LUT 2D / 3D", &e);
        lutRequest = selected;
    }
    ImGui::SameLine();
    if (ImGui::BeginCombo("+ Transition multitrack", "Choose…")) {
        for (auto &d : renderer.library.definitions)
            if (d.kind == "transition" && ImGui::Selectable(d.name.c_str())) {
                Effect e;
                e.id = "transition";
                e.code = "//@input 0 \"Incoming\"\n//@input 1 \"Outgoing\"\n//@param progress float 0 0 1 "
                         "\"Progress\"\n" +
                         d.code + "\n//@premultiplied\n";
                e.params["progress"].keys = {{time, 0, Curve::Smooth, 0}, {time + 1, 1, Curve::Smooth, 0}};
                for (auto &p : d.parameters) {
                    int count = p.type == "float2"                          ? 2
                                : p.type == "float3"                        ? 3
                                : (p.type == "float4" || p.type == "color") ? 4
                                                                            : 1;
                    for (int k = 0; k < count; ++k)
                        e.params[p.key + (count > 1 ? "." + std::to_string(k) : "")] = {p.value[k]};
                }
                add(NodeKind::Effect, "Transition / " + d.name, &e);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Split RGBA"))
        add(NodeKind::Split, "Split RGBA");
    ImGui::SameLine();
    if (ImGui::BeginCombo("+ Effect / Timeline", "Choose…")) {
        for (auto &track : project.tracks)
            if (track.kind == TrackKind::Video && ImGui::Selectable(("Timeline / " + track.name).c_str()))
                add(NodeKind::Surface, track.name, nullptr, track.id);
        for (auto &def : renderer.library.definitions)
            if (def.kind == "effect" && ImGui::Selectable((def.category + " / " + def.name).c_str())) {
                auto e = renderer.library.instantiate(def.id);
                if (e.id != "grading")
                    e.code = def.code;
                add(NodeKind::Effect, def.name, &e);
            }
        if (ImGui::Selectable("Custom HLSL")) {
            Effect e;
            e.id = "custom";
            e.code = "float4 Effect(EffectContext ctx){return Input(0,ctx.px);}";
            add(NodeKind::Effect, "HLSL", &e);
        }
        ImGui::EndCombo();
    }
    bool fit = ImGui::Button("Fit All [F]");
    ImGui::SameLine();
    ImGui::TextDisabled("Wheel: zoom - middle button: pan - connect ports - right-click: disconnect");
    ImGui::BeginChild("ShaderixelComposition",
                      {0, selected ? std::max(180.f, ImGui::GetContentRegionAvail().y * .60f) : 0}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##graph", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    auto &io = ImGui::GetIO();
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 mouse = io.MousePos;
    fit |= hovered && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F);
    if ((fit || needsFit) && size.x > 100 && size.y > 80 && !g.nodes.empty()) {
        float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
        for (auto &n : g.nodes) {
            mn.x = std::min(mn.x, n.pos.x);
            mn.y = std::min(mn.y, n.pos.y);
            mx.x = std::max(mx.x, n.pos.x + kNodeW);
            mx.y = std::max(mx.y, n.pos.y + NodeLayout(n, float(project.width) / project.height).h);
        }
        ed.zoom =
            std::clamp(std::min((size.x - 40) / (mx.x - mn.x), (size.y - 40) / (mx.y - mn.y)), .2f, 1.f);
        ed.pan = {20 / ed.zoom - mn.x, 20 / ed.zoom - mn.y};
        needsFit = false;
    }
    auto toScreen = [&](float2 p) {
        return ImVec2(origin.x + (p.x + ed.pan.x) * ed.zoom, origin.y + (p.y + ed.pan.y) * ed.zoom);
    };
    auto toGraph = [&](ImVec2 p) {
        return float2{(p.x - origin.x) / ed.zoom - ed.pan.x, (p.y - origin.y) / ed.zoom - ed.pan.y};
    };
    if (hovered && io.MouseWheel) {
        auto anchor = toGraph(mouse);
        ed.zoom = std::clamp(ed.zoom * std::pow(1.15f, io.MouseWheel), .2f, 2.5f);
        ed.pan = ed.pan + (toGraph(mouse) - anchor);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0))
        ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
    dl->PushClipRect(origin, origin + size, true);
    dl->AddRectFilled(origin, origin + size, IM_COL32(19, 20, 24, 255));
    float step = 24 * ed.zoom;
    for (float x = std::fmod(ed.pan.x * ed.zoom, step); x < size.x; x += step)
        dl->AddLine(origin + ImVec2(x, 0), origin + ImVec2(x, size.y), ui::col::Grid);
    for (float y = std::fmod(ed.pan.y * ed.zoom, step); y < size.y; y += step)
        dl->AddLine(origin + ImVec2(0, y), origin + ImVec2(size.x, y), ui::col::Grid);
    const float aspect = float(project.width) / project.height;
    float pinR = kPin * ed.zoom;
    auto outPin = [&](const Node &n, int k) {
        return toScreen(n.pos + float2{kNodeW, kHeader + kRow * (k + .5f)});
    };
    auto inPin = [&](const Node &n, int k) {
        return toScreen(n.pos + float2{0, kHeader + kRow * (k + .5f)});
    };
    Id hoverNode = 0, hoverOut = 0, hoverInNode = 0;
    int hoverIn = -1, hoverOutCh = 0;
    auto pinNear = [&](ImVec2 a) {
        auto d = a - mouse;
        return d.x * d.x + d.y * d.y < pinR * pinR * 4;
    };
    if (hovered)
        for (auto it = g.nodes.rbegin(); it != g.nodes.rend(); ++it) {
            auto &n = *it;
            auto l = NodeLayout(n, aspect);
            for (int k = 0; k < l.outputs; ++k)
                if (pinNear(outPin(n, k))) {
                    hoverOut = n.id;
                    hoverOutCh = k;
                }
            for (int k = 0; k < l.inputs; ++k)
                if (pinNear(inPin(n, k))) {
                    hoverInNode = n.id;
                    hoverIn = k;
                }
            if (hoverOut || hoverInNode)
                break;
            auto a = toScreen(n.pos), b = toScreen(n.pos + float2{kNodeW, l.h});
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) {
                hoverNode = n.id;
                break;
            }
        }
    if (hovered && ImGui::IsMouseClicked(0)) {
        if (hoverOut) {
            ed.draggingLink = true;
            ed.linkFrom = hoverOut;
            ed.linkChannel = NodeOutputChannel(*g.Find(hoverOut), hoverOutCh);
        } else if (hoverNode) {
            selected = hoverNode;
            ed.selection = {selected};
            dragging = selected;
        }
    }
    if (hovered && hoverInNode && ImGui::IsMouseClicked(1))
        mutate(hoverInNode, [&](Json &n) {
            auto pins = Json::Array();
            for (int k = 0; k < int(n["inputs"].Size()); ++k)
                pins.Push(k == hoverIn ? Json(0) : n["inputs"].At(k));
            n["inputs"] = pins;
        });
    if (dragging && ImGui::IsMouseDragging(0))
        mutate(dragging, [&](Json &n) {
            n["pos"] = Json::Array();
            auto p = g.Find(dragging)->pos + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
            n["pos"].Push(p.x);
            n["pos"].Push(p.y);
        });
    if (ImGui::IsMouseReleased(0)) {
        if (ed.draggingLink && hoverInNode && !g.WouldCycle(ed.linkFrom, hoverInNode))
            mutate(hoverInNode, [&](Json &n) {
                auto pins = Json::Array(), channels = Json::Array();
                for (int k = 0; k < int(n["inputs"].Size()); ++k) {
                    pins.Push(k == hoverIn ? Json(ed.linkFrom) : n["inputs"].At(k));
                    channels.Push(k == hoverIn ? ed.linkChannel : int(InputChannel(*g.Find(hoverInNode), k)));
                }
                n["inputs"] = pins;
                n["inputChannels"] = channels;
            });
        ed.draggingLink = false;
        ed.linkFrom = 0;
        dragging = 0;
    }
    if (hovered && !hoverInNode && ImGui::IsMouseClicked(1)) {
        contextNode = hoverNode;
        ImGui::OpenPopup("LayerGraphContext");
    }
    if (ImGui::BeginPopup("LayerGraphContext")) {
        if (contextNode) {
            auto *node = g.Find(contextNode);
            if (node) {
                selected = contextNode;
                ed.selection = {selected};
                ImGui::TextUnformatted(node->name.c_str());
                ImGui::Separator();
                auto name = node->name;
                if (ImGui::InputText("Name", &name))
                    mutate(selected, [&](Json &v) { v["name"] = name; });
                if (node->kind != NodeKind::Output) {
                    if (ImGui::MenuItem("Enable / Bypass"))
                        mutate(selected, [&](Json &v) { v["bypass"] = !node->bypass; });
                    if (ImGui::MenuItem("Copy Node")) {
                        for (auto &v : document["nodes"].Elements())
                            if (v["id"].U() == selected)
                                clipboard = v;
                    }
                    if (ImGui::MenuItem("Delete")) {
                        auto nodes = Json::Array();
                        for (auto item : document["nodes"].Elements())
                            if (item["id"].U() != selected) {
                                auto pins = Json::Array();
                                for (auto &p : item["inputs"].Elements())
                                    pins.Push(p.U() == selected ? Json(0) : p);
                                item["inputs"] = pins;
                                nodes.Push(item);
                            }
                        document["nodes"] = nodes;
                        selected = 0;
                    }
                }
                if (node->kind == NodeKind::Effect && decoded.effects.at(node->id).id != "grading" &&
                    ImGui::MenuItem("Edit Shader Graph"))
                    shaderRequest = node->id;
            }
        } else {
            if (ImGui::MenuItem("Shaderixel Blend"))
                add(NodeKind::Blend, "Blend");
            if (ImGui::MenuItem("Color Correction")) {
                auto e = makeGrade();
                add(NodeKind::Effect, "Correction", &e);
            }
            if (ImGui::MenuItem("Shader Graph")) {
                Effect e;
                e.id = "graph";
                e.graph = EffectGraph::Default().ToJson();
                add(NodeKind::Effect, "Shader Graph", &e);
            }
            if (ImGui::MenuItem("Split RGBA"))
                add(NodeKind::Split, "Split RGBA");
            if (ImGui::BeginMenu("Timeline / Layer")) {
                for (auto &t : project.tracks)
                    if (t.kind == TrackKind::Video && ImGui::MenuItem(t.name.c_str()))
                        add(NodeKind::Surface, t.name, nullptr, t.id);
                ImGui::EndMenu();
            }
            if (!clipboard.IsNull() && ImGui::MenuItem("Paste Node")) {
                auto n = clipboard;
                Id next = 1;
                for (auto &v : g.nodes)
                    next = std::max(next, v.id + 1);
                n["id"] = next;
                n["pos"] = Json::Array();
                auto p = toGraph(mouse);
                n["pos"].Push(p.x);
                n["pos"].Push(p.y);
                document["nodes"].Push(n);
                selected = next;
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Fit All [F]"))
            needsFit = true;
        ImGui::EndPopup();
    }
#include "adapters/ShaderixelNodeDrawing.inl"
    dl->PopClipRect();
    ImGui::EndChild();
    if (auto *n = g.Find(selected)) {
        ImGui::Text("%s / node %u", n->name.c_str(), n->id);
        if (n->kind != NodeKind::Output) {
            bool bypass = n->bypass;
            if (ImGui::Checkbox("Bypass", &bypass))
                mutate(selected, [&](Json &v) { v["bypass"] = bypass; });
            ImGui::SameLine();
            if (ImGui::Button("Delete Node")) {
                auto nodes = Json::Array();
                for (auto item : document["nodes"].Elements())
                    if (item["id"].U() != selected) {
                        auto pins = Json::Array();
                        for (auto &pin : item["inputs"].Elements())
                            pins.Push(pin.U() == selected ? Json(0) : pin);
                        item["inputs"] = pins;
                        nodes.Push(item);
                    }
                document["nodes"] = nodes;
                selected = 0;
                return true;
            }
        }
        if (n->kind == NodeKind::Blend) {
            int mode = int(n->blend);
            const char *names[] = {"Normal",
                                   "Multiply",
                                   "Screen",
                                   "Keying",
                                   "Add",
                                   "Subtract",
                                   "Darken",
                                   "Lighten",
                                   "Difference",
                                   "Color Dodge",
                                   "Color Burn",
                                   "Hard Light",
                                   "Soft Light",
                                   "Dissolve",
                                   "Linear Burn",
                                   "Vivid Light",
                                   "Linear Light",
                                   "Pin Light",
                                   "Hard Mix",
                                   "Exclusion",
                                   "Divide",
                                   "Hue",
                                   "Saturation",
                                   "Color",
                                   "Luminosity",
                                   "Darker Color",
                                   "Lighter Color"};
            if (ImGui::Combo("Shaderixel Blend", &mode, names, 27))
                mutate(selected, [&](Json &v) { v["blend"] = mode; });
            bool clip = n->clip;
            if (ImGui::Checkbox("Clip to Background Alpha", &clip))
                mutate(selected, [&](Json &v) { v["clip"] = clip; });
            for (int c = 0; c < 4; ++c) {
                ImGui::PushID(c);
                if (c)
                    ImGui::SameLine();
                bool active = !(n->channelOff & (1 << c));
                if (ImGui::Checkbox(c == 0 ? "R" : c == 1 ? "V" : c == 2 ? "B" : "A", &active))
                    mutate(selected, [&](Json &v) {
                        v["channelOff"] =
                            unsigned(active ? n->channelOff & ~(1 << c) : n->channelOff | (1 << c));
                    });
                ImGui::PopID();
            }
            if (ImGui::TreeNode("Blend If / Conditional Blend")) {
                ImGui::TextDisabled("Handles 0-255: soft in, soft out");
                for (int c = 0; c < 4; ++c)
                    for (int side = 0; side < 2; ++side) {
                        ImGui::PushID(c * 2 + side);
                        int range[4];
                        for (int k = 0; k < 4; ++k)
                            range[k] = side ? n->blendIf.under[c][k] : n->blendIf.self[c][k];
                        const char *channel = c == 0 ? "Gray" : c == 1 ? "Red" : c == 2 ? "Green" : "Blue";
                        if (ImGui::SliderInt4((std::string(side ? "Background / " : "Layer / ") + channel).c_str(),
                                              range, 0, 255)) {
                            for (int k = 1; k < 4; ++k)
                                range[k] = std::max(range[k], range[k - 1]);
                            mutate(selected, [&](Json &v) {
                                auto values = Json::Array();
                                for (int ch = c ? 1 : 0; ch < (c ? 4 : 1); ++ch)
                                    for (int s = 0; s < 2; ++s)
                                        for (int k = 0; k < 4; ++k)
                                            values.Push(ch == c && s == side
                                                            ? range[k]
                                                            : int(s ? n->blendIf.under[ch][k]
                                                                    : n->blendIf.self[ch][k]));
                                v[c ? "blendIfRGB" : "blendIf"] = values;
                            });
                        }
                        ImGui::PopID();
                    }
                ImGui::TreePop();
            }
        }
        std::vector<EffectParameter> descriptors;
        if (n->kind == NodeKind::Blend)
            descriptors.push_back({"opacity", "Opacity", "float", {n->opacity, 0, 0, 0}, 0, 1});
        else if (n->kind == NodeKind::Effect) {
            auto &e = decoded.effects.at(n->id);
            descriptors =
                effectParameters(e.id == "grading" ? gradeShader(e, project.workingSpace) : effectCode(e));
            if (e.id == "grading") {
                ImGui::Combo("Image LUT layout", &lutLayout, "Automatic\0 3D atlas\0 2D RG map\0");
                if (ImGui::Button("Import .cube / 2D image LUT"))
                    lutRequest = selected;
                ImGui::SameLine();
                if (ImGui::BeginCombo("Look", "Choose…")) {
                    for (auto preset :
                         {"Neutral", "Warm", "Cool", "Black and White", "Cyan shadows / warm highlights"})
                        if (ImGui::Selectable(preset)) {
                            auto grade = gradePreset(preset);
                            mutate(selected, [&](Json &v) {
                                v["effect"]["grading"] = grade.grading;
                                v["params"] = Json::Object();
                                for (auto &[key, p] : grade.params)
                                    v["params"][key] = parameterJson(p);
                            });
                        }
                    ImGui::EndCombo();
                }
                if (ImGui::Button("Remove LUT"))
                    mutate(selected, [&](Json &v) { v["effect"]["grading"]["lut"] = Json(); });
            }
            if (e.id != "grading") {
                if (ImGui::Button("Open Shader Graph")) {
                    if (e.graph.IsNull())
                        mutate(selected,
                               [&](Json &v) { v["effect"]["graph"] = EffectGraph::Default().ToJson(); });
                    shaderRequest = selected;
                }
                auto identity = std::to_string(selected) + effectCode(e);
                if (identity != draftIdentity) {
                    shaderDraft = effectCode(e);
                    draftIdentity = identity;
                }
                ImGui::InputTextMultiline("Shader HLSL / EffectGraph", &shaderDraft, {-1, 100});
                if (ImGui::Button("Compile and Apply")) {
                    renderer.validateShader(shaderDraft);
                    mutate(selected, [&](Json &v) {
                        v["effect"]["code"] = shaderDraft;
                        v["effect"]["graph"] = Json();
                    });
                }
            }
        }
        ImGui::BeginChild("NodeParameters", {0, 0});
        std::vector<EffectParameter> components;
        for (auto d : descriptors) {
            int count = d.type == "float2"                          ? 2
                        : d.type == "float3"                        ? 3
                        : (d.type == "float4" || d.type == "color") ? 4
                                                                    : 1;
            for (int k = 0; k < count; ++k) {
                auto component = d;
                if (count > 1) {
                    component.key += "." + std::to_string(k);
                    component.label += " / " + std::string("RGBA").substr(k, 1);
                    component.value.x = d.value[k];
                }
                components.push_back(component);
            }
        }
        for (auto &d : components) {
            ImGui::PushID(d.key.c_str());
            auto it = decoded.parameters[selected].find(d.key);
            auto p = it == decoded.parameters[selected].end() ? Parameter{d.value.x} : it->second;
            float value = p.at(time);
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat(d.label.c_str(), &value, d.minimum, d.maximum, "%.4f")) {
                if (p.keys.empty())
                    p.value = value;
                else
                    p.setKey({time, value, Curve::Smooth, 0});
                mutate(selected, [&](Json &v) { v["params"][d.key] = parameterJson(p); });
            }
            ImGui::SameLine();
            if (ImGui::Button("K")) {
                setLayerGraphKey(document, selected, d.key, {time, value, Curve::Smooth, 0});
                automationKey = d.key;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Curve"))
                automationKey = d.key;
            if (automationKey == d.key && !p.keys.empty()) {
                float values[128];
                for (int k = 0; k < 128; ++k)
                    values[k] = p.at(project.duration * k / 127.);
                ImGui::PlotLines("##lane", values, 128, 0, nullptr, FLT_MAX, FLT_MAX, {-1, 55});
                for (size_t k = 0; k < p.keys.size(); ++k) {
                    ImGui::PushID(int(k));
                    auto frame = p.keys[k];
                    ImGui::SetNextItemWidth(100);
                    bool edit = ImGui::InputDouble("s", &frame.time, 0, 0, "%.4f",
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    edit |= ImGui::InputFloat("Value", &frame.value, 0, 0, "%.4f",
                                              ImGuiInputTextFlags_EnterReturnsTrue);
                    int curve = int(frame.curve);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(110);
                    edit |= ImGui::Combo("Interpolation", &curve, "Linear\0Smooth\0Step\0Curved\0");
                    frame.curve = Curve(curve);
                    if (curve == 3)
                        edit |= ImGui::SliderFloat("Curvature", &frame.shape, -1, 1);
                    bool remove = ImGui::SmallButton("Remove");
                    if (edit || remove) {
                        p.keys.erase(p.keys.begin() + k);
                        if (!remove)
                            p.setKey(frame);
                        mutate(selected, [&](Json &v) { v["params"][d.key] = parameterJson(p); });
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    return before != document.Dump(0);
}
} // namespace sf

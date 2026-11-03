#include <cyui/Core.h>
#include "ui/LayerCanvas.h"
#include "ui/Theme.h"
#include "render/Grading.h"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
namespace sf {
bool LayerCanvas::draw(Json &document, const Project &project, Renderer &renderer, double time) {
    using namespace sx;
    const auto before = document.Dump(0);
    auto decoded = track ? decodeTrackGraph(document, project, track) : decodeLayerGraph(document, project);
    auto nodeView = [&](Id id) { return track ? renderer.trackNodeView(track, id) : renderer.layerNodeView(id); };
    // A track graph reads its own track and the tracks below it; the layer graph reads every video track.
    auto readable = [&](const Track &t) {
        if (t.kind != TrackKind::Video)
            return false;
        if (!track)
            return true;
        auto self = std::find_if(project.tracks.begin(), project.tracks.end(), [&](auto &x) { return x.id == track; });
        auto other = std::find_if(project.tracks.begin(), project.tracks.end(), [&](auto &x) { return x.id == t.id; });
        return t.id == track || other > self;
    };
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
        n["pos"].Push(60 / ed.zoom - ed.pan.x);
        n["pos"].Push(80 / ed.zoom - ed.pan.y);
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
    if (ImGui::Button(cyui::trLabel("+ Blend").c_str()))
        add(NodeKind::Blend, "Blend");
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("+ Correction").c_str())) {
        auto e = makeGrade();
        add(NodeKind::Effect, "Color Correction", &e);
    }
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("+ Shader Graph").c_str())) {
        Effect e;
        e.id = "graph";
        e.graph = EffectGraph::Default().ToJson();
        e.code = effectCode(e);
        add(NodeKind::Effect, "Shader Graph", &e);
    }
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("+ LUT 2D / 3D").c_str())) {
        auto e = makeGrade("LUT 2D / 3D");
        add(NodeKind::Effect, "LUT 2D / 3D", &e);
        lutRequest = selected;
    }
    ImGui::SameLine();
    if (ImGui::BeginCombo(cyui::trLabel("+ Transition multitrack").c_str(), "Choose…")) {
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
    if (ImGui::Button(cyui::trLabel("+ Split RGBA").c_str()))
        add(NodeKind::Split, "Split RGBA");
    ImGui::SameLine();
    if (ImGui::BeginCombo(cyui::trLabel("+ Effect / Timeline").c_str(), "Choose…")) {
        for (auto &layer : project.tracks)
            if (readable(layer) && ImGui::Selectable(("Timeline / " + layer.name).c_str()))
                add(NodeKind::Surface, layer.name, nullptr, layer.id);
        for (auto &def : renderer.library.definitions)
            if (def.kind == "effect" && ImGui::Selectable((def.category + " / " + def.name).c_str())) {
                auto e = renderer.library.instantiate(def.id);
                if (e.id != "grading")
                    e.code = def.code;
                add(NodeKind::Effect, def.name, &e);
            }
        if (ImGui::Selectable(cyui::trLabel("Custom HLSL").c_str())) {
            Effect e;
            e.id = "custom";
            e.code = "float4 Effect(EffectContext ctx){return Input(0,ctx.px);}";
            add(NodeKind::Effect, "HLSL", &e);
        }
        ImGui::EndCombo();
    }
    bool fit = ImGui::Button(cyui::trLabel("Fit All [F]").c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(cyui::tr("Wheel: zoom - middle button: pan - connect ports - right-click: disconnect").c_str());
    ImGui::BeginChild("ShaderixelComposition",
                      {0, selected ? std::max(180.f, ImGui::GetContentRegionAvail().y * .60f) : 0}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    auto &io=ImGui::GetIO(); auto mouse=io.MousePos;
    bool hovered=ImGui::IsWindowHovered();
    auto toGraph=[&](ImVec2 p){auto w=nodesState.canvas.view.world(p,origin);return float2{float(w.x),float(w.y)};};
    auto pin=[](Id id,int port,bool output){return (std::uint64_t(id)<<16)|(output?0x8000:0)|(port+1);};
    if(stamp!=before){stamp=before;++revision;}
    std::vector<cyui::Node> nodes;std::vector<cyui::NodeLink> links;
    for(auto& n:g.nodes) {
        cyui::Node item{n.id,{n.pos.x,n.pos.y},n.name+(n.bypass?" [bypassed]":"")};item.colorIndex=int(n.kind);
        for(int i=0;i<NodeInputCount(n);++i)item.inputs.push_back({pin(n.id,i,false),NodeInputName(n,i),int(NodeInputType(n,i))});
        for(int i=0;i<NodeOutputCount(n);++i)item.outputs.push_back({pin(n.id,i,true),NodeOutputName(n,i),int(NodeOutputType(n,i))});
        nodes.push_back(std::move(item));
        for(int i=0;i<int(n.inputs.size());++i)if(auto* src=g.Find(n.inputs[i])) {
            int channel=InputChannel(n,i),port=src->kind==NodeKind::Split&&channel>0?channel-1:0;
            links.push_back({pin(n.id,i,false),pin(src->id,port,true),pin(n.id,i,false),channel?2:0});
        }
    }
    if((fit||needsFit||(hovered&&!io.WantTextInput&&ImGui::IsKeyPressed(ImGuiKey_F)))&&size.x>100&&size.y>80&&!nodes.empty()) {
        cyui::Point lo=nodes.front().position,hi=lo;
        for(auto& n:nodes){lo.x=std::min(lo.x,n.position.x);lo.y=std::min(lo.y,n.position.y);
            hi.x=std::max(hi.x,n.position.x+cyui::dp(190));hi.y=std::max(hi.y,n.position.y+cyui::dp(36+72*std::max({size_t(1),n.inputs.size(),n.outputs.size()})));}
        nodesState.canvas.view.fit(lo,hi,size,cyui::dp(24));needsFit=false;
    }
    cyui::NodeOptions options;options.width=190;options.row=72;options.revision=revision;
    options.canConnect=[&](auto from,auto to){auto* a=g.Find(Id(from>>16));auto* b=g.Find(Id(to>>16));
        return a&&b&&(from&0x8000)&&!(to&0x8000)&&!g.WouldCycle(a->id,b->id);};
    options.drawBody=[&](const cyui::Node& n,const cyui::Rect& body){
        if(auto* image=nodeView(Id(n.id))) {
            float z=float(nodesState.canvas.view.zoom),w=body.max.x-body.min.x-cyui::dp(8)*z;
            float h=std::min(cyui::dp(44)*z,w*float(project.height)/std::max(1,project.width));
            ImVec2 a{body.min.x,body.max.y-h-cyui::dp(5)*z},b{a.x+w,a.y+h};
            ImGui::GetWindowDrawList()->AddImage((ImTextureID)image,a,b);
        }
    };
    cyui::NodeResult result;{cyui::ScopedFont font(cyui::uiFont(),float(nodesState.canvas.view.zoom));result=cyui::nodeEditor("##graph",nodesState,nodes,links,size,options);}
    if(hovered&&ImGui::IsMouseClicked(1))for(auto& n:nodes)for(size_t i=0;i<n.inputs.size();++i) {
        auto at=nodesState.canvas.view.screen(n.position,origin);at.y+=cyui::dp(28+72*(float(i)+.5f))*float(nodesState.canvas.view.zoom);
        if(std::hypot(at.x-io.MousePos.x,at.y-io.MousePos.y)<cyui::dp(9)){result.linkRemoved=true;result.removeLink=n.inputs[i].id;}
    }
    ed.zoom=float(nodesState.canvas.view.zoom);ed.pan={float(-nodesState.canvas.view.origin.x),float(-nodesState.canvas.view.origin.y)};
    ed.selection.clear();for(auto id:nodesState.selection)ed.selection.insert(Id(id));
    if(!nodesState.selection.empty())selected=Id(nodesState.selection.back());
    if(result.moved.committed())for(auto& move:result.moved.value)mutate(Id(move.id),[&](Json& n){n["pos"]=Json::Array();n["pos"].Push(move.position.x);n["pos"].Push(move.position.y);});
    if(result.connect) {
        Id from=Id(result.connectFrom>>16),to=Id(result.connectTo>>16);int input=int(result.connectTo&0x7fff)-1,output=int(result.connectFrom&0x7fff)-1;
        int channel=NodeOutputChannel(*g.Find(from),output);
        mutate(to,[&](Json& n){auto pins=Json::Array(),channels=Json::Array();for(int i=0;i<int(n["inputs"].Size());++i){pins.Push(i==input?Json(from):n["inputs"].At(i));channels.Push(i==input?channel:int(InputChannel(*g.Find(to),i)));}n["inputs"]=pins;n["inputChannels"]=channels;});
    }
    if(result.linkRemoved) {Id id=Id(result.removeLink>>16);int port=int(result.removeLink&0x7fff)-1;
        mutate(id,[&](Json& n){auto pins=Json::Array();for(int i=0;i<int(n["inputs"].Size());++i)pins.Push(i==port?Json(0):n["inputs"].At(i));n["inputs"]=pins;});}
    if(!result.removeNodes.empty()) {
        std::set<Id> ids;for(auto id:result.removeNodes)if(auto* n=g.Find(Id(id));n&&n->kind!=NodeKind::Output)ids.insert(Id(id));
        auto ns=Json::Array();for(auto n:document["nodes"].Elements())if(!ids.contains(n["id"].U())){auto pins=Json::Array();for(auto& p:n["inputs"].Elements())pins.Push(ids.contains(p.U())?Json(0):p);n["inputs"]=pins;ns.Push(n);}document["nodes"]=ns;
        if(ids.contains(selected))selected=0;
    }
    if(hovered&&ImGui::IsMouseClicked(1)&&!result.linkRemoved) {
        contextNode=0;
        for(auto& n:nodes){auto a=nodesState.canvas.view.screen(n.position,origin);float z=ed.zoom;
            if(ImGui::IsMouseHoveringRect(a,{a.x+cyui::dp(190)*z,a.y+cyui::dp(36+72*std::max({size_t(1),n.inputs.size(),n.outputs.size()}))*z}))contextNode=Id(n.id);}
        ImGui::OpenPopup("LayerGraphContext");
    }
    if (ImGui::BeginPopup("LayerGraphContext")) {
        if (contextNode) {
            auto *node = g.Find(contextNode);
            if (node) {
                selected = contextNode;
                ed.selection = {selected};
                nodesState.selection={selected};
                ImGui::TextUnformatted(node->name.c_str());
                ImGui::Separator();
                auto name = node->name;
                if (ImGui::InputText(cyui::trLabel("Name").c_str(), &name))
                    mutate(selected, [&](Json &v) { v["name"] = name; });
                if (node->kind != NodeKind::Output) {
                    if (ImGui::MenuItem(cyui::trLabel("Enable / Bypass").c_str()))
                        mutate(selected, [&](Json &v) { v["bypass"] = !node->bypass; });
                    if (ImGui::MenuItem(cyui::trLabel("Copy Node").c_str())) {
                        for (auto &v : document["nodes"].Elements())
                            if (v["id"].U() == selected)
                                clipboard = v;
                    }
                    if (ImGui::MenuItem(cyui::trLabel("Delete").c_str())) {
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
                    ImGui::MenuItem(cyui::trLabel("Edit Shader Graph").c_str()))
                    shaderRequest = node->id;
            }
        } else {
            if (ImGui::MenuItem(cyui::trLabel("Shaderixel Blend").c_str()))
                add(NodeKind::Blend, "Blend");
            if (ImGui::MenuItem(cyui::trLabel("Color Correction").c_str())) {
                auto e = makeGrade();
                add(NodeKind::Effect, "Correction", &e);
            }
            if (ImGui::MenuItem(cyui::trLabel("Shader Graph").c_str())) {
                Effect e;
                e.id = "graph";
                e.graph = EffectGraph::Default().ToJson();
                add(NodeKind::Effect, "Shader Graph", &e);
            }
            if (ImGui::MenuItem(cyui::trLabel("Split RGBA").c_str()))
                add(NodeKind::Split, "Split RGBA");
            if (ImGui::BeginMenu(cyui::trLabel("Timeline / Layer").c_str())) {
                for (auto &t : project.tracks)
                    if (readable(t) && ImGui::MenuItem(t.name.c_str()))
                        add(NodeKind::Surface, t.name, nullptr, t.id);
                ImGui::EndMenu();
            }
            if (!clipboard.IsNull() && ImGui::MenuItem(cyui::trLabel("Paste Node").c_str())) {
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
        if (ImGui::MenuItem(cyui::trLabel("Fit All [F]").c_str()))
            needsFit = true;
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    if (auto *n = g.Find(selected)) {
        ImGui::Text(cyui::tr("%s / node %u").c_str(), n->name.c_str(), n->id);
        if (n->kind != NodeKind::Output) {
            bool bypass = n->bypass;
            if (ImGui::Checkbox(cyui::trLabel("Bypass").c_str(), &bypass))
                mutate(selected, [&](Json &v) { v["bypass"] = bypass; });
            ImGui::SameLine();
            if (ImGui::Button(cyui::trLabel("Delete Node").c_str())) {
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
            if (ImGui::Combo(cyui::trLabel("Shaderixel Blend").c_str(), &mode, names, 27))
                mutate(selected, [&](Json &v) { v["blend"] = mode; });
            bool clip = n->clip;
            if (ImGui::Checkbox(cyui::trLabel("Clip to Background Alpha").c_str(), &clip))
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
            if (ImGui::TreeNode(cyui::trLabel("Blend If / Conditional Blend").c_str())) {
                ImGui::TextDisabled(cyui::tr("Handles 0-255: soft in, soft out").c_str());
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
                ImGui::Combo(cyui::trLabel("Image LUT layout").c_str(), &lutLayout, "Automatic\0 3D atlas\0 2D RG map\0");
                if (ImGui::Button(cyui::trLabel("Import .cube / 2D image LUT").c_str()))
                    lutRequest = selected;
                ImGui::SameLine();
                if (ImGui::BeginCombo(cyui::trLabel("Look").c_str(), "Choose…")) {
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
                if (ImGui::Button(cyui::trLabel("Remove LUT").c_str()))
                    mutate(selected, [&](Json &v) { v["effect"]["grading"]["lut"] = Json(); });
            }
            if (e.id != "grading") {
                if (ImGui::Button(cyui::trLabel("Open Shader Graph").c_str())) {
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
                codeFields.draw("Shader HLSL / EffectGraph", shaderDraft, {-1, 160});
                if (ImGui::Button(cyui::trLabel("Compile and Apply").c_str())) {
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
            if (ImGui::Button(cyui::trLabel("K").c_str())) {
                setLayerGraphKey(document, selected, d.key, {time, value, Curve::Smooth, 0});
                automationKey = d.key;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(cyui::trLabel("Curve").c_str()))
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
                    bool edit = ImGui::InputDouble(cyui::trLabel("s").c_str(), &frame.time, 0, 0, "%.4f",
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    edit |= ImGui::InputFloat(cyui::trLabel("Value").c_str(), &frame.value, 0, 0, "%.4f",
                                              ImGuiInputTextFlags_EnterReturnsTrue);
                    int curve = int(frame.curve);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(110);
                    edit |= ImGui::Combo(cyui::trLabel("Interpolation").c_str(), &curve, "Linear\0Smooth\0Step\0Curved\0");
                    frame.curve = Curve(curve);
                    if (curve == 3)
                        edit |= ImGui::SliderFloat(cyui::trLabel("Curvature").c_str(), &frame.shape, -1, 1);
                    bool remove = ImGui::SmallButton(cyui::trLabel("Remove").c_str());
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
    if(selected && !ed.selection.contains(selected))nodesState.selection={selected};
    return before != document.Dump(0);
}
} // namespace sf

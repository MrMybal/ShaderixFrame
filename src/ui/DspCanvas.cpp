#include "ui/DspCanvas.h"
#include "adapters/CyDawAudio.h"
#include "adapters/CyDawParameters.h"
#include <cyui/modules/Audio.h>
#include "ui/MixerStrip.h"
#include "ui/ParameterCurve.h"
#include "adapters/LayerGraph.h"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
namespace sf {
static Json pointJson(sx::float2 p) {
    auto a = Json::Array();
    a.Push(p.x);
    a.Push(p.y);
    return a;
}
void DspCanvas::open(const Json &json) {
    graph = json;
    nodesState = {}; curveState = {}; stamp.clear(); curveIdentity.clear();
    past.clear();
    future.clear();
    selection.clear();
    hasWire = false;
    fitPending = true;
    error.clear();
}
void DspCanvas::checkpoint() {
    past.push_back(graph);
    if (past.size() > 64)
        past.erase(past.begin());
    future.clear();
}
bool DspCanvas::undo() {
    if (past.empty())
        return false;
    future.push_back(graph);
    graph = past.back();
    past.pop_back();
    selection.clear();
    nodesState.selection.clear();
    return true;
}
bool DspCanvas::redo() {
    if (future.empty())
        return false;
    past.push_back(graph);
    graph = future.back();
    future.pop_back();
    selection.clear();
    nodesState.selection.clear();
    return true;
}
sx::float2 DspCanvas::position(Id node) const {
    if (node == 0 || node == outputBoundary) {
        const auto &p = graph["layout"][node ? "output" : "input"];
        return p.IsArray() && p.Size() == 2 ? sx::float2{p.At(0).F(), p.At(1).F()}
                                            : sx::float2{node ? 860.f : 20.f, 200};
    }
    unsigned index = 0;
    for (const auto &n : graph["nodes"].Elements()) {
        if (n["id"].U() == node) {
            const auto &p = n["pos"];
            return p.IsArray() && p.Size() == 2
                       ? sx::float2{p.At(0).F(), p.At(1).F()}
                       : sx::float2{260.f + (index % 3) * 270.f, 70.f + (index / 3) * 200.f};
        }
        ++index;
    }
    return {};
}
void DspCanvas::setPosition(Id node, sx::float2 p) {
    if (node == 0 || node == outputBoundary) {
        graph["layout"][node ? "output" : "input"] = pointJson(p);
        return;
    }
    auto nodes = Json::Array();
    for (auto item : graph["nodes"].Elements()) {
        if (item["id"].U() == node)
            item["pos"] = pointJson(p);
        nodes.Push(item);
    }
    graph["nodes"] = nodes;
}
Id DspCanvas::add(const std::string &processor, sx::float2 point) {
    if (graph["nodes"].Size() >= 128)
        throw std::runtime_error("128 DSP nodes maximum");
    Id id = 1;
    for (const auto &n : graph["nodes"].Elements())
        id = std::max(id, n["id"].U() + 1);
    if (id > 2147483647)
        throw std::runtime_error("DSP IDs exhausted");
    auto effect = processor == "sf.sum" ? Effect{} : instantiateAudioEffect(processor);
    if (processor != "sf.sum" && (processor == "cydaw.graph" || externalAudio(processor)))
        throw std::runtime_error("A library DSP is required");
    checkpoint();
    Json node = Json::Object();
    node["id"] = id;
    node["processor"] = processor;
    node["name"] = processor == "sf.sum" ? "Stereo Sum" : std::string(audioEffect(processor)->displayName);
    node["pos"] = pointJson(point);
    node["params"] = Json::Object();
    for (auto &[key, parameter] : effect.params)
        node["params"][key] = parameter.value;
    graph["nodes"].Push(node);
    selection = {id};
    nodesState.selection={id};
    return id;
}
void DspCanvas::connect(Id from, Id to) {
    if (from == outputBoundary || to == 0 || from == to)
        throw std::runtime_error("Connect an output to a different input");
    to = to == outputBoundary ? 0 : to;
    bool existsFrom = from == 0, existsTo = to == 0, many = false;
    for (auto &node : graph["nodes"].Elements()) {
        existsFrom |= node["id"].U() == from;
        existsTo |= node["id"].U() == to;
        if (node["id"].U() == to)
            many = node["processor"].S() == "sf.sum";
    }
    if (!existsFrom || !existsTo)
        throw std::runtime_error("Missing DSP port");
    auto next = graph;
    auto wires = Json::Array();
    for (auto item : graph["connections"].Elements()) {
        if (item["source"].U() == from && item["destination"].U() == to)
            return;
        if (many || item["destination"].U() != to)
            wires.Push(item);
    }
    Json wire = Json::Object();
    wire["source"] = from;
    wire["destination"] = to;
    wires.Push(wire);
    next["connections"] = wires;
    // Validate cycles even when the draft output is temporarily disconnected.
    auto validation = next;
    bool output = false;
    for (auto &wire : wires.Elements())
        output |= wire["destination"].U() == 0;
    if (!output) {
        Json pass = Json::Object();
        pass["source"] = 0;
        pass["destination"] = 0;
        validation["connections"].Push(pass);
    }
    decodeDspGraph(validation);
    checkpoint();
    graph = next;
}
void DspCanvas::remove(const std::set<Id> &selected) {
    auto editable = selected;
    editable.erase(0);
    editable.erase(outputBoundary);
    if (editable.empty())
        return;
    checkpoint();
    auto nodes = Json::Array(), wires = Json::Array();
    for (auto n : graph["nodes"].Elements())
        if (!editable.contains(n["id"].U()))
            nodes.Push(n);
    for (auto w : graph["connections"].Elements())
        if (!editable.contains(w["source"].U()) &&
            !editable.contains(w["destination"].U() ? w["destination"].U() : outputBoundary))
            wires.Push(w);
    graph["nodes"] = nodes;
    graph["connections"] = wires;
    selection.clear();
}
void DspCanvas::disconnect(Id from, Id to) {
    to = to == outputBoundary ? 0 : to;
    auto wires = Json::Array();
    for (auto w : graph["connections"].Elements())
        if (w["source"].U() != from || w["destination"].U() != to)
            wires.Push(w);
    if (wires.Size() != graph["connections"].Size()) {
        checkpoint();
        graph["connections"] = wires;
    }
}
void DspCanvas::arrange() {
    auto check = graph;
    bool output = false;
    for (auto &w : check["connections"].Elements())
        output |= w["destination"].U() == 0;
    if (!output) {
        Json pass = Json::Object();
        pass["source"] = 0;
        pass["destination"] = 0;
        check["connections"].Push(pass);
    }
    decodeDspGraph(check);
    std::vector<cyui::Node> nodes;
    std::vector<cyui::NodeLink> links;
    auto pin=[](Id id,bool output){return std::uint64_t(id)*2+(output?2:1);};
    auto add=[&](Id id){cyui::Node n;n.id=id;if(id)n.inputs.push_back({pin(id,false)});if(id!=outputBoundary)n.outputs.push_back({pin(id,true)});nodes.push_back(std::move(n));};
    add(0);for(auto& n:graph["nodes"].Elements())add(n["id"].U());add(outputBoundary);
    for(auto& w:graph["connections"].Elements()){Id from=w["source"].U(),to=w["destination"].U();if(!to)to=outputBoundary;links.push_back({(std::uint64_t(from)<<32)|to,pin(from,true),pin(to,false)});}
    auto positions=cyui::arrangeNodes(nodes,links,{272,110});
    checkpoint();for(auto& n:positions)setPosition(Id(n.id),{float(n.position.x),float(n.position.y)});
    fitPending = true;
}
void DspCanvas::nodeSearch() {
    if (ImGui::BeginPopup("Add DSP")) {
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputText(cyui::trLabel("Search").c_str(), &search);
        auto entry = [&](const std::string &id, const std::string &label) {
            if (!search.empty() && label.find(search) == std::string::npos &&
                id.find(search) == std::string::npos)
                return;
            if (ImGui::Selectable(label.c_str())) {
                try {
                    Id added = add(id, newPosition);
                    if (hasWire) {
                        if (wireOutput)
                            connect(wire, added);
                        else
                            connect(added, wire);
                    }
                    hasWire = false;
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception &e) {
                    error = e.what();
                }
            }
        };
        entry("sf.sum", "Stereo Sum");
        for (auto &d : cydaw::engine::effectCatalogue)
            if (audioInsert(d))
                entry("cydaw." + std::string(d.typeKey), std::string(d.displayName));
        ImGui::EndPopup();
    }
}
bool DspCanvas::draw(double time) {
    auto before = graph.Dump(0);
    if (ImGui::Button(cyui::trLabel("Add DSP...").c_str())) {
        newPosition = view.world({300, 120});
        search.clear();
        ImGui::OpenPopup("Add DSP");
    }
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("Fit All [F]").c_str()))
        fitPending = true;
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("Arrange [A]").c_str()))
        try {
            arrange();
        } catch (const std::exception &e) {
            error = e.what();
        }
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("Undo").c_str()))
        undo();
    ImGui::SameLine();
    if (ImGui::Button(cyui::trLabel("Redo").c_str()))
        redo();
    ImGui::TextDisabled(
        cyui::tr("Tab/double-click: DSP - wheel: zoom - middle button: pan - Delete: remove").c_str());
    ImGui::BeginChild("dspCanvas",
                      {0, std::max(160.f, ImGui::GetContentRegionAvail().y * ((automationNode || !selection.empty()) ? .52f : 1.f))},
                      true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    origin = ImGui::GetCursorScreenPos();
    auto size = ImGui::GetContentRegionAvail();
    auto &io = ImGui::GetIO();
    const bool hovered = ImGui::IsWindowHovered();
    if (stamp != graph.Dump(0)) { stamp = graph.Dump(0); ++revision; }
    std::vector<cyui::Node> model;
    std::vector<cyui::NodeLink> links;
    auto pin = [](Id id, bool output) { return std::uint64_t(id) * 2 + (output ? 2 : 1); };
    auto card = [&](Id id, const std::string &name) {
        auto p = position(id);
        cyui::Node n{id, {p.x,p.y}, name};
        if (id != 0) n.inputs.push_back({pin(id,false), "Stereo in", 1});
        if (id != outputBoundary) n.outputs.push_back({pin(id,true), "Stereo out", 1});
        model.push_back(std::move(n));
    };
    card(0,"STEREO INPUT"); card(outputBoundary,"STEREO OUTPUT");
    for (auto &n : graph["nodes"].Elements()) card(n["id"].U(),n["name"].S(n["processor"].S()));
    for (auto &w : graph["connections"].Elements()) {
        Id from=w["source"].U(), to=w["destination"].U();
        if (!to) to=outputBoundary;
        links.push_back({(std::uint64_t(from)<<32)|to,pin(from,true),pin(to,false),1});
    }
    if (fitPending && size.x>100 && size.y>80 && !model.empty()) {
        cyui::Point lo=model.front().position,hi=lo;
        for(auto& n:model) { lo.x=std::min(lo.x,n.position.x); lo.y=std::min(lo.y,n.position.y);
            hi.x=std::max(hi.x,n.position.x+cyui::dp(212)); hi.y=std::max(hi.y,n.position.y+cyui::dp(68)); }
        nodesState.canvas.view.fit(lo,hi,size,cyui::dp(24)); fitPending=false;
    }
    cyui::NodeOptions options; options.width=212; options.revision=revision;
    options.canConnect=[&](auto from,auto to) { return from%2==0 && to%2==1 && (from-1)/2!=(to-1)/2; };
    const auto pendingWire=nodesState.wire;const bool pendingOutput=nodesState.wireOutput,pendingWiring=nodesState.wiring;
    bool addRequested=false;
    cyui::NodeResult result;{cyui::ScopedFont font(cyui::uiFont(),float(nodesState.canvas.view.zoom));result=cyui::nodeEditor("DSP nodes",nodesState,model,links,size,options);}
    view.zoom=float(nodesState.canvas.view.zoom);
    view.pan={float(-nodesState.canvas.view.origin.x*view.zoom),float(-nodesState.canvas.view.origin.y*view.zoom)};
    selection.clear(); for(auto id:nodesState.selection) selection.insert(Id(id));
    if(result.moved.committed()) {
        bool changed=false; for(auto& n:result.moved.value) {auto p=position(Id(n.id));changed|=p.x!=n.position.x||p.y!=n.position.y;}
        if(changed) {checkpoint();for(auto& n:result.moved.value)setPosition(Id(n.id),{float(n.position.x),float(n.position.y)});}
    }
    try {
        if(result.connect) connect(Id((result.connectFrom-1)/2),Id((result.connectTo-1)/2));
        if(result.linkRemoved) disconnect(Id(result.removeLink>>32),Id(result.removeLink));
        if(!result.removeNodes.empty()) {std::set<Id> ids;for(auto id:result.removeNodes)ids.insert(Id(id));remove(ids);}
    } catch(const std::exception& e) {error=e.what();}
    if(pendingWiring&&!nodesState.wiring&&!result.connect&&ImGui::IsMouseReleased(0)&&hovered) {
        bool empty=true;for(auto& n:model){auto at=nodesState.canvas.view.screen(n.position,origin);if(ImGui::IsMouseHoveringRect(at,{at.x+cyui::dp(212)*view.zoom,at.y+cyui::dp(60)*view.zoom}))empty=false;}
        if(empty){wire=Id((pendingWire-1)/2);wireOutput=pendingOutput;hasWire=true;newPosition=view.world({io.MousePos.x-origin.x,io.MousePos.y-origin.y});search.clear();addRequested=true;}
    }
    if(hovered && ImGui::IsMouseClicked(1) && !result.linkRemoved) {
        contextNode=0;
        for(auto& n:model) {auto at=nodesState.canvas.view.screen(n.position,origin);float z=view.zoom;
            if(ImGui::IsMouseHoveringRect(at,{at.x+cyui::dp(212)*z,at.y+cyui::dp(60)*z})) contextNode=Id(n.id);}
        newPosition=view.world({io.MousePos.x-origin.x,io.MousePos.y-origin.y});
        ImGui::OpenPopup("DSP Context");
    }
    if(ImGui::BeginPopup("DSP Context")) {
        if(contextNode && contextNode!=outputBoundary) {
            Json node;for(auto& n:graph["nodes"].Elements())if(n["id"].U()==contextNode)node=n;
            auto name=node["name"].S();
            if(ImGui::InputText(cyui::trLabel("Name").c_str(),&name)) {checkpoint();auto ns=Json::Array();for(auto n:graph["nodes"].Elements()){if(n["id"].U()==contextNode)n["name"]=name;ns.Push(n);}graph["nodes"]=ns;}
            if(ImGui::MenuItem(cyui::trLabel("Duplicate").c_str())) {auto id=add(node["processor"].S(),position(contextNode)+sx::float2{40,40});auto ns=Json::Array();for(auto n:graph["nodes"].Elements()){if(n["id"].U()==id){n["params"]=node["params"];n["name"]=name+" copy";}ns.Push(n);}graph["nodes"]=ns;}
            if(ImGui::MenuItem(cyui::trLabel("Remove This Node").c_str()))remove({contextNode});
        }
        if(ImGui::MenuItem(cyui::trLabel("Add DSP...").c_str())) {search.clear();ImGui::CloseCurrentPopup();hasWire=false;addRequested=true;}
        ImGui::EndPopup();
    }
    if(hovered && !io.WantTextInput) {
        if(ImGui::IsKeyPressed(ImGuiKey_Tab)||ImGui::IsMouseDoubleClicked(0)) {
            newPosition=view.world({io.MousePos.x-origin.x,io.MousePos.y-origin.y});search.clear();ImGui::OpenPopup("Add DSP");
        }
        if(ImGui::IsKeyPressed(ImGuiKey_F))fitPending=true;
        if(!io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_A)) {try {arrange();}catch(const std::exception& e){error=e.what();}}
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Z))undo();
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Y))redo();
    }
    if(addRequested)ImGui::OpenPopup("Add DSP");
    nodeSearch();
    ImGui::EndChild();
    nodeSearch();
    // The node model owns routing; its parameter inspector uses the same shared audio controls as the mixer.
    if (!selection.empty()) {
        const Id id=*selection.begin();
        auto updated=Json::Array();
        for(auto node:graph["nodes"].Elements()) {
            if(node["id"].U()==id) {
                ImGui::SeparatorText(static_cast<const Json&>(node)["name"].S(static_cast<const Json&>(node)["processor"].S()).c_str());
                if(auto* descriptor=audioEffect(node["processor"].S())) {
                    unsigned index=0;
                    for(auto& parameter:descriptor->parameters) {
                        if(index++%5)ImGui::SameLine();
                        auto key=std::string(parameter.key), label=std::string(audio_ui::parameterLabel(parameter.labelKey)),unit=std::string(parameter.unit);
                        const auto& read=node;
                        auto automation=read["params"].Has(key)?graphParameter(read["params"][key]):Parameter{parameter.defaultValue};
                        float value=automation.at(time);
                        ImGui::PushID(int(id));ImGui::PushID(key.c_str());ImGui::BeginGroup();
                        bool changed=cyui::knob("##value",&value,parameter.minimum,parameter.maximum,parameter.defaultValue,
                            {30,0,label.c_str(),"%.2f",unit.c_str(),parameter.minimum>0&&parameter.maximum/parameter.minimum>100?cyui::knobLogarithmic:0});
                        if(ImGui::IsItemActivated() || (changed&&!ImGui::IsItemActive()))checkpoint();
                        if(changed){if(automation.keys.empty())automation.value=value;else automation.setKey({time,value,Curve::Smooth,0});node["params"][key]=parameterJson(automation);}
                        if(ImGui::SmallButton(cyui::trLabel("K").c_str())){checkpoint();automationNode=id;automationKey=key;automation.setKey({time,value,Curve::Smooth,0});node["params"][key]=parameterJson(automation);}
                        cyui::tooltip("Edit automation at the playhead");
                        ImGui::EndGroup();ImGui::PopID();ImGui::PopID();
                    }
                }
            }
            updated.Push(node);
        }
        graph["nodes"]=updated;
    }
    if (automationNode) {
        Json node;
        for (auto &n : graph["nodes"].Elements())
            if (n["id"].U() == automationNode)
                node = n;
        if (!node.IsNull()) {
            auto *d = audioEffect(node["processor"].S());
            if (d && ImGui::BeginCombo(cyui::trLabel("Automation DSP").c_str(), automationKey.c_str())) {
                for (auto &p : d->parameters)
                    if (ImGui::Selectable(std::string(p.key).c_str(), automationKey == p.key))
                        automationKey = p.key;
                ImGui::EndCombo();
            }
            if (node["params"].Has(automationKey)) {
                auto p = graphParameter(node["params"][automationKey]);
                double end=p.keys.empty()?std::max(1.,time):std::max({1.,time,p.keys.back().time});
                auto identity=std::to_string(automationNode)+"/"+automationKey;
                if(curveIdentity!=identity){curveIdentity=identity;curveState={};curveState.view.x.range={0,end};}
                auto curve=parameterCurve("##DSPLane",curveState,p,end,revision);
                if(curve.committed()) {
                    auto candidate=graph;auto ns=Json::Array();
                    for(auto n:candidate["nodes"].Elements()){if(n["id"].U()==automationNode)n["params"][automationKey]=parameterJson(curve.value);ns.Push(n);}
                    candidate["nodes"]=ns;
                    try{decodeDspGraph(candidate);if(candidate.Dump(0)!=graph.Dump(0)){checkpoint();graph=candidate;}p=curve.value;}catch(const std::exception& e){error=e.what();}
                }
                for (size_t i = 0; i < p.keys.size(); ++i) {
                    ImGui::PushID(int(i));
                    auto key = p.keys[i];
                    ImGui::SetNextItemWidth(95);
                    bool edited = ImGui::InputDouble(cyui::trLabel("s").c_str(), &key.time, 0, 0, "%.4f",
                                                     ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(95);
                    edited |= ImGui::InputFloat(cyui::trLabel("Value").c_str(), &key.value, 0, 0, "%.4f",
                                                ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    int curve = int(key.curve);
                    ImGui::SetNextItemWidth(110);
                    edited |= ImGui::Combo(cyui::trLabel("Curve").c_str(), &curve, "Linear\0Smooth\0Step\0Curved\0");
                    key.curve = Curve(curve);
                    if (curve == 3)
                        edited |= ImGui::SliderFloat(cyui::trLabel("Shape").c_str(), &key.shape, -1, 1);
                    bool removeKey = ImGui::SmallButton(cyui::trLabel("Remove Keyframe").c_str());
                    if (edited || removeKey) {
                        try {
                            key.time = std::clamp(key.time, 0., 86400.);
                            p.keys.erase(p.keys.begin() + i);
                            if (!removeKey)
                                p.setKey(key);
                            auto candidate = graph;
                            auto nodes = Json::Array();
                            for (auto n : candidate["nodes"].Elements()) {
                                if (n["id"].U() == automationNode)
                                    n["params"][automationKey] = parameterJson(p);
                                nodes.Push(n);
                            }
                            candidate["nodes"] = nodes;
                            decodeDspGraph(candidate);
                            checkpoint();
                            graph = candidate;
                            error.clear();
                        } catch (const std::exception &e) {
                            error = e.what();
                        }
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
            }
        }
    }
    if (!error.empty())
        ImGui::TextColored(cyui::vec(cyui::palette().warning), "%s", error.c_str());
    return before != graph.Dump(0);
}
} // namespace sf

#include "ui/DspCanvas.h"
#include "adapters/CyDawAudio.h"
#include "adapters/CyDawWidgets.h"
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
    past.clear();
    future.clear();
    selection.clear();
    hasWire = false;
    dragging = 0;
    beforeDrag.reset();
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
    return true;
}
bool DspCanvas::redo() {
    if (future.empty())
        return false;
    past.push_back(graph);
    graph = future.back();
    future.pop_back();
    selection.clear();
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
        throw std::runtime_error("128 nœuds DSP maximum");
    Id id = 1;
    for (const auto &n : graph["nodes"].Elements())
        id = std::max(id, n["id"].U() + 1);
    if (id > 2147483647)
        throw std::runtime_error("Identités DSP épuisées");
    auto effect = processor == "sf.sum" ? Effect{} : instantiateAudioEffect(processor);
    if (processor != "sf.sum" && (processor == "cydaw.graph" || externalAudio(processor)))
        throw std::runtime_error("DSP de catalogue requis");
    checkpoint();
    Json node = Json::Object();
    node["id"] = id;
    node["processor"] = processor;
    node["name"] = processor == "sf.sum" ? "Somme stéréo" : std::string(audioEffect(processor)->displayName);
    node["pos"] = pointJson(point);
    node["params"] = Json::Object();
    for (auto &[key, parameter] : effect.params)
        node["params"][key] = parameter.value;
    graph["nodes"].Push(node);
    selection = {id};
    return id;
}
void DspCanvas::connect(Id from, Id to) {
    if (from == outputBoundary || to == 0 || from == to)
        throw std::runtime_error("Connecter une sortie à une entrée distincte");
    to = to == outputBoundary ? 0 : to;
    bool existsFrom = from == 0, existsTo = to == 0, many = false;
    for (auto &node : graph["nodes"].Elements()) {
        existsFrom |= node["id"].U() == from;
        existsTo |= node["id"].U() == to;
        if (node["id"].U() == to)
            many = node["processor"].S() == "sf.sum";
    }
    if (!existsFrom || !existsTo)
        throw std::runtime_error("Port DSP absent");
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
    auto decoded = decodeDspGraph(check);
    std::map<Id, int> depth;
    depth[0] = 0;
    for (auto id : decoded.order) {
        depth[id] = 1;
        for (auto &w : graph["connections"].Elements())
            if (w["destination"].U() == id)
                depth[id] = std::max(depth[id], depth[w["source"].U()] + 1);
    }
    checkpoint();
    std::map<int, std::vector<Id>> columns;
    int maximum = 0;
    columns[0].push_back(0);
    for (auto &[id, d] : depth)
        if (id) {
            columns[d].push_back(id);
            maximum = std::max(maximum, d);
        }
    columns[maximum + 1].push_back(outputBoundary);
    for (auto &[column, members] : columns) {
        float total = 0;
        std::vector<float> heights;
        for (auto id : members) {
            float h = 68;
            for (auto &n : graph["nodes"].Elements())
                if (n["id"].U() == id) {
                    auto *d = audioEffect(n["processor"].S());
                    if (d)
                        h += ((d->parameters.size() + 3) / 4) * 56.f;
                }
            heights.push_back(h);
            total += h + 32;
        }
        float y = 200 - total * .5f;
        for (unsigned row = 0; row < members.size(); ++row) {
            setPosition(members[row], {20.f + column * 272.f, y});
            y += heights[row] + 32;
        }
    }
    fitPending = true;
}
void DspCanvas::select(Id node, bool extend) {
    if (!extend)
        selection.clear();
    if (extend && selection.contains(node))
        selection.erase(node);
    else
        selection.insert(node);
}
void DspCanvas::nodeSearch() {
    if (ImGui::BeginPopup("Ajouter DSP")) {
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputText("Rechercher", &search);
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
        entry("sf.sum", "Somme stéréo");
        for (auto &d : cydaw::engine::effectCatalogue)
            if (!d.convertsChannels())
                entry("cydaw." + std::string(d.typeKey), std::string(d.displayName));
        ImGui::EndPopup();
    }
}
bool DspCanvas::draw(double time) {
    auto before = graph.Dump(0);
    if (ImGui::Button("Ajouter DSP…")) {
        newPosition = view.world({300, 120});
        search.clear();
        ImGui::OpenPopup("Ajouter DSP");
    }
    ImGui::SameLine();
    if (ImGui::Button("Tout voir [F]"))
        fitPending = true;
    ImGui::SameLine();
    if (ImGui::Button("Organiser [A]"))
        try {
            arrange();
        } catch (const std::exception &e) {
            error = e.what();
        }
    ImGui::SameLine();
    if (ImGui::Button("Annuler"))
        undo();
    ImGui::SameLine();
    if (ImGui::Button("Rétablir"))
        redo();
    ImGui::TextDisabled(
        "Tab/double-clic : DSP · molette : zoom · bouton central : déplacement · Suppr : retirer");
    ImGui::BeginChild("dspCanvas",
                      {0, std::max(160.f, ImGui::GetContentRegionAvail().y * (automationNode ? .62f : 1.f))},
                      true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    origin = ImGui::GetCursorScreenPos();
    auto size = ImGui::GetContentRegionAvail();
    auto *list = ImGui::GetWindowDrawList();
    auto screen = [&](sx::float2 world) {
        auto p = view.screen(world);
        return ImVec2(origin.x + p.x, origin.y + p.y);
    };
    auto world = [&](ImVec2 screen) { return view.world({screen.x - origin.x, screen.y - origin.y}); };
    if (fitPending) {
        sx::float2 low = position(0), high = position(outputBoundary) + sx::float2{212, 120};
        for (auto &n : graph["nodes"].Elements()) {
            auto p = position(n["id"].U());
            auto *d = audioEffect(n["processor"].S());
            float h = 68.f + (d ? (d->parameters.size() + 3) / 4 : 0) * 56.f;
            low.x = std::min(low.x, p.x);
            low.y = std::min(low.y, p.y);
            high.x = std::max(high.x, p.x + 212);
            high.y = std::max(high.y, p.y + h);
        }
        view.fit(low, high, {size.x, size.y});
        fitPending = false;
    }
    auto &io = ImGui::GetIO();
    bool hovered = ImGui::IsWindowHovered();
    if (hovered && io.MouseWheel)
        view.zoomAt({io.MousePos.x - origin.x, io.MousePos.y - origin.y},
                    view.zoom * std::pow(1.12f, io.MouseWheel));
    if (hovered && ImGui::IsMouseDragging(2)) {
        view.pan.x += io.MouseDelta.x;
        view.pan.y += io.MouseDelta.y;
    }
    ImGui::SetWindowFontScale(view.zoom);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * view.zoom, 2 * view.zoom});
    for (float x = std::fmod(view.pan.x, 32 * view.zoom); x < size.x; x += 32 * view.zoom)
        list->AddLine({origin.x + x, origin.y}, {origin.x + x, origin.y + size.y}, IM_COL32(56, 57, 64, 100));
    for (float y = std::fmod(view.pan.y, 32 * view.zoom); y < size.y; y += 32 * view.zoom)
        list->AddLine({origin.x, origin.y + y}, {origin.x + size.x, origin.y + y}, IM_COL32(56, 57, 64, 100));
    cards.clear();
    auto layout = [&](Id id, unsigned controls) {
        auto p = position(id);
        auto a = screen(p), b = screen(p + sx::float2{212, 68.f + ((controls + 3) / 4) * 56.f});
        cards.push_back({id, p, a, b, {a.x, a.y + 42 * view.zoom}, {b.x, a.y + 42 * view.zoom}});
    };
    layout(0, 0);
    layout(outputBoundary, 0);
    for (auto &node : graph["nodes"].Elements()) {
        auto *d = audioEffect(node["processor"].S());
        layout(node["id"].U(), d ? unsigned(d->parameters.size()) : 0);
    }
    auto card = [&](Id id) -> const Card * {
        for (auto &c : cards)
            if (c.id == id)
                return &c;
        return nullptr;
    };
    // Per-canvas cable context, isolated from other clip editors.

    for (auto &w : graph["connections"].Elements()) {
        auto *a = card(w["source"].U()),
             *b = card(w["destination"].U() ? w["destination"].U() : outputBoundary);
        if (!a || !b)
            continue;
        auto from = a->output, to = b->input;
        float bend = std::max(50.f * view.zoom, std::abs(to.x - from.x) * .5f);
        list->AddBezierCubic(from, {from.x + bend, from.y}, {to.x - bend, to.y}, to,
                             IM_COL32(39, 183, 190, 255), 2 * view.zoom);
        if (hovered && ImGui::IsMouseClicked(1))
            for (int i = 0; i <= 40; ++i) {
                float t = i / 40.f, u = 1 - t;
                ImVec2 p{u * u * u * from.x + 3 * u * u * t * (from.x + bend) +
                             3 * u * t * t * (to.x - bend) + t * t * t * to.x,
                         u * u * u * from.y + 3 * u * u * t * from.y + 3 * u * t * t * to.y +
                             t * t * t * to.y};
                if (std::hypot(p.x - io.MousePos.x, p.y - io.MousePos.y) < 7) {
                    disconnectFrom = w["source"].U();
                    disconnectTo = w["destination"].U();
                    ImGui::OpenPopup("Câble DSP");
                    break;
                }
            }
    }
    if (ImGui::BeginPopup("Câble DSP")) {
        if (ImGui::MenuItem("Déconnecter"))
            disconnect(disconnectFrom, disconnectTo);
        ImGui::EndPopup();
    }
    bool hit = false;
    auto updated = Json::Array();
    for (const auto &c : cards) {
        ImGui::PushID(int(c.id));
        std::string label = c.id == 0 ? "ENTRÉE STÉRÉO" : c.id == outputBoundary ? "SORTIE STÉRÉO" : "";
        Json node;
        for (const auto &n : graph["nodes"].Elements())
            if (n["id"].U() == c.id) {
                node = n;
                label = n["name"].S(n["processor"].S());
                break;
            }
        cydaw::v2::ui::graphCard(list, c.min, c.max, 30 * view.zoom, IM_COL32(66, 179, 184, 255),
                                 c.id == 0 || c.id == outputBoundary, selection.contains(c.id), view.zoom);
        list->AddText(nullptr, ImGui::GetFontSize(), {c.min.x + 10 * view.zoom, c.min.y + 7 * view.zoom},
                      IM_COL32(227, 230, 238, 255), label.c_str());
        ImGui::SetCursorScreenPos(c.min);
        ImGui::InvisibleButton("header", {c.max.x - c.min.x, 28 * view.zoom});
        if (ImGui::IsItemClicked()) {
            hit = true;
            select(c.id, io.KeyCtrl);
            if (selection.contains(c.id)) {
                dragging = c.id;
                mouseOrigin = io.MousePos;
                moveOrigins.clear();
                for (auto selected : selection)
                    moveOrigins[selected] = position(selected);
                beforeDrag = graph;
            }
        }
        if (ImGui::BeginPopupContextItem("nodeMenu")) {
            if (c.id && c.id != outputBoundary) {
                std::string name = node["name"].S();
                if (ImGui::InputText("Nom", &name)) {
                    checkpoint();
                    auto nodes = Json::Array();
                    for (auto n : graph["nodes"].Elements()) {
                        if (n["id"].U() == c.id)
                            n["name"] = name;
                        nodes.Push(n);
                    }
                    graph["nodes"] = nodes;
                }
                if (ImGui::MenuItem("Dupliquer")) {
                    auto added = add(node["processor"].S(), c.world + sx::float2{40, 40});
                    auto nodes = Json::Array();
                    for (auto n : graph["nodes"].Elements()) {
                        if (n["id"].U() == added) {
                            n["params"] = node["params"];
                            n["name"] = name + " copie";
                        }
                        nodes.Push(n);
                    }
                    graph["nodes"] = nodes;
                }
            }
            if (c.id && c.id != outputBoundary && ImGui::MenuItem("Retirer ce nœud")) {
                remove({c.id});
            }
            ImGui::EndPopup();
        }
        auto port = [&](bool output, ImVec2 at) {
            if ((c.id == 0 && !output) || (c.id == outputBoundary && output))
                return;
            list->AddCircleFilled(at, 5 * view.zoom, IM_COL32(40, 195, 200, 255));
            ImGui::SetCursorScreenPos({at.x - 8 * view.zoom, at.y - 8 * view.zoom});
            ImGui::InvisibleButton(output ? "out" : "in", {16 * view.zoom, 16 * view.zoom});
            if (ImGui::IsMouseHoveringRect({at.x - 8 * view.zoom, at.y - 8 * view.zoom},
                                           {at.x + 8 * view.zoom, at.y + 8 * view.zoom}) &&
                ImGui::IsMouseClicked(0)) {
                hit = true;
                wire = c.id;
                wireOutput = output;
                hasWire = true;
            }
            if (hasWire &&
                ImGui::IsMouseHoveringRect({at.x - 8 * view.zoom, at.y - 8 * view.zoom},
                                           {at.x + 8 * view.zoom, at.y + 8 * view.zoom}) &&
                ImGui::IsMouseReleased(0)) {
                hit = true;
                try {
                    if (output != wireOutput)
                        connect(output ? c.id : wire, output ? wire : c.id);
                } catch (const std::exception &e) {
                    error = e.what();
                }
                hasWire = false;
            }
        };
        port(false, c.input);
        port(true, c.output);
        if (!node.IsNull()) {
            auto *d = audioEffect(node["processor"].S());
            unsigned row = 0;
            if (d)
                for (auto &parameter : d->parameters) {
                    float y = c.min.y + (62 + (row / 4) * 56) * view.zoom;
                    auto key = std::string(parameter.key),
                         label = std::string(cydaw::v2::ui::parameterLabel(parameter.labelKey)),
                         unit = std::string(parameter.unit);
                    ImGui::SetCursorScreenPos({c.min.x + (6 + (row % 4) * 50) * view.zoom, y});
                    const auto &read = node;
                    auto automation = read["params"].Has(key) ? graphParameter(read["params"][key])
                                                              : Parameter{parameter.defaultValue};
                    float value = automation.at(time);
                    ImGui::PushID(key.c_str());
                    bool changed = cydaw::v2::ui::knob(
                        "##value", &value, parameter.minimum, parameter.maximum, parameter.defaultValue,
                        {30.f * view.zoom, 0, label.c_str(), "%.2f", unit.c_str(),
                         parameter.minimum > 0 && parameter.maximum / parameter.minimum > 100
                             ? cydaw::v2::ui::knobLogarithmic
                             : 0});
                    if (ImGui::IsItemActivated())
                        checkpoint();
                    if (changed) {
                        if (automation.keys.empty())
                            automation.value = value;
                        else
                            automation.setKey({time, value, Curve::Smooth, 0});
                        node["params"][key] = parameterJson(automation);
                    }
                    ImGui::SetCursorScreenPos({c.min.x + (38 + (row % 4) * 50) * view.zoom, y});
                    if (ImGui::SmallButton("K")) {
                        checkpoint();
                        automationNode = c.id;
                        automationKey = key;
                        automation.setKey({time, value, Curve::Smooth, 0});
                        node["params"][key] = parameterJson(automation);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Automation à %.3f s / %s", time, key.c_str());
                    ImGui::PopID();
                    ++row;
                }
            updated.Push(node);
        }
        ImGui::PopID();
    }
    // Apply parameter changes by ID so a node removed while drawing is not resurrected.
    auto merged = Json::Array();
    for (auto n : graph["nodes"].Elements()) {
        for (const auto &u : updated.Elements())
            if (u["id"].U() == n["id"].U())
                n["params"] = u["params"];
        merged.Push(n);
    }
    graph["nodes"] = merged;
    if (beforeDrag && ImGui::IsMouseDragging(0)) {
        auto delta = (sx::float2{io.MousePos.x - mouseOrigin.x, io.MousePos.y - mouseOrigin.y}) / view.zoom;
        for (auto &[id, p] : moveOrigins)
            setPosition(id, p + delta);
    }
    if (beforeDrag && ImGui::IsMouseReleased(0)) {
        if (beforeDrag->Dump(0) != graph.Dump(0)) {
            past.push_back(*beforeDrag);
            future.clear();
            if (past.size() > 64)
                past.erase(past.begin());
        }
        dragging = 0;
        beforeDrag.reset();
    }
    if (hasWire) {
        auto *c = card(wire);
        if (c) {
            auto from = wireOutput ? c->output : c->input;
            list->AddLine(from, io.MousePos, IM_COL32(255, 166, 70, 255), 2);
        }
        if (ImGui::IsMouseReleased(0) && !hit) {
            newPosition = world(io.MousePos);
            search.clear();
            ImGui::OpenPopup("Ajouter DSP");
        }
    }
    if (hovered && !hit && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0) && !hasWire) {
        box = true;
        boxOrigin = io.MousePos;
        if (!io.KeyCtrl)
            selection.clear();
    }
    if (box) {
        ImVec2 low{std::min(boxOrigin.x, io.MousePos.x), std::min(boxOrigin.y, io.MousePos.y)},
            high{std::max(boxOrigin.x, io.MousePos.x), std::max(boxOrigin.y, io.MousePos.y)};
        list->AddRect(low, high, IM_COL32(255, 162, 72, 255));
        if (ImGui::IsMouseReleased(0)) {
            for (auto &c : cards)
                if (c.max.x >= low.x && c.min.x <= high.x && c.max.y >= low.y && c.min.y <= high.y)
                    selection.insert(c.id);
            box = false;
        }
    }
    if (hovered && (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsMouseDoubleClicked(0))) {
        newPosition = world(io.MousePos);
        search.clear();
        ImGui::OpenPopup("Ajouter DSP");
    }
    if (hovered && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F))
            fitPending = true;
        if (ImGui::IsKeyPressed(ImGuiKey_A))
            try {
                arrange();
            } catch (const std::exception &e) {
                error = e.what();
            }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            remove(selection);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
            undo();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
            redo();
    }
    nodeSearch();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1);
    ImGui::EndChild();
    nodeSearch();
    if (automationNode) {
        Json node;
        for (auto &n : graph["nodes"].Elements())
            if (n["id"].U() == automationNode)
                node = n;
        if (!node.IsNull()) {
            auto *d = audioEffect(node["processor"].S());
            if (d && ImGui::BeginCombo("Automation DSP", automationKey.c_str())) {
                for (auto &p : d->parameters)
                    if (ImGui::Selectable(std::string(p.key).c_str(), automationKey == p.key))
                        automationKey = p.key;
                ImGui::EndCombo();
            }
            if (node["params"].Has(automationKey)) {
                auto p = graphParameter(node["params"][automationKey]);
                float values[128];
                double end = p.keys.empty() ? std::max(1., time) : std::max({1., time, p.keys.back().time});
                for (int i = 0; i < 128; ++i)
                    values[i] = p.at(end * i / 127.);
                ImGui::PlotLines("##DSPLane", values, 128, 0, automationKey.c_str(), FLT_MAX, FLT_MAX,
                                 {0, 70});
                for (size_t i = 0; i < p.keys.size(); ++i) {
                    ImGui::PushID(int(i));
                    auto key = p.keys[i];
                    ImGui::SetNextItemWidth(95);
                    bool edited = ImGui::InputDouble("s", &key.time, 0, 0, "%.4f",
                                                     ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(95);
                    edited |= ImGui::InputFloat("Valeur", &key.value, 0, 0, "%.4f",
                                                ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();
                    int curve = int(key.curve);
                    ImGui::SetNextItemWidth(110);
                    edited |= ImGui::Combo("Courbe", &curve, "Linéaire\0Douce\0Palier\0Courbée\0");
                    key.curve = Curve(curve);
                    if (curve == 3)
                        edited |= ImGui::SliderFloat("Forme", &key.shape, -1, 1);
                    bool removeKey = ImGui::SmallButton("Retirer la clé");
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
        ImGui::TextColored({1, .65f, .3f, 1}, "%s", error.c_str());
    return before != graph.Dump(0);
}
} // namespace sf

#include "adapters/LayerGraph.h"
#include "plugins/Effects.h"
#include "render/Grading.h"
#include <algorithm>
#include <set>
namespace sx {
// Shaderixel's blend names and Photoshop menu order (Document.cpp, not compiled here).
const int kBlendMenuOrder[] = {0, 13, -1, 6, 1, 10, 14, 25, -1, 7, 2, 9, 4, 26, -1, 3, 12, 11, 15, 16, 17, 18, -1, 8, 19, 5, 20, -1, 21, 22, 23, 24};
const int kBlendMenuCount = int(sizeof(kBlendMenuOrder) / sizeof(int));
const char *BlendModeName(BlendMode m) {
    static const char *names[] = {"Normal",     "Multiply",     "Screen",      "Overlay",     "Linear Dodge (Add)",
                                  "Subtract",   "Darken",       "Lighten",     "Difference",  "Color Dodge",
                                  "Color Burn", "Hard Light",   "Soft Light",  "Dissolve",    "Linear Burn",
                                  "Vivid Light", "Linear Light", "Pin Light",  "Hard Mix",    "Exclusion",
                                  "Divide",     "Hue",          "Saturation",  "Color",       "Luminosity",
                                  "Darker Color", "Lighter Color"};
    int i = int(m);
    return i >= 0 && i < int(BlendMode::Count) ? names[i] : "?";
}
} // namespace sx
namespace sf {
Json parameterJson(const Parameter &p) {
    Json j = Json::Object();
    j["value"] = p.value;
    j["keys"] = Json::Array();
    for (auto &k : p.keys) {
        Json f = Json::Object();
        f["time"] = k.time;
        f["value"] = k.value;
        f["curve"] = int(k.curve);
        f["shape"] = k.shape;
        j["keys"].Push(f);
    }
    return j;
}
Parameter graphParameter(const Json &j) {
    Parameter p;
    auto number = [](const Json &v) { return v.GetType() == Json::Type::Number && std::isfinite(v.Num()); };
    if (j.GetType() == Json::Type::Number) {
        if (!number(j))
            throw std::runtime_error("Non-finite value");
        p.value = j.F();
        return p;
    }
    if (!j.IsObject() || !number(j["value"]) || !j["keys"].IsArray() || j["keys"].Size() > 100000)
        throw std::runtime_error("Invalid graph automation");
    p.value = j["value"].F();
    double previous = -1e100;
    for (auto &f : j["keys"].Elements()) {
        if (!number(f["time"]) || !number(f["value"]) || f["time"].Num() <= previous || f["curve"].I() < 0 ||
            f["curve"].I() > 3 || !std::isfinite(f["shape"].F()) || std::abs(f["shape"].F()) > 1)
            throw std::runtime_error("Invalid graph key");
        previous = f["time"].Num();
        p.keys.push_back({previous, f["value"].F(), Curve(f["curve"].I()), f["shape"].F()});
    }
    return p;
}
Json defaultLayerGraph(const Project &p) {
    Json j = Json::Object();
    j["schema"] = 1;
    j["engine"] = "shaderixel.composition";
    j["nodes"] = Json::Array();
    Id id = 1, previous = 0;
    float row = 0;
    for (auto it = p.tracks.rbegin(); it != p.tracks.rend(); ++it)
        if (it->kind == TrackKind::Video) {
            Json source = Json::Object();
            source["id"] = id++;
            source["kind"] = "surface";
            source["surface"] = it->id;
            source["name"] = it->name;
            source["pos"] = Json::Array();
            source["pos"].Push(20);
            source["pos"].Push(row);
            source["inputs"] = Json::Array();
            j["nodes"].Push(source);
            Json blend = Json::Object();
            blend["id"] = id++;
            // The track's blending: a mode and opacity, or a library blend effect wired below / track / mask.
            if (it->blendEffect.id.empty()) {
                blend["kind"] = "blend";
                blend["blend"] = it->blend;
                blend["opacity"] = it->opacity;
            } else {
                blend["kind"] = "effect";
                blend["effect"] = Json::Object();
                blend["effect"]["id"] = it->blendEffect.id;
                blend["effect"]["code"] = it->blendEffect.code;
                blend["params"] = Json::Object();
                for (auto &[key, value] : it->blendEffect.params)
                    blend["params"][key] = parameterJson(value);
                if (!blend["params"].Has("sx_Opacity"))
                    blend["params"]["sx_Opacity"] = it->opacity;
            }
            blend["name"] = "Blend / " + it->name;
            blend["pos"] = Json::Array();
            blend["pos"].Push(250 + row * .6f);
            blend["pos"].Push(row);
            blend["inputs"] = Json::Array();
            blend["inputs"].Push(previous);
            blend["inputs"].Push(source["id"]);
            blend["inputs"].Push(0);
            previous = blend["id"].U();
            j["nodes"].Push(blend);
            row += 210;
        }
    Json out = Json::Object();
    out["id"] = id;
    out["kind"] = "output";
    out["name"] = "Video Output";
    out["inputs"] = Json::Array();
    out["inputs"].Push(previous);
    out["pos"] = Json::Array();
    out["pos"].Push(540 + row * .6f);
    out["pos"].Push(row * .5f);
    j["nodes"].Push(out);
    j["output"] = id;
    return j;
}
Json defaultTrackGraph(const Project &p, Id track) {
    auto t = std::find_if(p.tracks.begin(), p.tracks.end(), [&](const Track &x) { return x.id == track; });
    Json j = Json::Object();
    j["schema"] = 1;
    j["engine"] = "shaderixel.composition";
    j["nodes"] = Json::Array();
    Json source = Json::Object();
    source["id"] = 1;
    source["kind"] = "surface";
    source["surface"] = track;
    source["name"] = t != p.tracks.end() ? t->name : std::string("Track");
    source["pos"] = Json::Array();
    source["pos"].Push(20);
    source["pos"].Push(40);
    source["inputs"] = Json::Array();
    j["nodes"].Push(source);
    Json out = Json::Object();
    out["id"] = 2;
    out["kind"] = "output";
    out["name"] = "Track Output";
    out["inputs"] = Json::Array();
    out["inputs"].Push(1);
    out["pos"] = Json::Array();
    out["pos"].Push(480);
    out["pos"].Push(40);
    j["nodes"].Push(out);
    j["output"] = 2;
    return j;
}
LayerComposition decodeTrackGraph(const Json &json, const Project &p, Id track) {
    auto position = std::find_if(p.tracks.begin(), p.tracks.end(), [&](const Track &t) { return t.id == track; });
    if (position == p.tracks.end() || position->kind != TrackKind::Video)
        throw std::runtime_error("Track graph: video track not found");
    auto result = decodeLayerGraph(json, p);
    // Tracks render bottom up: only the track itself and the tracks below it are ready to be read.
    for (auto &node : result.graph.nodes)
        if (node.kind == sx::NodeKind::Surface && node.surface != track) {
            auto other = std::find_if(p.tracks.begin(), p.tracks.end(),
                                      [&](const Track &t) { return t.id == node.surface; });
            if (other <= position)
                throw std::runtime_error("Track graph: a surface may only read this track or a track below it");
        }
    return result;
}
Id appendTrackEffect(Json &graph, const Effect &effect, const std::string &name) {
    Id next = 1;
    float x = 20, y = 40;
    for (auto &n : graph["nodes"].Elements())
        next = std::max(next, n["id"].U() + 1);
    auto nodes = Json::Array();
    Json added = Json::Object();
    added["id"] = next;
    added["kind"] = "effect";
    added["name"] = name;
    added["inputs"] = Json::Array();
    for (auto n : graph["nodes"].Elements()) {
        if (n["id"].U() == graph["output"].U()) {
            // The effect takes what fed the output, and the output moves right to make room.
            added["inputs"].Push(n["inputs"].Size() ? n["inputs"].At(0) : Json(sx::u32(0)));
            x = n["pos"].At(0).F();
            y = n["pos"].At(1).F();
            n["inputs"] = Json::Array();
            n["inputs"].Push(next);
            n["pos"] = Json::Array();
            n["pos"].Push(x + 230);
            n["pos"].Push(y);
        }
        nodes.Push(n);
    }
    for (unsigned k = 1, count = effectInputs(effectCode(effect)); k < count; ++k)
        added["inputs"].Push(0);
    added["pos"] = Json::Array();
    added["pos"].Push(x);
    added["pos"].Push(y);
    added["effect"] = Json::Object();
    added["effect"]["id"] = effect.id;
    added["effect"]["code"] = effect.code;
    added["effect"]["graph"] = effect.graph;
    added["effect"]["grading"] = effect.grading;
    added["params"] = Json::Object();
    for (auto &[key, value] : effect.params)
        added["params"][key] = parameterJson(value);
    nodes.Push(added);
    graph["nodes"] = nodes;
    return next;
}
Json withoutSurface(const Json &graph, Id track) {
    std::set<Id> removed;
    for (auto &node : graph["nodes"].Elements())
        if (node["kind"].S() == "surface" && node["surface"].U() == track)
            removed.insert(node["id"].U());
    if (removed.empty())
        return graph;
    auto result = graph;
    auto nodes = Json::Array();
    for (auto node : graph["nodes"].Elements()) {
        if (removed.count(node["id"].U()))
            continue;
        auto inputs = Json::Array();
        for (auto &input : node["inputs"].Elements())
            inputs.Push(removed.count(input.U()) ? Json(sx::u32(0)) : input);
        node["inputs"] = inputs;
        nodes.Push(node);
    }
    result["nodes"] = nodes;
    return removed.count(result["output"].U()) ? Json() : result;
}
void appendLayerSurface(Project &p, Id track, const std::string &name) {
    if (p.layerGraph.IsNull())
        return;
    auto &g = p.layerGraph;
    Id next = 1;
    for (auto &n : g["nodes"].Elements())
        next = std::max(next, n["id"].U() + 1);
    auto nodes = Json::Array();
    Json blend = Json::Object();
    blend["id"] = next + 1;
    blend["kind"] = "blend";
    blend["name"] = "Blend / " + name;
    blend["opacity"] = 1;
    blend["pos"] = Json::Array();
    blend["pos"].Push(320);
    blend["pos"].Push(50);
    blend["inputs"] = Json::Array();
    for (auto n : g["nodes"].Elements()) {
        if (n["id"].U() == g["output"].U()) {
            blend["inputs"].Push(n["inputs"].At(0));
            blend["inputs"].Push(next);
            blend["inputs"].Push(0);
            n["inputs"] = Json::Array();
            n["inputs"].Push(next + 1);
        }
        nodes.Push(n);
    }
    Json source = Json::Object();
    source["id"] = next;
    source["kind"] = "surface";
    source["surface"] = track;
    source["name"] = name;
    source["pos"] = Json::Array();
    source["pos"].Push(50);
    source["pos"].Push(50);
    source["inputs"] = Json::Array();
    nodes.Push(source);
    nodes.Push(blend);
    g["nodes"] = nodes;
}
LayerComposition decodeLayerGraph(const Json &j, const Project &p) {
    if (!j.IsObject() || j["schema"].I() != 1 || j["engine"].S() != "shaderixel.composition" ||
        !j["nodes"].IsArray() || j["nodes"].Size() > 128)
        throw std::runtime_error("Invalid Shaderixel composition graph");
    LayerComposition result;
    std::set<Id> ids;
    auto identity = [](const Json &v, bool zero = false) -> Id {
        if (v.GetType() != Json::Type::Number || !std::isfinite(v.Num()) || v.Num() < (zero ? 0 : 1) ||
            v.Num() > 2147483647 || std::floor(v.Num()) != v.Num())
            throw std::runtime_error("Invalid graph identity");
        return v.U();
    };
    for (auto &item : j["nodes"].Elements()) {
        sx::Node n;
        n.id = identity(item["id"]);
        if (!ids.insert(n.id).second)
            throw std::runtime_error("Duplicate node");
        int kind = item["kind"].I(-1);
        if (item["kind"].GetType() == Json::Type::String) {
            for (int k = 0; k < 5; ++k)
                if (item["kind"].S() == layerNodeKinds[k])
                    kind = k;
        }
        if (kind < 0 || kind > 4 ||
            (item["kind"].GetType() == Json::Type::Number && item["kind"].Num() != kind))
            throw std::runtime_error("Unknown Shaderixel type");
        n.kind = sx::NodeKind(kind);
        n.name = item["name"].S();
        if (!item["pos"].IsArray() || item["pos"].Size() != 2)
            throw std::runtime_error("Invalid graph position");
        n.pos = {item["pos"].At(0).F(), item["pos"].At(1).F()};
        if (!std::isfinite(n.pos.x) || !std::isfinite(n.pos.y) || std::abs(n.pos.x) > 1e6 ||
            std::abs(n.pos.y) > 1e6)
            throw std::runtime_error("Position out of bounds");
        n.bypass = item["bypass"].B();
        n.opacity = item["opacity"].F(1);
        int blend = item["blend"].I();
        if (blend < 0 || blend >= int(sx::BlendMode::Count) || !std::isfinite(n.opacity) || n.opacity < 0 ||
            n.opacity > 1)
            throw std::runtime_error("Invalid blend");
        n.blend = sx::BlendMode(blend);
        n.clip = item["clip"].B();
        n.channelOff = item["channelOff"].U();
        if (item["channelOff"].U() > 15)
            throw std::runtime_error("Invalid channel mask");
        auto ranges = [&](const char *key, int first, int count) {
            if (!item.Has(key))
                return;
            const auto &values = item[key];
            if (!values.IsArray() || values.Size() != size_t(count * 8))
                throw std::runtime_error("Invalid Blend If ranges");
            for (int c = 0; c < count; ++c)
                for (int side = 0; side < 2; ++side) {
                    auto *range = side ? n.blendIf.under[first + c] : n.blendIf.self[first + c];
                    int previous = 0;
                    for (int k = 0; k < 4; ++k) {
                        const auto &v = values.At(c * 8 + side * 4 + k);
                        if (v.GetType() != Json::Type::Number || !std::isfinite(v.Num()) ||
                            std::floor(v.Num()) != v.Num() || v.Num() < previous || v.Num() > 255)
                            throw std::runtime_error("Invalid Blend If handles");
                        previous = v.I();
                        range[k] = sx::u8(previous);
                    }
                }
        };
        ranges("blendIf", 0, 1);
        ranges("blendIfRGB", 1, 3);
        if (n.kind == sx::NodeKind::Surface) {
            n.surface = identity(item["surface"]);
            auto it = std::find_if(p.tracks.begin(), p.tracks.end(),
                                   [&](auto &t) { return t.id == n.surface && t.kind == TrackKind::Video; });
            if (it == p.tracks.end())
                throw std::runtime_error("Source timeline not found");
        }
        if (!item["inputs"].IsArray() || item["inputs"].Size() > 4)
            throw std::runtime_error("Invalid inputs");
        for (auto &v : item["inputs"].Elements())
            n.inputs.push_back(identity(v, true));
        for (auto &v : item["inputChannels"].Elements()) {
            if (v.Num() < 0 || v.Num() > 4 || std::floor(v.Num()) != v.Num())
                throw std::runtime_error("Invalid channel");
            n.inputChannel.push_back(v.U());
        }
        if (n.inputChannel.size() > n.inputs.size())
            throw std::runtime_error("Too many channels");
        for (auto &[key, value] : item["params"].Items())
            result.parameters[n.id][key] = graphParameter(value);
        if (n.kind == sx::NodeKind::Effect) {
            auto &f = item["effect"];
            Effect e;
            e.id = f["id"].S("custom");
            e.code = f["code"].S();
            e.graph = f["graph"];
            e.grading = f["grading"];
            e.enabled = f["enabled"].B(true);
            e.params = result.parameters[n.id];
            if (e.id == "grading")
                validateGrade(e);
            else {
                auto code = effectCode(e);
                if (code.empty())
                    throw std::runtime_error("A graph effect must embed its shader");
                if (code.size() > 1024 * 1024)
                    throw std::runtime_error("Shader too large");
            }
            n.fx.definition = std::make_shared<sx::CompositionEffect::Definition>();
            n.fx.definition->inputs = {"Image", "Image 2", "Mask", "Image 4"};
            n.fx.definition->inputs.resize(n.inputs.size());
            n.fx.customCode = e.code;
            result.effects[n.id] = e;
        }
        if (n.inputs.size() != size_t(sx::NodeInputCount(n)))
            throw std::runtime_error("Nombre de ports Shaderixel incorrect");
        for (auto &[key, param] : result.parameters[n.id])
            if (n.kind != sx::NodeKind::Effect) {
                if (n.kind != sx::NodeKind::Blend || key != "opacity")
                    throw std::runtime_error("Unknown blend parameter");
                auto valid = [](float v) { return v >= 0 && v <= 1; };
                if (!valid(param.value))
                    throw std::runtime_error("Invalid opacity");
                for (auto &k : param.keys)
                    if (!valid(k.value))
                        throw std::runtime_error("Invalid animated opacity");
            }
        result.graph.nodes.push_back(std::move(n));
    }
    result.graph.output = identity(j["output"]);
    auto *out = result.graph.Find(result.graph.output);
    if (!out || out->kind != sx::NodeKind::Output)
        throw std::runtime_error("Missing video output");
    for (auto &n : result.graph.nodes)
        for (size_t pin = 0; pin < n.inputs.size(); ++pin)
            if (n.inputs[pin]) {
                auto *source = result.graph.Find(n.inputs[pin]);
                if (!source || source->kind == sx::NodeKind::Output ||
                    result.graph.WouldCycle(source->id, n.id))
                    throw std::runtime_error("Connexion manquante ou cycle Shaderixel");
                if (sx::InputChannel(n, pin) && source->kind != sx::NodeKind::Split)
                    throw std::runtime_error("The channel must come from a Split node");
            }
    return result;
}
void setLayerGraphKey(Json &j, Id node, const std::string &key, Keyframe frame) {
    auto nodes = Json::Array();
    bool found = false;
    for (auto item : j["nodes"].Elements()) {
        if (item["id"].U() == node) {
            const auto &read = item;
            auto p = read["params"].Has(key) ? graphParameter(read["params"][key])
                                             : Parameter{key == "opacity" ? read["opacity"].F(1) : 0};
            p.setKey(frame);
            item["params"][key] = parameterJson(p);
            found = true;
        }
        nodes.Push(item);
    }
    if (!found)
        throw std::runtime_error("Node not found");
    j["nodes"] = nodes;
}
} // namespace sf

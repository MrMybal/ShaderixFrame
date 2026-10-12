#include "adapters/LayerGraph.h"
#include "plugins/Effects.h"
#include "render/Grading.h"
#include <set>
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
            throw std::runtime_error("Valeur non finie");
        p.value = j.F();
        return p;
    }
    if (!j.IsObject() || !number(j["value"]) || !j["keys"].IsArray() || j["keys"].Size() > 100000)
        throw std::runtime_error("Automation de graphe invalide");
    p.value = j["value"].F();
    double previous = -1e100;
    for (auto &f : j["keys"].Elements()) {
        if (!number(f["time"]) || !number(f["value"]) || f["time"].Num() <= previous || f["curve"].I() < 0 ||
            f["curve"].I() > 3 || !std::isfinite(f["shape"].F()) || std::abs(f["shape"].F()) > 1)
            throw std::runtime_error("Clé de graphe invalide");
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
            blend["kind"] = "blend";
            blend["name"] = "Fusion / " + it->name;
            blend["blend"] = 0;
            blend["opacity"] = 1;
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
    out["name"] = "Sortie vidéo";
    out["inputs"] = Json::Array();
    out["inputs"].Push(previous);
    out["pos"] = Json::Array();
    out["pos"].Push(540 + row * .6f);
    out["pos"].Push(row * .5f);
    j["nodes"].Push(out);
    j["output"] = id;
    return j;
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
    blend["name"] = "Fusion / " + name;
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
        throw std::runtime_error("Graphe de composition Shaderixel invalide");
    LayerComposition result;
    std::set<Id> ids;
    auto identity = [](const Json &v, bool zero = false) -> Id {
        if (v.GetType() != Json::Type::Number || !std::isfinite(v.Num()) || v.Num() < (zero ? 0 : 1) ||
            v.Num() > 2147483647 || std::floor(v.Num()) != v.Num())
            throw std::runtime_error("Identité de graphe invalide");
        return v.U();
    };
    for (auto &item : j["nodes"].Elements()) {
        sx::Node n;
        n.id = identity(item["id"]);
        if (!ids.insert(n.id).second)
            throw std::runtime_error("Nœud dupliqué");
        int kind = item["kind"].I(-1);
        if (item["kind"].GetType() == Json::Type::String) {
            for (int k = 0; k < 5; ++k)
                if (item["kind"].S() == layerNodeKinds[k])
                    kind = k;
        }
        if (kind < 0 || kind > 4 ||
            (item["kind"].GetType() == Json::Type::Number && item["kind"].Num() != kind))
            throw std::runtime_error("Type Shaderixel inconnu");
        n.kind = sx::NodeKind(kind);
        n.name = item["name"].S();
        if (!item["pos"].IsArray() || item["pos"].Size() != 2)
            throw std::runtime_error("Position du graphe invalide");
        n.pos = {item["pos"].At(0).F(), item["pos"].At(1).F()};
        if (!std::isfinite(n.pos.x) || !std::isfinite(n.pos.y) || std::abs(n.pos.x) > 1e6 ||
            std::abs(n.pos.y) > 1e6)
            throw std::runtime_error("Position hors limites");
        n.bypass = item["bypass"].B();
        n.opacity = item["opacity"].F(1);
        int blend = item["blend"].I();
        if (blend < 0 || blend >= int(sx::BlendMode::Count) || !std::isfinite(n.opacity) || n.opacity < 0 ||
            n.opacity > 1)
            throw std::runtime_error("Fusion invalide");
        n.blend = sx::BlendMode(blend);
        n.clip = item["clip"].B();
        n.channelOff = item["channelOff"].U();
        if (item["channelOff"].U() > 15)
            throw std::runtime_error("Masque canal invalide");
        auto ranges = [&](const char *key, int first, int count) {
            if (!item.Has(key))
                return;
            const auto &values = item[key];
            if (!values.IsArray() || values.Size() != size_t(count * 8))
                throw std::runtime_error("Plages Blend If invalides");
            for (int c = 0; c < count; ++c)
                for (int side = 0; side < 2; ++side) {
                    auto *range = side ? n.blendIf.under[first + c] : n.blendIf.self[first + c];
                    int previous = 0;
                    for (int k = 0; k < 4; ++k) {
                        const auto &v = values.At(c * 8 + side * 4 + k);
                        if (v.GetType() != Json::Type::Number || !std::isfinite(v.Num()) ||
                            std::floor(v.Num()) != v.Num() || v.Num() < previous || v.Num() > 255)
                            throw std::runtime_error("Poignées Blend If invalides");
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
                throw std::runtime_error("Timeline source introuvable");
        }
        if (!item["inputs"].IsArray() || item["inputs"].Size() > 4)
            throw std::runtime_error("Entrées invalides");
        for (auto &v : item["inputs"].Elements())
            n.inputs.push_back(identity(v, true));
        for (auto &v : item["inputChannels"].Elements()) {
            if (v.Num() < 0 || v.Num() > 4 || std::floor(v.Num()) != v.Num())
                throw std::runtime_error("Canal invalide");
            n.inputChannel.push_back(v.U());
        }
        if (n.inputChannel.size() > n.inputs.size())
            throw std::runtime_error("Canaux surnuméraires");
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
                    throw std::runtime_error("Un effet du graphe doit incorporer son shader");
                if (code.size() > 1024 * 1024)
                    throw std::runtime_error("Shader trop grand");
            }
            n.fx.definition = std::make_shared<sx::CompositionEffect::Definition>();
            n.fx.definition->inputs = {"Image", "Image 2", "Masque", "Image 4"};
            n.fx.definition->inputs.resize(n.inputs.size());
            n.fx.customCode = e.code;
            result.effects[n.id] = e;
        }
        if (n.inputs.size() != size_t(sx::NodeInputCount(n)))
            throw std::runtime_error("Nombre de ports Shaderixel incorrect");
        for (auto &[key, param] : result.parameters[n.id])
            if (n.kind != sx::NodeKind::Effect) {
                if (n.kind != sx::NodeKind::Blend || key != "opacity")
                    throw std::runtime_error("Paramètre de fusion inconnu");
                auto valid = [](float v) { return v >= 0 && v <= 1; };
                if (!valid(param.value))
                    throw std::runtime_error("Opacité invalide");
                for (auto &k : param.keys)
                    if (!valid(k.value))
                        throw std::runtime_error("Opacité animée invalide");
            }
        result.graph.nodes.push_back(std::move(n));
    }
    result.graph.output = identity(j["output"]);
    auto *out = result.graph.Find(result.graph.output);
    if (!out || out->kind != sx::NodeKind::Output)
        throw std::runtime_error("Sortie vidéo manquante");
    for (auto &n : result.graph.nodes)
        for (size_t pin = 0; pin < n.inputs.size(); ++pin)
            if (n.inputs[pin]) {
                auto *source = result.graph.Find(n.inputs[pin]);
                if (!source || source->kind == sx::NodeKind::Output ||
                    result.graph.WouldCycle(source->id, n.id))
                    throw std::runtime_error("Connexion manquante ou cycle Shaderixel");
                if (sx::InputChannel(n, pin) && source->kind != sx::NodeKind::Split)
                    throw std::runtime_error("Le canal doit venir d'un nœud Split");
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
        throw std::runtime_error("Nœud introuvable");
    j["nodes"] = nodes;
}
} // namespace sf

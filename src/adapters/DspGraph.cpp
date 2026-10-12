#include "adapters/DspGraph.h"
#include "adapters/CyDawAudio.h"
#include "adapters/LayerGraph.h"
#include <set>
namespace sf {
DspGraph decodeDspGraph(const Json &json) {
    using namespace cydaw::graph;
    if (!json.IsObject() || json["schema"].I() != 2 || json["nodes"].Size() > 128 ||
        json["connections"].Size() > 512)
        throw std::runtime_error("Graphe DSP v2 invalide ou trop grand");
    DspGraph result;
    auto &g = result.definition;
    g.schemaVersion = 2;
    g.reference = {json["id"].S("shaderixframe.dsp"), {2, 0, 0}};
    g.role = GraphRole::audioEffect;
    g.displayName = json["name"].S("Graphe DSP");
    auto port = [](PortId id, PortDirection direction, bool many = false) {
        GraphPort p;
        p.id = id;
        p.key = direction == PortDirection::input ? "in" : "out";
        p.displayName = p.key;
        p.direction = direction;
        p.type = PortType::audio;
        p.audioType = AudioPortType::stereo;
        p.rate = PortRate::audio;
        p.channels = 2;
        p.acceptsMultipleConnections = many;
        return p;
    };
    g.ports = {port(1, PortDirection::output, true), port(2, PortDirection::input)};
    auto position = [](const Json &point) {
        if (point.IsNull())
            return;
        if (!point.IsArray() || point.Size() != 2)
            throw std::runtime_error("Position DSP invalide");
        for (auto &v : point.Elements())
            if (v.GetType() != Json::Type::Number || !std::isfinite(v.Num()) || std::abs(v.Num()) > 1000000)
                throw std::runtime_error("Position DSP hors limites");
    };
    position(json["layout"]["input"]);
    position(json["layout"]["output"]);
    std::set<Id> ids;
    std::map<Id, unsigned> pending;
    std::map<Id, std::vector<Id>> downstream;
    for (auto &item : json["nodes"].Elements()) {
        position(item["pos"]);
        if (item["id"].GetType() != Json::Type::Number || item["id"].Num() < 1 ||
            item["id"].Num() > 2147483647 || std::floor(item["id"].Num()) != item["id"].Num())
            throw std::runtime_error("Identité de nœud DSP invalide");
        Id id = item["id"].U();
        if (!ids.insert(id).second)
            throw std::runtime_error("Nœud DSP dupliqué");
        auto processor = item["processor"].S();
        Effect effect;
        if (processor == "sf.sum")
            effect.id = "sf.sum";
        else {
            if (!processor.starts_with("cydaw.") || processor == "cydaw.graph")
                throw std::runtime_error("Processeur DSP non supporté dans le graphe");
            effect = instantiateAudioEffect(processor);
        }
        for (auto &[key, value] : item["params"].Items()) {
            auto *descriptor = audioEffect(processor);
            bool known = false;
            if (descriptor)
                for (auto &parameter : descriptor->parameters)
                    if (key == parameter.key) {
                        auto automation = graphParameter(value);
                        auto valid = [&](float v) {
                            return v >= parameter.minimum && v <= parameter.maximum;
                        };
                        if (!valid(automation.value))
                            throw std::runtime_error("Paramètre DSP hors limites");
                        for (auto &k : automation.keys)
                            if (!valid(k.value))
                                throw std::runtime_error("Automation DSP hors limites");
                        known = true;
                        effect.params[key] = automation;
                        break;
                    }
            if (!known)
                throw std::runtime_error("Paramètre de nœud DSP invalide : " + key);
        }
        GraphNode node;
        node.id = id;
        node.processorKey = processor;
        node.displayName = item["name"].S(processor);
        node.ports = {port(1, PortDirection::input, processor == "sf.sum"), port(2, PortDirection::output)};
        g.nodes.push_back(node);
        result.nodes.push_back({id, effect});
        pending[id] = 0;
    }
    bool output = false;
    for (auto &item : json["connections"].Elements()) {
        for (auto key : {"source", "destination"})
            if (item[key].GetType() != Json::Type::Number || item[key].Num() < 0 ||
                item[key].Num() > 2147483647 || std::floor(item[key].Num()) != item[key].Num())
                throw std::runtime_error("Connexion DSP invalide");
        Id from = item["source"].U(), to = item["destination"].U();
        if ((from && !ids.contains(from)) || (to && !ids.contains(to)))
            throw std::runtime_error("Nœud de connexion introuvable");
        g.connections.push_back({{from, 2}, {to, 1}});
        if (from && to) {
            ++pending[to];
            downstream[from].push_back(to);
        }
        if (!to)
            output = true;
    }
    if (!output)
        throw std::runtime_error("La sortie du graphe doit être connectée");
    GraphLibrary library;
    auto validation = library.install(g);
    if (!validation.ok())
        throw std::runtime_error(validation.issues.front().message);
    if (!library.compile())
        throw std::runtime_error("Graphe DSP non compilable");
    for (auto [id, count] : pending)
        if (!count)
            result.order.push_back(id);
    for (size_t i = 0; i < result.order.size(); ++i)
        for (auto next : downstream[result.order[i]])
            if (--pending[next] == 0)
                result.order.push_back(next);
    if (result.order.size() != ids.size())
        throw std::runtime_error("Cycle de traitement DSP");
    return result;
}
} // namespace sf

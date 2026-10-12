#include "plugins/Effects.h"
#include "render/Grading.h"
#include <iomanip>
#include <set>
#include <sstream>
namespace sf {
unsigned effectInputs(const std::string &code) {
    if (code.find("//@generator") != std::string::npos)
        return 0;
    unsigned count = 0;
    std::istringstream lines(code);
    std::string line;
    while (std::getline(lines, line))
        if (line.starts_with("//@input "))
            ++count;
    return std::min(4u, std::max(1u, count));
}
int effectPasses(const std::string &code) {
    std::istringstream lines(code);
    std::string line;
    int passes = 1;
    while (std::getline(lines, line))
        if (line.starts_with("//@passes ")) {
            std::istringstream value(line.substr(10));
            if (!(value >> passes) || passes < 1 || passes > 8)
                throw std::runtime_error("1 à 8 passes HLSL requises");
        }
    return passes;
}
std::vector<EffectParameter> effectParameters(const std::string &code) {
    effectPasses(code);
    std::vector<EffectParameter> result;
    std::istringstream lines(code);
    std::string line;
    std::set<std::string> seen;
    while (std::getline(lines, line)) {
        if (line.starts_with("//@passes ")) {
            int passes = 1;
            std::istringstream(line.substr(10)) >> passes;
            if (passes < 1 || passes > 8)
                throw std::runtime_error("1 à 8 passes HLSL requises");
        }
        if (!line.starts_with("//@param "))
            continue;
        std::istringstream s(line.substr(9));
        EffectParameter p;
        s >> p.key >> p.type;
        if (p.key.empty() || !(std::isalpha(static_cast<unsigned char>(p.key[0])) || p.key[0] == '_') ||
            p.key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
                std::string::npos ||
            !seen.insert(p.key).second)
            throw std::runtime_error("Identifiant de paramètre HLSL invalide ou dupliqué");
        int n = p.type == "float2"                          ? 2
                : p.type == "float3"                        ? 3
                : (p.type == "float4" || p.type == "color") ? 4
                                                            : 1;
        if (p.type != "float" && p.type != "float2" && p.type != "float3" && p.type != "float4" &&
            p.type != "color" && p.type != "int" && p.type != "bool")
            throw std::runtime_error("Type de paramètre HLSL non supporté");
        for (int i = 0; i < n; ++i)
            if (!(s >> p.value[i]) || !std::isfinite(p.value[i]))
                throw std::runtime_error("Valeur de paramètre HLSL invalide");
        if (p.type != "color" && p.type != "bool") {
            if (!(s >> p.minimum >> p.maximum) || !std::isfinite(p.minimum) || !std::isfinite(p.maximum) ||
                p.minimum > p.maximum)
                throw std::runtime_error("Plage de paramètre HLSL invalide");
        }
        s >> std::quoted(p.label);
        if (p.label.empty())
            p.label = p.key;
        result.push_back(p);
        if (result.size() > 64)
            throw std::runtime_error("64 paramètres HLSL maximum");
    }
    return result;
}
std::string effectCode(const Effect &e) {
    if (e.id == "grading")
        return gradeShader(e, "linear709");
    if (e.id == "ofx")
        return {};
    if (!e.graph.IsNull()) {
        sx::EffectGraph g;
        if (!sx::EffectGraph::FromJson(e.graph, g))
            throw std::runtime_error("Graphe Shaderixel invalide");
        if (g.kind != sx::EGKind::Effect || g.inputs > 4)
            throw std::runtime_error("Les graphes d'effet acceptent jusqu'à quatre entrées image");
        for (auto &n : g.nodes)
            if ((n.op == "input" || n.op == "sample" || n.op == "texture") && (n.index < 0 || n.index > 3))
                throw std::runtime_error("Entrée image hors plage 0..3");
        auto r = sx::CompileEffectGraph(g);
        if (!r.error.empty())
            throw std::runtime_error(r.error);
        return (g.premultiplied ? "//@premultiplied\n" : "") + r.code;
    }
    return e.code;
}
const EffectDefinition *EffectLibrary::find(const std::string &id) const {
    for (auto &d : definitions)
        if (d.id == id)
            return &d;
    return nullptr;
}
Effect EffectLibrary::instantiate(const std::string &id) const {
    auto *d = find(id);
    if (!d)
        throw std::runtime_error("Effet introuvable: " + id);
    if (d->kind != "effect")
        throw std::runtime_error("Ce shader est une transition : utiliser set_transition");
    if (id == "shaderixel.curves" || id == "shaderixel.color_lookup")
        return makeGrade(d->name);
    Effect e;
    e.id = id;
    e.code = d->code;
    for (auto &p : d->parameters) {
        int n = p.type == "float2"                          ? 2
                : p.type == "float3"                        ? 3
                : (p.type == "float4" || p.type == "color") ? 4
                                                            : 1;
        for (int i = 0; i < n; ++i)
            e.params[p.key + (n > 1 ? "." + std::to_string(i) : "")] = {p.value[i]};
    }
    return e;
}
void validateTransitionParameters(const std::string &code) {
    if (effectPasses(code) != 1)
        throw std::runtime_error("Les transitions utilisent une seule passe");
    for (auto &parameter : effectParameters(code))
        if (!parameter.key.starts_with("transition_") || parameter.key == "transition_progress")
            throw std::runtime_error("Les paramètres de transition doivent commencer par transition_ ; "
                                     "utiliser ctx.transitionProgress pour la progression");
}
std::string EffectLibrary::transitionCode(const Transition &transition) const {
    if (!transition.code.empty()) {
        validateTransitionParameters(transition.code);
        return transition.code;
    }
    auto *definition = find(transition.id);
    if (!definition || definition->kind != "transition")
        throw std::runtime_error("Transition introuvable : " + transition.id);
    return definition->code;
}
void EffectLibrary::scan(const std::filesystem::path &root) {
    definitions.clear();
    diagnostics.clear();
    auto add = [&](const std::filesystem::path &manifest, bool builtin) {
        try {
            std::string text;
            sx::ReadTextFile(sx::Narrow(manifest.wstring()), text);
            Json j;
            std::string err;
            if (!Json::Parse(text, j, &err))
                throw std::runtime_error(err);
            if (j["apiVersion"].I() != 1 && j["apiVersion"].I() != 2)
                throw std::runtime_error("API plugin incompatible (version 1 ou 2 attendue)");
            for (auto &a : j["effects"].Elements()) {
                EffectDefinition d;
                d.id = a["id"].S();
                d.name = a["name"].S();
                d.category = a["category"].S("VFX");
                d.kind = a["kind"].S("effect");
                if (d.kind != "effect" && d.kind != "transition")
                    throw std::runtime_error("Type de shader inconnu : effect ou transition requis");
                if (d.kind == "transition" && j["apiVersion"].I() < 2)
                    throw std::runtime_error("Les transitions nécessitent apiVersion 2");
                d.origin = builtin ? "ShaderixFrame" : "Plugin · " + j["name"].S();
                if (d.id.empty() || find(d.id))
                    throw std::runtime_error("Identifiant d'effet absent ou dupliqué");
                auto base = std::filesystem::weakly_canonical(manifest.parent_path());
                auto source = std::filesystem::weakly_canonical(base / sx::Utf8Path(a["shader"].S()));
                auto relative = source.lexically_relative(base);
                if (relative.empty() || *relative.begin() == L".." ||
                    !std::filesystem::is_regular_file(source))
                    throw std::runtime_error("Shader hors du dossier plugin ou introuvable");
                if (!sx::ReadTextFile(sx::Narrow(source.wstring()), d.code))
                    throw std::runtime_error("Shader illisible");
                d.parameters = effectParameters(d.code);
                d.inputs = effectInputs(d.code);
                d.generator = d.inputs == 0;
                if (d.kind == "transition")
                    validateTransitionParameters(d.code);
                definitions.push_back(d);
            }
        } catch (const std::exception &e) {
            diagnostics.push_back(sx::Narrow(manifest.wstring()) + ": " + e.what());
        }
    };
    if (std::filesystem::exists(root / "shaders/effects.json"))
        add(root / "shaders/effects.json", true);
    if (std::filesystem::exists(root / "shaders/transitions.json"))
        add(root / "shaders/transitions.json", true);
    if (std::filesystem::exists(root / "plugins"))
        for (auto &entry : std::filesystem::directory_iterator(root / "plugins"))
            if (entry.is_directory() && std::filesystem::exists(entry.path() / "plugin.json"))
                add(entry.path() / "plugin.json", false);
}
} // namespace sf

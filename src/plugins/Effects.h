#pragma once
#include "core/Project.h"
#include "doc/EffectGraph.h"
namespace sf {
int effectPasses(const std::string &code);
unsigned effectInputs(const std::string &code);
struct EffectParameter {
    std::string key, label, type = "float";
    sx::float4 value;
    float minimum = 0, maximum = 1;
};
struct EffectDefinition {
    std::string id, name, category, code, origin, kind = "effect";
    std::vector<EffectParameter> parameters;
    unsigned inputs = 1;
    bool generator = false;
};
std::vector<EffectParameter> effectParameters(const std::string &code);
std::string effectCode(const Effect &e);
void validateTransitionParameters(const std::string &code);
class EffectLibrary {
  public:
    std::vector<EffectDefinition> definitions;
    std::vector<std::string> diagnostics;
    void scan(const std::filesystem::path &root);
    const EffectDefinition *find(const std::string &id) const;
    Effect instantiate(const std::string &id) const;
    std::string transitionCode(const Transition &transition) const;
};
} // namespace sf

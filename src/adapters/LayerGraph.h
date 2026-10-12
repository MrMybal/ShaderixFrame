#pragma once
#include "adapters/ShaderixelComposition.h"
#include "core/Project.h"
namespace sf {
struct LayerComposition {
    sx::Graph graph;
    std::map<Id, Effect> effects;
    std::map<Id, std::map<std::string, Parameter>> parameters;
};
inline constexpr const char *layerNodeKinds[] = {"output", "surface", "effect", "blend", "split"};
Json defaultLayerGraph(const Project &project);
void appendLayerSurface(Project &project, Id track, const std::string &name);
LayerComposition decodeLayerGraph(const Json &json, const Project &project);
Json parameterJson(const Parameter &parameter);
Parameter graphParameter(const Json &value);
void setLayerGraphKey(Json &graph, Id node, const std::string &key, Keyframe frame);
} // namespace sf

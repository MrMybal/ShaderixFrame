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
// Track effect graphs: the same schema, bound to one video track. Its surfaces are the track itself
// (its clips composited) or tracks below it, read as side inputs (mattes, displacement sources).
Json defaultTrackGraph(const Project &project, Id track);
LayerComposition decodeTrackGraph(const Json &json, const Project &project, Id track);
// Inserts an effect node just before the output (end of the chain); returns the new node id.
Id appendTrackEffect(Json &graph, const Effect &effect, const std::string &name);
// The graph without surfaces of a removed track: their links become empty inputs.
Json withoutSurface(const Json &graph, Id track);
} // namespace sf

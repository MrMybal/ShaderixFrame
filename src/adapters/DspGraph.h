#pragma once
#include "core/Project.h"
#include <cydaw/graph/GraphDefinition.h>
namespace sf {
struct DspNode {
    Id id;
    Effect effect;
};
struct DspGraph {
    cydaw::graph::GraphDefinition definition;
    std::vector<DspNode> nodes;
    std::vector<Id> order;
};
DspGraph decodeDspGraph(const Json &json);
} // namespace sf

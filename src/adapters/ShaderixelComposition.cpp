// Generated from Shaderixel Document.cpp; graph algorithms unchanged.
#include "adapters/ShaderixelComposition.h"
namespace sx {
int NodeInputCount(const Node& n) {
    switch (n.kind) {
        case NodeKind::Output: return 1;
        case NodeKind::Surface: return 0;
        case NodeKind::Blend: return 3;
        case NodeKind::Split: return 1;
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            return d ? (int)d->inputs.size() : (int)n.inputs.size();
        }
    }
    return 0;
}

PinType NodeInputType(const Node& n, int i) {
    if (n.kind == NodeKind::Blend) return i == 2 ? PinType::Channel : PinType::RGBA;
    if (n.kind == NodeKind::Effect) {
        // Inputs named after a single channel or a mask read one value.
        std::string name = NodeInputName(n, i);
        for (auto& c : name) c = (char)tolower((unsigned char)c);
        static const char* scalar[] = {"r", "g", "v", "b", "a", "masque", "mask", "alpha", "hauteur", "height"};
        for (const char* s : scalar) if (name == s) return PinType::Channel;
    }
    return PinType::RGBA;
}

int NodeOutputCount(const Node& n) { return n.kind == NodeKind::Output ? 0 : n.kind == NodeKind::Split ? 4 : 1; }
PinType NodeOutputType(const Node& n, int) { return n.kind == NodeKind::Split ? PinType::Channel : PinType::RGBA; }
u8 NodeOutputChannel(const Node& n, int k) { return n.kind == NodeKind::Split ? (u8)(k + 1) : 0; }
std::string NodeOutputName(const Node& n, int k) {
    static const char* ch[] = {"R", "G", "B", "A"};
    return n.kind == NodeKind::Split ? ch[std::clamp(k, 0, 3)] : "RGBA";
}

std::string NodeInputName(const Node& n, int i) {
    switch (n.kind) {
        case NodeKind::Output: return "Image";
        case NodeKind::Blend: return i == 0 ? "Background" : i == 1 ? "Layer" : "Mask";
        case NodeKind::Split: return "RGBA";
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            if (d && i < (int)d->inputs.size()) return d->inputs[i];
            return "Input";
        }
        default: return "";
    }
}

Node* Graph::Find(Id id) {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}
const Node* Graph::Find(Id id) const {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

bool Graph::WouldCycle(Id from, Id to) const {
    // Linking from -> to creates a cycle if `to` is reachable upstream of `from`.
    std::vector<Id> stack{from};
    std::set<Id> seen;
    while (!stack.empty()) {
        Id cur = stack.back();
        stack.pop_back();
        if (cur == to) return true;
        if (!seen.insert(cur).second) continue;
        if (auto* n = Find(cur))
            for (Id in : n->inputs) if (in) stack.push_back(in);
    }
    return false;
}

}

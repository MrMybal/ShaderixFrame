// Generated from Shaderixel NodeEditor.cpp.
#pragma once
#include "adapters/ShaderixelComposition.h"
#include "ui/Theme.h"
#include <imgui.h>
namespace sf::shaderixel_ui { using namespace sx;
constexpr float kNodeW = 184.0f;
constexpr float kHeader = 26.0f;
constexpr float kRow = 22.0f;
constexpr float kPin = 6.0f;
constexpr u64 kThumbKeyBit = 1ull << 62;

ImU32 HeaderColor(const Node& n) {
    switch (n.kind) {
        case NodeKind::Output: return IM_COL32(52, 160, 110, 255);
        case NodeKind::Surface: return IM_COL32(36, 150, 168, 255);
        case NodeKind::Blend: return IM_COL32(64, 104, 200, 255);
        case NodeKind::Split: return IM_COL32(70, 150, 90, 255);
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            if (!n.fx.customCode.empty()) return IM_COL32(200, 92, 150, 255);
            if (d && d->inputs.empty()) return IM_COL32(210, 130, 50, 255);
            return IM_COL32(112, 84, 220, 255);
        }
    }
    return IM_COL32(90, 90, 90, 255);
}

// Pins and links are colored by type (Unreal-style): RGBA image pink, single channel green.
ImU32 TypeColor(PinType t, int alpha = 255) {
    return t == PinType::Channel ? IM_COL32(110, 210, 120, alpha) : IM_COL32(230, 120, 210, alpha);
}
// Labels of the Split outputs keep their channel color.
ImU32 LabelColor(const Node& n, int k) {
    if (n.kind != NodeKind::Split) return TypeColor(PinType::RGBA, 230);
    static const ImU32 c[] = {IM_COL32(235, 90, 90, 255), IM_COL32(100, 215, 110, 255), IM_COL32(90, 150, 255, 255), IM_COL32(210, 210, 215, 255)};
    return c[std::clamp(k, 0, 3)];
}

struct Layout {
    float h;
    float thumbW, thumbH;
    int inputs, outputs, rows;
};

Layout NodeLayout(const Node& n, float aspect) {
    Layout l;
    l.inputs = NodeInputCount(n);
    l.outputs = NodeOutputCount(n);
    l.rows = std::max({l.inputs, l.outputs, 1});
    l.thumbW = kNodeW - 16;
    l.thumbH = std::clamp(l.thumbW / std::max(aspect, 0.1f), 40.0f, l.thumbW);
    l.h = kHeader + l.rows * kRow + l.thumbH + 12;
    return l;
}

}

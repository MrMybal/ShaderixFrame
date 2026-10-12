#pragma once
#include "core/Common.h"

namespace sf {
// View coordinates are local pixels; graph node coordinates stay in the upstream format.
struct CanvasView {
    float zoom = 1;
    sx::float2 pan;
    sx::float2 screen(sx::float2 world) const { return world * zoom + pan; }
    sx::float2 world(sx::float2 screen) const { return (screen - pan) / zoom; }
    void zoomAt(sx::float2 anchor, float value) {
        auto fixed = world(anchor);
        zoom = std::clamp(value, .2f, 2.5f);
        pan = anchor - fixed * zoom;
    }
    void fit(sx::float2 low, sx::float2 high, sx::float2 available) {
        auto span = high - low;
        zoom = std::clamp(std::min(std::max(1.f, available.x - 48) / std::max(1.f, span.x),
                                   std::max(1.f, available.y - 48) / std::max(1.f, span.y)),
                          .2f, 2.5f);
        pan = available * .5f - (low + high) * (.5f * zoom);
    }
};
} // namespace sf

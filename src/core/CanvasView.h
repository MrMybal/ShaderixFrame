#pragma once
#include "core/Common.h"
#include <cyui/modules/Canvas.h>

namespace sf {
// View coordinates are local pixels; graph node coordinates stay in the upstream format.
struct CanvasView {
    float zoom = 1;
    sx::float2 pan;
    cyui::CanvasView common() const { cyui::CanvasView v; v.origin={-pan.x/zoom,-pan.y/zoom}; v.zoom=zoom; return v; }
    sx::float2 screen(sx::float2 point) const { auto p=common().screen({point.x,point.y}); return {p.x,p.y}; }
    sx::float2 world(sx::float2 point) const { auto p=common().world({point.x,point.y}); return {float(p.x),float(p.y)}; }
    void zoomAt(sx::float2 anchor,float value) {
        auto v=common(); v.zoomAt({anchor.x,anchor.y},std::clamp(value,.2f,2.5f));
        zoom=float(v.zoom); pan={float(-v.origin.x*v.zoom),float(-v.origin.y*v.zoom)};
    }
    void fit(sx::float2 low,sx::float2 high,sx::float2 available) {
        auto v=common(); v.fit({low.x,low.y},{high.x,high.y},{available.x,available.y},24);
        zoom=float(std::clamp(v.zoom,.2,2.5)); pan=available*.5f-(low+high)*(.5f*zoom);
    }
};
} // namespace sf

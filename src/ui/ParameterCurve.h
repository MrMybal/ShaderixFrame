#pragma once
#include "core/Project.h"
#include <cyui/modules/Curves.h>
#include <optional>
namespace sf {
// Preserve ShaderixFrame interpolation (including shaped segments) while CyUI owns gestures.
inline Parameter curveParameter(const Parameter& source,cyui::ArrayView<cyui::CurvePoint> points) {
    Parameter result;result.value=source.value;
    for(size_t index=0;index<points.count;++index) {
        const auto& point=points[index];
        Keyframe key{std::clamp(point.time,0.,86400.),float(point.value)};
        if(point.id>0&&point.id<=source.keys.size()) {key.curve=source.keys[size_t(point.id)-1].curve;key.shape=source.keys[size_t(point.id)-1].shape;}
        result.setKey(key);
    }
    return result;
}
inline cyui::Edit<Parameter> parameterCurve(const char* id,cyui::CurveState& state,const Parameter& p,
                                           double end,std::uint64_t revision) {
    std::vector<cyui::CurvePoint> points;double lo=p.value,hi=p.value;
    for(size_t i=0;i<p.keys.size();++i){auto& k=p.keys[i];points.push_back({i+1,k.time,k.value});lo=std::min(lo,double(k.value));hi=std::max(hi,double(k.value));}
    if(!state.view.x.range.valid())state.view.x.range={0,std::max(1.,end)};
    if(!state.dragging){double pad=std::max(.1,(hi-lo)*.15);state.view.y.range={lo-pad,hi+pad};}
    cyui::CurveOptions options;options.revision=revision;options.handles=false;
    std::optional<Parameter> evaluated;
    options.evaluate=[&](auto preview,double t){if(!evaluated)evaluated=curveParameter(p,preview);return evaluated->at(t);};
    auto edit=cyui::curveEditor(id,state,points,{0,cyui::dp(110)},options);
    return {edit.phase,curveParameter(p,edit.value)};
}
}

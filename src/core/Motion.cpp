#include "core/Motion.h"
#include <cmath>
#include <set>
namespace sf {
static WidgetRect rect(const Project &p, const Clip &c, double time, std::set<Id> &seen) {
    if (!seen.insert(c.id).second || seen.size() > 64)
        throw std::runtime_error("Motion hierarchy contains a cycle or is too deep");
    WidgetRect parent{0, 0, float(p.width), float(p.height), 0, 0, float(p.width), float(p.height), true};
    if (auto id = c.widget["parent"].U()) {
        auto *owner = p.findClip(id);
        if (!owner || owner->widget.IsNull())
            throw std::runtime_error("Missing motion parent");
        parent = rect(p, *owner, time, seen);
    }
    WidgetRect r;
    r.width = c.param("widget_width", time, 320);
    r.height = c.param("widget_height", time, 120);
    r.x = parent.x + parent.width * c.widget["anchorX"].F() + c.param("widget_x", time, 0) -
          r.width * c.widget["pivotX"].F();
    r.y = parent.y + parent.height * c.widget["anchorY"].F() + c.param("widget_y", time, 0) -
          r.height * c.widget["pivotY"].F();
    r.visible = parent.visible && c.live(time) && !c.widget["hidden"].B();
    auto *track = p.owner(c.id);
    r.visible &= track && p.audible(*track);
    r.left = parent.left;
    r.top = parent.top;
    r.right = parent.right;
    r.bottom = parent.bottom;
    if (c.widget["clipChildren"].B()) {
        r.left = std::max(r.left, r.x);
        r.top = std::max(r.top, r.y);
        r.right = std::min(r.right, r.x + r.width);
        r.bottom = std::min(r.bottom, r.y + r.height);
    }
    seen.erase(c.id);
    return r;
}
WidgetRect widgetRect(const Project &p, const Clip &c, double time) {
    std::set<Id> seen;
    return rect(p, c, time, seen);
}
void validateWidgets(const Project &p) {
    for (auto &t : p.tracks)
        for (auto &c : t.clips)
            if (!c.widget.IsNull()) {
                auto type = c.widget["type"].S();
                if (t.kind != TrackKind::Video || !c.widget.IsObject() ||
                    (type != "rectangle" && type != "ellipse" && type != "text" && type != "image" &&
                     type != "group" && type != "container"))
                    throw std::runtime_error("Invalid motion widget");
                if (c.widget.Has("parent")) {
                    auto n = c.widget["parent"].Num(-1);
                    if (!std::isfinite(n) || n < 0 || n > 2147483647 || std::floor(n) != n)
                        throw std::runtime_error("Invalid motion parent ID");
                }
                for (auto key : {"anchorX", "anchorY", "pivotX", "pivotY"}) {
                    auto n = c.widget[key].F();
                    if (!std::isfinite(n) || n < 0 || n > 1)
                        throw std::runtime_error("Motion anchor out of range");
                }
                for (auto key : {"widget_x", "widget_y", "widget_width", "widget_height"}) {
                    auto it = c.params.find(key);
                    if (it == c.params.end())
                        continue;
                    auto check = [&](float n) {
                        if (!std::isfinite(n) || std::abs(n) > 65536 ||
                            ((key == std::string("widget_width") || key == std::string("widget_height")) &&
                             n < 1))
                            throw std::runtime_error("Invalid motion dimensions");
                    };
                    check(it->second.value);
                    for (auto &k : it->second.keys)
                        check(k.value);
                }
                widgetRect(p, c, 0);
            }
}
std::string widgetShader(const std::string &type) {
    if (type == "group")
        return "//@generator\nfloat4 Effect(EffectContext ctx){return 0;}";
    std::string shape =
        type == "ellipse" ? "length((ctx.uv-.5)*2)-1" : "length(max(abs(ctx.uv-.5)-(.5-radius),0))-radius";
    return "//@generator\n//@param fill color 0.14 0.45 0.9 1 \"Color\"\n//@param radius float 0.05 0 0.5 "
           "\"Radius\"\nfloat4 Effect(EffectContext ctx){float d=" +
           shape + ";float coverage=1-smoothstep(-.004,.004,d);return float4(fill.rgb,fill.a*coverage);}";
}
} // namespace sf

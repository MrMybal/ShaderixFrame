#include "render/Color.h"
namespace sf {
float srgbDecode(float x) {
    return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
}
float srgbEncode(float x) {
    return x <= .0031308f ? x * 12.92f : 1.055f * std::pow(x, 1 / 2.4f) - .055f;
}
ColorRGB convertColor(ColorRGB c, const std::string &from, const std::string &to) {
    if (from == to)
        return c;
    if (from == "logc3-awg3-ei800") {
        // ARRI LogC3 exposure-value coefficients, EI800 (2017 VFX white paper).
        for (int i = 0; i < 3; ++i)
            c[i] = c[i] > .149658f ? (std::pow(10.f, (c[i] - .385537f) / .247190f) - .052272f) / 5.555556f
                                   : (c[i] - .092809f) / 5.367655f;
        c = {1.617523f * c.x - .537287f * c.y - .080237f * c.z,
             -.070573f * c.x + 1.334613f * c.y - .264040f * c.z,
             -.021102f * c.x - .226954f * c.y + 1.248056f * c.z};
        return convertColor(c, "linear709", to);
    }
    if (to == "logc3-awg3-ei800") {
        c = convertColor(c, from, "linear709");
        c = {.63132127f * c.x + .27080104f * c.y + .09787833f * c.z,
             .03682010f * c.x + .79303706f * c.y + .17014288f * c.z,
             .01736990f * c.x + .14878930f * c.y + .83384082f * c.z};
        for (int i = 0; i < 3; ++i)
            c[i] = c[i] > .010591f ? .247190f * std::log10(5.555556f * c[i] + .052272f) + .385537f
                                   : 5.367655f * c[i] + .092809f;
        return c;
    }
    if (from == "srgb")
        c = {srgbDecode(c.x), srgbDecode(c.y), srgbDecode(c.z)};
    if (from == "linear2020" && to != "linear2020")
        c = {1.660491f * c.x - .587641f * c.y - .072850f * c.z,
             -.124550f * c.x + 1.132900f * c.y - .008349f * c.z,
             -.018151f * c.x - .100579f * c.y + 1.118730f * c.z};
    else if (from != "linear2020" && to == "linear2020")
        c = {.627404f * c.x + .329283f * c.y + .043313f * c.z,
             .069097f * c.x + .919540f * c.y + .011362f * c.z,
             .016391f * c.x + .088013f * c.y + .895595f * c.z};
    if (to == "srgb")
        c = {srgbEncode(c.x), srgbEncode(c.y), srgbEncode(c.z)};
    return c;
}
ColorRGB displayColor(ColorRGB c, const std::string &space, const std::string &tone, float exposure) {
    c = convertColor(c, space, "linear709");
    float e = std::exp2(exposure);
    for (int i = 0; i < 3; ++i) {
        float x = std::max(0.f, c[i] * e);
        if (tone == "reinhard")
            x = x / (1 + x);
        else if (tone == "aces")
            x = std::clamp((x * (2.51f * x + .03f)) / (x * (2.43f * x + .59f) + .14f), 0.f, 1.f);
        c[i] = srgbEncode(x);
    }
    return c;
}
} // namespace sf

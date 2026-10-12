#pragma once
#include "core/Common.h"
namespace sf {
struct ColorRGB {
    float x = 0, y = 0, z = 0;
    float &operator[](int i) { return i == 0 ? x : i == 1 ? y : z; }
    float operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
    ColorRGB operator/(float d) const { return {x / d, y / d, z / d}; }
};
// Linear working spaces; sRGB keeps legacy projects' encoded shader values.
float srgbDecode(float x);
float srgbEncode(float x);
ColorRGB convertColor(ColorRGB rgb, const std::string &from, const std::string &to);
ColorRGB displayColor(ColorRGB rgb, const std::string &space, const std::string &tone, float exposure);
} // namespace sf

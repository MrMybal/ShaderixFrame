#pragma once
#include "core/Project.h"
#include "media/Media.h"
#include "render/Color.h"
namespace sf {
struct GradeControl {
    const char *key, *label;
    float value, minimum, maximum;
};
const std::vector<GradeControl> &gradeControls();
Effect makeGrade(const std::string &name = "Correction");
void validateGrade(const Effect &effect);
std::string gradeShader(const Effect &effect, const std::string &workingSpace);
Json loadCube(const std::filesystem::path &path);
Json loadLut(const std::filesystem::path &path, const std::string &layout = "auto");
float gradeCurve(const Json &points, float x, float fallback);
float gradeSelection(const Effect &effect, ColorRGB linear, float u, float v, double time);
ColorRGB gradeColor(const Effect &effect, ColorRGB color, const std::string &space, double time,
                    float u = .5f, float v = .5f);
void neutralGrade(Effect &effect, ColorRGB linear);
Effect gradePreset(const std::string &name);
struct VideoScopes {
    std::vector<unsigned> waveform, parade, vectorscope, histogram;
    unsigned samples = 0, below = 0, above = 0;
    float minimum = 0, maximum = 0;
    Json json(bool bins = false) const;
};
VideoScopes videoScopes(const FloatImage &image, const std::string &tone = "clip", float exposure = 0);
} // namespace sf

#pragma once
#include "core/Project.h"
namespace sf {
struct WidgetRect {
    float x = 0, y = 0, width = 0, height = 0;
    float left = 0, top = 0, right = 0, bottom = 0;
    bool visible = true;
};
WidgetRect widgetRect(const Project &project, const Clip &clip, double time);
void validateWidgets(const Project &project);
std::string widgetShader(const std::string &type);
} // namespace sf

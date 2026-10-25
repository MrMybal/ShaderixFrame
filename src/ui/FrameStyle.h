#pragma once
// Palette and flat widgets of the CyDAW v2 interface (Theme.cpp, Widgets.cpp, Icons.h),
// adapted to ShaderixFrame's dark Shaderixel theme and orange accent.
#include <algorithm>
#include <cstdint>
#include <imgui.h>
namespace sf::style {
constexpr ImU32 bg0 = IM_COL32(18, 20, 25, 255), bg1 = IM_COL32(24, 26, 32, 255), bg2 = IM_COL32(31, 33, 40, 255),
                bg3 = IM_COL32(40, 43, 51, 255), bg4 = IM_COL32(52, 55, 65, 255), bg5 = IM_COL32(64, 68, 80, 255),
                line = IM_COL32(10, 11, 14, 255), lineStrong = IM_COL32(5, 6, 8, 255),
                grid = IM_COL32(255, 255, 255, 9), gridStrong = IM_COL32(255, 255, 255, 20),
                text = IM_COL32(222, 224, 232, 255), textDim = IM_COL32(140, 144, 158, 255),
                textFaint = IM_COL32(96, 100, 112, 255), accent = IM_COL32(239, 139, 58, 255),
                accentHover = IM_COL32(255, 165, 82, 255), accentDim = IM_COL32(110, 66, 30, 255),
                audio = IM_COL32(66, 179, 184, 255), control = IM_COL32(240, 201, 79, 255),
                modulation = IM_COL32(198, 154, 236, 255), mute = IM_COL32(90, 169, 240, 255),
                solo = IM_COL32(242, 209, 76, 255), play = IM_COL32(92, 210, 110, 255),
                warn = IM_COL32(245, 166, 35, 255), record = IM_COL32(238, 81, 73, 255),
                generator = IM_COL32(168, 124, 213, 255), title = IM_COL32(232, 197, 71, 255),
                container = IM_COL32(82, 209, 162, 255);
// CyDAW track colours, stored as 0xRRGGBB in the project.
constexpr std::uint32_t trackColours[12] = {0xf0a43a, 0x5ab0f0, 0x52d1a2, 0xc792ea, 0xe8765a, 0x7fd1e8,
                                            0xe8c547, 0x9ccc65, 0xf06292, 0x8c9eff, 0x4dd0b5, 0xffb74d};
inline ImU32 fromRgb(std::uint32_t rgb) { return IM_COL32(rgb >> 16 & 255, rgb >> 8 & 255, rgb & 255, 255); }
inline ImU32 alpha(ImU32 c, float a) {
    auto current = float(c >> IM_COL32_A_SHIFT & 255);
    return (c & ~IM_COL32_A_MASK) | ImU32(std::clamp(current * a, 0.f, 255.f)) << IM_COL32_A_SHIFT;
}
inline ImU32 mix(ImU32 a, ImU32 b, float t) {
    auto channel = [&](int shift) {
        float x = float(a >> shift & 255), y = float(b >> shift & 255);
        return ImU32(std::clamp(x + (y - x) * t, 0.f, 255.f)) << shift;
    };
    return channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) | channel(IM_COL32_B_SHIFT) |
           channel(IM_COL32_A_SHIFT);
}
inline ImU32 brighter(ImU32 c, float t) { return mix(c, IM_COL32(255, 255, 255, c >> IM_COL32_A_SHIFT & 255), t); }
inline ImU32 darker(ImU32 c, float t) { return mix(c, bg0, t); }
inline bool light(ImU32 c) {
    return .299f * float(c >> IM_COL32_R_SHIFT & 255) + .587f * float(c >> IM_COL32_G_SHIFT & 255) +
               .114f * float(c >> IM_COL32_B_SHIFT & 255) >
           150;
}
ImFont *boldFont();
void setBoldFont(ImFont *font);
// Text at a fraction of the current font size, for timeline labels.
void smallText(ImDrawList *list, ImVec2 at, ImU32 colour, const char *text, float scale = .8f);
ImVec2 smallTextSize(const char *text, float scale = .8f);
void clippedText(ImDrawList *list, ImVec2 at, float width, ImU32 colour, const char *text, float scale = 1);
void tooltip(const char *text);
// Flat button with a Segoe MDL2 glyph; `active` fills it with a translucent accent.
bool iconButton(const char *glyph, const char *tip, bool active = false, ImU32 activeColour = 0, ImVec2 size = {});
// Same button with one of Shaderixel's vector icons (sx::ui::Icon).
bool vectorButton(const char *id, int icon, const char *tip, bool active = false, ImU32 activeColour = 0);
// Rounded M/S/R toggle.
bool toggle(const char *label, bool value, ImU32 onColour, ImVec2 size, const char *tip = nullptr);
void toolbarSeparator();
bool modeButton(const char *label, bool active, const char *tip);
} // namespace sf::style
namespace sf::icon {
// Segoe MDL2 Assets, merged into the interface font by applyFrameTheme().
inline constexpr const char *play = "\xee\x9d\xa8", *pause = "\xee\x9d\xa9", *stop = "\xee\x9c\x9a",
                            *add = "\xee\x9c\x90", *trash = "\xee\x9d\x8d", *settings = "\xee\x9c\x93",
                            *undo = "\xee\x9e\xa7", *redo = "\xee\x9e\xa6", *mute = "\xee\x9d\x8f",
                            *volume = "\xee\x9d\xa7", *edit = "\xee\x9c\x8f", *close = "\xee\x9c\x91",
                            *chevronDown = "\xee\x9c\x8d", *chevronRight = "\xee\x9d\xac", *more = "\xee\x9c\x92",
                            *zoomIn = "\xee\xa2\xa3", *zoomOut = "\xee\x9c\x9f", *link = "\xee\x9c\x9b",
                            *lock = "\xee\x9c\xae", *loop = "\xee\xa3\xae", *view = "\xee\xa2\x90",
                            *copy = "\xee\xa3\x88", *cut = "\xee\xa3\x86", *fit = "\xee\x9d\x80",
                            *follow = "\xee\x9c\xaa", *previous = "\xee\xa2\x92", *next = "\xee\xa2\x93",
                            *tag = "\xee\xa3\xac", *video = "\xee\x9c\x94", *audio = "\xee\xa3\x96",
                            *exportFile = "\xee\xb7\xa1", *equalizer = "\xee\xa7\xa9", *grid = "\xef\x83\xa2";
} // namespace sf::icon

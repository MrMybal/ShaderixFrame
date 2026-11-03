#pragma once
// Thin semantic role and control adapters over the current CyUI context.
#include <algorithm>
#include <cstdint>
#include <imgui.h>
#include <cyui/Core.h>
namespace sf::style {
struct ColourRole { ImU32 cyui::Palette::*member; operator ImU32() const { return cyui::palette().*member; } };
inline const ColourRole bg0{&cyui::Palette::bg0};
inline const ColourRole bg1{&cyui::Palette::bg1};
inline const ColourRole bg2{&cyui::Palette::bg2};
inline const ColourRole bg3{&cyui::Palette::bg3};
inline const ColourRole bg4{&cyui::Palette::bg4};
inline const ColourRole bg5{&cyui::Palette::bg5};
inline const ColourRole line{&cyui::Palette::line};
inline const ColourRole lineStrong{&cyui::Palette::lineStrong};
inline const ColourRole grid{&cyui::Palette::grid};
inline const ColourRole gridStrong{&cyui::Palette::gridStrong};
inline const ColourRole text{&cyui::Palette::text};
inline const ColourRole textDim{&cyui::Palette::textDim};
inline const ColourRole textFaint{&cyui::Palette::textFaint};
inline const ColourRole accent{&cyui::Palette::accent};
inline const ColourRole accentHover{&cyui::Palette::accentHover};
inline const ColourRole accentDim{&cyui::Palette::accentDim};
inline const ColourRole audio{&cyui::Palette::info};
inline const ColourRole control{&cyui::Palette::warning};
inline const ColourRole modulation{&cyui::Palette::info};
inline const ColourRole mute{&cyui::Palette::info};
inline const ColourRole solo{&cyui::Palette::warning};
inline const ColourRole play{&cyui::Palette::success};
inline const ColourRole warn{&cyui::Palette::warning};
inline const ColourRole record{&cyui::Palette::error};
inline const ColourRole generator{&cyui::Palette::info};
inline const ColourRole title{&cyui::Palette::warning};
inline const ColourRole container{&cyui::Palette::success};
// CyDAW track colours, stored as 0xRRGGBB in the project.
constexpr std::uint32_t trackColours[12] = {0xf0a43a, 0x5ab0f0, 0x52d1a2, 0xc792ea, 0xe8765a, 0x7fd1e8,
                                            0xe8c547, 0x9ccc65, 0xf06292, 0x8c9eff, 0x4dd0b5, 0xffb74d};
inline ImU32 fromRgb(std::uint32_t rgb) { return IM_COL32(rgb >> 16 & 255, rgb >> 8 & 255, rgb & 255, 255); }
inline ImU32 alpha(ImU32 c, float a) { return cyui::alpha(c,a); }
inline ImU32 mix(ImU32 a, ImU32 b, float t) { return cyui::mix(a,b,t); }
inline ImU32 brighter(ImU32 c, float t) { return cyui::brighter(c,t); }
inline ImU32 darker(ImU32 c, float t) { return cyui::darker(c,t); }
inline bool light(ImU32 c) {
    return .299f * float(c >> IM_COL32_R_SHIFT & 255) + .587f * float(c >> IM_COL32_G_SHIFT & 255) +
               .114f * float(c >> IM_COL32_B_SHIFT & 255) >
           150;
}
ImFont *boldFont();
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
using cyui::icon::play;
using cyui::icon::pause;
using cyui::icon::stop;
using cyui::icon::add;
using cyui::icon::trash;
using cyui::icon::settings;
using cyui::icon::undo;
using cyui::icon::redo;
using cyui::icon::mute;
using cyui::icon::volume;
using cyui::icon::edit;
using cyui::icon::close;
using cyui::icon::chevronDown;
using cyui::icon::chevronRight;
using cyui::icon::more;
using cyui::icon::zoomIn;
using cyui::icon::zoomOut;
using cyui::icon::link;
using cyui::icon::lock;
using cyui::icon::loop;
using cyui::icon::view;
using cyui::icon::copy;
using cyui::icon::cut;
using cyui::icon::previous;
using cyui::icon::next;
using cyui::icon::tag;
using cyui::icon::video;
using cyui::icon::audio;
using cyui::icon::exportFile;
using cyui::icon::equalizer;
using cyui::icon::grid;
inline constexpr auto fit=cyui::icon::fullScreen, follow=cyui::icon::forward;
} // namespace sf::icon

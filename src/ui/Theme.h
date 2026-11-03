// Compatibility names for the Shaderixel graph model; presentation is provided by CyUI.
#pragma once
#include <imgui.h>
#include "ui/FrameStyle.h"

namespace sx::ui {

struct Fonts {
    ImFont* ui = nullptr;
    ImFont* mono = nullptr;
};

void ApplyTheme(float dpiScale);
Fonts LoadFonts(float dpiScale);
Fonts& GetFonts();

// Palette shared by custom-drawn widgets (node editor, toolbar icons...).
namespace col {
inline const sf::style::ColourRole Accent{&cyui::Palette::accent};
inline const sf::style::ColourRole AccentSoft{&cyui::Palette::selection};
inline const sf::style::ColourRole Accent2{&cyui::Palette::info};
inline const sf::style::ColourRole Selection{&cyui::Palette::accent};
inline const sf::style::ColourRole Text{&cyui::Palette::text};
inline const sf::style::ColourRole TextDim{&cyui::Palette::textDim};
inline const sf::style::ColourRole Panel{&cyui::Palette::bg1};
inline const sf::style::ColourRole PanelLight{&cyui::Palette::bg2};
inline const sf::style::ColourRole Border{&cyui::Palette::line};
inline const sf::style::ColourRole Grid{&cyui::Palette::grid};
inline const sf::style::ColourRole GridMajor{&cyui::Palette::gridStrong};
inline const sf::style::ColourRole Error{&cyui::Palette::error};
}

// Small helpers
bool IconButton(const char* id, int icon, bool selected, float size, const char* tooltip);
void DrawIcon(ImDrawList* dl, int icon, ImVec2 center, float size, ImU32 color);
void HelpTooltip(const char* text);

enum Icon { IconBrush, IconEraser, IconPicker, IconHand, IconZoom, IconGradient, IconBucket, IconEye, IconEyeOff,
            IconPlus, IconTrash, IconUp, IconDown, IconMask, IconCopy, IconLock, IconFx, IconSwap,
            IconMove, IconRectSel, IconEllipseSel, IconLasso, IconPolyLasso, IconWand,
            IconStamp, IconHeal, IconDrop, IconSharpen, IconFinger, IconDodge, IconBurn, IconSponge, IconFolder, IconCrop, IconText, IconShape, IconLiquify, IconQuickSelect, IconPen, IconTransform, IconTrim, IconParticles, IconPolyFill, IconFluid };

}  // namespace sx::ui

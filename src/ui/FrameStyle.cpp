#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <imgui_internal.h>
#include <cfloat>
#include <string>
namespace sf::style {
ImFont *boldFont() { return cyui::boldFont() ? cyui::boldFont() : ImGui::GetFont(); }
void smallText(ImDrawList *list, ImVec2 at, ImU32 colour, const char *text, float scale) {
    list->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, at, colour, text);
}
ImVec2 smallTextSize(const char *text, float scale) {
    return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * scale, FLT_MAX, 0, text);
}
void clippedText(ImDrawList *list, ImVec2 at, float width, ImU32 colour, const char *text, float scale) {
    if (width <= 4)
        return;
    float size = ImGui::GetFontSize() * scale;
    list->PushClipRect(at, {at.x + width, at.y + size + 2}, true);
    list->AddText(ImGui::GetFont(), size, at, colour, text);
    list->PopClipRect();
}
void tooltip(const char* text) { cyui::tooltip(text); }
bool iconButton(const char* glyph,const char* tip,bool active,ImU32 colour,ImVec2 size) {
    return cyui::iconButton(glyph,tip,active,colour,size);
}
bool vectorButton(const char* id,int icon,const char* tip,bool active,ImU32 colour) {
    return sx::ui::IconButton(id,icon,active,ImGui::GetFrameHeight(),tip);
}
bool toggle(const char* label,bool value,ImU32 colour,ImVec2 size,const char* tip) {
    return cyui::toggleButton(label,&value,colour,size,tip);
}
void toolbarSeparator() { cyui::toolbarSeparator(); }
bool modeButton(const char* label,bool active,const char* tip) {
    cyui::ScopedColor colour(ImGuiCol_Button,active?cyui::palette().accentDim:cyui::palette().bg2);
    bool result=cyui::button(cyui::trLabel(label).c_str()); cyui::tooltip(tip); return result;
}
} // namespace sf::style

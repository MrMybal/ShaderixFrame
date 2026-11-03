#include "ui/Theme.h"
#include <cyui/Core.h>
#include <imgui_internal.h>
namespace sx::ui {
Fonts& GetFonts() { thread_local Fonts fonts; fonts={cyui::uiFont(),cyui::monoFont()}; return fonts; }
void ApplyTheme(float dpi) { cyui::Config c; c.dpiScale=dpi; cyui::apply(c); }
Fonts LoadFonts(float) { cyui::loadFonts(); return GetFonts(); }
void HelpTooltip(const char* text) { cyui::tooltip(text); }
static const char* glyph(int icon) {
    switch(icon) {
    case IconMove: return cyui::icon::edit; case IconHand: return sf::icon::follow;
    case IconZoom: return cyui::icon::zoomIn; case IconSwap:return cyui::icon::refresh;
    case IconPlus:return cyui::icon::add; case IconTrash:return cyui::icon::trash;
    case IconCopy:return cyui::icon::copy; case IconLock:return cyui::icon::lock;
    default:return cyui::icon::edit;
    }
}
void DrawIcon(ImDrawList* list,int icon,ImVec2 center,float size,ImU32 colour) {
    const char* text=glyph(icon); auto extent=ImGui::CalcTextSize(text);
    list->AddText(ImGui::GetFont(),size,center-ImVec2(extent.x*.5f,extent.y*.5f),colour,text);
}
bool IconButton(const char* id,int icon,bool selected,float size,const char* tip) {
    ImGui::PushID(id); bool result=cyui::iconButton(glyph(icon),tip,selected,0,{size,size}); ImGui::PopID(); return result;
}
}

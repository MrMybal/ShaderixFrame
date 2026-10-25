#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <imgui_internal.h>
#include <cfloat>
#include <string>
namespace sf::style {
static ImFont *bold = nullptr;
ImFont *boldFont() { return bold ? bold : ImGui::GetFont(); }
void setBoldFont(ImFont *font) { bold = font; }
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
void tooltip(const char *text) {
    if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text);
}
static bool flatButton(const char *id, const char *tip, bool active, ImU32 activeColour, ImVec2 size,
                       const char *glyph, int vector) {
    float height = ImGui::GetFrameHeight();
    if (size.x <= 0)
        size = {height + 2, height};
    bool clicked = ImGui::InvisibleButton(id, size);
    bool disabled = ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
    auto *list = ImGui::GetWindowDrawList();
    auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImU32 colour = activeColour ? activeColour : accent;
    bool hovered = ImGui::IsItemHovered() && !disabled, held = ImGui::IsItemActive();
    if (active)
        list->AddRectFilled(min, max, ImGui::GetColorU32(alpha(colour, held ? .3f : .18f)), 4);
    else if (hovered)
        list->AddRectFilled(min, max, ImGui::GetColorU32(held ? bg5 : bg4), 4);
    ImU32 ink = ImGui::GetColorU32(active ? colour : hovered ? text : textDim);
    ImVec2 centre = (min + max) * .5f;
    if (glyph) {
        auto extent = ImGui::CalcTextSize(glyph);
        list->AddText(centre - extent * .5f, ink, glyph);
    } else
        sx::ui::DrawIcon(list, vector, centre, height * .62f, ink);
    tooltip(tip);
    return clicked;
}
bool iconButton(const char *glyph, const char *tip, bool active, ImU32 activeColour, ImVec2 size) {
    std::string id = std::string("##") + glyph + (tip ? tip : "");
    return flatButton(id.c_str(), tip, active, activeColour, size, glyph, 0);
}
bool vectorButton(const char *id, int icon, const char *tip, bool active, ImU32 activeColour) {
    return flatButton(id, tip, active, activeColour, {}, nullptr, icon);
}
bool toggle(const char *label, bool value, ImU32 onColour, ImVec2 size, const char *tip) {
    std::string id = std::string("##toggle") + label;
    bool clicked = ImGui::InvisibleButton(id.c_str(), size);
    auto *list = ImGui::GetWindowDrawList();
    auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    bool hovered = ImGui::IsItemHovered();
    list->AddRectFilled(min, max, ImGui::GetColorU32(value ? onColour : hovered ? bg4 : bg3), 3);
    auto extent = smallTextSize(label, .78f);
    list->AddText(boldFont(), ImGui::GetFontSize() * .78f, (min + max - extent) * .5f,
                  ImGui::GetColorU32(value ? bg0 : textDim), label);
    tooltip(tip);
    return clicked;
}
void toolbarSeparator() {
    ImGui::SameLine(0, 6);
    auto at = ImGui::GetCursorScreenPos();
    float height = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine({at.x, at.y + height * .18f}, {at.x, at.y + height * .82f}, bg5);
    ImGui::Dummy({1, height});
    ImGui::SameLine(0, 6);
}
bool modeButton(const char *label, bool active, const char *tip) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? accentDim : bg2);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? accent : bg3);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
    ImGui::PushStyleColor(ImGuiCol_Text, active ? text : textDim);
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    tooltip(tip);
    return clicked;
}
} // namespace sf::style

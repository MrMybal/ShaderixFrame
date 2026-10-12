#include "Widgets.h"

#include "Theme.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace cydaw::v2::ui {
namespace {

bool gFrench = true;
juce::String gLanguage { "fr" };
// Another language's translations, keyed by the English text, and the
// translation each English literal (by address) was found to have.
std::unordered_map<std::string, std::string> gDictionary;
std::unordered_map<const char*, const char*> gTranslated;
constexpr float kPi = 3.14159265358979f;
constexpr float kArcStart = kPi * 0.75f;
constexpr float kArcSweep = kPi * 1.5f;

float toNormalised(const float value, const float minimum, const float maximum, const bool logarithmic)
{
    if (maximum <= minimum)
        return 0.0f;
    if (logarithmic && minimum > 0.0f)
        return std::clamp(std::log(value / minimum) / std::log(maximum / minimum), 0.0f, 1.0f);
    return std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
}

float fromNormalised(const float t, const float minimum, const float maximum, const bool logarithmic)
{
    const auto clamped = std::clamp(t, 0.0f, 1.0f);
    if (logarithmic && minimum > 0.0f)
        return minimum * std::pow(maximum / minimum, clamped);
    return minimum + (maximum - minimum) * clamped;
}

void arc(ImDrawList* list, const ImVec2 centre, const float radius, const float from, const float to,
         const ImU32 colour, const float thickness)
{
    if (std::abs(to - from) < 0.001f)
        return;
    list->PathClear();
    list->PathArcTo(centre, radius, std::min(from, to), std::max(from, to), 32);
    list->PathStroke(colour, thickness);
}

std::string formatValue(const float value, const KnobStyle& style)
{
    char text[64];
    std::snprintf(text, sizeof(text), style.format != nullptr ? style.format : "%.2f",
                  (style.flags & knobInteger) != 0 ? std::round(value) : value);
    std::string out(text);
    if (style.unit != nullptr && style.unit[0] != 0) {
        out += ' ';
        out += style.unit;
    }
    return out;
}

} // namespace

float currentFontBase()
{
    return ImGui::GetCurrentContext()->FontSizeBase;
}

bool french()
{
    return gFrench;
}

const char* L(const char* fr, const char* en)
{
    if (gFrench)
        return fr;
    if (gDictionary.empty() || en == nullptr)
        return en;
    if (const auto known = gTranslated.find(en); known != gTranslated.end())
        return known->second;
    const auto found = gDictionary.find(en);
    const auto* text = found != gDictionary.end() && !found->second.empty() ? found->second.c_str() : en;
    gTranslated.emplace(en, text);
    return text;
}

void setFrench(const bool value)
{
    setLanguage(value ? "fr" : "en");
}

void setLanguage(const juce::String& code)
{
    gLanguage = code.isNotEmpty() ? code : juce::String("en");
    gFrench = gLanguage == "fr";
    gTranslated.clear();
    gDictionary.clear();
    bumpLanguageRevision();
    if (gLanguage == "fr" || gLanguage == "en")
        return;
    const auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                          .getSiblingFile("i18n")
                          .getChildFile("v2")
                          .getChildFile(gLanguage + ".json");
    if (!file.existsAsFile() || file.getSize() > 8 * 1024 * 1024)
        return;
    // The parsed value owns the object: kept alive while it is read.
    const auto parsed = juce::JSON::parse(file);
    if (const auto* object = parsed.getDynamicObject())
        for (const auto& property : object->getProperties())
            gDictionary.emplace(property.name.toString().toStdString(), property.value.toString().toStdString());
}

juce::String language()
{
    return gLanguage;
}

int translationCount()
{
    return static_cast<int>(gDictionary.size());
}

namespace {
int gLanguageRevision {};
}

int languageRevision()
{
    return gLanguageRevision;
}

void bumpLanguageRevision()
{
    ++gLanguageRevision;
}

std::string labelId(const char* label, const char* id)
{
    std::string out(label != nullptr ? label : "");
    out += "###";
    out += id;
    return out;
}

bool knob(const char* id, float* value, const float minimum, const float maximum, const float defaultValue,
          const KnobStyle& style)
{
    const auto& p = theme::palette();
    auto& io = ImGui::GetIO();
    const auto diameter = dp(style.diameter);
    const auto labelHeight = (style.flags & knobNoLabel) != 0 ? 0.0f : ImGui::GetFontSize() * 0.86f;
    const auto width = std::max(diameter, dp(style.diameter + 12.0f));
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("##knob", ImVec2(width, diameter + labelHeight));
    const auto hovered = ImGui::IsItemHovered();
    const auto active = ImGui::IsItemActive();
    const auto logarithmic = (style.flags & knobLogarithmic) != 0;
    auto changed = false;
    auto t = toNormalised(*value, minimum, maximum, logarithmic);

    if (active && (io.MouseDelta.y != 0.0f || io.MouseDelta.x != 0.0f)) {
        const auto speed = (io.KeyShift ? 0.0008f : 0.0055f) / std::max(0.5f, ImGui::GetStyle().FontScaleDpi);
        t = std::clamp(t + (-io.MouseDelta.y + io.MouseDelta.x * 0.35f) * speed, 0.0f, 1.0f);
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        t = std::clamp(t + io.MouseWheel * (io.KeyShift ? 0.002f : 0.02f), 0.0f, 1.0f);
        changed = true;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        t = toNormalised(defaultValue, minimum, maximum, logarithmic);
        changed = true;
    }
    if (changed) {
        auto next = fromNormalised(t, minimum, maximum, logarithmic);
        if ((style.flags & knobInteger) != 0)
            next = std::round(next);
        changed = next != *value;
        *value = next;
        t = toNormalised(*value, minimum, maximum, logarithmic);
    }

    auto* list = ImGui::GetWindowDrawList();
    const auto colour = style.colour != 0 ? style.colour : p.accent;
    const ImVec2 centre(origin.x + width * 0.5f, origin.y + diameter * 0.5f);
    const auto radius = diameter * 0.5f - dp(2.0f);
    const auto thickness = std::max(2.0f, diameter * 0.095f);
    arc(list, centre, radius, kArcStart, kArcStart + kArcSweep, p.bg5, thickness);
    const auto bipolar = (style.flags & knobBipolar) != 0;
    const auto from = bipolar ? kArcStart + kArcSweep * 0.5f : kArcStart;
    const auto to = kArcStart + kArcSweep * t;
    arc(list, centre, radius, from, to, active || hovered ? theme::brighter(colour, 0.15f) : colour, thickness);
    list->AddCircleFilled(centre, radius - thickness * 0.9f, hovered || active ? p.bg5 : p.bg4, 24);
    const auto angle = kArcStart + kArcSweep * t;
    const ImVec2 tip(centre.x + std::cos(angle) * (radius - thickness * 1.1f),
                     centre.y + std::sin(angle) * (radius - thickness * 1.1f));
    const ImVec2 inner(centre.x + std::cos(angle) * radius * 0.25f, centre.y + std::sin(angle) * radius * 0.25f);
    list->AddLine(inner, tip, p.text, std::max(1.5f, thickness * 0.55f));
    if (style.modulation >= 0.0f) {
        const auto m = kArcStart + kArcSweep * std::clamp(style.modulation, 0.0f, 1.0f);
        list->AddCircleFilled(ImVec2(centre.x + std::cos(m) * radius, centre.y + std::sin(m) * radius),
                              thickness * 0.7f, p.modulation, 12);
    }

    if (labelHeight > 0.0f) {
        const auto showValue = hovered || active;
        const auto text = showValue ? formatValue(*value, style) : std::string(style.label != nullptr ? style.label : "");
        ImGui::PushFont(nullptr, currentFontBase() * 0.82f);
        const auto size = ImGui::CalcTextSize(text.c_str());
        const auto x = origin.x + std::max(0.0f, (width - size.x) * 0.5f);
        list->PushClipRect(ImVec2(origin.x - dp(6.0f), origin.y + diameter),
                           ImVec2(origin.x + width + dp(6.0f), origin.y + diameter + labelHeight + dp(2.0f)), true);
        list->AddText(ImVec2(x, origin.y + diameter + dp(1.0f)), showValue ? p.text : p.textDim, text.c_str());
        list->PopClipRect();
        ImGui::PopFont();
    } else if (hovered || active) {
        ImGui::SetTooltip("%s", formatValue(*value, style).c_str());
    }
    ImGui::PopID();
    return changed;
}

float meterPosition(const float gainDb)
{
    if (gainDb <= -70.0f)
        return 0.0f;
    return std::clamp(std::exp((gainDb - 6.0f) / 26.0f), 0.0f, 1.0f);
}

namespace {

float positionToDb(const float position, const float minimumDb, const float maximumDb)
{
    if (position <= 0.0f)
        return minimumDb;
    return std::clamp(6.0f + 26.0f * std::log(position), minimumDb, maximumDb);
}

} // namespace

bool fader(const char* id, float* gainDb, const float minimumDb, const float maximumDb, const ImVec2 size,
           const ImU32 colour)
{
    const auto& p = theme::palette();
    auto& io = ImGui::GetIO();
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("##fader", size);
    const auto hovered = ImGui::IsItemHovered();
    const auto active = ImGui::IsItemActive();
    auto changed = false;
    const auto travel = size.y - dp(14.0f);
    auto position = meterPosition(*gainDb);
    if (active && io.MouseDelta.y != 0.0f) {
        position = std::clamp(position - io.MouseDelta.y / travel * (io.KeyShift ? 0.1f : 1.0f), 0.0f, 1.0f);
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        *gainDb = std::clamp(*gainDb + io.MouseWheel * (io.KeyShift ? 0.1f : 1.0f), minimumDb, maximumDb);
        position = meterPosition(*gainDb);
        changed = true;
    } else if (changed) {
        *gainDb = positionToDb(position, minimumDb, maximumDb);
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        *gainDb = 0.0f;
        changed = true;
    }
    auto* list = ImGui::GetWindowDrawList();
    const auto centreX = origin.x + size.x * 0.5f;
    const auto top = origin.y + dp(7.0f);
    const auto bottom = top + travel;
    // Scale ticks.
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.7f);
    for (const auto tick : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f }) {
        const auto y = bottom - meterPosition(tick) * travel;
        list->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + dp(4.0f), y), p.textFaint);
    }
    ImGui::PopFont();
    list->AddRectFilled(ImVec2(centreX - dp(2.0f), top), ImVec2(centreX + dp(2.0f), bottom), p.bg0, dp(2.0f));
    const auto handleY = bottom - meterPosition(*gainDb) * travel;
    const auto fill = colour != 0 ? colour : p.accent;
    list->AddRectFilled(ImVec2(centreX - dp(2.0f), handleY), ImVec2(centreX + dp(2.0f), bottom),
                        theme::alpha(fill, 0.55f), dp(2.0f));
    const auto handleWidth = std::min(size.x - dp(4.0f), dp(22.0f));
    const ImVec2 handleMin(centreX - handleWidth * 0.5f, handleY - dp(6.0f));
    const ImVec2 handleMax(centreX + handleWidth * 0.5f, handleY + dp(6.0f));
    list->AddRectFilled(handleMin, handleMax, hovered || active ? theme::brighter(p.bg5, 0.25f) : p.bg5, dp(3.0f));
    list->AddLine(ImVec2(handleMin.x + dp(3.0f), handleY), ImVec2(handleMax.x - dp(3.0f), handleY), p.text, 1.5f);
    if (hovered || active)
        ImGui::SetTooltip("%.1f dB", *gainDb);
    ImGui::PopID();
    return changed;
}

void meter(const char* id, const ImVec2 size, const float peakLeft, const float peakRight, const float rmsLeft,
           const float rmsRight, const bool vertical)
{
    const auto& p = theme::palette();
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::Dummy(size);
    auto* storage = ImGui::GetStateStorage();
    const auto holdId = ImGui::GetID("hold");
    const auto holdTimeId = ImGui::GetID("holdTime");
    auto* list = ImGui::GetWindowDrawList();
    const auto now = static_cast<float>(ImGui::GetTime());
    const auto toDb = [](const float gain) { return gain > 1.0e-5f ? 20.0f * std::log10(gain) : -100.0f; };
    const auto peakDb = std::max(toDb(peakLeft), toDb(peakRight));
    auto hold = storage->GetFloat(holdId, -100.0f);
    auto holdTime = storage->GetFloat(holdTimeId, 0.0f);
    if (peakDb >= hold || now - holdTime > 1.5f) {
        hold = peakDb;
        holdTime = now;
    }
    storage->SetFloat(holdId, hold);
    storage->SetFloat(holdTimeId, holdTime);

    list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), p.bg0, dp(2.0f));
    const auto gap = dp(1.0f);
    const auto drawBar = [&](const int index, const float peak, const float rms) {
        const auto peakPos = meterPosition(toDb(peak));
        const auto rmsPos = meterPosition(toDb(rms));
        const auto segment = [&](const float from, const float to, const ImU32 colour) {
            if (vertical) {
                const auto barWidth = (size.x - gap) * 0.5f;
                const auto x0 = origin.x + index * (barWidth + gap);
                list->AddRectFilled(ImVec2(x0, origin.y + size.y * (1.0f - to)),
                                    ImVec2(x0 + barWidth, origin.y + size.y * (1.0f - from)), colour);
            } else {
                const auto barHeight = (size.y - gap) * 0.5f;
                const auto y0 = origin.y + index * (barHeight + gap);
                list->AddRectFilled(ImVec2(origin.x + size.x * from, y0), ImVec2(origin.x + size.x * to, y0 + barHeight),
                                    colour);
            }
        };
        const auto mid = meterPosition(-12.0f);
        const auto high = meterPosition(-3.0f);
        const auto draw = [&](const float level, const float alphaValue) {
            segment(0.0f, std::min(level, mid), theme::alpha(p.meterLow, alphaValue));
            if (level > mid)
                segment(mid, std::min(level, high), theme::alpha(p.meterMid, alphaValue));
            if (level > high)
                segment(high, level, theme::alpha(p.meterHigh, alphaValue));
        };
        draw(peakPos, 0.45f);
        draw(rmsPos, 1.0f);
    };
    drawBar(0, peakLeft, rmsLeft);
    drawBar(1, peakRight, rmsRight);
    const auto holdPos = meterPosition(hold);
    if (holdPos > 0.0f) {
        const auto colour = hold > -0.1f ? p.meterHigh : p.text;
        if (vertical) {
            const auto y = origin.y + size.y * (1.0f - holdPos);
            list->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + size.x, y), theme::alpha(colour, 0.8f), 1.0f);
        } else {
            const auto x = origin.x + size.x * holdPos;
            list->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), theme::alpha(colour, 0.8f), 1.0f);
        }
    }
    ImGui::PopID();
}

bool toggle(const char* label, bool* value, const ImU32 onColour, ImVec2 size, const char* tip)
{
    const auto& p = theme::palette();
    const auto textSize = ImGui::CalcTextSize(label, nullptr, true);
    if (size.x <= 0.0f)
        size.x = textSize.x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (size.y <= 0.0f)
        size.y = ImGui::GetFrameHeight();
    const auto origin = ImGui::GetCursorScreenPos();
    const auto clicked = ImGui::InvisibleButton(label, size);
    if (clicked)
        *value = !*value;
    const auto hovered = ImGui::IsItemHovered();
    auto* list = ImGui::GetWindowDrawList();
    const auto on = *value;
    const auto fill = on ? onColour : (hovered ? p.bg4 : p.bg3);
    list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), fill, ImGui::GetStyle().FrameRounding);
    const auto textColour = on ? p.bg0 : (hovered ? p.text : p.textDim);
    const auto* end = ImGui::FindRenderedTextEnd(label);
    list->AddText(ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f),
                  textColour, label, end);
    if (tip != nullptr)
        tooltip(tip);
    return clicked;
}

bool iconButton(const char* icon, const char* tip, const bool active, const ImU32 activeColour, ImVec2 size)
{
    const auto& p = theme::palette();
    if (size.x <= 0.0f)
        size.x = ImGui::GetFrameHeight() + dp(2.0f);
    if (size.y <= 0.0f)
        size.y = ImGui::GetFrameHeight();
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(icon);
    ImGui::PushID(tip != nullptr ? tip : "");
    const auto clicked = ImGui::InvisibleButton("##icon", size);
    const auto hovered = ImGui::IsItemHovered();
    const auto held = ImGui::IsItemActive();
    ImGui::PopID();
    ImGui::PopID();
    auto* list = ImGui::GetWindowDrawList();
    const auto colour = activeColour != 0 ? activeColour : p.accent;
    if (active)
        list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), theme::alpha(colour, 0.18f),
                            ImGui::GetStyle().FrameRounding);
    else if (hovered)
        list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), held ? p.bg5 : p.bg4,
                            ImGui::GetStyle().FrameRounding);
    const auto textSize = ImGui::CalcTextSize(icon);
    list->AddText(ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f),
                  active ? colour : (hovered ? p.text : p.textDim), icon);
    if (tip != nullptr)
        tooltip(tip);
    return clicked;
}

bool primaryButton(const char* label, const ImVec2 size)
{
    const auto& p = theme::palette();
    ImGui::PushStyleColor(ImGuiCol_Button, p.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.accentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::brighter(p.accent, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_Text, p.bg0);
    const auto clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

void toolbarSeparator()
{
    ImGui::SameLine(0.0f, dp(6.0f));
    const auto origin = ImGui::GetCursorScreenPos();
    const auto height = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(origin.x, origin.y + height * 0.18f),
                                        ImVec2(origin.x, origin.y + height * 0.82f), theme::palette().lineStrong);
    ImGui::Dummy(ImVec2(dp(1.0f), height));
    ImGui::SameLine(0.0f, dp(6.0f));
}

void tooltip(const char* text)
{
    if (text != nullptr && text[0] != 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay))
        ImGui::SetTooltip("%s", text);
}

bool dragNumber(const char* id, float* value, const float speed, const float minimum, const float maximum,
                const char* format, const float width)
{
    ImGui::SetNextItemWidth(width);
    return ImGui::DragFloat(id, value, speed, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
}

void sectionTitle(const char* text)
{
    ImGui::PushFont(theme::boldFont(), ImGui::GetStyle().FontSizeBase * 0.85f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::palette().textDim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

bool colourChip(const char* id, const ImU32 colour, const ImVec2 size)
{
    const auto origin = ImGui::GetCursorScreenPos();
    const auto clicked = ImGui::InvisibleButton(id, size);
    ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), colour,
                                              dp(2.0f));
    return clicked;
}

void centredText(ImDrawList* list, const ImVec2 min, const ImVec2 max, const ImU32 colour, const char* text)
{
    const auto size = ImGui::CalcTextSize(text);
    list->AddText(ImVec2(min.x + (max.x - min.x - size.x) * 0.5f, min.y + (max.y - min.y - size.y) * 0.5f), colour,
                  text);
}

juce::String sizeText(const juce::int64 bytes)
{
    const auto value = static_cast<double>(bytes);
    juce::String text;
    if (value >= 1e9)
        text = juce::String(value / 1e9, 1) + L(" Go", " GB");
    else if (value >= 1e6)
        text = juce::String(juce::roundToInt(value / 1e6)) + L(" Mo", " MB");
    else if (value >= 1e3)
        text = juce::String(juce::roundToInt(value / 1e3)) + L(" Ko", " kB");
    else
        text = juce::String(bytes) + L(" o", " B");
    return french() ? text.replaceCharacter('.', ',') : text;
}

void emptyState(const char* title, const char* message)
{
    const auto& p = theme::palette();
    const auto area = ImGui::GetContentRegionAvail();
    const auto origin = ImGui::GetCursorScreenPos();
    auto* list = ImGui::GetWindowDrawList();
    const auto middle = origin.y + std::max(dp(40.0f), area.y * 0.45f);
    ImGui::PushFont(theme::boldFont(), ImGui::GetStyle().FontSizeBase * 1.05f);
    centredText(list, ImVec2(origin.x, middle - dp(22.0f)), ImVec2(origin.x + area.x, middle), p.textDim, title);
    ImGui::PopFont();
    centredText(list, ImVec2(origin.x, middle), ImVec2(origin.x + area.x, middle + dp(20.0f)), p.textFaint, message);
}

void clippedText(ImDrawList* list, const ImVec2 position, const float maxWidth, const ImU32 colour, const char* text)
{
    if (maxWidth <= 4.0f)
        return;
    const auto size = ImGui::CalcTextSize(text);
    if (size.x <= maxWidth) {
        list->AddText(position, colour, text);
        return;
    }
    list->PushClipRect(position, ImVec2(position.x + maxWidth, position.y + size.y + 2.0f), true);
    list->AddText(position, colour, text);
    list->PopClipRect();
}

} // namespace cydaw::v2::ui

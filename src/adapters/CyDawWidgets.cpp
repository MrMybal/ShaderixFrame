// Generated from CyDAW v2 Widgets/Theme; parameter controls unchanged.
#include "adapters/CyDawWidgets.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
namespace cydaw::v2::theme {
struct Palette {
    // Grounds, from the deepest (timeline background) to the raised frames.
    ImU32 bg0;
    ImU32 bg1;
    ImU32 bg2;
    ImU32 bg3;
    ImU32 bg4;
    ImU32 bg5;
    // Lines.
    ImU32 line;
    ImU32 lineStrong;
    ImU32 grid;
    ImU32 gridStrong;
    // Text.
    ImU32 text;
    ImU32 textDim;
    ImU32 textFaint;
    // The one accent, for what is active or selected.
    ImU32 accent;
    ImU32 accentHover;
    ImU32 accentDim;
    // Meaning colours: signal kinds and transport states.
    ImU32 audio;
    ImU32 midi;
    ImU32 control;
    ImU32 modulation;
    ImU32 record;
    ImU32 play;
    ImU32 warn;
    ImU32 solo;
    ImU32 mute;
    ImU32 meterLow;
    ImU32 meterMid;
    ImU32 meterHigh;
    ImU32 selection;
};

constexpr ImU32 rgb(const unsigned int hex, const float a = 1.0f)
{
    const auto r = (hex >> 16) & 0xffu;
    const auto g = (hex >> 8) & 0xffu;
    const auto b = hex & 0xffu;
    const auto alphaByte = static_cast<unsigned int>(a * 255.0f + 0.5f) & 0xffu;
    return (alphaByte << 24) | (b << 16) | (g << 8) | r;
}

const Palette kPalette {
    rgb(0x111317), // bg0
    rgb(0x16181d), // bg1
    rgb(0x1c1f25), // bg2
    rgb(0x24282f), // bg3
    rgb(0x2c313a), // bg4
    rgb(0x363c47), // bg5
    rgb(0x262a31), // line
    rgb(0x363b45), // lineStrong
    rgb(0x1f2228), // grid
    rgb(0x2b2f37), // gridStrong
    rgb(0xdce1e8), // text
    rgb(0x9aa3b0), // textDim
    rgb(0x5f6875), // textFaint
    rgb(0xf0a43a), // accent
    rgb(0xf8b95c), // accentHover
    rgb(0x7a5524), // accentDim
    rgb(0x5ab0f0), // audio
    rgb(0x52d1a2), // midi
    rgb(0xf2c94c), // control
    rgb(0xc792ea), // modulation
    rgb(0xe5484d), // record
    rgb(0x3dd68c), // play
    rgb(0xf5a623), // warn
    rgb(0xf2d14c), // solo
    rgb(0x5aa9f0), // mute
    rgb(0x3dd68c), // meterLow
    rgb(0xe8c547), // meterMid
    rgb(0xe5484d), // meterHigh
    rgb(0xf0a43a, 0.22f) // selection
};

const Palette& palette()
{
    return kPalette;
}

ImU32 alpha(const ImU32 colour, const float a)
{
    const auto current = static_cast<float>((colour >> 24) & 0xffu) / 255.0f;
    const auto byte = static_cast<unsigned int>(std::clamp(current * a, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (colour & 0x00ffffffu) | (byte << 24);
}

ImU32 mix(const ImU32 a, const ImU32 b, const float t)
{
    const auto lerp = [t](const unsigned int x, const unsigned int y) {
        return static_cast<unsigned int>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t + 0.5f);
    };
    unsigned int out = 0;
    for (auto shift = 0; shift < 32; shift += 8)
        out |= (lerp((a >> shift) & 0xffu, (b >> shift) & 0xffu) & 0xffu) << shift;
    return out;
}

ImU32 brighter(const ImU32 colour, const float amount)
{
    return mix(colour, (colour & 0xff000000u) | 0x00ffffffu, amount);
}

ImVec4 vec(const ImU32 colour)
{
    return ImGui::ColorConvertU32ToFloat4(colour);
}

}
namespace cydaw::v2::ui {
static float dp(float x){return x*ImGui::GetStyle().FontScaleDpi;}
static float currentFontBase(){return ImGui::GetCurrentContext()->FontSizeBase;}
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

static void tooltip(const char *text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))ImGui::SetTooltip("%s",text); }
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

}
namespace cydaw::v2::ui { const char *parameterLabel(std::string_view key){
if(key=="effect.parameter.pan")return "Pan";
if(key=="effect.parameter.balance")return "Balance";
if(key=="effect.parameter.rate")return "Rate";
if(key=="effect.parameter.depth")return "Depth";
if(key=="effect.parameter.delay")return "Delay";
if(key=="effect.parameter.width")return "Width";
if(key=="effect.parameter.mix")return "Mix";
if(key=="effect.parameter.feedback")return "Feedback";
if(key=="effect.parameter.frequency")return "Frequency";
if(key=="effect.parameter.shape")return "Shape";
if(key=="effect.parameter.spread")return "Spread";
if(key=="effect.parameter.bits")return "Bits";
if(key=="effect.parameter.downsample")return "Downsample";
if(key=="effect.parameter.drive")return "Drive";
if(key=="effect.parameter.mode")return "Mode";
if(key=="effect.parameter.cutoff")return "Cutoff";
if(key=="effect.parameter.resonance")return "Resonance";
if(key=="effect.parameter.low")return "Low";
if(key=="effect.parameter.mid")return "Mid";
if(key=="effect.parameter.high")return "High";
if(key=="effect.parameter.lowFrequency")return "Low Frequency";
if(key=="effect.parameter.highFrequency")return "High Frequency";
if(key=="effect.parameter.gain")return "Gain";
if(key=="effect.parameter.ceiling")return "Ceiling";
if(key=="effect.parameter.release")return "Release";
if(key=="effect.parameter.threshold")return "Threshold";
if(key=="effect.parameter.attack")return "Attack";
if(key=="effect.parameter.range")return "Range";
if(key=="effect.parameter.punch")return "Punch";
if(key=="effect.parameter.sustain")return "Sustain";
if(key=="effect.parameter.output")return "Output";
if(key=="effect.parameter.time")return "Time";
if(key=="effect.parameter.sync")return "Sync";
if(key=="effect.parameter.free")return "Free";
if(key=="effect.parameter.enabled")return "Enabled";
if(key=="effect.parameter.type")return "Type";
if(key=="effect.parameter.q")return "Q";
if(key=="effect.parameter.slope")return "Slope";
if(key=="effect.parameter.band")return "Band";
if(key=="effect.parameter.ratio")return "Ratio";
if(key=="effect.parameter.knee")return "Knee";
if(key=="effect.parameter.makeup")return "Makeup";
if(key=="effect.parameter.sidechain")return "Sidechain";
if(key=="effect.parameter.floor")return "Floor";
if(key=="effect.parameter.tilt")return "Tilt";
if(key=="effect.parameter.smoothing")return "Smoothing";
if(key=="effect.parameter.hold")return "Hold";
if(key=="effect.parameter.history")return "History";
return "Parameter";} }
namespace cydaw::v2::ui {
bool mixerStrip(const char *id,MixerStrip &strip,float width,float height,bool &picked){
    const auto &p=theme::palette();const auto colour=p.audio;
    const auto origin=ImGui::GetCursorScreenPos();auto *list=ImGui::GetWindowDrawList();
    ImGui::PushID(id);ImGui::BeginGroup();bool changed=false;
    list->AddRectFilled(origin,ImVec2(origin.x+width,origin.y+height),strip.selected?p.bg3:p.bg2,dp(4.f));
    list->AddRectFilled(origin,ImVec2(origin.x+width,origin.y+dp(4.f)),colour,dp(4.f),ImDrawFlags_RoundCornersTop);
    if(strip.selected)list->AddRect(origin,ImVec2(origin.x+width,origin.y+height),theme::alpha(p.accent,.6f),dp(4.f));
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+dp(6.f)));
    if(ImGui::InvisibleButton("##select",ImVec2(width,ImGui::GetFontSize()+dp(4.f))))picked=true;
    list->PushClipRect(origin,ImVec2(origin.x+width,origin.y+dp(40.f)),true);
    list->AddText(ImVec2(origin.x+dp(6.f),origin.y+dp(7.f)),p.text,strip.name);
    char count[40];std::snprintf(count,sizeof(count),"%d effets",strip.devices);
    list->AddText(ImVec2(origin.x+dp(6.f),origin.y+dp(24.f)),p.textDim,count);list->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin.x+width-dp(28.f),origin.y+dp(7.f)));
    changed|=knob("##pan",&strip.pan,-1,1,0,{20.f,colour,nullptr,"%.2f",nullptr,knobBipolar|knobNoLabel});
    const auto faderTop=origin.y+dp(48.f),faderHeight=std::max(dp(40.f),height-dp(100.f));
    const auto left=origin.x+std::max(dp(4.f),(width-dp(70.f))*.5f);
    ImGui::SetCursorScreenPos(ImVec2(left,faderTop));
    changed|=fader("##fader",&strip.gainDb,-80,12,ImVec2(dp(44.f),faderHeight),colour);
    ImGui::SetCursorScreenPos(ImVec2(left+dp(50.f),faderTop+dp(7.f)));
    meter("##meter",ImVec2(dp(20.f),faderHeight-dp(14.f)),strip.peak[0],strip.peak[1],strip.rms[0],strip.rms[1],true);
    char gain[32];std::snprintf(gain,sizeof(gain),"%+.1f dB",strip.gainDb);
    list->AddText(ImVec2(left,faderTop+faderHeight+dp(1.f)),p.textDim,gain);
    list->PushClipRect(origin,ImVec2(origin.x+width,origin.y+height),true);
    list->AddText(ImVec2(origin.x+dp(5.f),origin.y+height-dp(37.f)),p.textDim,strip.output);list->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin.x+dp(5.f),origin.y+height-dp(22.f)));
    const ImVec2 size((width-dp(13.f))*.5f,dp(18.f));
    ImGui::BeginDisabled(!strip.canMute);changed|=toggle("M",&strip.muted,p.mute,size,"Mute");ImGui::EndDisabled();ImGui::SameLine(0,dp(3.f));
    ImGui::BeginDisabled(!strip.canSolo);changed|=toggle("S",&strip.solo,p.solo,size,"Solo");ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+height));ImGui::Dummy(ImVec2(width,0));
    ImGui::EndGroup();ImGui::PopID();return changed;
}
} // namespace cydaw::v2::ui
namespace cydaw::v2::ui { void graphCard(ImDrawList *list,ImVec2 min,ImVec2 max,float headerHeight,ImU32 colour,bool boundary,bool selected,float s,bool invalid){const auto &p=theme::palette();const float rounding=7.f*s;
    // A soft shadow, the body, the header tinted with the card's colour.
    list->AddRectFilled(ImVec2(min.x + 1.0f * s, min.y + 4.0f * s), ImVec2(max.x + 3.0f * s, max.y + 7.0f * s),
                        IM_COL32(0, 0, 0, 45), rounding + 3.0f * s);
    list->AddRectFilled(ImVec2(min.x + 1.0f * s, min.y + 2.0f * s), ImVec2(max.x + 1.5f * s, max.y + 3.5f * s),
                        IM_COL32(0, 0, 0, 70), rounding);
    if (selected)
        list->AddRect(ImVec2(min.x - 3.0f * s, min.y - 3.0f * s), ImVec2(max.x + 3.0f * s, max.y + 3.0f * s),
                      theme::alpha(p.accent, 0.35f), rounding + 3.0f * s, std::max(2.0f, 3.0f * s));
    list->AddRectFilled(min, max, boundary ? p.bg3 : p.bg2, rounding);
    list->AddRectFilled(min, ImVec2(max.x, min.y + headerHeight),
                        theme::mix(colour, (boundary ? p.bg3 : p.bg2) | 0xff000000u, boundary ? 0.3f : 0.55f), rounding,
                        ImDrawFlags_RoundCornersTop);
    // The card's colour, as a band along the header's foot.
    list->AddRectFilled(ImVec2(min.x, min.y + headerHeight - 2.5f * s),
                        ImVec2(max.x, min.y + headerHeight), colour);
    const auto border = invalid ? p.record : selected ? p.accent : theme::alpha(p.lineStrong, 0.9f);
    list->AddRect(min, max, border, rounding, selected || invalid ? 1.8f : 1.0f);

}
void arrangerClipBackground(ImDrawList *list,ImVec2 min,ImVec2 max,ImU32 colour,bool hovered){
    const auto rounding = dp(3.0f);
    const auto headerHeight = std::min(dp(14.0f), (max.y - min.y) * 0.35f);
    list->AddRectFilled(min, max, theme::mix(colour,0xff000000u,hovered?.62f:.70f), rounding);
    list->AddRectFilled(min, ImVec2(max.x, min.y + headerHeight), theme::mix(colour,0xff000000u,.15f), rounding,
                        ImDrawFlags_RoundCornersTop);

} }

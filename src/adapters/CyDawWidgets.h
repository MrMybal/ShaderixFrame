// Generated from CyDAW v2 Widgets.h.
#pragma once
#include <imgui.h>
namespace cydaw::v2::ui {
enum KnobFlags : int {
    knobNone = 0,
    knobBipolar = 1 << 0,     // the arc starts at the centre (pan, detune)
    knobLogarithmic = 1 << 1, // frequency-like ranges
    knobNoLabel = 1 << 2,
    knobInteger = 1 << 3
};

struct KnobStyle {
    float diameter { 30.0f };
    ImU32 colour { 0 };        // 0: the accent
    const char* label {};      // under the knob
    const char* format { "%.2f" };
    const char* unit {};
    int flags { knobNone };
    float modulation { -1.0f };// a live position (0..1) shown as a dot, <0 for none
};

// Returns true while the value changes. The default is restored with a
// double click.
bool knob(const char* id, float* value, float minimum, float maximum, float defaultValue,
          const KnobStyle& style = {});

// A vertical fader in decibels with its scale; returns true while it changes.
bool fader(const char* id, float* gainDb, float minimumDb, float maximumDb, ImVec2 size,
           ImU32 colour = 0);

// Peak and RMS of up to two channels, in linear gain. Vertical or not.
void meter(const char* id, ImVec2 size, float peakLeft, float peakRight, float rmsLeft, float rmsRight,
           bool vertical = true);
[[nodiscard]] float meterPosition(float gainDb);

// A small rounded toggle: M, S, R, an icon, a word.
bool toggle(const char* label, bool* value, ImU32 onColour, ImVec2 size = ImVec2(0.0f, 0.0f),
            const char* tooltip = nullptr);
}
#include <string_view>
namespace cydaw::v2::ui { const char *parameterLabel(std::string_view key); }
namespace cydaw::v2::ui {
struct MixerStrip {
    const char *name=""; const char *output="MASTER"; int devices=0;
    float gainDb=0,pan=0; bool muted=false,solo=false,selected=false;
    bool canMute=true,canSolo=true;
    float peak[2]{},rms[2]{};
};
bool mixerStrip(const char *id,MixerStrip &strip,float width,float height,bool &picked);
} // namespace cydaw::v2::ui
namespace cydaw::v2::ui { void graphCard(ImDrawList *list, ImVec2 min, ImVec2 max,float headerHeight,ImU32 colour,bool boundary,bool selected,float s,bool invalid=false); void arrangerClipBackground(ImDrawList *list,ImVec2 min,ImVec2 max,ImU32 colour,bool hovered); }

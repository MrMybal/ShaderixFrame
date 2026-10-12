#pragma once

#include <imgui.h>
#include <juce_core/juce_core.h>

#include <string>

// Compact controls drawn for CyDAW 2: knobs, faders, meters, toggles and icon
// buttons. Each one behaves the same everywhere — drag vertically to change,
// Shift for fine steps, the wheel nudges, a double click restores the
// default, and the value shows while the pointer is over the control.

namespace cydaw::v2::ui {

// The current language. Interface strings are written inline in French and
// English; another language translates the English from
// i18n/v2/<code>.json beside the program ({"English": "translation"}),
// keeping the English for what it does not have.
[[nodiscard]] bool french();
[[nodiscard]] const char* L(const char* fr, const char* en);
// The same, as a juce::String (inline text is UTF-8).
[[nodiscard]] inline juce::String Ls(const char* fr, const char* en)
{
    return juce::String::fromUTF8(L(fr, en));
}
void setFrench(bool french);
void setLanguage(const juce::String& code);
[[nodiscard]] juce::String language();
// How many interface texts the current language translates (0 in French and
// English, which are inline).
[[nodiscard]] int translationCount();
// Bumped by every change of language (and of the node names' language), so
// that what caches translated names takes them again.
[[nodiscard]] int languageRevision();
void bumpLanguageRevision();

// "label###id": a visible label that may change (language, value) on a
// stable identity.
[[nodiscard]] std::string labelId(const char* label, const char* id);

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
// A flat icon button, highlighted when active.
bool iconButton(const char* icon, const char* tooltip, bool active = false, ImU32 activeColour = 0,
                ImVec2 size = ImVec2(0.0f, 0.0f));
// A button with a filled accent background for the main action of a panel.
bool primaryButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));
// A thin vertical bar between toolbar groups.
void toolbarSeparator();
// A delayed tooltip for the last item.
void tooltip(const char* text);
// A drag field styled for numbers (tempo, positions).
bool dragNumber(const char* id, float* value, float speed, float minimum, float maximum,
                const char* format, float width);
// Section title inside a panel.
void sectionTitle(const char* text);
// A coloured square, as the track colour in lists; returns true when clicked.
bool colourChip(const char* id, ImU32 colour, ImVec2 size);
// Text centred in a rectangle.
void centredText(ImDrawList* list, ImVec2 min, ImVec2 max, ImU32 colour, const char* text);
// A centred title and message filling the rest of a panel.
void emptyState(const char* title, const char* message);
// A size for people: "6,2 Go", "312 Mo" ("6.2 GB", "312 MB").
[[nodiscard]] juce::String sizeText(juce::int64 bytes);
// Text clipped to a width with an ellipsis.
void clippedText(ImDrawList* list, ImVec2 position, float maxWidth, ImU32 colour, const char* text);

// The current font size before the global scales, for PushFont(nullptr, …).
[[nodiscard]] float currentFontBase();

[[nodiscard]] inline float dp(const float value)
{
    return value * ImGui::GetStyle().FontScaleDpi;
}

} // namespace cydaw::v2::ui

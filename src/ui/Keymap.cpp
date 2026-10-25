#include "ui/Keymap.h"
#include <stdexcept>

namespace sf {
std::string chordName(ImGuiKey key, bool ctrl, bool shift, bool alt) {
    std::string name;
    if (ctrl)
        name += "Ctrl+";
    if (shift)
        name += "Shift+";
    if (alt)
        name += "Alt+";
    return name + (key == ImGuiKey_None ? "—" : ImGui::GetKeyName(key));
}
const std::vector<Shortcut> &Keymap::defaults() {
    static const std::vector<Shortcut> list = {
        {"tool.select", "Selection Tool", "Tools", ImGuiKey_V},
        {"tool.blade", "Blade Tool", "Tools", ImGuiKey_B},
        {"tool.slip", "Slip Tool", "Tools", ImGuiKey_Y},
        {"tool.roll", "Roll Tool", "Tools", ImGuiKey_N},
        {"tool.stretch", "Rate Stretch Tool", "Tools", ImGuiKey_R},
        {"tool.slide", "Slide Tool", "Tools", ImGuiKey_U},
        {"tool.hand", "Hand Tool", "Tools", ImGuiKey_H},
        {"edit.split", "Split Selection at Playhead", "Edit", ImGuiKey_S},
        {"trim.head", "Ripple Trim Start to Playhead", "Edit", ImGuiKey_Q},
        {"trim.head_keep", "Trim Start to Playhead (Keep Gap)", "Edit", ImGuiKey_Q, false, false, true},
        {"trim.tail", "Ripple Trim End to Playhead", "Edit", ImGuiKey_W},
        {"trim.tail_keep", "Trim End to Playhead (Keep Gap)", "Edit", ImGuiKey_W, false, false, true},
        {"cut.crossfade", "Crossfade at Nearest Cut", "Edit", ImGuiKey_D, false, true},
        {"clip.freeze", "2 s Freeze Frame", "Edit", ImGuiKey_F, false, true},
        {"clip.toggle", "Disable / Enable Selection", "Edit", ImGuiKey_E, false, true},
        {"clip.match", "Match Frame in Source", "Edit", ImGuiKey_F},
        {"select.forward", "Select Forward", "Selection", ImGuiKey_A},
        {"select.backward", "Select Backward", "Selection", ImGuiKey_A, false, false, true},
        {"view.fit_selection", "Zoom to Selection", "View", ImGuiKey_Z},
        {"view.palette", "Command Palette", "View", ImGuiKey_P, true, true},
        {"marker.add", "Marker at Playhead", "Markers and Work Area", ImGuiKey_M},
        {"area.in", "Work Area In", "Markers and Work Area", ImGuiKey_I},
        {"area.out", "Work Area Out", "Markers and Work Area", ImGuiKey_O},
        {"loop.toggle", "Loop", "Playback", ImGuiKey_L},
        {"shuttle.back", "Shuttle Backward", "Playback", ImGuiKey_J},
        {"shuttle.stop", "Shuttle: Stop", "Playback", ImGuiKey_K},
        {"shuttle.forward", "Shuttle forward", "Playback", ImGuiKey_L, false, true},
    };
    return list;
}
void Keymap::reset() {
    bindings.clear();
    for (auto &shortcut : defaults())
        bindings[shortcut.id] = shortcut;
}
const Shortcut &Keymap::get(const std::string &id) const {
    auto it = bindings.find(id);
    if (it == bindings.end())
        throw std::runtime_error("Unknown shortcut: " + id);
    return it->second;
}
bool Keymap::pressed(const std::string &id, bool repeat) const {
    auto &shortcut = get(id);
    auto &io = ImGui::GetIO();
    return shortcut.key != ImGuiKey_None && io.KeyCtrl == shortcut.ctrl && io.KeyShift == shortcut.shift &&
           io.KeyAlt == shortcut.alt && ImGui::IsKeyPressed(shortcut.key, repeat);
}
void Keymap::set(const std::string &id, ImGuiKey key, bool ctrl, bool shift, bool alt) {
    auto &shortcut = bindings.at(id);
    shortcut.key = key;
    shortcut.ctrl = ctrl;
    shortcut.shift = shift;
    shortcut.alt = alt;
}
std::string Keymap::label(const std::string &id) const {
    auto &shortcut = get(id);
    return chordName(shortcut.key, shortcut.ctrl, shortcut.shift, shortcut.alt);
}
std::vector<std::string> Keymap::conflicts(const std::string &id) const {
    std::vector<std::string> same;
    auto &mine = get(id);
    if (mine.key == ImGuiKey_None)
        return same;
    for (auto &[other, shortcut] : bindings)
        if (other != id && shortcut.key == mine.key && shortcut.ctrl == mine.ctrl && shortcut.shift == mine.shift &&
            shortcut.alt == mine.alt)
            same.push_back(other);
    return same;
}
Json Keymap::json() const {
    // Only what differs from the defaults is written.
    Json document = Json::Object();
    for (auto &original : defaults()) {
        auto &shortcut = get(original.id);
        if (shortcut.key == original.key && shortcut.ctrl == original.ctrl && shortcut.shift == original.shift &&
            shortcut.alt == original.alt)
            continue;
        Json item = Json::Object();
        item["key"] = int(shortcut.key);
        item["ctrl"] = shortcut.ctrl;
        item["shift"] = shortcut.shift;
        item["alt"] = shortcut.alt;
        document[original.id] = item;
    }
    return document;
}
void Keymap::load(const Json &document) {
    reset();
    if (!document.IsObject())
        return;
    for (auto &[id, item] : document.Items()) {
        auto it = bindings.find(id);
        int key = item["key"].I();
        if (it == bindings.end() || (key != ImGuiKey_None && (key < ImGuiKey_NamedKey_BEGIN || key >= ImGuiKey_NamedKey_END)))
            continue;
        set(id, ImGuiKey(key), item["ctrl"].B(), item["shift"].B(), item["alt"].B());
    }
}
} // namespace sf

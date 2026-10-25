#pragma once
#include "core/Project.h"
#include <imgui.h>
#include <map>
#include <string>
#include <vector>
namespace sf {
// A key and its exact modifiers for one editor action.
struct Shortcut {
    std::string id, label, group;
    ImGuiKey key = ImGuiKey_None;
    bool ctrl = false, shift = false, alt = false;
};
std::string chordName(ImGuiKey key, bool ctrl, bool shift, bool alt);
// Editable bindings of the single-key editor actions (tools, cuts, markers, transport); the others
// keep their fixed keys. A pressed chord matches only with exactly its modifiers.
class Keymap {
    std::map<std::string, Shortcut> bindings;

  public:
    Keymap() { reset(); }
    static const std::vector<Shortcut> &defaults();
    void reset();
    const Shortcut &get(const std::string &id) const;
    bool pressed(const std::string &id, bool repeat = true) const;
    void set(const std::string &id, ImGuiKey key, bool ctrl, bool shift, bool alt);
    std::string label(const std::string &id) const;
    // Other actions bound to the same chord.
    std::vector<std::string> conflicts(const std::string &id) const;
    Json json() const;
    void load(const Json &document);
};
} // namespace sf

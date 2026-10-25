#pragma once
// Legacy workspace names are data keys, not visible UI text. Keep old layouts readable.
#include "core/AtomicFile.h"
#include "core/Json.h"
#include <fstream>
#include <imgui_internal.h>
#include <map>
#include <string>

namespace sf {
inline const std::map<std::string, std::string> &legacyPanelNames() {
    static const std::map<std::string, std::string> names{{"Médias & effets", "Media & Effects"},
                                                          {"Calques", "Layers"},
                                                          {"Propriétés", "Properties"},
                                                          {"Animations", "Animation"},
                                                          {"Graphe VFX", "Shader Graph"},
                                                          {"Graphe des calques", "Layer Graph"},
                                                          {"Étalonnage", "Color Grading"},
                                                          {"Scopes vidéo", "Video Scopes"},
                                                          {"Mixeur & graphe audio", "Mixer & Audio Graph"},
                                                          {"Graphe DSP", "DSP Graph"},
                                                          {"Éditeur de clip", "Clip Editor"},
                                                          {"Motion design", "Motion Design"},
                                                          {"Bibliothèque Shaderixel", "Shaderixel Library"},
                                                          {"Repères", "Markers"},
                                                          {"Historique", "History"},
                                                          {"Sous-titres", "Subtitles"},
                                                          {"Angles", "Camera Angles"},
                                                          {"Raccourcis", "Keyboard Shortcuts"}};
    return names;
}
inline sx::Json englishPanelSettings(const sx::Json &value) {
    if (value.IsArray()) {
        auto out = sx::Json::Array();
        for (const auto &item : value.Elements())
            out.Push(englishPanelSettings(item));
        return out;
    }
    if (!value.IsObject())
        return value;
    auto out = sx::Json::Object();
    for (const auto &[key, item] : value.Items()) {
        auto found = legacyPanelNames().find(key);
        auto name = found == legacyPanelNames().end() ? key : found->second;
        if (name != key && value.Has(name))
            continue;
        out[name] = englishPanelSettings(item);
    }
    return out;
}
inline std::string englishWindowSettings(std::string text) {
    size_t at = 0;
    while ((at = text.find("[Window][", at)) != std::string::npos) {
        auto start = at + 9, end = text.find(']', start);
        if (end == std::string::npos)
            break;
        auto old = text.substr(start, end - start);
        for (const auto &[legacy, label] : legacyPanelNames()) {
            if (!old.starts_with(legacy))
                continue;
            auto suffix = old.substr(legacy.size());
            if (!suffix.empty() && !suffix.starts_with("##") && !suffix.starts_with("/"))
                continue;
            auto updated = label + suffix;
            text.replace(start, old.size(), updated);
            end = start + updated.size();
            char before[32], after[32];
            snprintf(before, sizeof(before), "Selected=0x%08X", ImHashStr(old.c_str()));
            snprintf(after, sizeof(after), "Selected=0x%08X", ImHashStr(updated.c_str()));
            for (size_t pos = 0; (pos = text.find(before, pos)) != std::string::npos; pos += strlen(after))
                text.replace(pos, strlen(before), after);
            break;
        }
        at = end + 1;
    }
    return text;
}
inline void migrateEnglishWorkspace(const std::string &path) {
    std::string text;
    if (!sx::ReadTextFile(path, text))
        return;
    auto updated = englishWindowSettings(text);
    if (updated == text)
        return;
    AtomicFile output(path);
    {
        std::ofstream stream(output.path(), std::ios::binary);
        stream << updated;
        if (!stream)
            throw std::runtime_error("Unable to migrate workspace settings");
    }
    output.commit();
}
} // namespace sf

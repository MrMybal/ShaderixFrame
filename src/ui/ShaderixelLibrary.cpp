#include "ui/Editor.h"
#include <imgui_stdlib.h>
#include <set>
namespace sf {
void Editor::addLibraryLayer(const std::string &id) {
    auto *d = s.renderer.library.find(id);
    if (!d || !d->generator) {
        error = "Un générateur Shaderixel est requis";
        return;
    }
    auto before = s.project;
    try {
        auto &track = s.project.addTrack(TrackKind::Video);
        track.name = d->name;
        Clip c;
        c.id = s.project.allocate();
        c.name = d->name;
        c.generator = true;
        c.start = s.time;
        c.duration = std::max(1., s.project.duration - s.time);
        c.effects = {s.renderer.library.instantiate(id)};
        track.clips = {c};
        std::rotate(s.project.tracks.begin(), s.project.tracks.end() - 1, s.project.tracks.end());
        appendLayerSurface(s.project, s.project.tracks.front().id, c.name);
        s.project.validate();
        s.history.checkpoint(before);
        s.selectClip(c.id);
        s.dirty = true;
        s.renderer.clearHistory();
        error.clear();
    } catch (const std::exception &e) {
        s.project = before;
        error = e.what();
    }
}
void Editor::shaderixelLibrary() {
    if (!panels["Bibliothèque Shaderixel"] || !beginPanel("Bibliothèque Shaderixel"))
        return;
    ImGui::InputTextWithHint("Recherche", "Nom / catégorie…", &shaderixelFilter);
    ImGui::Combo("Afficher", &shaderixelMode, "Tous\0Calques / générateurs\0Effets\0");
    std::set<std::string> categories;
    unsigned total = 0;
    for (auto &d : s.renderer.library.definitions)
        if (d.id.starts_with("shaderixel.")) {
            categories.insert(d.category);
            ++total;
        }
    if (ImGui::BeginCombo("Catégorie", shaderixelCategory.empty() ? "Toutes" : shaderixelCategory.c_str())) {
        if (ImGui::Selectable("Toutes", shaderixelCategory.empty()))
            shaderixelCategory.clear();
        for (auto &category : categories)
            if (ImGui::Selectable(category.c_str(), category == shaderixelCategory))
                shaderixelCategory = category;
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("%u éléments Shaderixel · sources embarquées", total);
    ImGui::BeginChild("ShaderixelCatalogue");
    for (auto &d : s.renderer.library.definitions) {
        if (!d.id.starts_with("shaderixel.") || (shaderixelMode == 1 && !d.generator) ||
            (shaderixelMode == 2 && d.generator) ||
            (!shaderixelCategory.empty() && shaderixelCategory != d.category) ||
            (!shaderixelFilter.empty() &&
             (d.name + " " + d.category).find(shaderixelFilter) == std::string::npos))
            continue;
        ImGui::PushID(d.id.c_str());
        if (ImGui::Selectable(d.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(0)) {
            if (d.generator)
                addLibraryLayer(d.id);
            else if (s.selected) {
                Json a = Json::Object();
                a["clip"] = s.selected;
                a["id"] = d.id;
                action("add_effect", a);
            }
        }
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(d.generator ? "SHADERIXEL_LAYER" : "EFFECT", d.id.c_str(),
                                      d.id.size() + 1);
            ImGui::TextUnformatted(d.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem()) {
            if (d.generator) {
                if (ImGui::MenuItem("Créer un calque dans la timeline"))
                    addLibraryLayer(d.id);
            } else if (ImGui::MenuItem("Appliquer au clip sélectionné", nullptr, false, s.selected != 0)) {
                Json a = Json::Object();
                a["clip"] = s.selected;
                a["id"] = d.id;
                action("add_effect", a);
            }
            ImGui::EndPopup();
        }
        ImGui::TextDisabled("%s · %s", d.category.c_str(), d.generator ? "CALQUE HLSL" : "EFFET HLSL");
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::TextWrapped("Double clic : créer un calque ou appliquer l'effet au clip sélectionné. Le graphe "
                       "des calques propose aussi ce catalogue.");
    ImGui::End();
}
} // namespace sf

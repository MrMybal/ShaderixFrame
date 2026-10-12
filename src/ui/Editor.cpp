#include "ui/Editor.h"
#include "adapters/CyDawWidgets.h"
#include "core/AtomicFile.h"
#include "core/Version.h"
#include "ui/Theme.h"
#include <commdlg.h>
#include <cstdio>
#include <cydaw/graph/GraphDefinition.h>
#include <fstream>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <set>

namespace sf {
static constexpr ImU32 Orange = IM_COL32(239, 139, 58, 255), Audio = IM_COL32(66, 179, 184, 255),
                       Muted = IM_COL32(120, 127, 143, 255);
static std::string timecode(double time, int fps) {
    int frame = int(std::max(0., time) * fps + 0.0001), sec = frame / fps;
    char b[40];
    snprintf(b, sizeof(b), "%02d:%02d:%02d:%02d", sec / 3600, sec / 60 % 60, sec % 60, frame % fps);
    return b;
}
static Json clipArgs(Id id) {
    Json a = Json::Object();
    a["clip"] = id;
    return a;
}
Json Editor::selectionArgs() const {
    Json args = Json::Object();
    args["clips"] = Json::Array();
    for (auto id : s.selection)
        args["clips"].Push(id);
    return args;
}
void applyFrameTheme() {
    sx::ui::ApplyTheme(1);
    sx::ui::LoadFonts(1);
    auto &style = ImGui::GetStyle();
    for (auto &c : style.Colors)
        if (c.x > 0.25f && c.z > c.x * 1.2f && c.z > c.y * 1.2f) {
            float a = c.w;
            c = ImVec4(0.94f, 0.55f, 0.23f, a);
        }
    style.WindowRounding = 3;
    style.FrameRounding = 3;
    style.WindowPadding = {10, 8};
}
bool Editor::action(const std::string &name, Json args) {
    try {
        if (name == "save_project" && !applyClipDrafts())
            return false;
        if ((name == "new_project" || name == "open_project" || name == "undo" || name == "redo") &&
            hasClipDrafts() && s.recovery.enabled())
            s.recovery.preserve(projectWithClipDrafts(), s.projectPath, s.time);
        protocol.execute(name, args);
        if (name == "start_export")
            exportsFocus = true;
        if (name == "new_project" || name == "open_project" || name == "undo" || name == "redo") {
            pendingImports.clear();
            thumbnails.clear();
            clipPreviews.clear();
            clipWorkspace.reset();
            clipSession.reset();
            editedContainer = 0;
            timelineDragging = graphDragging = 0;
            timelineDragBefore.reset();
            graphDragBefore.reset();
            marquee = graphPanning = false;
            graphViewIdentity.clear();
            draftIdentity.clear();
        }
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
void Editor::drop(const std::string &path) {
    try {
        auto absolute = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(path)).wstring());
        if (s.media.get(absolute).loading) {
            pendingImports.push_back({absolute, s.time});
            return;
        }
    } catch (const std::exception &failure) {
        error = failure.what();
        return;
    }
    Json a = Json::Object();
    a["path"] = path;
    action("import_media", a);
}
std::string Editor::dialog(bool save, const wchar_t *f, const wchar_t *ext) {
    wchar_t buffer[32768]{};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = GetActiveWindow();
    o.lpstrFilter = f;
    o.lpstrFile = buffer;
    o.nMaxFile = 32768;
    o.lpstrDefExt = ext;
    o.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    bool ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    return ok ? sx::Narrow(buffer) : "";
}
void Editor::editCheckpoint(const Project &before) {
    if (ImGui::IsItemActivated())
        s.history.checkpoint(before);
    if (ImGui::IsItemEdited()) {
        s.dirty = true;
        s.renderer.clearHistory();
    }
}
bool Editor::beginPanel(const char *name) {
    auto &open = panels[name];
    if (!ImGui::Begin(panelTitle(name).c_str(), &open)) {
        ImGui::End();
        return false;
    }
    return true;
}
void Editor::panelSettings() {
    if (!panelSuffix.empty() || !ImGui::GetIO().IniFilename)
        return;
    auto path = root / "workspace-panels.json";
    if (!panelSettingsRead) {
        panelSettingsRead = true;
        std::string text;
        Json j;
        if (sx::ReadTextFile(sx::Narrow(path.wstring()), text) && Json::Parse(text, j) && j.IsObject())
            for (auto &[name, open] : panels)
                if (j.Has(name))
                    open = j[name].B(open);
    }
    Json j = Json::Object();
    for (auto &[name, open] : panels)
        j[name] = open;
    auto signature = j.Dump(0);
    if (signature != panelSettingsSignature) {
        try {
            AtomicFile output(sx::Narrow(path.wstring()));
            {
                std::ofstream f(output.path(), std::ios::binary);
                f << signature;
                if (!f)
                    throw std::runtime_error("Écriture panneaux");
            }
            output.commit();
            panelSettingsSignature = signature;
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
}
void Editor::menu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Fichier")) {
            if (ImGui::MenuItem("Nouvelle séquence", "Ctrl+N"))
                action("new_project");
            if (ImGui::MenuItem("Ouvrir…", "Ctrl+O")) {
                auto path = dialog(false, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("open_project", a);
                }
            }
            if (ImGui::MenuItem("Enregistrer", "Ctrl+S")) {
                auto path = s.projectPath.empty() ? dialog(true, L"ShaderixFrame\0*.sframe\0\0", L"sframe")
                                                  : s.projectPath;
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("save_project", a);
                }
            }
            if (ImGui::MenuItem("Enregistrer sous…")) {
                auto path = dialog(true, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("save_project", a);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Récupérer une session…", nullptr, false, s.recovery.enabled())) {
                recoveries = s.recovery.list();
                recoveryVisible = true;
            }
            if (ImGui::MenuItem("Importer un média…", "Ctrl+I")) {
                auto path = dialog(false,
                                   L"Médias\0*.mp4;*.mov;*.mkv;*.avi;*.wmv;*.wav;*.mp3;*.flac;*.aac;*.png;*."
                                   L"jpg;*.jpeg;*.bmp\0Tous\0*.*\0\0",
                                   nullptr);
                if (!path.empty())
                    drop(path);
            }
            auto jobs = s.exports.list();
            bool activeExport =
                std::count_if(jobs.begin(), jobs.end(), [](const auto &job) { return job.active(); }) >= 32;
            if (ImGui::MenuItem("Exporter MP4 · H264 / AAC", nullptr, false, !activeExport)) {
                auto path = dialog(true, L"Vidéo MP4\0*.mp4\0\0", L"mp4");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    a["kind"] = "video";
                    action("start_export", a);
                }
            }
            if (ImGui::MenuItem("Exporter le mix WAV", nullptr, false, !activeExport)) {
                auto path = dialog(true, L"Audio WAV\0*.wav\0\0", L"wav");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    a["kind"] = "audio";
                    action("start_export", a);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Édition")) {
            if (ImGui::MenuItem("Annuler", "Ctrl+Z"))
                action("undo");
            if (ImGui::MenuItem("Rétablir", "Ctrl+Y"))
                action("redo");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Séquence")) {
            if (ImGui::MenuItem("Étalonnage")) {
                showGrading();
                ImGui::SetWindowFocus(panelTitle("Étalonnage").c_str());
            }
            if (ImGui::MenuItem("Scopes vidéo"))
                ImGui::SetWindowFocus(panelTitle("Scopes vidéo").c_str());
            ImGui::Separator();
            ImGui::TextDisabled("Composition RGBA · 16 bits flottants");
            const char *spaces[] = {"srgb", "linear709", "linear2020"};
            int space = s.project.workingSpace == "srgb" ? 0 : s.project.workingSpace == "linear709" ? 1 : 2;
            if (ImGui::Combo("Espace de travail", &space,
                             "sRGB historique\0Rec.709 linéaire\0Rec.2020 linéaire\0")) {
                Json a = Json::Object();
                a["space"] = spaces[space];
                action("set_color_management", a);
            }
            int tone = s.project.toneMap == "clip" ? 0 : s.project.toneMap == "reinhard" ? 1 : 2;
            const char *tones[] = {"clip", "reinhard", "aces"};
            if (ImGui::Combo("Affichage SDR", &tone, "Écrêtage\0Reinhard\0Courbe ACES ajustée\0")) {
                Json a = Json::Object();
                a["tone"] = tones[tone];
                action("set_color_management", a);
            }
            auto before = s.project;
            if (ImGui::SliderFloat("Exposition affichage", &s.project.exposure, -10, 10, "%.1f EV"))
                s.dirty = true;
            editCheckpoint(before);
            bool transparent = s.project.backgroundAlpha == 0;
            if (ImGui::Checkbox("Fond transparent", &transparent)) {
                Json a = Json::Object();
                a["backgroundAlpha"] = transparent ? 0 : 1;
                action("set_color_management", a);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Piste vidéo")) {
                Json a = Json::Object();
                a["kind"] = "video";
                action("add_track", a);
            }
            if (ImGui::MenuItem("Piste audio")) {
                Json a = Json::Object();
                a["kind"] = "audio";
                action("add_track", a);
            }
            if (ImGui::MenuItem("Générateur HLSL"))
                action("add_generator");
            if (ImGui::MenuItem("Titre / texte"))
                action("add_title");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Affichage")) {
            for (auto &[name, open] : panels)
                if (ImGui::MenuItem(name.c_str(), nullptr, open)) {
                    openPanel(name);
                }
            if (clipWorkspace && ImGui::BeginMenu("Fenêtres du clip")) {
                for (auto &[name, open] : clipWorkspace->panels)
                    if (ImGui::MenuItem(name.c_str(), nullptr, open)) {
                        clipWorkspace->openPanel(name);
                    }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Glisser un onglet hors de la fenêtre : écran séparé");
            if (ImGui::MenuItem("Réinitialiser les panneaux")) {
                for (auto &[name, open] : panels)
                    open = true;
                layoutReady = false;
                if (auto *workspace = ImGui::FindWindowByName("Workspace"))
                    ImGui::DockBuilderRemoveNode(workspace->GetID("FrameDock"));
                pendingPanelFocus = panelTitle("Composition");
            }
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 310);
        ImGui::TextColored(ImVec4(0.94f, 0.55f, 0.23f, 1), "SHADERIXFRAME");
        ImGui::SameLine();
        ImGui::TextDisabled("/  %s  ·  NATIVE", applicationVersion);
        ImGui::EndMainMenuBar();
    }
}
void Editor::parameter(const char *label, const std::string &key, float lo, float hi, float fallback,
                       int fx) {
    auto *c = s.project.findClip(s.selected);
    if (!c)
        return;
    auto &params = fx < 0 ? c->params : c->effects[fx].params;
    auto before = s.project;
    auto it = params.find(key);
    float value = it == params.end() ? fallback : it->second.value;
    bool animated = it != params.end() && !it->second.keys.empty();
    ImGui::PushID(key.c_str());
    ImGui::PushID(fx);
    ImGui::TextDisabled("%s%s", label, animated ? "  / ANIMÉ" : "");
    ImGui::SetNextItemWidth(std::max(50.f, ImGui::GetContentRegionAvail().x - 42));
    if (ImGui::SliderFloat("##value", &value, lo, hi, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        params.try_emplace(key, Parameter{fallback}).first->second.value = value;
    editCheckpoint(before);
    ImGui::SameLine();
    if (ImGui::Button("K", {30, 0})) {
        s.history.checkpoint(before);
        params.try_emplace(key, Parameter{fallback})
            .first->second.setKey({snapTime(s.time, s.project.fps, true), value, Curve::Smooth, 0});
        s.dirty = true;
        animationKey = key;
        animationEffect = fx;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ajouter un point au temps courant ; clic droit : afficher la courbe");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        animationKey = key;
        animationEffect = fx;
    }
    ImGui::PopID();
    ImGui::PopID();
}
void Editor::library() {
    if (!panels["Médias & effets"] || !beginPanel("Médias & effets"))
        return;
    if (ImGui::Button("+ Importer", {ImGui::GetContentRegionAvail().x, 0})) {
        auto path =
            dialog(false, L"Médias\0*.mp4;*.mov;*.avi;*.wav;*.mp3;*.png;*.jpg\0Tous\0*.*\0\0", nullptr);
        if (!path.empty())
            drop(path);
    }
    ImGui::TextDisabled("Glisser un fichier dans la fenêtre");
    ImGui::Checkbox("Proxies de prévisualisation", &s.media.proxiesEnabled);
    if (ImGui::Button("Préparer un proxy du clip sélectionné") && s.selected)
        action("prepare_proxy", clipArgs(s.selected));
    ImGui::Spacing();
    if (ImGui::BeginTabBar("library")) {
        if (ImGui::BeginTabItem("Médias")) {
            std::set<std::string> sources;
            for (auto &t : s.project.tracks)
                for (auto &c : t.clips)
                    if (!c.source.empty())
                        sources.insert(c.source);
            if (sources.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "Importez une vidéo, une image ou une bande sonore pour commencer votre montage.");
            }
            for (auto &path : sources) {
                ImGui::PushID(path.c_str());
                std::string name = sx::Narrow(sx::Utf8Path(path).filename().wstring());
                if (ImGui::Selectable(name.c_str())) {
                    for (auto &t : s.project.tracks)
                        for (auto &c : t.clips)
                            if (c.source == path) {
                                s.selectClip(c.id);
                                break;
                            }
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("MEDIA", path.c_str(), path.size() + 1);
                    ImGui::TextUnformatted(name.c_str());
                    ImGui::EndDragDropSource();
                }
                try {
                    auto &a = s.media.get(path);
                    if (!a.thumbnail.pixels.empty()) {
                        auto found = thumbnails.find(path);
                        if (found == thumbnails.end()) {
                            if (thumbnails.size() >= 64)
                                thumbnails.erase(thumbnails.begin());
                            D3D11_TEXTURE2D_DESC desc{};
                            desc.Width = a.thumbnail.width;
                            desc.Height = a.thumbnail.height;
                            desc.MipLevels = desc.ArraySize = 1;
                            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                            desc.SampleDesc.Count = 1;
                            desc.Usage = D3D11_USAGE_IMMUTABLE;
                            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            D3D11_SUBRESOURCE_DATA pixels{a.thumbnail.pixels.data(), a.thumbnail.width * 4,
                                                          0};
                            Com<ID3D11Texture2D> texture;
                            check(s.renderer.gpu()->CreateTexture2D(&desc, &pixels, &texture),
                                  "Vignette média");
                            Com<ID3D11ShaderResourceView> view;
                            check(s.renderer.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &view),
                                  "Vue vignette");
                            found = thumbnails.emplace(path, std::move(view)).first;
                        }
                        ImGui::Image((ImTextureID)(intptr_t)found->second.Get(),
                                     {160, float(a.thumbnail.height)});
                    }
                    if (a.loading)
                        ImGui::TextDisabled("Préparation en arrière-plan…");
                    ImGui::TextDisabled("%s  %.1f s", a.video ? "VIDÉO / IMAGE" : "AUDIO", a.duration);
                } catch (...) {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Média hors ligne");
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Effets")) {
            ImGui::InputTextWithHint("##search", "Chercher un effet…", &filter);
            for (auto &d : s.renderer.library.definitions) {
                if (!filter.empty() && d.name.find(filter) == std::string::npos)
                    continue;
                ImGui::PushID(d.id.c_str());
                if (ImGui::Selectable(d.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
                    ImGui::IsMouseDoubleClicked(0) && s.selected) {
                    auto a = clipArgs(s.selected);
                    a["id"] = d.id;
                    auto *track = s.project.owner(s.selected);
                    if (d.id.starts_with("cydaw.") && track && track->kind == TrackKind::Audio) {
                        a = Json::Object();
                        a["scope"] = "track";
                        a["target"] = track->id;
                        a["id"] = d.id;
                        action("add_mix_effect", a);
                    } else
                        action(d.kind == "transition" ? "set_transition" : "add_effect", a);
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(d.kind == "transition" ? "TRANSITION" : "EFFECT", d.id.c_str(),
                                              d.id.size() + 1);
                    ImGui::TextUnformatted(d.name.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::TextDisabled("%s  /  %s", d.category.c_str(), d.origin.c_str());
                ImGui::PopID();
            }
            ImGui::TextWrapped("Double-clic : ajouter au clip sélectionné.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sources")) {
            if (ImGui::Button("+ Titre / texte", {-1, 0}))
                action("add_title");
            if (ImGui::Button("+ Lumière procédurale", {-1, 0}))
                action("add_generator");
            ImGui::TextWrapped(
                "Générateur animé calculé en HLSL. Ajoutez vos effets et animez tous leurs paramètres.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
void Editor::preview() {
    if (panelSuffix.empty() && !ImGui::FindWindowByName("Composition"))
        ImGui::SetNextWindowFocus();
    if (!panels["Composition"] || !beginPanel("Composition"))
        return;
    ImGui::TextUnformatted(s.project.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%d × %d  /  %d fps%s", s.project.width, s.project.height, s.project.fps,
                        s.dirty ? "  •" : "");
    ImVec2 available = ImGui::GetContentRegionAvail();
    float w = std::max(64.f, available.x), h = std::max(64.f, available.y - 55);
    float ratio = float(s.project.width) / s.project.height;
    if (w / h > ratio)
        w = h * ratio;
    else
        h = w / ratio;
    ImGui::BeginChild("CompositionViewport", {0, std::max(80.f, available.y - 60)}, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto areaOrigin = ImGui::GetCursorScreenPos(), areaSize = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##compositionView", areaSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    bool imageHovered = ImGui::IsItemHovered();
    auto &viewIo = ImGui::GetIO();
    auto centre = areaOrigin + areaSize * .5f;
    if (imageHovered && viewIo.MouseWheel) {
        auto fixed =
            (sx::float2{viewIo.MousePos.x - centre.x, viewIo.MousePos.y - centre.y} - compositionView.pan) /
            compositionView.zoom;
        compositionView.zoom =
            std::clamp(compositionView.zoom * std::pow(1.15f, viewIo.MouseWheel), .125f, 32.f);
        compositionView.pan = sx::float2{viewIo.MousePos.x - centre.x, viewIo.MousePos.y - centre.y} -
                              fixed * compositionView.zoom;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(2, 0))
        compositionView.pan = compositionView.pan + sx::float2{viewIo.MouseDelta.x, viewIo.MouseDelta.y};
    if (imageHovered && ImGui::IsMouseDoubleClicked(0) && !gradePick) {
        compositionView = {};
    }
    if (ImGui::BeginPopupContextItem("CompositionViewMenu")) {
        if (ImGui::MenuItem("Adapter à la fenêtre"))
            compositionView = {};
        if (ImGui::MenuItem("Pixels 1:1")) {
            compositionView.pan = {};
            compositionView.zoom = float(s.project.width) / w;
        }
        ImGui::EndPopup();
    }
    w *= compositionView.zoom;
    h *= compositionView.zoom;
    ImVec2 imageOrigin = centre + ImVec2(compositionView.pan.x - w * .5f, compositionView.pan.y - h * .5f);
    if (s.renderer.view())
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)s.renderer.view(), imageOrigin,
                                             imageOrigin + ImVec2(w, h));
    bool wasGradePicking = gradePick != 0;
    gradePreview(imageOrigin, w, h);
    auto *selected = s.project.findClip(s.selected);
    if (maskGuides && !wasGradePicking && selected && selected->active(s.time) &&
        s.project.owner(selected->id)->kind == TrackKind::Video && selected->mask.shape != MaskShape::None) {
        auto origin = imageOrigin;
        bool hovered = imageHovered;
        auto *draw = ImGui::GetWindowDrawList();
        auto mouse = ImGui::GetIO().MousePos;
        float angle = selected->param("rotation", s.time, 0) * .01745329252f;
        float scale = std::max(.001f, selected->param("scale", s.time, 1));
        auto rotate = [](ImVec2 v, float a) {
            return ImVec2(std::cos(a) * v.x - std::sin(a) * v.y, std::sin(a) * v.x + std::cos(a) * v.y);
        };
        ImVec2 maskCenter{selected->param("mask_x", s.time, 0) * ratio, selected->param("mask_y", s.time, 0)};
        auto screen = [&](ImVec2 local) {
            auto transformed = rotate(local * scale, angle);
            return ImVec2(origin.x + w * (.5f + transformed.x / ratio + selected->param("x", s.time, 0)),
                          origin.y + h * (.5f + transformed.y + selected->param("y", s.time, 0)));
        };
        auto center = screen(maskCenter);
        if (hovered && std::abs(mouse.x - center.x) < 9 && std::abs(mouse.y - center.y) < 9 &&
            !s.project.owner(selected->id)->locked) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            ImGui::SetTooltip(
                "Déplacer le masque ; les paramètres animés reçoivent un point au temps courant");
            if (ImGui::IsMouseClicked(0)) {
                s.history.checkpoint(s.project);
                s.playing = false;
                maskDragging = selected->id;
                maskDragMouse = mouse;
                maskDragX = selected->param("mask_x", s.time, 0);
                maskDragY = selected->param("mask_y", s.time, 0);
                maskDragTime = s.time;
            }
        }
        if (maskDragging == selected->id && ImGui::IsMouseDown(0) && !s.project.owner(selected->id)->locked) {
            ImVec2 delta{(mouse.x - maskDragMouse.x) / h, (mouse.y - maskDragMouse.y) / h};
            delta = rotate(delta, -angle) / scale;
            auto update = [&](const char *key, float value) {
                auto &parameter = selected->params[key];
                value = std::clamp(value, -2.f, 2.f);
                parameter.value = value;
                if (!parameter.keys.empty()) {
                    Keyframe point{maskDragTime, value, Curve::Smooth};
                    for (auto &existing : parameter.keys)
                        if (std::abs(existing.time - maskDragTime) < 1e-7) {
                            point.curve = existing.curve;
                            point.shape = existing.shape;
                        }
                    parameter.setKey(point);
                }
            };
            update("mask_x", maskDragX + delta.x / ratio);
            update("mask_y", maskDragY + delta.y);
            maskCenter = {selected->param("mask_x", s.time, 0) * ratio, selected->param("mask_y", s.time, 0)};
            center = screen(maskCenter);
            s.dirty = true;
        }
        float maskAngle = selected->param("mask_rotation", s.time, 0) * .01745329252f;
        float rx = selected->param("mask_width", s.time, 1) * ratio * .5f;
        float ry = selected->param("mask_height", s.time, 1) * .5f;
        std::vector<ImVec2> outline;
        if (selected->mask.shape == MaskShape::Rectangle) {
            for (ImVec2 corner : {ImVec2(-rx, -ry), ImVec2(rx, -ry), ImVec2(rx, ry), ImVec2(-rx, ry)})
                outline.push_back(screen(maskCenter + rotate(corner, maskAngle)));
        } else if (selected->mask.shape == MaskShape::Bezier) {
            for (size_t i = 0; i < selected->mask.points.size(); ++i) {
                auto &a = selected->mask.points[i];
                auto &b = selected->mask.points[(i + 1) % selected->mask.points.size()];
                for (int step = 0; step < 16; ++step) {
                    float t = step / 16.f, u = 1 - t;
                    ImVec2 point{
                        u * u * u * a.x + 3 * u * u * t * a.outX + 3 * u * t * t * b.inX + t * t * t * b.x,
                        u * u * u * a.y + 3 * u * u * t * a.outY + 3 * u * t * t * b.inY + t * t * t * b.y};
                    outline.push_back(
                        screen(maskCenter + rotate({point.x * rx * 2, point.y * ry * 2}, maskAngle)));
                }
            }
        } else {
            for (int i = 0; i < 64; ++i) {
                float a = i * (6.28318530718f / 64);
                outline.push_back(
                    screen(maskCenter + rotate({rx * std::cos(a), ry * std::sin(a)}, maskAngle)));
            }
        }
        draw->PushClipRect(origin, {origin.x + w, origin.y + h}, true);
        draw->AddPolyline(outline.data(), int(outline.size()), Orange, ImDrawFlags_Closed, 1.5f);
        draw->AddCircle(center, 5, Orange, 16, 1.5f);
        draw->PopClipRect();
    }
    if (!ImGui::IsMouseDown(0) || maskDragging != s.selected || !maskGuides)
        maskDragging = 0;
    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button("|<")) {
        s.time = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("<"))
        s.time = std::max(0., s.time - 1. / s.project.fps);
    ImGui::SameLine();
    if (ImGui::Button(s.playing ? "Pause" : "Lecture", {90, 0}))
        s.playing = !s.playing;
    ImGui::SameLine();
    if (ImGui::Button(">"))
        s.time = std::min(s.project.duration, s.time + 1. / s.project.fps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(.94f, .55f, .23f, 1), "%s", timecode(s.time, s.project.fps).c_str());
    ImGui::SameLine();
    ImGui::Checkbox("Boucle", &loop);
    ImGui::SameLine();
    ImGui::Checkbox("Guides du masque", &maskGuides);
    ImGui::End();
}
void Editor::layers() {
    if (!panels["Calques"] || !beginPanel("Calques"))
        return;
    ImGui::TextDisabled("HAUT → AVANT-PLAN");
    for (size_t i = 0; i < s.project.tracks.size(); ++i) {
        auto &t = s.project.tracks[i];
        ImGui::PushID(int(t.id));
        auto before = s.project;
        bool visible = !t.muted;
        if (ImGui::Checkbox("##eye", &visible)) {
            s.history.checkpoint(before);
            t.muted = !visible;
            s.dirty = true;
        }
        ImGui::SameLine();
        bool selected = false;
        for (auto &c : t.clips)
            selected |= std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
        if (ImGui::Selectable(t.name.c_str(), selected))
            if (!t.clips.empty())
                s.selectClip(t.clips.front().id);
        if (ImGui::BeginPopupContextItem("trackMenu")) {
            if (ImGui::MenuItem("Monter", nullptr, false, i > 0)) {
                s.history.checkpoint(before);
                std::swap(s.project.tracks[i], s.project.tracks[i - 1]);
                s.dirty = true;
            }
            if (ImGui::MenuItem("Descendre", nullptr, false, i + 1 < s.project.tracks.size())) {
                s.history.checkpoint(before);
                std::swap(s.project.tracks[i], s.project.tracks[i + 1]);
                s.dirty = true;
            }
            if (ImGui::MenuItem(t.locked ? "Déverrouiller" : "Verrouiller")) {
                s.history.checkpoint(before);
                t.locked = !t.locked;
                s.dirty = true;
            }
            ImGui::EndPopup();
        }
        ImGui::TextDisabled("    %s  ·  %zu clip(s)%s", t.kind == TrackKind::Video ? "VIDÉO" : "AUDIO",
                            t.clips.size(), t.locked ? "  / VERROUILLÉ" : "");
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ImGui::Button("+ Vidéo")) {
        Json a = Json::Object();
        a["kind"] = "video";
        action("add_track", a);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Audio")) {
        Json a = Json::Object();
        a["kind"] = "audio";
        action("add_track", a);
    }
    ImGui::End();
}
bool Editor::effectControls(Id clip, int index) {
    auto *c = s.project.findClip(clip);
    if (!c)
        return false;
    int destination = index;
    ImGui::BeginDisabled(index == 0);
    if (ImGui::SmallButton("Monter"))
        destination = index - 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= int(c->effects.size()));
    if (ImGui::SmallButton("Descendre"))
        destination = index + 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool remove = ImGui::SmallButton("Retirer");
    if (!remove && destination == index)
        return false;
    auto args = clipArgs(clip);
    args["effect"] = index;
    if (!remove)
        args["position"] = destination;
    action(remove ? "remove_effect" : "set_effect", args);
    if (error.empty()) {
        auto remap = [&](int old) {
            if (remove)
                return old == index ? -1 : old > index ? old - 1 : old;
            if (old == index)
                return destination;
            if (index < destination && old > index && old <= destination)
                return old - 1;
            if (index > destination && old >= destination && old < index)
                return old + 1;
            return old;
        };
        bool animationRemoved = remove && animationEffect == index;
        selectedEffect = std::max(0, remap(selectedEffect));
        animationEffect = remap(animationEffect);
        if (animationRemoved)
            animationKey = s.project.owner(clip)->kind == TrackKind::Audio ? "gain" : "opacity";
        draftIdentity.clear();
        graphNode = wireNode = 0;
        graphViewIdentity.clear();
    }
    return true;
}
bool Editor::transitionControls(Id id) {
    auto *c = s.project.findClip(id);
    if (!c || s.project.owner(id)->kind != TrackKind::Video ||
        !ImGui::CollapsingHeader("Transition d'entrée",
                                 c->transition.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        return false;
    std::string label = c->transition.enabled ? c->transition.id : "Aucune";
    if (auto *definition = s.renderer.library.find(c->transition.id); c->transition.enabled && definition)
        label = definition->name;
    std::string choice;
    bool remove = false;
    if (ImGui::BeginCombo("Transition", label.c_str())) {
        remove = ImGui::Selectable("Aucune", !c->transition.enabled);
        for (auto &definition : s.renderer.library.definitions)
            if (definition.kind == "transition" &&
                ImGui::Selectable(definition.name.c_str(), c->transition.id == definition.id))
                choice = definition.id;
        ImGui::EndCombo();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("TRANSITION"))
            choice = static_cast<const char *>(payload->Data);
        ImGui::EndDragDropTarget();
    }
    if (remove || !choice.empty()) {
        auto args = clipArgs(id);
        if (!remove)
            args["id"] = choice;
        action(remove ? "remove_transition" : "set_transition", args);
        draftIdentity.clear();
        return true;
    }
    if (!c->transition.enabled)
        return false;
    auto before = s.project;
    double duration = c->transition.duration, minimum = 1e-5, maximum = c->duration;
    if (ImGui::DragScalar("Durée transition", ImGuiDataType_Double, &duration, .01f, &minimum, &maximum,
                          "%.3f s", ImGuiSliderFlags_AlwaysClamp) &&
        std::isfinite(duration))
        c->transition.duration = duration;
    editCheckpoint(before);
    ImGui::TextWrapped(
        "Superposer le début de ce clip aux calques dessous. Le son se règle avec ses propres fondus.");
    ImGui::Text("Progression au temps courant : %.0f %%", c->transitionProgress(s.time) * 100.f);
    parameter("Progression (remplace le temps automatique)", "transition_progress", 0, 1,
              c->transitionProgress(s.time));
    if (ImGui::SmallButton("Progression automatique")) {
        auto args = clipArgs(id);
        args["reset_progress"] = true;
        action("set_transition", args);
        return true;
    }
    try {
        for (auto &p : effectParameters(s.renderer.library.transitionCode(c->transition))) {
            int count = p.type == "float2"                        ? 2
                        : p.type == "float3"                      ? 3
                        : p.type == "float4" || p.type == "color" ? 4
                                                                  : 1;
            for (int index = 0; index < count; ++index)
                parameter(p.label.c_str(), p.key + (count > 1 ? "." + std::to_string(index) : ""), p.minimum,
                          p.maximum, p.value[index]);
        }
    } catch (const std::exception &ex) {
        ImGui::TextWrapped("%s", ex.what());
    }
    if (ImGui::SmallButton("Éditer le HLSL de transition")) {
        selectedEffect = -2;
        draftIdentity.clear();
        ImGui::SetWindowFocus(panelTitle("HLSL").c_str());
    }
    return false;
}
void Editor::inspector() {
    if (!panels["Propriétés"] || !beginPanel("Propriétés"))
        return;
    auto *c = s.project.findClip(s.selected);
    auto *track = s.project.owner(s.selected);
    if (!c || !track) {
        ImGui::TextWrapped("Sélectionnez un clip dans la timeline pour régler ses propriétés.");
        ImGui::End();
        return;
    }
    ImGui::BeginDisabled(track->locked);
    auto before = s.project;
    ImGui::InputText("Nom", &c->name);
    editCheckpoint(before);
    double frame = 1. / s.project.fps;
    auto number = [&](const char *label, double value, double lo, double hi, int field) {
        auto b = s.project;
        bool edited = ImGui::DragScalar(label, ImGuiDataType_Double, &value, 0.03f, &lo, &hi,
                                        field == 3 ? "%.3f ×" : "%.3f s");
        editCheckpoint(b);
        if (edited) {
            try {
                s.project.setTiming(c->id, field == 0 ? value : c->start, field == 1 ? value : c->duration,
                                    field == 2 ? value : c->in, field == 3 ? value : c->speed);
                error.clear();
            } catch (const std::exception &e) {
                error = e.what();
            }
        }
    };
    number("Début", c->start, 0, 86400, 0);
    number("Durée", c->duration, frame, 86400, 1);
    number("Entrée source", c->in, 0, 86400, 2);
    number("Vitesse", c->speed, 0.05, 16, 3);
    if (c->link)
        ImGui::TextColored(ImVec4(.25f, .7f, .72f, 1), "Groupe lié · %u", c->link);
    ImGui::SeparatorText("Fondus du clip");
    auto fade = [&](const char *label, double &value) {
        auto b = s.project;
        double lo = 0, hi = c->duration;
        ImGui::DragScalar(label, ImGuiDataType_Double, &value, .01f, &lo, &hi, "%.3f s",
                          ImGuiSliderFlags_AlwaysClamp);
        editCheckpoint(b);
    };
    fade("Entrée", c->fadeIn);
    fade("Sortie", c->fadeOut);
    if (transitionControls(c->id)) {
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }
    if (c->title.enabled && ImGui::CollapsingHeader("Titre", ImGuiTreeNodeFlags_DefaultOpen)) {
        char content[16385]{};
        memcpy(content, c->title.text.data(), std::min(c->title.text.size(), sizeof(content) - 1));
        before = s.project;
        if (ImGui::InputTextMultiline("##titleText", content, sizeof(content), {-1, 64}))
            c->title.text = content;
        editCheckpoint(before);
        auto font = c->title.font;
        before = s.project;
        if (ImGui::InputText("Police", &font) && !font.empty() && font.size() <= 256)
            c->title.font = font;
        editCheckpoint(before);
        before = s.project;
        ImGui::Checkbox("Gras", &c->title.bold);
        editCheckpoint(before);
        ImGui::SameLine();
        before = s.project;
        ImGui::Checkbox("Italique", &c->title.italic);
        editCheckpoint(before);
        before = s.project;
        ImGui::Combo("Alignement", &c->title.alignment, "Gauche\0Centre\0Droite\0");
        editCheckpoint(before);
        parameter("Taille du texte (px)", "font_size", 4, 256, 48);
        parameter("Contour noir (px)", "text_outline", 0, 16, 0);
        parameter("Opacité de l'ombre", "text_shadow", 0, 1, 0);
        parameter("Ombre X (px)", "text_shadow_x", -256, 256, 4);
        parameter("Ombre Y (px)", "text_shadow_y", -256, 256, 4);
        parameter("Diffusion de l'ombre (px)", "text_shadow_blur", 0, 32, 0);
        float color[4];
        const char *keys[] = {"text_r", "text_g", "text_b", "text_a"};
        for (int i = 0; i < 4; ++i) {
            auto parameter = c->params.find(keys[i]);
            color[i] = parameter == c->params.end() ? 1 : parameter->second.value;
        }
        before = s.project;
        ImGui::SetNextItemWidth(std::max(50.f, ImGui::GetContentRegionAvail().x - 42));
        if (ImGui::ColorEdit4("##textColor", color, ImGuiColorEditFlags_AlphaBar))
            for (int i = 0; i < 4; ++i)
                c->params[keys[i]].value = color[i];
        editCheckpoint(before);
        ImGui::SameLine();
        if (ImGui::Button("K##textColor")) {
            s.history.checkpoint(s.project);
            for (int i = 0; i < 4; ++i)
                c->params[keys[i]].setKey({snapTime(s.time, s.project.fps, true), color[i], Curve::Smooth});
            animationKey = "text_r";
            animationEffect = -1;
            s.dirty = true;
        }
    }
    if (track->kind == TrackKind::Video && ImGui::CollapsingHeader("Couleurs de la source")) {
        const char *spaces[] = {"auto", "srgb", "linear709", "linear2020", "logc3-awg3-ei800"};
        int choice = 0;
        for (int i = 0; i < 5; ++i)
            if (c->sourceColor == spaces[i])
                choice = i;
        before = s.project;
        if (ImGui::Combo(
                "Espace source", &choice,
                "Automatique\0sRGB\0Rec.709 linéaire\0Rec.2020 linéaire\0ARRI LogC3 / AWG3 · EI800\0"))
            c->sourceColor = spaces[choice];
        editCheckpoint(before);
        before = s.project;
        ImGui::Checkbox("Pixels source prémultipliés", &c->sourcePremultiplied);
        editCheckpoint(before);
    }
    if (track->kind == TrackKind::Video && ImGui::CollapsingHeader("Masque")) {
        before = s.project;
        int shape = int(c->mask.shape);
        if (ImGui::Combo("Forme", &shape, "Aucun\0Rectangle\0Ellipse\0Bézier\0")) {
            c->mask.shape = MaskShape(shape);
            if (shape == 3 && c->mask.points.empty())
                c->mask.points = {{-.4f, -.3f, -.4f, -.3f, -.4f, -.3f},
                                  {.4f, -.3f, .4f, -.3f, .4f, -.3f},
                                  {.4f, .3f, .4f, .3f, .4f, .3f},
                                  {-.4f, .3f, -.4f, .3f, -.4f, .3f}};
        }
        editCheckpoint(before);
        if (c->mask.shape != MaskShape::None) {
            before = s.project;
            ImGui::Checkbox("Inverser", &c->mask.inverted);
            editCheckpoint(before);
            parameter("Centre X", "mask_x", -1, 1, 0);
            parameter("Centre Y", "mask_y", -1, 1, 0);
            parameter("Largeur", "mask_width", .001f, 2, 1);
            parameter("Hauteur", "mask_height", .001f, 2, 1);
            parameter("Rotation du masque", "mask_rotation", -180, 180, 0);
            parameter("Contour progressif (px)", "mask_feather", 0, 256, 0);
            if (c->mask.shape == MaskShape::Bezier) {
                for (size_t i = 0; i < c->mask.points.size(); ++i) {
                    ImGui::PushID(int(i));
                    auto &point = c->mask.points[i];
                    if (ImGui::TreeNode("Point", "Point %u", unsigned(i + 1))) {
                        before = s.project;
                        ImGui::SliderFloat2("Position", &point.x, -1, 1);
                        editCheckpoint(before);
                        before = s.project;
                        ImGui::SliderFloat2("Poignée entrante", &point.inX, -2, 2);
                        editCheckpoint(before);
                        before = s.project;
                        ImGui::SliderFloat2("Poignée sortante", &point.outX, -2, 2);
                        editCheckpoint(before);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (c->mask.points.size() < 16 && ImGui::Button("Ajouter un point Bézier")) {
                    before = s.project;
                    c->mask.points.push_back({0, .5f, 0, .5f, 0, .5f});
                    s.history.checkpoint(before);
                    s.dirty = true;
                }
                if (c->mask.points.size() > 3 && ImGui::Button("Retirer le dernier point")) {
                    before = s.project;
                    c->mask.points.pop_back();
                    s.history.checkpoint(before);
                    s.dirty = true;
                }
            }

            if (ImGui::Button("Suivre le masque…")) {
                trackingBegin = std::max(s.time, c->start);
                trackingEnd = std::min(c->start + c->duration, trackingBegin + 5);
                ImGui::OpenPopup("Suivi du masque");
            }
            bool requested = false;
            if (ImGui::BeginPopup("Suivi du masque")) {
                ImGui::TextWrapped("Le suivi calcule une translation à partir de la zone centrale du masque. "
                                   "Choisissez une zone contrastée. Les points restent éditables.");
                ImGui::InputDouble("Début s", &trackingBegin);
                ImGui::InputDouble("Fin s", &trackingEnd);
                ImGui::SliderInt("Rayon de recherche px", &trackingRadius, 1, 64);
                auto status = s.tracking.status();
                ImGui::Text("%s · %s", status["state"].S().c_str(), status["error"].S().c_str());
                ImGui::ProgressBar(status["progress"].F());
                if (ImGui::Button("Calculer")) {
                    auto args = clipArgs(c->id);
                    args["begin"] = trackingBegin;
                    args["end"] = trackingEnd;
                    args["radius"] = trackingRadius;
                    action("start_mask_tracking", args);
                    requested = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Annuler")) {
                    action("cancel_mask_tracking");
                    requested = true;
                }
                ImGui::BeginDisabled(status["state"].S() != "completed");
                if (ImGui::Button("Appliquer les animations")) {
                    Json args = Json::Object();
                    args["job"] = status["job"];
                    action("apply_mask_tracking", args);
                    requested = true;
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            if (requested) {
                ImGui::EndDisabled();
                ImGui::End();
                return;
            }
        }
    }
    if (track->kind == TrackKind::Video) {
        ImGui::SeparatorText("Transformation");
        parameter("Opacité", "opacity", 0, 1, 1);
        parameter("Position X", "x", -1, 1, 0);
        parameter("Position Y", "y", -1, 1, 0);
        parameter("Échelle", "scale", 0.05f, 4, 1);
        parameter("Rotation", "rotation", -180, 180, 0);
        int mode = int(c->param("blend", s.time, 0));
        auto blendBefore = s.project;
        if (ImGui::Combo("Fusion", &mode, "Normal\0Addition\0Produit\0Écran\0")) {
            s.history.checkpoint(blendBefore);
            c->params["blend"].value = float(mode);
            s.dirty = true;
        }
    } else {
        ImGui::SeparatorText("Mixage du clip");
        parameter("Gain dB", "gain", -60, 12, 0);
        parameter("Panoramique", "pan", -1, 1, 0);
        animationKey = animationEffect < 0 && animationKey == "opacity" ? "gain" : animationKey;
    }
    if (track->kind == TrackKind::Video) {
        ImGui::SeparatorText("Pile d'effets HLSL");
        if (ImGui::BeginCombo("+ Effet", "Ajouter…")) {
            for (auto &d : s.renderer.library.definitions)
                if (d.kind == "effect" && ImGui::Selectable(d.name.c_str())) {
                    auto a = clipArgs(c->id);
                    a["id"] = d.id;
                    action("add_effect", a);
                    break;
                }
            ImGui::EndCombo();
        }
        if (ImGui::Button("+ Plugin OFX…")) {
            auto path = dialog(false, L"Plugin OpenFX Windows x64\0*.ofx\0Tous\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto a = clipArgs(c->id);
                a["path"] = path;
                action("add_ofx_effect", a);
            }
        }
        for (int i = 0; i < int(c->effects.size()); ++i) {
            auto &e = c->effects[i];
            ImGui::PushID(i);
            before = s.project;
            ImGui::Checkbox("##enabled", &e.enabled);
            editCheckpoint(before);
            ImGui::SameLine();
            auto *d = s.renderer.library.find(e.id);
            std::string title = e.id == "grading"
                                    ? static_cast<const Effect &>(e).grading["name"].S("Correction")
                                : e.id == "ofx" ? e.graph["name"].S("OFX")
                                : d             ? d->name
                                                : e.id;
            if (ImGui::Selectable(title.c_str(), selectedEffect == i)) {
                selectedEffect = i;
                draftIdentity.clear();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto *p = ImGui::AcceptDragDropPayload("EFFECT")) {
                    auto a = clipArgs(c->id);
                    a["id"] = static_cast<const char *>(p->Data);
                    action("add_effect", a);
                }
                ImGui::EndDragDropTarget();
            }
            if (selectedEffect == i) {
                try {
                    if (e.id == "grading") {
                        if (ImGui::Button("Ouvrir Étalonnage"))
                            ImGui::SetWindowFocus(panelTitle("Étalonnage").c_str());
                    }
                    if (e.id == "ofx")
                        for (auto &p : e.graph["parameters"].Elements())
                            parameter(p["name"].S().c_str(), p["key"].S(), p["minimum"].F(), p["maximum"].F(),
                                      p["default"].F(), i);
                    for (auto &param : effectParameters(e.id == "grading" ? "" : effectCode(e))) {
                        int n = param.type == "float2"                              ? 2
                                : param.type == "float3"                            ? 3
                                : (param.type == "float4" || param.type == "color") ? 4
                                                                                    : 1;
                        for (int k = 0; k < n; ++k) {
                            std::string key = param.key + (n > 1 ? "." + std::to_string(k) : "");
                            std::string label = param.label + (n > 1 ? " " + std::to_string(k) : "");
                            parameter(label.c_str(), key, param.minimum, param.maximum, param.value[k], i);
                        }
                    }
                } catch (const std::exception &ex) {
                    ImGui::TextWrapped("%s", ex.what());
                }
                if (effectControls(c->id, i)) {
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopID();
        }
    }
    if (track->kind == TrackKind::Audio) {
        ImGui::SeparatorText("Effets audio CyDAW 2");
        int stretch = int(std::lround(
            c->param("stretch_mode", s.time, c->param("preserve_pitch", s.time, 0) > .5f ? 1 : 0)));
        if (ImGui::Combo("Adaptation temporelle", &stretch, "Repitch\0Tonal\0Battements\0Texture\0")) {
            auto a = clipArgs(c->id);
            a["key"] = "stretch_mode";
            a["value"] = stretch;
            action("set_parameter", a);
        }
        auto graphTarget = clipArgs(c->id);
        graphTarget["scope"] = "clip";
        if (dspGraphEditor(graphTarget, c->effects)) {
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        if (ImGui::Button("+ Plugin natif / VST3")) {
            auto path = dialog(false, L"Plugins audio\0*.dll;*.vst3\0Tous\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto args = clipArgs(c->id);
                args["scope"] = "clip";
                args["path"] = path;
                args["kind"] = sx::Utf8Path(path).extension() == L".vst3" ? "vst3" : "native";
                action("add_external_audio", args);
            }
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        if (ImGui::BeginCombo("+ DSP", "Ajouter…")) {
            for (auto &d : cydaw::engine::effectCatalogue)
                if (!d.convertsChannels() && ImGui::Selectable(std::string(d.displayName).c_str())) {
                    auto a = clipArgs(c->id);
                    a["id"] = "cydaw." + std::string(d.typeKey);
                    action("add_audio_effect", a);
                    break;
                }
            ImGui::EndCombo();
        }
        for (int i = 0; i < int(c->effects.size()); ++i) {
            auto &e = c->effects[i];
            auto *d = audioEffect(e.id);
            if (!d)
                continue;
            ImGui::PushID(i);
            auto b = s.project;
            ImGui::Checkbox("##bypass", &e.enabled);
            editCheckpoint(b);
            ImGui::SameLine();
            ImGui::TextUnformatted(std::string(d->displayName).c_str());
            auto pluginTarget = clipArgs(c->id);
            pluginTarget["scope"] = "clip";
            pluginTarget["effect"] = i;
            if (pluginControls(pluginTarget, e)) {
                ImGui::PopID();
                break;
            }
            for (auto &param : d->parameters)
                parameter(std::string(param.key).c_str(), std::string(param.key), param.minimum,
                          param.maximum, param.defaultValue, i);
            if (externalAudio(e.id))
                for (auto &[key, value] : e.params)
                    parameter((e.parameterNames.contains(key) ? e.parameterNames.at(key) : key).c_str(), key,
                              0, 1, value.value, i);
            if (effectControls(c->id, i)) {
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}
void Editor::timeline() {
    clipPreviews.beginFrame();
    if (!panels["Timeline"] || !beginPanel("Timeline"))
        return;
    if (ImGui::Button("Découper [S]") && !s.selection.empty()) {
        auto a = selectionArgs();
        a["time"] = s.time;
        action("split_clips", a);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Magnétisme / images", &snap);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Zoom", &zoom, 2, 2000, "%.0f px/s", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    if (ImGui::Button("Tout voir")) {
        zoom = std::clamp((ImGui::GetWindowWidth() - 220) * .95f / float(s.project.duration), 2.f, 2000.f);
        scroll = 0;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Alt : libre  ·  Molette : zoom · Maj+molette : déplacement  ·  bords : trim");
    auto selectAll = [&]() {
        std::vector<Id> ids;
        for (auto &track : s.project.tracks)
            for (auto &clip : track.clips)
                ids.push_back(clip.id);
        s.setSelection(ids);
    };
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
            selectAll();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s.selectClip(0);
            timelineDragging = 0;
            timelineDragBefore.reset();
            marquee = false;
        }
    }
    if (ImGui::SmallButton("Éditer le clip / conteneur") && s.selected)
        openClipEditor(s.selected);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu clips sélectionnés", s.selection.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Tout [Ctrl+A]"))
        selectAll();
    ImGui::SameLine();
    ImGui::BeginDisabled(s.selection.empty());
    if (ImGui::SmallButton("Dupliquer"))
        action("duplicate_clips", selectionArgs());
    ImGui::SameLine();
    if (ImGui::SmallButton("Supprimer"))
        action("delete_clips", selectionArgs());
    ImGui::SameLine();
    if (ImGui::SmallButton("Supprimer + fermer l'espace")) {
        auto args = selectionArgs();
        args["ripple"] = true;
        action("delete_clips", args);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Referme les intervalles de la sélection sur toutes les pistes.\nLes clips qui les "
                          "traversent doivent être découpés ou sélectionnés.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl/clic · rectangle : sélection");
    ImGui::SameLine();
    if (ImGui::SmallButton("Montage avancé…"))
        ImGui::OpenPopup("Montage avancé");
    if (ImGui::BeginPopup("Montage avancé")) {
        auto &delta = trimDelta;
        auto &overlap = transitionOverlap;

        ImGui::TextUnformatted("Trim sur le clip sélectionné");
        ImGui::InputFloat("Variation de durée (s)", &delta, .1f, 1.f);
        ImGui::Checkbox("Régler l'entrée source", &leftTrim);
        ImGui::BeginDisabled(!s.selected);
        if (ImGui::Button("Trim ripple")) {
            auto args = clipArgs(s.selected);
            args["delta"] = delta;
            args["left"] = leftTrim;
            action("ripple_trim", args);
        }
        Id next = 0;
        if (auto *clip = s.project.findClip(s.selected)) {
            for (auto &other : s.project.owner(clip->id)->clips)
                if (other.id != clip->id && std::abs(clip->start + clip->duration - other.start) < 1e-6) {
                    next = other.id;
                    break;
                }
        }
        ImGui::BeginDisabled(!next);
        if (ImGui::Button("Trim roll avec le clip suivant")) {
            auto args = clipArgs(s.selected);
            args["next"] = next;
            args["delta"] = delta;
            action("roll_trim", args);
        }
        ImGui::InputFloat("Chevauchement (s)", &overlap, .1f, 1.f);
        if (ImGui::Button("Créer un fondu avec le suivant")) {
            auto args = clipArgs(s.selected);
            args["next"] = next;
            args["duration"] = overlap;
            action("overlap_transition", args);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::SeparatorText("Retirer un intervalle sur toutes les pistes");
        ImGui::InputDouble("Début", &rangeBegin, .1, 1, "%.3f");
        ImGui::InputDouble("Fin", &rangeEnd, .1, 1, "%.3f");
        if (ImGui::Button("Découper et fermer")) {
            Json args = Json::Object();
            args["begin"] = rangeBegin;
            args["end"] = rangeEnd;
            action("remove_range", args);
        }
        ImGui::EndPopup();
    }
    ImVec2 navOrigin = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(navOrigin + ImVec2(180, 0));
    ImGui::InvisibleButton("CyDAWNavigator", {std::max(40.f, ImGui::GetContentRegionAvail().x), 16});
    auto &navIo = ImGui::GetIO();
    if (ImGui::IsItemActivated())
        navigateAnchor = scroll + (navIo.MousePos.x - navOrigin.x - 180) / zoom;
    if (ImGui::IsItemActive()) {
        zoom = std::clamp(zoom * std::exp(navIo.MouseDelta.y * .012f), 2.f, 2000.f);
        scroll = std::max(0., navigateAnchor - (navIo.MousePos.x - navOrigin.x - 180) / zoom);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        ImGui::SetTooltip("Glisser : gauche/droite pour naviguer, bas pour zoomer, haut pour dézoomer. "
                          "Double clic : tout voir.");
        if (ImGui::IsMouseDoubleClicked(0)) {
            zoom =
                std::clamp((ImGui::GetContentRegionAvail().x - 180) / float(s.project.duration), 2.f, 2000.f);
            scroll = 0;
        }
    }
    auto *navDraw = ImGui::GetWindowDrawList();
    auto navMin = ImGui::GetItemRectMin(), navMax = ImGui::GetItemRectMax();
    navDraw->AddRectFilled(navMin, navMax,
                           ImGui::IsItemActive() ? IM_COL32(95, 63, 35, 255) : IM_COL32(44, 46, 54, 255), 3);
    for (float xx = navMin.x; xx < navMax.x; xx += 16)
        navDraw->AddLine({xx, navMin.y + 8}, {xx, navMax.y - 2}, IM_COL32(112, 116, 128, 180));
    ImGui::BeginChild("timelineCanvas", {0, -24}, false,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float header = 180, lane = 68;
    float width = std::max(300.f, ImGui::GetContentRegionAvail().x),
          height = std::max(120.f, 32 + lane * s.project.tracks.size() + 78);
    ImGui::InvisibleButton("timelineInput", {width, height},
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    float left = origin.x + header, right = origin.x + width, bottom = origin.y + height;
    auto x = [&](double t) { return left + float((t - scroll) * zoom); };
    auto time = [&](float px) {
        return snapTime(scroll + (px - left) / zoom, s.project.fps, snap && !ImGui::GetIO().KeyAlt);
    };
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("MEDIA")) {
            Json a = Json::Object();
            a["path"] = static_cast<const char *>(payload->Data);
            a["start"] = time(mouse.x);
            action("import_media", a);
        }
        if (auto *payload = ImGui::AcceptDragDropPayload("EFFECT")) {
            int target = int((mouse.y - origin.y - 32) / lane);
            if (target >= 0 && target < int(s.project.tracks.size())) {
                for (auto &clip : s.project.tracks[target].clips)
                    if (clip.active(time(mouse.x))) {
                        Json a = clipArgs(clip.id);
                        a["id"] = static_cast<const char *>(payload->Data);
                        auto &track = s.project.tracks[target];
                        if (track.kind == TrackKind::Audio && a["id"].S().starts_with("cydaw.")) {
                            auto id = a["id"];
                            a = Json::Object();
                            a["scope"] = "track";
                            a["target"] = track.id;
                            a["id"] = id;
                            action("add_mix_effect", a);
                        } else
                            action("add_effect", a);
                        break;
                    }
            }
        }
        if (auto *payload = ImGui::AcceptDragDropPayload("SHADERIXEL_LAYER"))
            addLibraryLayer(static_cast<const char *>(payload->Data));
        if (auto *payload = ImGui::AcceptDragDropPayload("TRANSITION")) {
            int target = int((mouse.y - origin.y - 32) / lane);
            if (target >= 0 && target < int(s.project.tracks.size()))
                for (auto &clip : s.project.tracks[target].clips)
                    if (clip.active(time(mouse.x))) {
                        auto args = clipArgs(clip.id);
                        args["id"] = static_cast<const char *>(payload->Data);
                        action("set_transition", args);
                        break;
                    }
        }
        ImGui::EndDragDropTarget();
    }
    dl->AddRectFilled(origin, {right, bottom}, IM_COL32(18, 20, 25, 255));
    dl->PushClipRect({left, origin.y}, {right, bottom}, true);
    float minimumStep = (ImGui::CalcTextSize("00:00:00:00").x + 18) / zoom;
    float step = 3600;
    for (float interval : {.25f, .5f, 1.f, 2.f, 5.f, 10.f, 30.f, 60.f, 120.f, 300.f, 600.f, 1800.f, 3600.f})
        if (interval >= minimumStep) {
            step = interval;
            break;
        }
    for (double t = std::floor(scroll / step) * step; t < scroll + (right - left) / zoom; t += step) {
        float xx = x(t);
        dl->AddLine({xx, origin.y}, {xx, bottom}, IM_COL32(255, 255, 255, 16));
        auto text = timecode(t, s.project.fps);
        dl->AddText({xx + 4, origin.y + 7}, Muted, text.c_str());
    }
    if (hovered && mouse.y < origin.y + 30 && mouse.x >= left && ImGui::IsMouseDown(0))
        s.time = std::clamp(time(mouse.x), 0., s.project.duration);
    if (hovered && ImGui::GetIO().MouseWheel != 0) {
        if (!ImGui::GetIO().KeyShift) {
            double anchor = scroll + (mouse.x - left) / zoom;
            zoom = std::clamp(zoom * std::pow(1.2f, ImGui::GetIO().MouseWheel), 2.f, 2000.f);
            scroll = std::max(0., anchor - (mouse.x - left) / zoom);
        } else
            scroll = std::max(0., scroll - ImGui::GetIO().MouseWheel * 80 / zoom);
    }
    bool clickedClip = false;
    for (size_t ti = 0; ti < s.project.tracks.size(); ++ti) {
        auto &t = s.project.tracks[ti];
        float y = origin.y + 32 + ti * lane;
        dl->AddRectFilled({left, y}, {right, y + lane - 1},
                          ti % 2 ? IM_COL32(25, 27, 33, 255) : IM_COL32(22, 24, 30, 255));
        for (auto &c : t.clips) {
            float a = x(c.start), b = x(c.start + c.duration);
            if (b < left || a > right)
                continue;
            ImU32 tint = t.kind == TrackKind::Audio ? Audio : Orange;
            if (t.muted)
                tint = Muted;
            bool chosen = std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
            cydaw::v2::ui::arrangerClipBackground(dl, {a, y + 7}, {b, y + lane - 7}, tint,
                                                  hovered && mouse.x >= a && mouse.x <= b &&
                                                      mouse.y >= y + 7 && mouse.y <= y + lane - 7);
            dl->AddRect({a, y + 7}, {b, y + lane - 7}, tint, 4, 0, chosen ? 2.f : 1.f);
            dl->PushClipRect({std::max(a + 5, left), y + 8}, {std::min(b - 4, right), y + lane - 8}, true);
            dl->AddText({a + 8, y + 12}, tint, c.name.c_str());
            if (t.kind == TrackKind::Audio && !c.source.empty()) {
                try {
                    auto &asset = s.media.get(c.source);
                    for (float xx = std::max(a, left); xx < std::min(b, right); xx += 3) {
                        double st = c.in + (scroll + (xx - left) / zoom - c.start) * c.speed;
                        size_t index = size_t(std::max(0., st) * 100);
                        float peak = index < asset.peaks.size() ? asset.peaks[index] / asset.peakCeiling : 0;
                        float mid = y + 44;
                        dl->AddLine({xx, mid - peak * 15}, {xx, mid + peak * 15}, Audio);
                    }
                } catch (...) {
                }
            } else {
                float start = std::max(a, left), end = std::min(b, right);
                for (float px = start; px < end; px += 72) {
                    auto sample = std::clamp(scroll + (px + 36 - left) / zoom,
                                             c.start + std::min(.05, c.duration * .25),
                                             c.start + c.duration - std::min(.001, c.duration * .1));
                    sample = c.start + std::floor((sample - c.start) * 2) / 2;
                    sample = std::clamp(sample, c.start + std::min(.05, c.duration * .25),
                                        c.start + c.duration - std::min(.001, c.duration * .1));
                    if (auto *view = clipPreviews.get(s.project, c, sample, s.renderer))
                        dl->AddImage((ImTextureID)view, {px, y + 28}, {std::min(px + 70, end), y + lane - 9});
                }
            }
            dl->PopClipRect();
            if (c.fadeIn > 0)
                dl->AddLine({a, y + lane - 8}, {x(c.start + c.fadeIn), y + 8}, tint, 1.5f);
            if (c.fadeOut > 0)
                dl->AddLine({x(c.start + c.duration - c.fadeOut), y + 8}, {b, y + lane - 8}, tint, 1.5f);
            if (c.link)
                dl->AddCircleFilled({a + 5, y + lane - 11}, 2, Audio);
            if (c.transition.enabled) {
                float end = x(c.start + c.transition.duration);
                dl->AddRectFilled({a, y + lane - 15}, {end, y + lane - 7}, IM_COL32(66, 179, 184, 140));
            }
            bool hit = hovered && mouse.x >= std::max(a, left) && mouse.x <= std::min(b, right) &&
                       mouse.y > y + 7 && mouse.y < y + lane - 7;
            if (hit && ImGui::IsMouseDoubleClicked(0) && t.kind == TrackKind::Video) {
                openClipEditor(c.id);
                clickedClip = true;
            } else if (hit && ImGui::IsMouseClicked(0)) {
                clickedClip = true;
                bool additive = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
                s.selectClip(c.id, additive, !additive);
                animationEffect = -1;
                animationKey = t.kind == TrackKind::Audio ? "gain" : "opacity";
                draftIdentity.clear();
                if (!t.locked && !additive) {
                    try {
                        timelineTrim = mouse.x - a < 8 ? -1 : b - mouse.x < 8 ? 1 : 0;
                        timelineDragClips =
                            s.project.selection(timelineTrim == 0 ? s.selection : std::vector<Id>{c.id});
                        timelineDragBefore = s.project;
                        timelineDragging = c.id;
                        timelineStart = c.start;
                        timelineLength = c.duration;
                        timelineIn = c.in;
                        timelineDragX = mouse.x;
                        timelineMoved = 0;
                        timelineMinimum = c.start;
                        timelineSequenceLength = s.project.duration;
                        for (auto id : timelineDragClips)
                            timelineMinimum = std::min(timelineMinimum, s.project.findClip(id)->start);
                        s.playing = false;
                    } catch (const std::exception &e) {
                        error = e.what();
                    }
                }
            }
            if (hit && ImGui::IsMouseClicked(1)) {
                s.selectClip(c.id, false, true);
                ImGui::OpenPopup("clipContext");
            }
        }
    }
    if (timelineDragging && ImGui::IsMouseDown(0)) {
        auto *c = s.project.findClip(timelineDragging);
        if (c) {
            double delta = (mouse.x - timelineDragX) / zoom;
            double start = c->start, length = c->duration, in = c->in;
            if (timelineTrim == 0)
                start = snapTime(timelineStart + delta, s.project.fps, snap && !ImGui::GetIO().KeyAlt);
            if (timelineTrim == 1)
                length = std::max(1. / s.project.fps,
                                  time(x(timelineStart + timelineLength) + mouse.x - timelineDragX) -
                                      timelineStart);
            if (timelineTrim == -1) {
                double pos = snapTime(timelineStart + delta, s.project.fps, snap && !ImGui::GetIO().KeyAlt);
                pos = std::clamp(pos, std::max(0., timelineStart - timelineIn / c->speed),
                                 timelineStart + timelineLength - 1. / s.project.fps);
                length = timelineLength - (pos - timelineStart);
                in = timelineIn + (pos - timelineStart) * c->speed;
                start = pos;
            }
            try {
                bool edited = false;
                if (timelineTrim == 0) {
                    double moved = std::max(-timelineMinimum, start - timelineStart);
                    double change = moved - timelineMoved;
                    if (change != 0) {
                        s.project.moveClips(timelineDragClips, change);
                        edited = true;
                    }
                    timelineMoved = moved;
                    s.project.duration = timelineSequenceLength;
                    for (auto id : timelineDragClips) {
                        auto &clip = *s.project.findClip(id);
                        s.project.duration = std::max(s.project.duration, clip.start + clip.duration);
                    }
                } else if (start != c->start || length != c->duration || in != c->in) {
                    s.project.setTiming(timelineDragging, start, length, in, c->speed, false);
                    edited = true;
                }
                if (edited) {
                    if (timelineDragBefore) {
                        s.history.checkpoint(*timelineDragBefore);
                        timelineDragBefore.reset();
                    }
                    s.dirty = true;
                }
            } catch (const std::exception &e) {
                error = e.what();
            }
        }
    }
    if (timelineDragging && ImGui::IsMouseReleased(0)) {
        auto *owner = s.project.owner(timelineDragging);
        if (timelineTrim == 0 && owner && !owner->locked &&
            timelineDragClips.size() == s.project.linked(timelineDragging, false).size()) {
            int target = int((mouse.y - origin.y - 32) / lane);
            if (owner && target >= 0 && target < int(s.project.tracks.size())) {
                auto &dest = s.project.tracks[target];
                if (&dest != owner && dest.kind == owner->kind && !dest.locked) {
                    if (timelineDragBefore) {
                        s.history.checkpoint(*timelineDragBefore);
                        timelineDragBefore.reset();
                    }
                    Clip moved = *s.project.findClip(timelineDragging);
                    std::erase_if(owner->clips,
                                  [&](const Clip &clip) { return clip.id == timelineDragging; });
                    dest.clips.push_back(moved);
                    s.dirty = true;
                }
            }
        }
        timelineDragging = 0;
        timelineDragBefore.reset();
    }
    if (hovered && !clickedClip && ImGui::IsMouseClicked(0) && mouse.x >= left && mouse.y >= origin.y + 32 &&
        mouse.y < origin.y + 32 + lane * s.project.tracks.size()) {
        marquee = true;
        marqueeAnchor = mouse;
        marqueeAdd = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
        marqueeBase = marqueeAdd ? s.selection : std::vector<Id>{};
        if (!marqueeAdd)
            s.selectClip(0);
    }
    if (marquee) {
        ImVec2 low{std::max(left, std::min(marqueeAnchor.x, mouse.x)), std::min(marqueeAnchor.y, mouse.y)};
        ImVec2 high{std::min(right, std::max(marqueeAnchor.x, mouse.x)), std::max(marqueeAnchor.y, mouse.y)};
        auto ids = marqueeBase;
        if (std::abs(mouse.x - marqueeAnchor.x) > 3 || std::abs(mouse.y - marqueeAnchor.y) > 3)
            for (size_t ti = 0; ti < s.project.tracks.size(); ++ti) {
                float y = origin.y + 32 + ti * lane;
                for (auto &clip : s.project.tracks[ti].clips)
                    if (x(clip.start) <= high.x && x(clip.start + clip.duration) >= low.x &&
                        y + 7 <= high.y && y + lane - 7 >= low.y)
                        ids.push_back(clip.id);
            }
        s.setSelection(ids);
        dl->AddRectFilled(low, high, IM_COL32(239, 139, 58, 30));
        dl->AddRect(low, high, Orange);
        if (ImGui::IsMouseReleased(0) || !ImGui::IsMouseDown(0))
            marquee = false;
    }
    auto *selected = s.project.findClip(s.selected);
    if (selected) {
        bool animationLocked = s.project.owner(selected->id)->locked;
        if (animationEffect >= int(selected->effects.size()))
            animationEffect = -1;
        float top = origin.y + 32 + lane * s.project.tracks.size() + 5, band = 65;
        auto &ps =
            animationEffect < 0
                ? selected->params
                : selected->effects[std::min(animationEffect, int(selected->effects.size()) - 1)].params;
        auto it = ps.find(animationKey);
        if (it != ps.end()) {
            auto &param = it->second;
            float lo = animationKey == "gain" ? -60.f : 0, hi = animationKey == "gain" ? 12.f : 1;
            for (auto &k : param.keys) {
                lo = std::min(lo, k.value);
                hi = std::max(hi, k.value);
            }
            auto ky = [&](float v) { return top + band - 7 - (v - lo) / (hi - lo) * (band - 14); };
            float oldX = left, oldY = ky(param.at(scroll));
            for (float xx = left + 2; xx < right; xx += 2) {
                float yy = ky(param.at(scroll + (xx - left) / zoom));
                dl->AddLine({oldX, oldY}, {xx, yy}, Audio, 1.7f);
                oldX = xx;
                oldY = yy;
            }
            auto &keyDrag = timelineKeyDrag;
            for (int i = 0; i < int(param.keys.size()); ++i) {
                auto &k = param.keys[i];
                ImVec2 center{x(k.time), ky(k.value)};
                dl->AddCircleFilled(center, 4, Orange);
                if (!animationLocked && hovered && ImGui::IsMouseClicked(0) &&
                    std::abs(mouse.x - center.x) < 8 && std::abs(mouse.y - center.y) < 8) {
                    s.history.checkpoint(s.project);
                    keyDrag = i;
                }
                if (!animationLocked && hovered && ImGui::IsMouseClicked(1) &&
                    std::abs(mouse.x - center.x) < 8 && std::abs(mouse.y - center.y) < 8) {
                    s.history.checkpoint(s.project);
                    param.keys.erase(param.keys.begin() + i);
                    s.dirty = true;
                    break;
                }
            }
            if (!animationLocked && keyDrag >= 0 && keyDrag < int(param.keys.size()) &&
                ImGui::IsMouseDown(0)) {
                auto &k = param.keys[keyDrag];
                double low = keyDrag ? param.keys[keyDrag - 1].time + 1. / s.project.fps : 0,
                       high = keyDrag + 1 < int(param.keys.size())
                                  ? param.keys[keyDrag + 1].time - 1. / s.project.fps
                                  : s.project.duration;
                if (low <= high)
                    k.time = std::clamp(time(mouse.x), low, high);
                k.value = std::clamp(lo + (top + band - 7 - mouse.y) / (band - 14) * (hi - lo), lo, hi);
                s.dirty = true;
            }
            if (ImGui::IsMouseReleased(0))
                keyDrag = -1;
            if (!animationLocked && hovered && mouse.y > top && mouse.y < top + band && mouse.x > left &&
                ImGui::IsMouseDoubleClicked(0)) {
                s.history.checkpoint(s.project);
                param.setKey({time(mouse.x),
                              std::clamp(lo + (top + band - 7 - mouse.y) / (band - 14) * (hi - lo), lo, hi),
                              Curve::Smooth, 0});
                s.dirty = true;
            }
        }
        dl->AddText(
            {left + 8, top + 4}, Muted,
            ("AUTOMATION  /  " + animationKey + "  ·  double-clic : point  /  clic droit : retirer").c_str());
    }
    float play = x(s.time);
    dl->AddLine({play, origin.y + 23}, {play, bottom}, Orange, 2);
    dl->AddTriangleFilled({play - 5, origin.y + 19}, {play + 5, origin.y + 19}, {play, origin.y + 26},
                          Orange);
    dl->PopClipRect();
    dl->AddRectFilled(origin, {left, origin.y + 32}, IM_COL32(29, 31, 38, 255));
    dl->AddText({origin.x + 12, origin.y + 7}, Muted, "PISTES  /  SECONDES");
    for (size_t i = 0; i < s.project.tracks.size(); ++i) {
        auto &t = s.project.tracks[i];
        float y = origin.y + 32 + i * lane;
        dl->AddRectFilled({origin.x, y}, {left, y + lane - 1}, IM_COL32(31, 33, 40, 255));
        dl->AddRectFilled({origin.x, y}, {origin.x + 3, y + lane - 1},
                          t.kind == TrackKind::Audio ? Audio : Orange);
        dl->PushClipRect({origin.x + 10, y + 4}, {left - 7, y + 30}, true);
        dl->AddText({origin.x + 10, y + 11}, IM_COL32(222, 224, 232, 255), t.name.c_str());
        dl->PopClipRect();
        ImGui::SetCursorScreenPos({origin.x + 10, y + 36});
        ImGui::PushID(int(t.id));
        auto before = s.project;
        if (ImGui::SmallButton(t.muted ? "M •" : "M")) {
            s.history.checkpoint(before);
            t.muted = !t.muted;
            s.dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(t.solo ? "S •" : "S")) {
            s.history.checkpoint(before);
            t.solo = !t.solo;
            s.dirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled(t.locked ? "VERROUILLÉ" : t.kind == TrackKind::Audio ? "AUDIO" : "VIDÉO");
        ImGui::PopID();
    }
    if (ImGui::BeginPopup("clipContext")) {
        if (ImGui::MenuItem("Découper à la tête")) {
            auto a = selectionArgs();
            a["time"] = s.time;
            action("split_clips", a);
        }
        if (ImGui::MenuItem("Dupliquer"))
            action("duplicate_clips", selectionArgs());
        if (auto *c = s.project.findClip(s.selected)) {
            if (c->link && ImGui::MenuItem("Délier le groupe"))
                action("unlink_clip", clipArgs(s.selected));
            if (ImGui::BeginMenu("Lier avec…")) {
                Id other = 0;
                for (auto &track : s.project.tracks)
                    for (auto &candidate : track.clips) {
                        if (candidate.id == s.selected || (c->link && candidate.link == c->link))
                            continue;
                        ImGui::PushID(int(candidate.id));
                        if (ImGui::MenuItem((track.name + " / " + candidate.name).c_str(), nullptr, false,
                                            !track.locked))
                            other = candidate.id;
                        ImGui::PopID();
                    }
                if (other) {
                    auto a = clipArgs(s.selected);
                    a["other"] = other;
                    action("link_clips", a);
                }
                ImGui::EndMenu();
            }
        }
        if (ImGui::MenuItem("Supprimer"))
            action("delete_clips", selectionArgs());
        if (ImGui::MenuItem("Supprimer et fermer l'espace", "Maj+Suppr")) {
            auto args = selectionArgs();
            args["ripple"] = true;
            action("delete_clips", args);
        }
        ImGui::EndPopup();
    }
    ImGui::SetCursorScreenPos({origin.x, bottom});
    ImGui::Dummy({width, 1});
    ImGui::EndChild();
    ImGui::SetNextItemWidth(-1);
    float pos = float(scroll);
    if (ImGui::SliderFloat("##scroll", &pos, 0, float(std::max(0., s.project.duration - 1)),
                           "Position %.2f s"))
        scroll = pos;
    ImGui::End();
}
void Editor::shaderEditor() {
    if (!panels["HLSL"] || !beginPanel("HLSL"))
        return;
    auto *c = s.project.findClip(s.selected);
    bool transition = selectedEffect == -2 && c && c->transition.enabled;
    if (c && !transition && selectedEffect >= 0 && selectedEffect < int(c->effects.size()) &&
        c->effects[selectedEffect].id == "grading") {
        ImGui::TextUnformatted("Shader généré par Étalonnage, adapté à l'espace de travail.");
        auto generated = gradeShader(c->effects[selectedEffect], s.project.workingSpace);
        ImGui::InputTextMultiline("##gradeShader", &generated, {-1, -1}, ImGuiInputTextFlags_ReadOnly);
        ImGui::End();
        return;
    }
    if (c && !transition && selectedEffect >= 0 && selectedEffect < int(c->effects.size()) &&
        c->effects[selectedEffect].id == "ofx") {
        ImGui::TextWrapped("OFX · %s. Ses paramètres et animations sont dans Propriétés.",
                           c->effects[selectedEffect].graph["name"].S().c_str());
        ImGui::End();
        return;
    }
    if (!c || (!transition && (selectedEffect < 0 || selectedEffect >= int(c->effects.size())))) {
        ImGui::TextWrapped("Choisissez un effet dans Propriétés pour éditer son shader. EffectContext "
                           "fournit time, localTime, deltaTime et frame.");
        ImGui::End();
        return;
    }
    std::string identity = std::to_string(c->id) + ":" + std::to_string(selectedEffect);
    if (identity != draftIdentity) {
        try {
            shaderDraft = transition ? s.renderer.library.transitionCode(c->transition)
                                     : effectCode(c->effects[selectedEffect]);
        } catch (...) {
            shaderDraft = transition ? c->transition.code : c->effects[selectedEffect].code;
        }
        draftIdentity = identity;
    }
    if (ImGui::Button("Compiler & appliquer")) {
        auto a = clipArgs(c->id);
        if (!transition)
            a["effect"] = selectedEffect;
        a["code"] = shaderDraft;
        action(transition ? "set_transition_shader" : "set_shader", a);
    }
    ImGui::SameLine();
    if (ImGui::Button("Recharger"))
        draftIdentity.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("HLSL 5.0  /  pixel shader");
    ImGui::PushFont(sx::ui::GetFonts().mono);
    ImGui::InputTextMultiline("##code", &shaderDraft, {-1, -1}, ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopFont();
    ImGui::End();
}
void Editor::animations() {
    if (!panels["Animations"] || !beginPanel("Animations"))
        return;
    auto *clip = s.project.findClip(s.selected);
    if (!clip) {
        ImGui::TextWrapped(
            "Sélectionnez un clip, puis un paramètre. K ajoute un point à la tête de lecture.");
        ImGui::End();
        return;
    }
    if (s.project.owner(clip->id)->locked) {
        ImGui::TextDisabled("Piste verrouillée");
        ImGui::End();
        return;
    }
    if (animationEffect >= int(clip->effects.size()))
        animationEffect = -1;
    if (ImGui::BeginCombo("Paramètre", animationKey.c_str())) {
        for (auto &[key, p] : clip->params) {
            if (ImGui::Selectable(("Clip / " + key).c_str(), animationEffect < 0 && animationKey == key)) {
                animationKey = key;
                animationEffect = -1;
            }
        }
        for (int i = 0; i < int(clip->effects.size()); ++i)
            for (auto &[key, p] : clip->effects[i].params) {
                std::string label = clip->effects[i].id + " / " + key;
                if (ImGui::Selectable(label.c_str(), animationEffect == i && animationKey == key)) {
                    animationKey = key;
                    animationEffect = i;
                }
            }
        ImGui::EndCombo();
    }
    auto &parameters = animationEffect < 0 ? clip->params : clip->effects[animationEffect].params;
    auto found = parameters.find(animationKey);
    if (found == parameters.end()) {
        ImGui::End();
        return;
    }
    auto &parameter = found->second;
    ImGui::Text("Valeur à %s : %.3f", timecode(s.time, s.project.fps).c_str(), parameter.at(s.time));
    if (ImGui::Button("+ Point à la tête")) {
        s.history.checkpoint(s.project);
        parameter.setKey({snapTime(s.time, s.project.fps, true), parameter.value, Curve::Smooth, 0});
        s.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Retirer l'animation") && !parameter.keys.empty()) {
        s.history.checkpoint(s.project);
        parameter.value = parameter.at(s.time);
        parameter.keys.clear();
        s.dirty = true;
    }
    if (ImGui::BeginTable("keys", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Temps");
        ImGui::TableSetupColumn("Valeur");
        ImGui::TableSetupColumn("Interpolation");
        ImGui::TableSetupColumn("Courbure");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();
        for (int i = 0; i < int(parameter.keys.size()); ++i) {
            ImGui::PushID(i);
            auto &k = parameter.keys[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            auto before = s.project;
            double lo = i ? parameter.keys[i - 1].time + 1e-5 : 0;
            double hi = i + 1 < int(parameter.keys.size()) ? parameter.keys[i + 1].time - 1e-5
                                                           : std::max(s.project.duration, k.time);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragScalar("##time", ImGuiDataType_Double, &k.time, .01f, &lo, &hi, "%.3f s",
                              ImGuiSliderFlags_AlwaysClamp);
            editCheckpoint(before);
            ImGui::TableNextColumn();
            before = s.project;
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##value", &k.value, .01f);
            editCheckpoint(before);
            ImGui::TableNextColumn();
            int curve = int(k.curve);
            before = s.project;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##curve", &curve, "Linéaire\0Douce\0Palier\0CyDAW\0")) {
                s.history.checkpoint(before);
                k.curve = Curve(curve);
                s.dirty = true;
            }
            ImGui::TableNextColumn();
            before = s.project;
            ImGui::BeginDisabled(k.curve != Curve::Shaped);
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##shape", &k.shape, -1, 1, "%.2f");
            editCheckpoint(before);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Aller"))
                s.time = k.time;
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                s.history.checkpoint(s.project);
                parameter.keys.erase(parameter.keys.begin() + i);
                s.dirty = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
void Editor::layerGraphEditor() {
    if (!panels["Graphe des calques"] || !beginPanel("Graphe des calques"))
        return;
    try {
        const bool implicit = s.project.layerGraph.IsNull();
        if (implicit)
            ImGui::TextWrapped("Timelines → calques → graphe de composition Shaderixel. Le graphe organise "
                               "le mélange nodal.");
        if (ImGui::Button("Reconstruire depuis les pistes")) {
            Json args = Json::Object();
            args["graph"] = defaultLayerGraph(s.project);
            action("set_layer_graph", args);
            ImGui::End();
            return;
        }
        auto draft = implicit ? defaultLayerGraph(s.project) : s.project.layerGraph;
        if (layerCanvas.draw(draft, s.project, s.renderer, s.time)) {
            Json args = Json::Object();
            args["graph"] = draft;
            action("set_layer_graph", args);
        }
        if (auto node = layerCanvas.takeLutRequest()) {
            auto path = dialog(false, L"LUT 1D / 2D / 3D\0*.cube;*.png;*.tif;*.exr\0Tous\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto lut = loadLut(sx::Utf8Path(path), layerCanvas.lutLayoutName());
                auto graph = s.project.layerGraph;
                auto nodes = Json::Array();
                for (auto item : graph["nodes"].Elements()) {
                    if (item["id"].U() == node)
                        item["effect"]["grading"]["lut"] = lut;
                    nodes.Push(item);
                }
                graph["nodes"] = nodes;
                Json args = Json::Object();
                args["graph"] = graph;
                action("set_layer_graph", args);
            }
        }
        if (auto node = layerCanvas.takeShaderRequest()) {
            layerShaderNode = node;
            ImGui::SetWindowFocus(panelTitle("Graphe VFX").c_str());
        }
    } catch (const std::exception &e) {
        error = e.what();
        ImGui::TextWrapped("%s", error.c_str());
    }
    ImGui::End();
}
void Editor::graphEditor() {
    if (!panels["Graphe VFX"] || !beginPanel("Graphe VFX"))
        return;
    Effect layerEffect;
    if (layerShaderNode) {
        try {
            auto graph = decodeLayerGraph(s.project.layerGraph, s.project);
            layerEffect = graph.effects.at(layerShaderNode);
        } catch (...) {
            layerShaderNode = 0;
        }
    }
    if (layerShaderNode) {
        ImGui::Text("Graphe shader du nœud layer %u", layerShaderNode);
        ImGui::SameLine();
        if (ImGui::Button("Retour aux effets du clip"))
            layerShaderNode = 0;
    }
    auto *c = s.project.findClip(s.selected);
    if (!layerShaderNode) {
        if (!c || s.project.owner(c->id)->kind != TrackKind::Video) {
            ImGui::TextWrapped(
                "Sélectionnez un clip vidéo pour créer ou ouvrir un graphe d'effet Shaderixel.");
            ImGui::End();
            return;
        }
        if (s.project.owner(c->id)->locked) {
            ImGui::TextDisabled("Piste verrouillée");
            ImGui::End();
            return;
        }
        if (ImGui::Button("+ Graphe Shaderixel")) {
            s.history.checkpoint(s.project);
            Effect e;
            e.id = "graph";
            e.graph = sx::EffectGraph::Default().ToJson();
            e.code = effectCode(e);
            c->effects.push_back(e);
            selectedEffect = int(c->effects.size() - 1);
            s.dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Importer .json")) {
            auto path = dialog(false, L"Graphe Shaderixel\0*.json;*.sxgraph\0Tous\0*.*\0\0", nullptr);
            if (!path.empty())
                try {
                    std::string text;
                    sx::ReadTextFile(path, text);
                    Json j;
                    std::string err;
                    if (!Json::Parse(text, j, &err))
                        throw std::runtime_error(err);
                    Effect e;
                    e.id = "graph";
                    e.graph = j;
                    e.code = effectCode(e);
                    s.renderer.validateShader(e.code);
                    s.history.checkpoint(s.project);
                    c->effects.push_back(e);
                    selectedEffect = int(c->effects.size() - 1);
                    s.dirty = true;
                } catch (const std::exception &ex) {
                    error = ex.what();
                }
        }
        if (selectedEffect < 0 || selectedEffect >= int(c->effects.size()) ||
            c->effects[selectedEffect].graph.IsNull() || c->effects[selectedEffect].id == "ofx") {
            ImGui::TextWrapped(
                "Le graphe génère le même HLSL que Shaderixel. Les shaders texte se trouvent dans "
                "le panneau HLSL.");
            ImGui::End();
            return;
        }
    }
    auto &effect = layerShaderNode ? layerEffect : c->effects[selectedEffect];
    auto identity = layerShaderNode ? "layer:" + std::to_string(layerShaderNode)
                                    : std::to_string(c->id) + ":" + std::to_string(selectedEffect);
    bool fitView = graphViewIdentity != identity;
    if (fitView) {
        graphFitPending = true;
        graphViewIdentity = identity;
        graphView = {};
        graphDragging = graphNode = wireNode = 0;
        graphPanning = false;
        graphDragBefore.reset();
    }
    sx::EffectGraph graph;
    if (!sx::EffectGraph::FromJson(effect.graph, graph)) {
        ImGui::End();
        return;
    }
    bool changed = false;
    auto before = s.project;
    ImGui::SameLine();
    if (ImGui::BeginCombo("##node", "+ Nœud")) {
        for (auto &op : sx::EffectGraphOps())
            if (sx::OpAllowed(op, sx::EGKind::Effect) && std::string(op.op) != "output" &&
                std::string(op.op) != "opacity") {
                if (ImGui::Selectable(op.label)) {
                    sx::EGNode node;
                    node.id = graph.nextId++;
                    node.op = op.op;
                    auto visible = ImGui::GetContentRegionAvail();
                    node.pos = graphView.world({visible.x * .5f, visible.y * .5f});
                    node.inputs.resize(op.inputs.size());
                    node.value = {1, 1, 1, 1};
                    node.name = "p" + std::to_string(node.id);
                    graph.nodes.push_back(node);
                    changed = true;
                }
            }
        ImGui::EndCombo();
    }
    if (auto *node = graph.Find(graphNode)) {
        ImGui::Text("Nœud %u · %s", node->id, node->op.c_str());
        if (ImGui::InputText("Nom paramètre", &node->name))
            changed = true;
        float v[4] = {node->value.x, node->value.y, node->value.z, node->value.w};
        if (ImGui::DragFloat4("Valeur", v, .01f)) {
            node->value = {v[0], v[1], v[2], v[3]};
            changed = true;
        }
        if (node->op == "custom" && ImGui::InputTextMultiline("Code", &node->text, {-1, 70}))
            changed = true;
        ImGui::SameLine();
        if (node->op != "output" && ImGui::SmallButton("Retirer nœud")) {
            Id id = node->id;
            std::erase_if(graph.nodes, [id](auto &n) { return n.id == id; });
            for (auto &n : graph.nodes)
                for (auto &in : n.inputs)
                    if (in.node == id)
                        in = {};
            graphNode = 0;
            changed = true;
        }
    }
    if (ImGui::Button("Tout voir"))
        graphFitPending = true;
    ImGui::SameLine();
    bool actualSize = ImGui::Button("1:1");
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f %% · molette : zoom · bouton central : déplacer", graphView.zoom * 100);
    ImGui::BeginChild("graphCanvas", {0, 0}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
    avail = {std::max(1.f, avail.x), std::max(1.f, avail.y)};
    ImGui::InvisibleButton("graphInput", avail,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool hovered = ImGui::IsItemHovered();
    if (graphFitPending && avail.x >= 100 && avail.y >= 80 && !graph.nodes.empty()) {
        sx::float2 low{FLT_MAX, FLT_MAX}, high{-FLT_MAX, -FLT_MAX};
        for (auto &n : graph.nodes) {
            auto def = sx::EffectiveOp(n);
            float height = std::max(68.f, 42.f + 18 * std::max(def.inputs.size(), def.outputs.size()));
            low.x = std::min(low.x, n.pos.x);
            low.y = std::min(low.y, n.pos.y);
            high.x = std::max(high.x, n.pos.x + 165);
            high.y = std::max(high.y, n.pos.y + height);
        }
        graphView.fit(low, high, {avail.x, avail.y});
        graphFitPending = false;
    }
    if (actualSize)
        graphView.zoomAt({avail.x * .5f, avail.y * .5f}, 1);
    if (hovered && ImGui::GetIO().MouseWheel != 0 && !graphDragging)
        graphView.zoomAt({mouse.x - origin.x, mouse.y - origin.y},
                         graphView.zoom * std::pow(1.15f, ImGui::GetIO().MouseWheel));
    if (hovered && ImGui::IsMouseClicked(2))
        graphPanning = true;
    if (!ImGui::IsMouseDown(2))
        graphPanning = false;
    if (graphPanning) {
        graphView.pan.x += ImGui::GetIO().MouseDelta.x;
        graphView.pan.y += ImGui::GetIO().MouseDelta.y;
    }
    if (!ImGui::IsMouseDown(0)) {
        graphDragging = 0;
        graphDragBefore.reset();
    }
    float zoom = graphView.zoom;
    auto screen = [&](sx::float2 pos) {
        auto p = graphView.screen(pos);
        return origin + ImVec2(p.x, p.y);
    };
    auto textAt = [&](ImVec2 point, ImU32 color, const char *text) {
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * zoom, point, color, text);
    };
    dl->PushClipRect(origin, origin + avail, true);
    dl->AddRectFilled(origin, origin + avail, IM_COL32(18, 20, 25, 255));
    float grid = 24 * zoom;
    for (float xx = origin.x + std::fmod(graphView.pan.x, grid); xx < origin.x + avail.x; xx += grid)
        for (float yy = origin.y + std::fmod(graphView.pan.y, grid); yy < origin.y + avail.y; yy += grid)
            dl->AddCircleFilled({xx, yy}, .8f, IM_COL32(61, 64, 75, 255));
    auto inputPos = [&](const sx::EGNode &n, int pin) { return screen({n.pos.x, n.pos.y + 38 + pin * 18}); };
    auto outputPos = [&](const sx::EGNode &n, int pin) {
        return screen({n.pos.x + 165, n.pos.y + 38 + pin * 18});
    };
    for (auto &n : graph.nodes)
        for (int pin = 0; pin < int(n.inputs.size()); ++pin) {
            auto &wire = n.inputs[pin];
            if (auto *src = graph.Find(wire.node)) {
                ImVec2 a = outputPos(*src, wire.output), b = inputPos(n, pin);
                dl->AddBezierCubic(a, a + ImVec2(60 * zoom, 0), b - ImVec2(60 * zoom, 0), b, Audio, 2);
            }
        }
    if (wireNode) {
        if (auto *n = graph.Find(wireNode)) {
            auto a = outputPos(*n, wireOutput);
            dl->AddBezierCubic(a, a + ImVec2(50, 0), mouse - ImVec2(50, 0), mouse, Orange, 2);
        }
    }
    for (auto &n : graph.nodes) {
        auto def = sx::EffectiveOp(n);
        float height = std::max(68.f, 42.f + 18 * std::max(def.inputs.size(), def.outputs.size()));
        ImVec2 a = screen(n.pos), b = a + ImVec2(165 * zoom, height * zoom);
        dl->AddRectFilled(a, b, IM_COL32(35, 38, 46, 255), 5);
        dl->AddRect(a, b, n.id == graphNode ? Orange : IM_COL32(75, 78, 90, 255), 5);
        textAt(a + ImVec2(9 * zoom, 8 * zoom), Orange, def.label);
        bool header = hovered && mouse.x > a.x && mouse.x < b.x && mouse.y > a.y && mouse.y < a.y + 27 * zoom;
        if (header && ImGui::IsMouseClicked(0)) {
            graphNode = n.id;
            graphDragging = n.id;
            graphDragBefore = before;
        }
        if (n.id == graphDragging && ImGui::IsMouseDragging(0) &&
            (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0)) {
            if (graphDragBefore) {
                s.history.checkpoint(*graphDragBefore);
                graphDragBefore.reset();
            }
            n.pos.x += ImGui::GetIO().MouseDelta.x / zoom;
            n.pos.y += ImGui::GetIO().MouseDelta.y / zoom;
            changed = true;
        }
        for (int p = 0; p < int(def.inputs.size()); ++p) {
            auto point = inputPos(n, p);
            dl->AddCircleFilled(point, std::max(3.f, 4 * zoom), Audio);
            textAt(point + ImVec2(8 * zoom, -7 * zoom), Muted, def.inputs[p].name);
            float radius = std::max(6.f, 8 * zoom);
            bool hit =
                hovered && std::abs(mouse.x - point.x) < radius && std::abs(mouse.y - point.y) < radius;
            if (hit && ImGui::IsMouseClicked(0) && wireNode && !graph.WouldCycle(wireNode, n.id)) {
                if (n.inputs.size() <= size_t(p))
                    n.inputs.resize(p + 1);
                n.inputs[p] = {wireNode, wireOutput};
                wireNode = 0;
                changed = true;
            }
            if (hit && ImGui::IsMouseClicked(1)) {
                if (n.inputs.size() > size_t(p))
                    n.inputs[p] = {};
                changed = true;
            }
        }
        for (int p = 0; p < int(def.outputs.size()); ++p) {
            auto point = outputPos(n, p);
            dl->AddCircleFilled(point, std::max(3.f, 4 * zoom), Orange);
            float radius = std::max(6.f, 8 * zoom);
            if (hovered && std::abs(mouse.x - point.x) < radius && std::abs(mouse.y - point.y) < radius &&
                ImGui::IsMouseClicked(0)) {
                wireNode = n.id;
                wireOutput = p;
            }
        }
    }
    dl->PopClipRect();
    ImGui::EndChild();
    if (changed) {
        effect.graph = graph.ToJson();
        if (layerShaderNode) {
            auto draft = s.project.layerGraph;
            auto nodes = Json::Array();
            for (auto node : draft["nodes"].Elements()) {
                if (node["id"].U() == layerShaderNode) {
                    node["effect"]["graph"] = effect.graph;
                    node["effect"]["code"] = effectCode(effect);
                }
                nodes.Push(node);
            }
            draft["nodes"] = nodes;
            Json args = Json::Object();
            args["graph"] = draft;
            action("set_layer_graph", args);
        }
        s.dirty = true;
        draftIdentity.clear();
        if (!ImGui::IsMouseDragging(0))
            s.history.checkpoint(before);
    }
    ImGui::End();
}
void Editor::mixDetails() {
    auto *track = mixScope == "track" ? s.project.findTrack(mixTarget) : nullptr;
    auto *bus = mixScope == "bus" ? s.project.findBus(mixTarget) : nullptr;
    auto &mix = track ? track->mix : bus ? bus->mix : s.project.masterMix;
    auto args = [&] {
        Json a = Json::Object();
        a["scope"] = mixScope;
        a["target"] = mixTarget;
        return a;
    };
    if (track || bus) {
        std::string name = track ? track->name : bus->name;
        if (ImGui::InputText("Nom du canal", &name, ImGuiInputTextFlags_EnterReturnsTrue)) {
            auto a = args();
            a["name"] = name;
            action("set_mix_channel", a);
            return;
        }
        if (bus && ImGui::SmallButton("Supprimer ce bus et reconnecter au master")) {
            Json a = Json::Object();
            a["bus"] = mixTarget;
            action("remove_bus", a);
            return;
        }
    }
    auto parameter = [&](const char *label, const std::string &key, float low, float high, float fallback,
                         int index = -1) {
        auto &params = index < 0 ? mix.params : mix.effects[index].params;
        auto it = params.find(key);
        float value = it == params.end() ? fallback : it->second.at(s.time);
        bool animated = it != params.end() && !it->second.keys.empty();
        ImGui::PushID(index);
        ImGui::PushID(key.c_str());
        ImGui::TextDisabled("%s%s", label, animated ? " / ANIMÉ" : "");
        auto before = s.project;
        ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x - 45));
        cydaw::v2::ui::KnobStyle knobStyle;
        knobStyle.diameter = 38;
        knobStyle.label = label;
        knobStyle.flags = low > 0 && high / low > 100 ? cydaw::v2::ui::knobLogarithmic
                          : low == -high              ? cydaw::v2::ui::knobBipolar
                                                      : 0;
        if (cydaw::v2::ui::knob("##value", &value, low, high, fallback, knobStyle)) {
            auto &p = params.try_emplace(key, Parameter{fallback}).first->second;
            if (animated)
                p.setKey({s.time, value, Curve::Smooth, 0});
            else
                p.value = value;
        }
        editCheckpoint(before);
        ImGui::SameLine();
        bool add = ImGui::Button("K", {30, 0});
        bool inspect = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        ImGui::PopID();
        ImGui::PopID();
        if (add || inspect) {
            mixAnimationKey = key;
            mixAnimationEffect = index;
        }
        if (add) {
            auto a = args();
            a["key"] = key;
            a["effect"] = index;
            a["time"] = s.time;
            a["value"] = value;
            a["curve"] = 1;
            action("set_mix_keyframe", a);
        }
        return add;
    };
    ImGui::TextWrapped(track ? "Pan clip + piste → DSP → envois pré-fader → gain → sortie + envois post-fader"
                       : bus ? "DSP → envois pré-fader → gain/pan → sortie + envois post-fader"
                             : "Somme → DSP → gain/pan → sortie stéréo");
    if (parameter("Gain dB", "gain", -80, 12, track ? track->gain : bus ? bus->gain : s.project.master))
        return;
    if (parameter("Panoramique", "pan", -1, 1, track ? track->pan : bus ? bus->pan : 0))
        return;
    if (track || bus) {
        auto before = s.project;
        auto &muted = track ? track->muted : bus->muted;
        ImGui::Checkbox("Mute", &muted);
        editCheckpoint(before);
        if (track) {
            ImGui::SameLine();
            before = s.project;
            ImGui::Checkbox("Solo", &track->solo);
            editCheckpoint(before);
        }
        auto destinationName = [&](Id id) {
            auto *b = s.project.findBus(id);
            return b ? b->name : "Master";
        };
        Id destination = mix.output;
        bool changed = false;
        if (ImGui::BeginCombo("Sortie", destinationName(destination).c_str())) {
            if (ImGui::Selectable("Master", !destination)) {
                destination = 0;
                changed = true;
            }
            for (auto &b : s.project.buses)
                if (b.id != mixTarget && ImGui::Selectable(b.name.c_str(), destination == b.id)) {
                    destination = b.id;
                    changed = true;
                }
            ImGui::EndCombo();
        }
        if (changed) {
            auto a = args();
            a["output"] = destination;
            action("set_mix_channel", a);
            return;
        }
        if (ImGui::CollapsingHeader("Envois", ImGuiTreeNodeFlags_DefaultOpen)) {
            Id add = 0;
            if (ImGui::BeginCombo("+ Envoi", "Choisir un bus…")) {
                for (auto &b : s.project.buses)
                    if (b.id != mixTarget && ImGui::Selectable(b.name.c_str()))
                        add = b.id;
                ImGui::EndCombo();
            }
            if (add) {
                auto a = args();
                a["bus"] = add;
                action("set_send", a);
                return;
            }
            for (auto &send : mix.sends) {
                ImGui::PushID(int(send.bus));
                ImGui::TextColored(ImVec4(.25f, .7f, .72f, 1), "%s", destinationName(send.bus).c_str());
                before = s.project;
                ImGui::Checkbox("Actif", &send.enabled);
                editCheckpoint(before);
                ImGui::SameLine();
                before = s.project;
                ImGui::Checkbox("Avant fader", &send.preFader);
                editCheckpoint(before);
                before = s.project;
                ImGui::SetNextItemWidth(220);
                ImGui::SliderFloat("Niveau dB", &send.gain, -80, 12, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                editCheckpoint(before);
                ImGui::SameLine();
                bool remove = ImGui::SmallButton("Retirer");
                if (parameter("Niveau d'envoi animé", "send." + std::to_string(send.bus), -80, 12,
                              send.gain)) {
                    ImGui::PopID();
                    return;
                }
                Id id = send.bus;
                ImGui::PopID();
                if (remove) {
                    auto a = args();
                    a["bus"] = id;
                    action("remove_send", a);
                    return;
                }
            }
        }
    }
    if (ImGui::CollapsingHeader("Pile DSP CyDAW 2", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (dspGraphEditor(args(), mix.effects))
            return;
        if (ImGui::Button("+ Plugin natif / VST3")) {
            auto path = dialog(false, L"Plugins audio\0*.dll;*.vst3\0Tous\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto a = args();
                a["path"] = path;
                a["kind"] = sx::Utf8Path(path).extension() == L".vst3" ? "vst3" : "native";
                action("add_external_audio", a);
            }
            return;
        }
        std::string id;
        if (ImGui::BeginCombo("+ DSP", "Ajouter…")) {
            for (auto &d : cydaw::engine::effectCatalogue)
                if (!d.convertsChannels() && ImGui::Selectable(std::string(d.displayName).c_str()))
                    id = "cydaw." + std::string(d.typeKey);
            ImGui::EndCombo();
        }
        if (!id.empty()) {
            auto a = args();
            a["id"] = id;
            action("add_mix_effect", a);
            return;
        }
        for (int i = 0; i < int(mix.effects.size()); ++i) {
            auto &effect = mix.effects[i];
            auto *d = audioEffect(effect.id);
            ImGui::PushID(i);
            auto before = s.project;
            ImGui::Checkbox("##active", &effect.enabled);
            editCheckpoint(before);
            ImGui::SameLine();
            bool open = ImGui::TreeNode(std::string(d->displayName).c_str());
            bool edited = false;
            if (open) {
                auto pluginTarget = args();
                pluginTarget["effect"] = i;
                if (pluginControls(pluginTarget, effect)) {
                    ImGui::TreePop();
                    ImGui::PopID();
                    return;
                }
                int to = i;
                ImGui::BeginDisabled(i == 0);
                if (ImGui::SmallButton("Monter"))
                    to = i - 1;
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(i + 1 == int(mix.effects.size()));
                if (ImGui::SmallButton("Descendre"))
                    to = i + 1;
                ImGui::EndDisabled();
                ImGui::SameLine();
                bool remove = ImGui::SmallButton("Retirer DSP");
                if (remove || to != i) {
                    auto a = args();
                    a["effect"] = i;
                    a["position"] = to;
                    ImGui::TreePop();
                    ImGui::PopID();
                    action(remove ? "remove_mix_effect" : "set_mix_effect", a);
                    mixAnimationEffect = -1;
                    mixAnimationKey = "gain";
                    return;
                }
                for (auto &p : d->parameters)
                    if (parameter(std::string(p.key).c_str(), std::string(p.key), p.minimum, p.maximum,
                                  p.defaultValue, i)) {
                        edited = true;
                        break;
                    }
                if (externalAudio(effect.id))
                    for (auto &[key, value] : effect.params)
                        if (parameter(
                                (effect.parameterNames.contains(key) ? effect.parameterNames.at(key) : key)
                                    .c_str(),
                                key, 0, 1, value.value, i)) {
                            edited = true;
                            break;
                        }
                ImGui::TreePop();
            }
            ImGui::PopID();
            if (edited)
                return;
        }
    }
    if (ImGui::CollapsingHeader("Automation de mixage", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (mixAnimationEffect >= int(mix.effects.size())) {
            mixAnimationEffect = -1;
            mixAnimationKey = "gain";
        }
        auto &params = mixAnimationEffect < 0 ? mix.params : mix.effects[mixAnimationEffect].params;
        ImGui::Text("%s / %s", mixAnimationEffect < 0 ? "Canal" : mix.effects[mixAnimationEffect].id.c_str(),
                    mixAnimationKey.c_str());
        ImGui::TextDisabled("K : ajouter au temps courant ; clic droit sur K : afficher une courbe");
        auto it = params.find(mixAnimationKey);
        if (it != params.end() && !it->second.keys.empty()) {
            float curve[128];
            for (int n = 0; n < 128; ++n)
                curve[n] = it->second.at(s.project.duration * n / 127.);
            ImGui::PlotLines("##curve", curve, 128, 0, nullptr, FLT_MAX, FLT_MAX, {-1, 65});
            for (size_t n = 0; n < it->second.keys.size(); ++n) {
                auto key = it->second.keys[n];
                double oldTime = key.time;
                ImGui::PushID(int(n));
                ImGui::SetNextItemWidth(120);
                bool changed = ImGui::InputDouble("Temps s", &key.time, 0, 0, "%.4f",
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SetNextItemWidth(120);
                changed |= ImGui::InputFloat("Valeur", &key.value, 0, 0, "%.4f",
                                             ImGuiInputTextFlags_EnterReturnsTrue);
                int interpolation = int(key.curve);
                ImGui::SetNextItemWidth(180);
                changed |= ImGui::Combo("Courbe", &interpolation, "Linéaire\0Douce\0Palier\0Courbe CyDAW\0");
                if (interpolation == int(Curve::Shaped)) {
                    ImGui::SetNextItemWidth(120);
                    changed |= ImGui::InputFloat("Forme (-1 à 1)", &key.shape, 0, 0, "%.3f",
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
                }
                bool remove = ImGui::SmallButton("Supprimer le point");
                ImGui::PopID();
                if (changed || remove) {
                    auto a = args();
                    a["effect"] = mixAnimationEffect;
                    a["key"] = mixAnimationKey;
                    a["time"] = remove ? oldTime : key.time;
                    a["previous_time"] = oldTime;
                    a["value"] = key.value;
                    a["curve"] = interpolation;
                    a["shape"] = key.shape;
                    action(remove ? "remove_mix_keyframe" : "set_mix_keyframe", a);
                    return;
                }
                ImGui::Separator();
            }
        } else
            ImGui::TextDisabled("Aucun point ; le réglage de base est utilisé.");
        if (ImGui::SmallButton("Rétablir le réglage de base")) {
            auto a = args();
            a["effect"] = mixAnimationEffect;
            a["key"] = mixAnimationKey;
            action("remove_mix_keyframe", a);
            return;
        }
    }
}
bool Editor::pluginControls(Json target, const Effect &effect) {
    if (effect.id != "vst3")
        return false;
    if (ImGui::Button("Ouvrir VST3")) {
        target["visible"] = true;
        action("show_plugin_editor", target);
        return true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fermer la fenêtre")) {
        action("capture_plugin_state", target);
        target["visible"] = false;
        action("show_plugin_editor", target);
        return true;
    }
    if (ImGui::Button("Capturer l'état")) {
        action("capture_plugin_state", target);
        return true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer preset")) {
        auto path = dialog(true, L"Preset VST3\0*.sfvstpreset\0\0", L"sfvstpreset");
        if (!path.empty()) {
            target["path"] = path;
            action("save_plugin_preset", target);
        }
        return true;
    }
    if (ImGui::Button("Charger preset")) {
        auto path = dialog(false, L"Preset VST3\0*.sfvstpreset\0\0", L"sfvstpreset");
        if (!path.empty()) {
            target["path"] = path;
            action("load_plugin_preset", target);
        }
        return true;
    }
    ImGui::TextDisabled("État complet embarqué ; les courbes d'automation restent prioritaires.");
    return false;
}
bool Editor::dspGraphEditor(Json target, const std::vector<Effect> &chain) {
    bool applied = false;
    auto &draft = dspModalDraft;
    auto &index = dspModalIndex;
    auto &choices = dspModalChoices;
    auto identity = target.Dump(0);
    if (!choices.contains(identity)) {
        int first = -1;
        for (int i = 0; i < int(chain.size()); ++i)
            if (chain[i].id == "cydaw.graph") {
                first = i;
                break;
            }
        choices[identity] = first;
    }
    int &choice = choices[identity];
    if (choice >= int(chain.size()) || (choice >= 0 && chain[choice].id != "cydaw.graph"))
        choice = -1;
    auto label = choice < 0 ? "Nouveau graphe"
                            : chain[choice].graph["name"].S("Graphe DSP") + " #" + std::to_string(choice + 1);
    if (ImGui::BeginCombo("Graphe à éditer", label.c_str())) {
        if (ImGui::Selectable("Nouveau graphe", choice < 0))
            choice = -1;
        for (int i = 0; i < int(chain.size()); ++i)
            if (chain[i].id == "cydaw.graph") {
                auto name = chain[i].graph["name"].S("Graphe DSP") + " #" + std::to_string(i + 1);
                if (ImGui::Selectable(name.c_str(), choice == i))
                    choice = i;
            }
        ImGui::EndCombo();
    }
    bool requested = openDspNext && target["scope"].S() == mixScope && target["target"].U() == mixTarget;
    if (ImGui::Button("Graphe DSP : éditer / importer…") || requested) {
        openDspNext = false;
        index = choice;
        auto graph = index >= 0 ? chain[index].graph : Json::Object();
        if (index < 0) {
            std::string failure;
            Json::Parse("{\"schema\":2,\"nodes\":[],\"connections\":[{\"source\":0,\"destination\":0}]}",
                        graph, &failure);
        }
        dspCanvas.open(graph);
        draft = graph.Dump(2);
        ImGui::OpenPopup("Éditeur DSP v2");
    }
    ImGui::SetNextWindowSize({1040, 650}, ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Éditeur DSP v2")) {
        ImGui::TextWrapped("0 = entrée/sortie ; nœuds à identité stable, processeurs cydaw.*, somme sf.sum. "
                           "Les connexions sont validées par CyDAW v2.");
        if (ImGui::BeginTabBar("dspTabs")) {
            if (ImGui::BeginTabItem("Graphe")) {
                if (dspCanvas.draw(s.time))
                    draft = dspCanvas.document().Dump(2);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("JSON")) {
                if (ImGui::InputTextMultiline("##dsp-json", &draft, {980, 420})) {
                    Json graph;
                    std::string failure;
                    if (Json::Parse(draft, graph, &failure)) {
                        try {
                            decodeDspGraph(graph);
                            dspCanvas.open(graph);
                        } catch (const std::exception &e) {
                            error = e.what();
                        }
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (ImGui::Button("Importer JSON")) {
            auto path = dialog(false, L"Graphe DSP\0*.json\0\0", L"json");
            if (!path.empty()) {
                std::string text;
                if (!sx::ReadTextFile(path, text))
                    error = "Graphe illisible";
                else {
                    Json graph;
                    std::string failure;
                    if (Json::Parse(text, graph, &failure)) {
                        try {
                            decodeDspGraph(graph);
                            dspCanvas.open(graph);
                            draft = text;
                        } catch (const std::exception &e) {
                            error = e.what();
                        }
                    } else
                        error = failure;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Exporter JSON")) {
            auto path = dialog(true, L"Graphe DSP\0*.json\0\0", L"json");
            if (!path.empty())
                try {
                    AtomicFile output(path);
                    std::ofstream file(output.path(), std::ios::binary);
                    file << draft;
                    file.close();
                    if (!file)
                        throw std::runtime_error("Écriture du graphe échouée");
                    output.commit();
                } catch (const std::exception &e) {
                    error = e.what();
                }
        }
        ImGui::SameLine();
        if (ImGui::Button("Compiler et appliquer")) {
            applied = true;
            Json graph;
            std::string failure;
            if (Json::Parse(draft, graph, &failure)) {
                target["graph"] = graph;
                if (index >= 0)
                    target["effect"] = index;
                action("set_dsp_graph", target);
                if (error.empty())
                    ImGui::CloseCurrentPopup();
            } else
                error = failure;
        }
        ImGui::EndPopup();
    }
    return applied;
}
void Editor::mixer() {
    if (!panels["Mixeur & graphe audio"] || !beginPanel("Mixeur & graphe audio"))
        return;
    if (ImGui::BeginTabBar("audioTabs")) {
        if (ImGui::BeginTabItem("Bandeaux CyDAW")) {
            ImGui::BeginChild("CyDAWMixerStrips", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
            bool first = true;
            auto strip = [&](const char *scope, Id id, const std::string &name, MixChannel &mix, float gain,
                             float pan, bool muted, bool solo, bool locked) {
                if (!first)
                    ImGui::SameLine();
                first = false;
                auto before = s.project;
                cydaw::v2::ui::MixerStrip data;
                data.name = name.c_str();
                auto *dest = s.project.findBus(mix.output);
                data.output = dest ? dest->name.c_str() : "MASTER";
                data.devices = int(mix.effects.size());
                data.selected = mixScope == scope && mixTarget == id;
                data.gainDb = mix.param("gain", s.time, gain);
                data.pan = mix.param("pan", s.time, pan);
                data.muted = muted;
                data.solo = solo;
                data.canMute = std::string(scope) != "master";
                data.canSolo = std::string(scope) == "track";
                auto peak = s.media.mixPeaks.find(id), rms = s.media.mixRms.find(id);
                if (peak != s.media.mixPeaks.end())
                    for (int k = 0; k < 2; ++k)
                        data.peak[k] = peak->second[k];
                if (rms != s.media.mixRms.end())
                    for (int k = 0; k < 2; ++k)
                        data.rms[k] = rms->second[k];
                bool picked = false;
                ImGui::BeginDisabled(locked);
                bool changed =
                    cydaw::v2::ui::mixerStrip((std::string(scope) + std::to_string(id)).c_str(), data, 96,
                                              std::max(160.f, ImGui::GetContentRegionAvail().y - 20), picked);
                ImGui::EndDisabled();
                if (picked) {
                    mixScope = scope;
                    mixTarget = id;
                }
                if (changed) {
                    auto parameter = [&](const char *key, float value, float fallback) {
                        auto &p = mix.params.try_emplace(key, Parameter{fallback}).first->second;
                        if (p.keys.empty())
                            p.value = value;
                        else
                            p.setKey({s.time, value, Curve::Smooth, 0});
                    };
                    parameter("gain", data.gainDb, gain);
                    parameter("pan", data.pan, pan);
                    if (std::string(scope) == "track") {
                        auto *t = s.project.findTrack(id);
                        t->muted = data.muted;
                        t->solo = data.solo;
                    } else if (std::string(scope) == "bus")
                        s.project.findBus(id)->muted = data.muted;
                    editCheckpoint(before);
                }
            };
            for (auto &t : s.project.tracks)
                if (t.kind == TrackKind::Audio)
                    strip("track", t.id, t.name, t.mix, t.gain, t.pan, t.muted, t.solo, t.locked);
            for (auto &b : s.project.buses)
                strip("bus", b.id, b.name, b.mix, b.gain, b.pan, b.muted, false, b.locked);
            strip("master", 0, "MASTER", s.project.masterMix, s.project.master, 0, false, false, false);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mixeur", nullptr,
                                openDspNext ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
            if ((mixScope == "track" && (!s.project.findTrack(mixTarget) ||
                                         s.project.findTrack(mixTarget)->kind != TrackKind::Audio)) ||
                (mixScope == "bus" && !s.project.findBus(mixTarget))) {
                mixScope = "master";
                mixTarget = 0;
            }
            bool addBus = ImGui::Button("+ Bus stéréo");
            ImGui::SameLine();
            ImGui::TextDisabled("Sélectionner un canal pour ses envois, DSP et animations");
            ImGui::BeginChild("channels", {220, 0}, true);
            auto row = [&](const char *scope, Id id, const std::string &name) {
                ImGui::PushID(scope);
                ImGui::PushID(int(id));
                if (ImGui::Selectable(name.c_str(), mixScope == scope && mixTarget == id)) {
                    mixScope = scope;
                    mixTarget = id;
                    mixAnimationEffect = -1;
                    mixAnimationKey = "gain";
                }
                auto it = s.media.mixPeaks.find(id);
                float peak =
                    s.playing && it != s.media.mixPeaks.end() ? std::max(it->second[0], it->second[1]) : 0;
                char text[32];
                snprintf(text, sizeof(text), "%.1f dB%s", 20 * std::log10(std::max(peak, 1e-4f)),
                         peak > 1 ? " / CLIP" : "");
                ImGui::ProgressBar(std::min(peak, 1.f), {-1, 20}, text);
                // CyDAW v2 Widgets::meter: hold a rising peak for 1.5 seconds.
                auto *storage = ImGui::GetStateStorage();
                auto heldId = ImGui::GetID("peakHold"), timeId = ImGui::GetID("peakTime");
                float held = storage->GetFloat(heldId, 0);
                double stamp = storage->GetFloat(timeId, 0);
                if (peak >= held || ImGui::GetTime() - stamp > 1.5) {
                    held = peak;
                    storage->SetFloat(heldId, held);
                    storage->SetFloat(timeId, float(ImGui::GetTime()));
                }
                auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
                float marker = low.x + (high.x - low.x) * std::clamp(held, 0.f, 1.f);
                ImGui::GetWindowDrawList()->AddLine({marker, low.y}, {marker, high.y},
                                                    IM_COL32(255, 189, 110, 255), 2);
                auto rms = s.media.mixRms.find(id);
                if (rms != s.media.mixRms.end())
                    ImGui::TextDisabled(
                        "RMS %.1f dB",
                        20 * std::log10(std::max(1e-6f, std::max(rms->second[0], rms->second[1]))));
                ImGui::PopID();
                ImGui::PopID();
            };
            ImGui::SeparatorText("PISTES");
            for (auto &track : s.project.tracks)
                if (track.kind == TrackKind::Audio)
                    row("track", track.id, track.name);
            ImGui::SeparatorText("BUS");
            for (auto &bus : s.project.buses)
                row("bus", bus.id, bus.name);
            ImGui::SeparatorText("SORTIE");
            row("master", 0, "MASTER");
            ImGui::TextDisabled("M %.1f LUFS", s.media.loudness.momentary());
            ImGui::TextDisabled("S %.1f LUFS · TP %.1f dBTP", s.media.loudness.shortTerm(),
                                s.media.loudness.truePeak());
            ImGui::TextDisabled("I %.1f LUFS", s.media.loudness.integrated());
            ImGui::TextDisabled("Latence compensée : %u samples", s.media.latencySamples);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("channelDetails", {0, 0}, true);
            auto *track = mixScope == "track" ? s.project.findTrack(mixTarget) : nullptr;
            auto *bus = mixScope == "bus" ? s.project.findBus(mixTarget) : nullptr;
            bool locked = track ? track->locked : bus ? bus->locked : false;
            std::string label = track ? track->name : bus ? bus->name : "MASTER / 48 kHz stéréo";
            ImGui::SeparatorText(label.c_str());
            bool toggleLock = (track || bus) && ImGui::Checkbox("Verrouillé", &locked);
            if (toggleLock) {
                Json a = Json::Object();
                a["scope"] = mixScope;
                a["target"] = mixTarget;
                a["locked"] = locked;
                action("set_mix_channel", a);
            } else {
                ImGui::BeginDisabled(locked);
                mixDetails();
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
            if (addBus) {
                action("add_bus");
                if (error.empty()) {
                    mixScope = "bus";
                    mixTarget = s.project.buses.back().id;
                }
            }
        }
        if (ImGui::BeginTabItem("Signal")) {
            if (s.playing) {
                ImGui::PlotLines("Forme d'onde", audio.analysis.waveform.data(), audio.analysis.waveformCount,
                                 0, nullptr, -1, 1, {-1, 80});
                float bins[128]{};
                cydaw::engine::computeLogSpectrum(audio.analysis, 20, 20000, bins, 128);
                ImGui::PlotHistogram("Spectre logarithmique", bins, 128, 0, nullptr, 0, 1, {-1, 95});
            } else
                ImGui::TextWrapped("Lancez la lecture pour analyser le mix avec CyDAW 2.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Graphe")) {
            try {
                auto graph = audioGraph(s.project);
                ImGui::TextDisabled("CYDAW 2 / %zu nœuds · %zu câbles stéréo validés", graph.nodes.size(),
                                    graph.connections.size());
                ImGui::BeginChild("routingGraph", {0, 0}, true, ImGuiWindowFlags_HorizontalScrollbar);
                std::map<uint64_t, int> depth;
                std::map<uint64_t, int> pending;
                std::map<uint64_t, std::vector<uint64_t>> downstream;
                for (auto &wire : graph.connections)
                    if (wire.source.nodeId && wire.destination.nodeId) {
                        ++pending[wire.destination.nodeId];
                        downstream[wire.source.nodeId].push_back(wire.destination.nodeId);
                    }
                std::vector<uint64_t> ready;
                for (auto &node : graph.nodes)
                    if (!pending[node.id])
                        ready.push_back(node.id);
                for (size_t index = 0; index < ready.size(); ++index)
                    for (auto destination : downstream[ready[index]]) {
                        depth[destination] = std::max(depth[destination], depth[ready[index]] + 1);
                        if (!--pending[destination])
                            ready.push_back(destination);
                    }
                std::map<int, int> rows;
                std::map<uint64_t, ImVec2> points;
                auto base = ImGui::GetCursorScreenPos();
                auto *dl = ImGui::GetWindowDrawList();
                float width = 0, height = 0;
                for (auto &node : graph.nodes) {
                    int column = depth[node.id], row = rows[column]++;
                    points[node.id] = base + ImVec2(column * 205.f, row * 70.f);
                    width = std::max(width, column * 205.f + 190);
                    height = std::max(height, row * 70.f + 55);
                }
                for (auto &wire : graph.connections)
                    if (points.contains(wire.source.nodeId) && points.contains(wire.destination.nodeId)) {
                        auto a = points[wire.source.nodeId] + ImVec2(190, 25),
                             b = points[wire.destination.nodeId] + ImVec2(0, 25);
                        dl->AddBezierCubic(a, a + ImVec2(50, 0), b - ImVec2(50, 0), b, Audio, 2);
                    }
                for (auto &node : graph.nodes) {
                    auto a = points[node.id];
                    dl->AddRectFilled(a, a + ImVec2(190, 50), IM_COL32(35, 42, 48, 255), 4);
                    dl->PushClipRect(a + ImVec2(4, 4), a + ImVec2(186, 46), true);
                    dl->AddText(a + ImVec2(8, 6), Orange, node.displayName.c_str());
                    dl->AddText(a + ImVec2(8, 27), Muted, node.processorKey.c_str());
                    dl->PopClipRect();
                }
                ImGui::Dummy({width, height});
                ImGui::EndChild();
            } catch (const std::exception &e) {
                ImGui::TextWrapped("%s", e.what());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
void Editor::integrations() {
    if (!panels["Plugins & MCP"] || !beginPanel("Plugins & MCP"))
        return;
    ImGui::SeparatorText("Plugins HLSL · API 1 / 2");
    ImGui::TextWrapped("Les paquets dans plugins/ déclarent leurs effets via plugin.json. Le code est "
                       "embarqué dans le projet pour stabiliser les rendus.");
    if (ImGui::Button("Recharger les plugins"))
        s.renderer.library.scan(root);
    for (auto &d : s.renderer.library.definitions)
        if (d.origin.starts_with("Plugin"))
            ImGui::BulletText("%s", d.name.c_str());
    for (auto &e : s.renderer.library.diagnostics)
        ImGui::TextWrapped("%s", e.c_str());
    ImGui::SeparatorText("MCP natif · stdio");
    ImGui::TextWrapped(
        "Démarrage : ShaderixFrame.exe --mcp\nLe client MCP et la fenêtre de ce processus partagent la "
        "séquence. Montage, animations, graphes, effets et export sont accessibles aux outils.");
    ImGui::TextDisabled("Dernière commande : %s", s.lastCommand.empty() ? "—" : s.lastCommand.c_str());
    ImGui::SeparatorText("Séquence");
    auto before = s.project;
    ImGui::DragScalar("Durée (s)", ImGuiDataType_Double, &s.project.duration, .1f);
    s.project.duration = std::clamp(s.project.duration, 0.1, 86400.);
    editCheckpoint(before);
    before = s.project;
    ImGui::SliderInt("Images/s", &s.project.fps, 1, 120);
    editCheckpoint(before);
    ImGui::TextDisabled("%d × %d pixels", s.project.width, s.project.height);
    ImGui::TextWrapped("Raccourcis : Espace lecture, S découpe, Suppr retire, Ctrl+Z/Y annuler/rétablir.");
    ImGui::End();
}
void Editor::recoveryPanel() {
    if (!recoveryScanned) {
        recoveryScanned = true;
        try {
            recoveries = s.recovery.list();
            recoveryVisible = !recoveries.empty();
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
    if (s.recovery.enabled() && s.dirty && ImGui::GetTime() >= nextRecovery) {
        nextRecovery = ImGui::GetTime() + 5;
        try {
            s.syncPluginEditors(true);
            s.recovery.capture(s.project, s.projectPath, s.time);
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
    if (!recoveryVisible)
        return;
    ImGui::SetNextWindowSize({600, 300}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Récupération des sessions", &recoveryVisible)) {
        ImGui::TextWrapped("Les modifications non enregistrées sont conservées toutes les 5 secondes et "
                           "à la fermeture. Choisissez une session à reprendre.");
        ImGui::Separator();
        if (recoveries.empty())
            ImGui::TextDisabled("Aucune session interrompue à récupérer.");
        for (auto &entry : recoveries) {
            ImGui::PushID(sx::Narrow(entry.file.wstring()).c_str());
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TextDisabled("%s", entry.originalPath.empty() ? "Projet non enregistré"
                                                                 : entry.originalPath.c_str());
            bool restore = ImGui::Button("Reprendre"), discard = false;
            ImGui::SameLine();
            discard = ImGui::Button("Supprimer cette récupération");
            ImGui::PopID();
            if (restore || discard) {
                try {
                    if (restore) {
                        auto recovered = s.recovery.restore(entry);
                        s.syncPluginEditors(true);
                        s.pluginEditors.clear();
                        if (s.dirty)
                            s.recovery.preserve(s.project, s.projectPath, s.time);
                        s.history.clear();
                        s.project = std::move(recovered);
                        s.projectPath = entry.originalPath;
                        s.time = std::clamp(entry.time, 0., s.project.duration);
                        s.playing = false;
                        s.selectClip(0);
                        s.dirty = true;
                        s.recovery.capture(s.project, s.projectPath, s.time);
                        draftIdentity.clear();
                    }
                    s.recovery.discard(entry);
                    recoveries = s.recovery.list();
                    recoveryVisible = !recoveries.empty();
                } catch (const std::exception &e) {
                    error = e.what();
                }
                break;
            }
            ImGui::Separator();
        }
    }
    ImGui::End();
}
void Editor::exportPanel() {
    if (auto *timeline = ImGui::FindWindowByName(panelTitle("Timeline").c_str());
        timeline && timeline->DockId)
        ImGui::SetNextWindowDockID(timeline->DockId, ImGuiCond_FirstUseEver);
    if (exportsFocus)
        panels["Exports"] = true;
    if (!panels["Exports"])
        return;
    bool visible = ImGui::Begin(panelTitle("Exports").c_str(), &panels["Exports"]);
    if (exportsFocus) {
        ImGui::SetWindowFocus(panelTitle("Exports").c_str());
        exportsFocus = false;
    }
    if (visible) {
        ImGui::TextWrapped("Le rendu utilise la séquence et les effets au moment du lancement. "
                           "Vous pouvez continuer le montage ou ouvrir un autre projet.");
        auto &preset = exportPreset;
        auto &format = exportFormat;
        if (ImGui::BeginCombo("Preset", exportPresets()[preset].name.c_str())) {
            for (int i = 0; i < int(exportPresets().size()); ++i)
                if (ImGui::Selectable(exportPresets()[i].name.c_str(), preset == i))
                    preset = i;
            ImGui::EndCombo();
        }
        ImGui::Combo("Format", &format,
                     "MP4 H264 / AAC\0WAV stéréo 48 kHz\0Séquence PNG\0Rapport de sonorité JSON\0AVI RGB / "
                     "PCM sans compression\0");
        if (format == 2) {
            int bits = s.project.imageFormat == "png8" ? 0 : s.project.imageFormat == "png16" ? 1 : 2;
            const char *formats[] = {"png8", "png16", "exr16"};
            if (ImGui::Combo("Précision des images", &bits,
                             "PNG 8 bits\0PNG 16 bits\0EXR half 16 bits HDR\0")) {
                Json a = Json::Object();
                a["imageFormat"] = formats[bits];
                action("set_color_management", a);
            }
        }
        if (ImGui::Button("Ajouter à la file")) {
            auto path = dialog(true,
                               format == 0   ? L"Vidéo MP4\0*.mp4\0\0"
                               : format == 1 ? L"Audio WAV\0*.wav\0\0"
                               : format == 3 ? L"Rapport JSON\0*.json\0\0"
                               : format == 4 ? L"Vidéo AVI\0*.avi\0\0"
                                             : L"Nouveau dossier de séquence\0*.*\0\0",
                               format == 0   ? L"mp4"
                               : format == 1 ? L"wav"
                               : format == 3 ? L"json"
                               : format == 4 ? L"avi"
                                             : nullptr);
            if (!path.empty()) {
                Json args = Json::Object();
                args["path"] = path;
                args["kind"] = format == 0 || format == 4 ? "video"
                               : format == 1              ? "audio"
                               : format == 2              ? "images"
                                                          : "analysis";
                args["preset"] = exportPresets()[preset].id;
                action("start_export", args);
            }
        }
        auto jobs = s.exports.list();
        if (jobs.empty())
            ImGui::TextDisabled("Aucun export. Choisir Fichier > Exporter MP4 ou Exporter le mix WAV.");
        for (auto &job : jobs) {
            ImGui::PushID(int(job.id));
            ImGui::Separator();
            const char *state = job.state == ExportState::Queued       ? "En attente"
                                : job.state == ExportState::Running    ? "Rendu en cours"
                                : job.state == ExportState::Cancelling ? "Annulation en cours"
                                : job.state == ExportState::Finalizing ? "Finalisation du fichier"
                                : job.state == ExportState::Completed  ? "Terminé"
                                : job.state == ExportState::Cancelled  ? "Annulé"
                                                                       : "Échec";
            ImGui::Text("#%u · %s · %s", job.id,
                        job.kind == ExportKind::Video
                            ? (job.json()["format"].S() == "avi" ? "AVI / RGB · PCM" : "MP4 / H264 · AAC")
                        : job.kind == ExportKind::Audio  ? "WAV / stéréo 48 kHz"
                        : job.kind == ExportKind::Images ? "Séquence PNG"
                                                         : "Rapport LUFS",
                        state);
            ImGui::TextUnformatted(job.path.c_str());
            ImGui::TextDisabled("%s · %.2f s · %d × %d / %d fps · temps écoulé %.1f s", job.project.c_str(),
                                job.duration, job.width, job.height, job.fps, job.elapsed);
            ImGui::ProgressBar(job.progress, {-1, 18});
            if (job.cancellable() && ImGui::Button("Annuler cet export")) {
                Json args = Json::Object();
                args["job"] = job.id;
                action("cancel_export", args);
            }
            if (!job.error.empty())
                ImGui::TextColored(ImVec4(1, .65f, .3f, 1), "%s", job.error.c_str());
            ImGui::PopID();
        }
    }
    ImGui::End();
}
Editor *Editor::keyboardOwner() {
    auto *nav = GImGui->NavWindow;
    Editor *owner = this;
    if (!nav)
        return owner;
    std::string name = nav->Name;
    while (owner->clipWorkspace && name.find(owner->clipWorkspace->panelSuffix) != std::string::npos)
        owner = owner->clipWorkspace.get();
    return owner;
}
void Editor::handleShortcuts() {
    auto &io = ImGui::GetIO();
    auto *nav = GImGui->NavWindow;
    bool graph = nav && (std::string(nav->Name).find("graphCanvas") != std::string::npos ||
                         std::string(nav->Name).find("ShaderixelComposition") != std::string::npos ||
                         std::string(nav->Name).find("dspCanvas") != std::string::npos);
    bool localDsp = nav && std::string(nav->Name).find("dspCanvas") != std::string::npos;
    if (!io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Space))
            s.playing = !s.playing;
        if (ImGui::IsKeyPressed(ImGuiKey_S) && !io.KeyCtrl && !graph && s.selected) {
            auto a = selectionArgs();
            a["time"] = s.time;
            action("split_clips", a);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !graph && s.selected) {
            auto a = selectionArgs();
            a["ripple"] = io.KeyShift;
            action("delete_clips", a);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && !graph && s.selected)
            action("duplicate_clips", selectionArgs());
        if (io.KeyCtrl && !localDsp && ImGui::IsKeyPressed(ImGuiKey_Z) &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            action("undo");
        if (io.KeyCtrl && !localDsp && ImGui::IsKeyPressed(ImGuiKey_Y) &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            action("redo");
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))
            action("new_project");
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            auto path = s.projectPath.empty() ? dialog(true, L"ShaderixFrame\0*.sframe\0\0", L"sframe")
                                              : s.projectPath;
            if (!path.empty()) {
                Json a = Json::Object();
                a["path"] = path;
                action("save_project", a);
            }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
            auto path = dialog(false, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
            if (!path.empty()) {
                Json a = Json::Object();
                a["path"] = path;
                action("open_project", a);
            }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I)) {
            auto path = dialog(false, L"Médias\0*.*\0\0", nullptr);
            if (!path.empty())
                drop(path);
        }
    }
}
void Editor::draw() {
    panelSettings();
    try {
        s.syncPluginEditors();
    } catch (const std::exception &ex) {
        error = ex.what();
    }
    s.pollJobs();
    s.media.poll();
    for (auto it = pendingImports.begin(); it != pendingImports.end();) {
        try {
            if (s.media.get(it->path).loading) {
                ++it;
                continue;
            }
            Json args = Json::Object();
            args["path"] = it->path;
            args["start"] = it->time;
            action("import_media", args);
        } catch (const std::exception &failure) {
            error = failure.what();
        }
        it = pendingImports.erase(it);
    }
    s.tidySelection();
    keyboardOwner()->handleShortcuts();
    bool sought = std::abs(s.time - lastTransport) > 1e-7;
    if (s.playing) {
        if (!wasPlaying || sought) {
            s.media.resetAudio();
            audio.start(s.time);
            wallTime = ImGui::GetTime() - s.time;
        }
        try {
            audio.pump(s.project, s.media);
            s.time = audio.playing() ? audio.time() : ImGui::GetTime() - wallTime;
        } catch (const std::exception &e) {
            error = e.what();
            s.playing = false;
        }
        if (s.time >= s.project.duration) {
            s.time = loop ? 0 : s.project.duration;
            s.playing = loop;
            audio.stop();
            if (loop) {
                s.media.resetAudio();
                audio.start(0);
                wallTime = ImGui::GetTime();
            }
            wasPlaying = false;
        }
    } else
        audio.stop();
    wasPlaying = s.playing;
    lastTransport = s.time;
    try {
        float scale = std::min(1.f, std::min(1280.f / s.project.width, 720.f / s.project.height));
        if (gradeComparison == 3) {
            if (!gradeBefore) {
                gradeBefore = std::make_unique<Renderer>();
                gradeBefore->initialize(s.renderer.gpu(), s.renderer.gpuContext());
                gradeBefore->library = s.renderer.library;
            }
            gradeBefore->render(s.project, s.media, s.time, unsigned(s.project.width * scale),
                                unsigned(s.project.height * scale), true);
        }
        s.renderer.captureGradeClip = gradePick ? s.selected : 0;
        s.renderer.captureGradeEffect = selectedEffect;
        s.renderer.render(s.project, s.media, s.time, unsigned(s.project.width * scale),
                          unsigned(s.project.height * scale), gradeComparison == 1,
                          gradeMatte ? s.selected : 0, selectedEffect);
        if (gradeScopesVisible && ImGui::GetTime() >= gradeScopesNext) {
            gradeScopes = videoScopes(s.renderer.readbackFloat(), s.project.toneMap, s.project.exposure);
            gradeScopesNext = ImGui::GetTime() + .25;
        }
    } catch (const std::exception &e) {
        error = e.what();
    }
    menu();
    auto *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize({vp->WorkSize.x, vp->WorkSize.y - 28});
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("Workspace", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);
    ImGuiID dock = ImGui::GetID("FrameDock");
    ImGui::DockSpace(dock);
    ImGui::PopStyleVar();
    if (!layoutReady) {
        layoutReady = true;
        if (!ImGui::DockBuilderGetNode(dock) || !ImGui::DockBuilderGetNode(dock)->IsSplitNode()) {
            ImGui::DockBuilderRemoveNode(dock);
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
            ImGuiID center = dock, left, right, bottom, upperRight;
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .18f, &left, &center);
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .27f, &right, &center);
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .44f, &bottom, &center);
            ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .30f, &upperRight, &right);
            ImGui::DockBuilderDockWindow("Médias & effets", left);
            ImGui::DockBuilderDockWindow("Composition", center);
            ImGui::DockBuilderDockWindow("Calques", upperRight);
            ImGui::DockBuilderDockWindow("Propriétés", right);
            ImGui::DockBuilderDockWindow("Scopes vidéo", right);
            ImGui::DockBuilderDockWindow("Timeline", bottom);
            ImGui::DockBuilderDockWindow("Éditeur de clip", center);
            ImGui::DockBuilderDockWindow("Graphe DSP", bottom);
            ImGui::DockBuilderDockWindow("Motion design", bottom);
            ImGui::DockBuilderDockWindow("Bibliothèque Shaderixel", left);
            ImGui::DockBuilderDockWindow("Animations", bottom);
            ImGui::DockBuilderDockWindow("Étalonnage", bottom);
            ImGui::DockBuilderDockWindow("Graphe VFX", bottom);
            ImGui::DockBuilderDockWindow("Graphe des calques", bottom);
            ImGui::DockBuilderDockWindow("HLSL", bottom);
            ImGui::DockBuilderDockWindow("Mixeur & graphe audio", bottom);
            ImGui::DockBuilderDockWindow("Exports", bottom);
            ImGui::DockBuilderDockWindow("Plugins & MCP", left);
            ImGui::DockBuilderFinish(dock);
        }
    }
    ImGui::End();
    clipEditor();
    motionPanel();
    shaderixelLibrary();
    dspPanel();
    library();
    preview();
    layers();
    inspector();
    timeline();
    animations();
    shaderEditor();
    graphEditor();
    layerGraphEditor();
    gradingPanel();
    scopesPanel();
    mixer();
    integrations();
    recoveryPanel();
    exportPanel();
    if (!pendingPanelFocus.empty()) {
        ImGui::SetWindowFocus(pendingPanelFocus.c_str());
        pendingPanelFocus.clear();
    }
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - 28});
    ImGui::SetNextWindowSize({vp->WorkSize.x, 28});
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::Begin("status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    auto jobs = s.exports.list();
    if (s.media.pending()) {
        ImGui::TextDisabled("Préparation des médias…");
        ImGui::SameLine();
    }
    auto active = std::find_if(jobs.begin(), jobs.end(), [](const auto &job) { return job.active(); });
    if (active != jobs.end()) {
        auto &job = *active;
        ImGui::ProgressBar(job.progress, {150, 16});
        ImGui::SameLine();
        ImGui::BeginDisabled(!job.cancellable());
        if (ImGui::SmallButton("Annuler export")) {
            Json args = Json::Object();
            args["job"] = job.id;
            action("cancel_export", args);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (!error.empty()) {
        ImGui::TextColored(ImVec4(1, .65f, .3f, 1), "%s", error.c_str());
    } else {
        ImGui::TextDisabled("%s   /   %zu pistes   /   HLSL GPU   /   espace : lecture",
                            s.project.name.c_str(), s.project.tracks.size());
    }
    ImGui::End();
}
} // namespace sf

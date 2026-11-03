// Effects Browser: Shaderixel's effects, blends, generators and transitions as cards with a live
// preview each (rendered in the background on a reference picture or the selected clip), and
// CyDAW's audio inserts. Double-click applies to the selection; cards drag onto the timeline.
#include "adapters/CyDawAudio.h"
#include "ui/Editor.h"
#include "ui/FrameStyle.h"
#include <cydaw/engine/EffectCatalogue.h>
#include <imgui_stdlib.h>
#include <set>

namespace sf {
using namespace style;
namespace {
enum Kind { Effects, Blends, Generators, Transitions, Audio };
const char *kindNames[] = {"Effects", "Blends", "Generators", "Transitions", "Audio"};
bool isBlend(const EffectDefinition &d) { return d.kind == "effect" && d.code.find("//@blend") != std::string::npos; }
bool ofKind(const EffectDefinition &d, int kind) {
    switch (kind) {
    case Effects:
        return d.kind == "effect" && !d.generator && !isBlend(d);
    case Blends:
        return isBlend(d);
    case Generators:
        return d.generator;
    case Transitions:
        return d.kind == "transition";
    }
    return false;
}
std::string lowered(std::string text) {
    for (auto &c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
} // namespace

std::shared_ptr<const Project> Editor::previewDocument(const EffectDefinition &d, int kind,
                                                       const Clip *reference) {
    auto q = std::make_shared<Project>();
    q->width = 320;
    q->height = 180;
    q->duration = 2;
    q->fps = 30;
    // The picture under test: the selected clip's frame, or Shaderixel's reference generator.
    auto base = [&] {
        Clip c;
        c.id = q->allocate();
        c.duration = 2;
        if (reference) {
            c.source = reference->source;
            c.in = reference->sourceTime(std::clamp(s.time, reference->start, reference->start + reference->duration));
            c.sourceColor = reference->sourceColor;
            c.sourcePremultiplied = reference->sourcePremultiplied;
            c.hold = true;
        } else
            c.generator = true;
        return c;
    };
    auto instance = [&] {
        auto e = s.renderer.library.instantiate(d.id);
        if (e.id != "grading")
            e.code = d.code;
        return e;
    };
    if (kind == Generators) {
        Clip c;
        c.id = q->allocate();
        c.duration = 2;
        c.generator = true;
        c.effects = {instance()};
        q->addTrack(TrackKind::Video).clips = {c};
    } else if (kind == Blends || kind == Transitions) {
        // Two pictures: the reference below, a bold title above, blended or crossed by the card.
        auto below = base();
        Clip above;
        above.id = q->allocate();
        above.title.enabled = true;
        above.title.text = "Aa";
        above.duration = 2;
        q->addTrack(TrackKind::Video).clips = {below};
        if (kind == Blends) {
            auto &top = q->addTrack(TrackKind::Video);
            top.clips = {above};
            top.blendEffect = instance();
            std::rotate(q->tracks.begin(), q->tracks.end() - 1, q->tracks.end());
        } else {
            above.start = .5;
            above.duration = 1.5;
            above.transition.enabled = true;
            above.transition.id = d.id;
            above.transition.duration = 1;
            q->tracks[0].clips.push_back(above);
        }
    } else {
        auto c = base();
        c.effects = {instance()};
        q->addTrack(TrackKind::Video).clips = {c};
    }
    return q;
}

void Editor::effectsBrowser() {
    if (!panels["Effects Browser"] || !beginPanel("Effects Browser"))
        return;
    effectPreviews.beginFrame();
    auto &p = s.project;
    // Kind tabs, search, family and preview options.
    for (int k = 0; k < 5; ++k) {
        if (k)
            ImGui::SameLine(0, 2);
        bool on = browserKind == k;
        if (on)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(accentDim));
        if (ImGui::Button(kindNames[k])) {
            browserKind = k;
            browserCategory.clear();
        }
        if (on)
            ImGui::PopStyleColor();
    }
    ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x * .45f));
    ImGui::InputTextWithHint("##browserSearch", "Search", &browserSearch);
    ImGui::SameLine();
    std::set<std::string> families;
    if (browserKind == Audio) {
        for (auto &d : cydaw::engine::effectCatalogue)
            if (audioInsert(d))
                families.insert(std::string(d.categoryKey));
    } else
        for (auto &d : s.renderer.library.definitions)
            if (ofKind(d, browserKind))
                families.insert(d.category);
    ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x - 4));
    if (ImGui::BeginCombo("##browserFamily", browserCategory.empty() ? "All families" : browserCategory.c_str())) {
        if (ImGui::Selectable(cyui::trLabel("All families").c_str(), browserCategory.empty()))
            browserCategory.clear();
        for (auto &family : families)
            if (ImGui::Selectable(family.c_str(), family == browserCategory))
                browserCategory = family;
        ImGui::EndCombo();
    }
    auto *selectedClip = p.findClip(s.selected);
    const Clip *reference = selectedClip && !selectedClip->source.empty() && p.owner(selectedClip->id)->kind == TrackKind::Video
                                ? selectedClip
                                : nullptr;
    if (browserKind != Audio) {
        ImGui::Checkbox(cyui::trLabel("Preview on selected clip").c_str(), &browserOnClip);
        tooltip("Render the cards on the selected clip's picture instead of the reference image");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        ImGui::SliderFloat("##browserCard", &browserCard, 80, 220, "Size %.0f");
    }
    if (browserKind == Effects) {
        // A LUT file goes through the native grading stage (1D/3D .cube, image LUTs), not a shader input.
        bool video = selectedClip && p.owner(selectedClip->id)->kind == TrackKind::Video;
        ImGui::BeginDisabled(!video);
        if (ImGui::Button(cyui::trLabel("Apply LUT File...").c_str())) {
            auto path = dialog(false, L"1D / 2D / 3D LUT\0*.cube;*.png;*.tif;*.exr\0All\0*.*\0\0", nullptr);
            if (!path.empty()) {
                Json grade = Json::Object();
                grade["clip"] = s.selected;
                grade["name"] = sx::Narrow(sx::Utf8Path(path).stem().wstring());
                if (action("add_grade", grade)) {
                    Json lut = Json::Object();
                    lut["clip"] = s.selected;
                    lut["effect"] = actionResult["effect"];
                    lut["path"] = path;
                    if (action("import_grade_lut", lut))
                        say("LUT applied to the selected clip as a color correction");
                }
            }
        }
        ImGui::EndDisabled();
        tooltip("Load a .cube (1D/3D) or image LUT onto the selected video clip");
    }
    if (!browserOnClip)
        reference = nullptr;
    std::string referenceKey =
        reference ? reference->source + "@" + std::to_string(int(reference->sourceTime(s.time) * 10)) : "reference";
    if (referenceKey != browserReferenceKey) {
        browserReferenceKey = referenceKey;
        browserDocuments.clear();
    }
    auto filter = lowered(browserSearch);
    ImGui::BeginChild("##browserCards", {0, 0}, ImGuiChildFlags_None);
    float width = ImGui::GetContentRegionAvail().x;
    float card = browserKind == Audio ? 150 : browserCard;
    int columns = std::max(1, int((width + 8) / (card + 8)));
    int column = 0;
    std::string apply, applyKind;
    auto place = [&] {
        if (column++ % columns)
            ImGui::SameLine(0, 8);
    };
    auto *videoTrack = selectedClip && p.owner(selectedClip->id)->kind == TrackKind::Video ? p.owner(selectedClip->id) : nullptr;
    if (browserKind == Audio) {
        for (auto &d : cydaw::engine::effectCatalogue) {
            std::string name(d.displayName), family(d.categoryKey);
            if (!audioInsert(d) || (!browserCategory.empty() && family != browserCategory) ||
                (!filter.empty() && lowered(name).find(filter) == std::string::npos))
                continue;
            place();
            std::string id = "cydaw." + std::string(d.typeKey);
            ImGui::PushID(id.c_str());
            ImU32 colour = d.colour ? fromRgb(d.colour & 0xffffff) : style::audio;
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(alpha(colour, .35f)));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(alpha(colour, .6f)));
            ImGui::Button(name.c_str(), {card, 44});
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                apply = id;
                applyKind = "audio";
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("EFFECT", id.c_str(), id.size() + 1);
                ImGui::Text(cyui::tr("%s  (drop on an audio clip or the Track Effects panel)").c_str(), name.c_str());
                ImGui::EndDragDropSource();
            }
            tooltip((family + "\nDouble-click: add to the channel shown in Track Effects").c_str());
            ImGui::PopID();
        }
    } else {
        for (auto &d : s.renderer.library.definitions) {
            if (!ofKind(d, browserKind) || (!browserCategory.empty() && d.category != browserCategory) ||
                (!filter.empty() && lowered(d.name + " " + d.category).find(filter) == std::string::npos))
                continue;
            place();
            ImGui::PushID(d.id.c_str());
            ImGui::BeginGroup();
            float thumbHeight = card * 9 / 16;
            auto at = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##card", {card, thumbHeight + ImGui::GetTextLineHeight() + 4});
            bool hovered = ImGui::IsItemHovered();
            auto *dl = ImGui::GetWindowDrawList();
            ID3D11ShaderResourceView *thumb = nullptr;
            if (ImGui::IsItemVisible()) {
                auto &document = browserDocuments[d.id];
                if (!document)
                    document = previewDocument(d, browserKind, reference);
                double time = browserKind == Transitions ? 1 : .5;
                thumb = effectPreviews.getDocument("browser:" + d.id + ":" + browserReferenceKey, document, time,
                                                   s.renderer);
            }
            if (thumb)
                dl->AddImageRounded((ImTextureID)thumb, at, at + ImVec2(card, thumbHeight), {0, 0}, {1, 1},
                                    IM_COL32_WHITE, 4);
            else {
                dl->AddRectFilled(at, at + ImVec2(card, thumbHeight), bg3, 4);
                dl->AddText(at + ImVec2(8, thumbHeight * .5f - 7), textFaint, "...");
            }
            if (hovered)
                dl->AddRect(at, at + ImVec2(card, thumbHeight), accent, 4.f, ImDrawFlags_None, 2.f);
            clippedText(dl, at + ImVec2(2, thumbHeight + 2), card - 4, hovered ? text : textDim, d.name.c_str());
            ImGui::EndGroup();
            if (hovered && ImGui::IsMouseDoubleClicked(0)) {
                apply = d.id;
                applyKind = kindNames[browserKind];
            }
            if (ImGui::BeginDragDropSource()) {
                const char *payload = browserKind == Generators   ? "SHADERIXEL_LAYER"
                                      : browserKind == Transitions ? "TRANSITION"
                                      : browserKind == Blends      ? "BLEND"
                                                                   : "EFFECT";
                ImGui::SetDragDropPayload(payload, d.id.c_str(), d.id.size() + 1);
                if (thumb)
                    ImGui::Image((ImTextureID)thumb, {120, 68});
                ImGui::TextUnformatted(d.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem("##cardMenu")) {
                ImGui::TextDisabled(cyui::tr("%s").c_str(), d.name.c_str());
                ImGui::Separator();
                if (browserKind == Effects) {
                    if (ImGui::MenuItem(cyui::trLabel("Apply to Selected Clip").c_str(), nullptr, false, videoTrack != nullptr)) {
                        apply = d.id;
                        applyKind = "Effects";
                    }
                    if (ImGui::MenuItem(cyui::trLabel("Add to the Track's Effects").c_str(), nullptr, false, videoTrack != nullptr)) {
                        apply = d.id;
                        applyKind = "track";
                    }
                } else if (browserKind == Blends) {
                    if (ImGui::MenuItem(cyui::trLabel("Blend the Selected Clip's Track").c_str(), nullptr, false, videoTrack != nullptr)) {
                        apply = d.id;
                        applyKind = "Blends";
                    }
                } else if (ImGui::MenuItem(browserKind == Generators ? "Create Layer at Playhead"
                                                                     : "Apply to Selected Clip")) {
                    apply = d.id;
                    applyKind = kindNames[browserKind];
                }
                ImGui::EndPopup();
            }
            if (hovered)
                ImGui::SetTooltip(cyui::tr("%s\n%s\nDouble-click: %s").c_str(), d.name.c_str(), d.category.c_str(),
                                  browserKind == Effects       ? "apply to the selected clip"
                                  : browserKind == Blends      ? "blend the selected clip's track over the tracks below"
                                  : browserKind == Generators  ? "create a layer at the playhead"
                                                               : "transition into the selected clip");
            ImGui::PopID();
        }
    }
    if (column == 0)
        ImGui::TextDisabled(cyui::tr("Nothing matches.").c_str());
    ImGui::EndChild();
    ImGui::End();
    if (apply.empty())
        return;
    // The action runs after the panel so a changed project never invalidates what was drawn.
    if (applyKind == "audio") {
        Json a = Json::Object();
        a["scope"] = mixScope;
        a["target"] = mixTarget;
        if (auto *c = p.findClip(s.selected); c && p.owner(c->id)->kind == TrackKind::Audio) {
            a["scope"] = "track";
            a["target"] = p.owner(c->id)->id;
        }
        a["id"] = apply;
        if (action("add_mix_effect", a))
            say("Added to " + std::string(a["scope"].S() == "master" ? "the master" : "the channel") +
                ": open Track Effects to set it");
    } else if (applyKind == "Generators")
        addLibraryLayer(apply);
    else if (!s.selected)
        say("Select a clip first");
    else if (applyKind == "Effects" || applyKind == "Transitions") {
        Json a = Json::Object();
        a["clip"] = s.selected;
        a["id"] = apply;
        action(applyKind == "Transitions" ? "set_transition" : "add_effect", a);
    } else if (auto *track = p.owner(s.selected); track && track->kind == TrackKind::Video) {
        Json a = Json::Object();
        a["track"] = track->id;
        if (applyKind == "track") {
            a["id"] = apply;
            if (action("add_track_effect", a))
                openTrackGraph(track->id);
        } else {
            a["blend_effect"] = apply;
            action("set_track", a);
        }
    }
}
} // namespace sf

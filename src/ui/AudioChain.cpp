// Track Effects: the insert chain of an audio track, bus or the master as a row of CyDAW-style
// devices (bypass, remove, drag to reorder, knobs), with a "+" catalogue at its end.
#include "adapters/CyDawAudio.h"
#include "adapters/CyDawParameters.h"
#include <cyui/modules/Audio.h>
#include "ui/MixerStrip.h"
#include "plugins/IsolatedAudio.h"
#include "ui/Editor.h"
#include "ui/FrameStyle.h"
#include <algorithm>
#include <cydaw/engine/EffectCatalogue.h>
#include <imgui_stdlib.h>

namespace sf {
using namespace style;
namespace {
const char *groupName(cydaw::engine::EffectGroup group) {
    using cydaw::engine::EffectGroup;
    switch (group) {
    case EffectGroup::modulation:
        return "Modulation";
    case EffectGroup::delay:
        return "Delay & Space";
    case EffectGroup::filter:
        return "Filter & EQ";
    case EffectGroup::dynamics:
        return "Dynamics";
    case EffectGroup::channels:
        return "Channels";
    case EffectGroup::analysis:
        return "Analysis";
    }
    return "Other";
}
std::string lowered(std::string text) {
    for (auto &c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
} // namespace

void Editor::trackEffectsPanel() {
    if (!panels["Track Effects"] || !beginPanel("Track Effects"))
        return;
    auto &p = s.project;
    // The chain follows the selected clip's sound (its own audio track or that of its linked sound).
    if (s.selected != chainFollowed) {
        chainFollowed = s.selected;
        if (p.findClip(s.selected))
            for (auto member : p.linked(s.selected, false))
                if (auto *owner = p.owner(member); owner && owner->kind == TrackKind::Audio) {
                    mixScope = "track";
                    mixTarget = owner->id;
                    break;
                }
    }
    auto *track = mixScope == "track" ? p.findTrack(mixTarget) : nullptr;
    auto *bus = mixScope == "bus" ? p.findBus(mixTarget) : nullptr;
    if ((mixScope == "track" && (!track || track->kind != TrackKind::Audio)) || (mixScope == "bus" && !bus)) {
        mixScope = "master";
        mixTarget = 0;
        track = nullptr;
        bus = nullptr;
    }
    auto &chain = track ? track->mix.effects : bus ? bus->mix.effects : p.masterMix.effects;
    auto args = [&] {
        Json a = Json::Object();
        a["scope"] = mixScope;
        a["target"] = mixTarget;
        return a;
    };
    std::string current = track ? track->name : bus ? bus->name : "Master";
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("##chainChannel", current.c_str())) {
        if (ImGui::Selectable(cyui::trLabel("Master").c_str(), mixScope == "master")) {
            mixScope = "master";
            mixTarget = 0;
        }
        for (auto &t : p.tracks)
            if (t.kind == TrackKind::Audio && ImGui::Selectable((t.name + "##track" + std::to_string(t.id)).c_str(),
                                                                 mixScope == "track" && mixTarget == t.id)) {
                mixScope = "track";
                mixTarget = t.id;
            }
        for (auto &b : p.buses)
            if (ImGui::Selectable((b.name + "  (bus)##bus" + std::to_string(b.id)).c_str(),
                                  mixScope == "bus" && mixTarget == b.id)) {
                mixScope = "bus";
                mixTarget = b.id;
            }
        ImGui::EndCombo();
    }
    tooltip("Channel whose insert chain is shown; selecting an audio clip shows its track");
    ImGui::SameLine();
    ImGui::TextDisabled(cyui::tr("%zu device%s  ·  signal flows left to right").c_str(), chain.size(), chain.size() == 1 ? "" : "s");
    std::string pending;     // action decided while drawing, run after the row
    Json pendingArgs = args();
    float height = std::max(150.f, ImGui::GetContentRegionAvail().y - 4);
    ImGui::BeginChild("##chainRow", {0, height}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    const float cardWidth = 196;
    for (int i = 0; i < int(chain.size()); ++i) {
        auto &effect = chain[i];
        auto *d = audioEffect(effect.id);
        if (!d)
            continue;
        ImGui::PushID(i);
        if (i)
            ImGui::SameLine(0, 6);
        ImU32 colour = d->colour ? fromRgb(d->colour & 0xffffff) : accent;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(effect.enabled ? bg2 : bg1));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5);
        ImGui::BeginChild("##device", {cardWidth, height - 18}, ImGuiChildFlags_Borders);
        auto *dl = ImGui::GetWindowDrawList();
        auto top = ImGui::GetWindowPos();
        dl->AddRectFilled(top, top + ImVec2(cardWidth, 26), alpha(colour, effect.enabled ? .85f : .3f), 5,
                          ImDrawFlags_RoundCornersTop);
        // Header: power (bypass), name (drag handle), remove.
        bool on = effect.enabled;
        if (cyui::toggleButton(on ? "on" : "off", &on, colour, {30, 18},
                                  on ? "Bypass this device" : "Turn this device on")) {
            pending = "set_mix_effect";
            pendingArgs["effect"] = i;
            pendingArgs["enabled"] = on;
        }
        ImGui::SameLine(0, 6);
        std::string title(d->displayName);
        ImGui::Selectable(title.c_str(), false, 0, {cardWidth - 74, 18});
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("AUDIO_DEVICE", &i, sizeof(i));
            ImGui::Text(cyui::tr("Move %s").c_str(), title.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (auto *payload = ImGui::AcceptDragDropPayload("AUDIO_DEVICE")) {
                int from = *static_cast<const int *>(payload->Data);
                if (from != i) {
                    pending = "set_mix_effect";
                    pendingArgs["effect"] = from;
                    pendingArgs["position"] = i;
                }
            }
            ImGui::EndDragDropTarget();
        }
        tooltip("Drag onto another device to move it in the chain");
        ImGui::SameLine(cardWidth - 26);
        if (ImGui::SmallButton(cyui::trLabel("x").c_str())) {
            pending = "remove_mix_effect";
            pendingArgs["effect"] = i;
        }
        tooltip("Remove this device (the signal passes straight through)");
        ImGui::Dummy({0, 4});
        // Body: one knob per parameter, three per row; plugins get their editor button.
        if (externalAudio(effect.id)) {
            auto target = args();
            target["effect"] = i;
            if (pluginControls(target, effect)) {
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::PopID();
                ImGui::EndChild();
                ImGui::End();
                return;
            }
        }
        ImGui::BeginChild("##knobs", {0, 0}, ImGuiChildFlags_None);
        int column = 0;
        auto knobFor = [&](const std::string &key, const char *label, float lo, float hi, float fallback,
                           const char *unit) {
            auto &param = effect.params.try_emplace(key, Parameter{fallback}).first->second;
            bool animated = !param.keys.empty();
            float value = animated ? param.at(s.time) : param.value;
            if (column++ % 3)
                ImGui::SameLine(0, 4);
            cyui::KnobStyle look;
            look.diameter = 34;
            look.colour = colour;
            look.label = label;
            look.unit = unit && *unit ? unit : nullptr;
            look.format = hi - lo > 50 ? "%.0f" : hi - lo > 5 ? "%.1f" : "%.2f";
            if (cyui::knob(key.c_str(), &value, lo, hi, fallback, look)) {
                // Animated values take a key at the playhead; others change directly (live while dragging).
                if (animated)
                    param.setKey({s.time, value, Curve::Smooth, 0});
                else
                    param.value = value;
                s.dirty = true;
            }
            if (ImGui::IsItemActivated())
                chainEditStart = param;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                // One undo step per gesture: the project as it was, with the parameter as it was.
                auto before = p;
                auto &channel = mixScope == "track" ? before.findTrack(mixTarget)->mix
                                : mixScope == "bus"  ? before.findBus(mixTarget)->mix
                                                     : before.masterMix;
                channel.effects[i].params[key] = chainEditStart;
                s.history.checkpoint(before, "Track effect");
            }
        };
        for (auto &parameter : d->parameters) {
            std::string key(parameter.key);
            // CyDAW's label, else the key spelled out ("lowFrequency" -> "Low Frequency").
            std::string label = sf::audio_ui::parameterLabel(parameter.labelKey);
            if (label == "Parameter") {
                label.clear();
                for (char c : key) {
                    if (std::isupper(static_cast<unsigned char>(c)) && !label.empty())
                        label += ' ';
                    label += label.empty() ? char(std::toupper(static_cast<unsigned char>(c))) : c;
                }
                if (parameter.band >= 0)
                    label += " " + std::to_string(parameter.band + 1);
            }
            knobFor(key, label.c_str(), parameter.minimum, parameter.maximum, parameter.defaultValue,
                    std::string(parameter.unit).c_str());
        }
        if (externalAudio(effect.id))
            for (auto &[key, value] : effect.params) {
                auto name = effect.parameterNames.contains(key) ? effect.parameterNames.at(key) : key;
                knobFor(key, name.c_str(), 0, 1, value.value, "");
            }
        if (d->parameters.empty() && !externalAudio(effect.id))
            ImGui::TextDisabled(cyui::tr("No settings").c_str());
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    // The "+" device: the CyDAW catalogue by family, searchable, and external plugins.
    if (!chain.empty())
        ImGui::SameLine(0, 6);
    if (ImGui::Button(cyui::trLabel("+\nAdd\ndevice").c_str(), {chain.empty() ? 140.f : 70.f, height - 18})) {
        chainSearch.clear();
        ImGui::OpenPopup("chainCatalogue");
    }
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("AUDIO_DEVICE")) {
            pending = "set_mix_effect";
            pendingArgs["effect"] = *static_cast<const int *>(payload->Data);
            pendingArgs["position"] = int(chain.size()) - 1;
        }
        ImGui::EndDragDropTarget();
    }
    if (chain.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled(cyui::tr("No insert effect on this channel yet.\nAdd a CyDAW device or a VST3 plugin.").c_str());
    }
    if (ImGui::BeginPopup("chainCatalogue")) {
        ImGui::SetNextItemWidth(240);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##chainSearch", "Search a device", &chainSearch);
        auto filter = lowered(chainSearch);
        using cydaw::engine::EffectGroup;
        for (auto group : {EffectGroup::dynamics, EffectGroup::filter, EffectGroup::delay, EffectGroup::modulation,
                           EffectGroup::channels, EffectGroup::analysis}) {
            bool header = false;
            for (auto &d : cydaw::engine::effectCatalogue) {
                if (!audioInsert(d) || d.group != group)
                    continue;
                std::string name(d.displayName);
                if (!filter.empty() && lowered(name).find(filter) == std::string::npos)
                    continue;
                if (!header) {
                    ImGui::SeparatorText(groupName(group));
                    header = true;
                }
                if (ImGui::Selectable(name.c_str())) {
                    pending = "add_mix_effect";
                    pendingArgs["id"] = "cydaw." + std::string(d.typeKey);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::Selectable(cyui::trLabel("VST3 or native plugin...").c_str())) {
            auto path = dialog(false, L"Audio plugins\0*.dll;*.vst3\0All\0*.*\0\0", nullptr);
            if (!path.empty()) {
                pending = "add_external_audio";
                pendingArgs["path"] = path;
                pendingArgs["kind"] = sx::Utf8Path(path).extension() == L".vst3" ? "vst3" : "native";
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::End();
    if (!pending.empty())
        action(pending, pendingArgs);
}
} // namespace sf

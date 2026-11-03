#pragma once
#include <cyui/modules/Audio.h>
namespace sf {
struct MixerStrip {
    const char* name=""; const char* output="MASTER"; int devices=0;
    float gainDb=0,pan=0; bool muted=false,solo=false,selected=false,canMute=true,canSolo=true;
    float peak[2]{},rms[2]{};
};
// Product composition of common controls; no copied widget renderer.
inline bool mixerStrip(const char* id,MixerStrip& s,float width,float height,bool& picked) {
    ImGui::PushID(id); ImGui::BeginGroup(); bool changed=false;
    const auto origin=ImGui::GetCursorScreenPos(); width=cyui::dp(width);
    ImGui::BeginChild("strip",{width,height},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetNextItemWidth(width); if(ImGui::Selectable(s.name,s.selected,0,{width,0})) picked=true;
    cyui::textFaint("%d effects",s.devices);
    changed|=cyui::knob("pan",&s.pan,-1,1,0,{22,0,"Pan","%.2f",nullptr,cyui::knobBipolar});
    const float h=std::max(cyui::dp(40),height-cyui::dp(165));
    changed|=cyui::fader("gain",&s.gainDb,-80,12,{cyui::dp(44),h});
    ImGui::SameLine(); cyui::meter("meter",{cyui::dp(18),h},s.peak[0],s.peak[1],s.rms[0],s.rms[1]);
    cyui::textDim("%s",s.output);
    { cyui::ScopedDisabled disabled(!s.canMute); changed|=cyui::toggleButton("M",&s.muted,cyui::palette().info,{cyui::dp(28),0},"Mute"); }
    ImGui::SameLine(); { cyui::ScopedDisabled disabled(!s.canSolo); changed|=cyui::toggleButton("S",&s.solo,cyui::palette().warning,{cyui::dp(28),0},"Solo"); }
    ImGui::EndChild(); ImGui::EndGroup(); ImGui::PopID(); return changed;
}
}

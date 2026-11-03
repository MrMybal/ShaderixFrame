#pragma once
#pragma push_macro("pascal")
#undef pascal // winnt.h legacy calling convention collides with CodeLanguage::pascal.
#include <cyui/modules/CodeEditor.h>
#pragma pop_macro("pascal")
#include <map>
#include <imgui_internal.h>
namespace sf {
// One editor per stable host item; buffers and compile/apply remain application-owned.
class CodeFields {
    struct Field { cyui::CodeEditor view; std::string synced; };
    std::map<ImGuiID,Field> fields;
    ImGuiWindow* keyboardWindow=nullptr;
    int keyboardFrame=-10;
public:
    bool ownsKeyboard() const {return keyboardFrame>=ImGui::GetFrameCount()-1&&GImGui->NavWindow==keyboardWindow;}
    bool draw(const char* id,std::string& text,ImVec2 size,cyui::CodeLanguage language=cyui::CodeLanguage::hlsl,bool readOnly=false) {
        auto& f=fields[ImGui::GetID(id)];
        if(f.synced!=text) { f.view.setText(text); f.synced=text; }
        if(f.view.language()!=language)f.view.setLanguage(language); f.view.setReadOnly(readOnly);
        const bool changed=f.view.draw(id,size);
        auto* parent=ImGui::GetCurrentWindow();
        for(auto* child=GImGui->NavWindow;child;child=child->ParentWindow)
            if(child->ParentWindow==parent&&child->ChildId==ImGui::GetID(id)){keyboardWindow=GImGui->NavWindow;keyboardFrame=ImGui::GetFrameCount();break;}
        if(changed) {text=f.view.text();f.synced=text;}
        return changed;
    }
};
}

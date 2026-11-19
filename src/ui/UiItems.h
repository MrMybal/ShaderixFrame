#pragma once
#include <imgui_internal.h>
#include <map>
#include <optional>
#include <string>
namespace sf {
void automatedDialogs(bool active, const std::string &path = {});
std::optional<std::string> takeAutomatedDialogPath();
const std::string &automatedDialogError();
const std::string &automatedUiErrorCode();
bool automatedUiInput();
void rejectAutomatedUiAction(const std::string &code, const std::string &message);
// Host implementation of ImGui's MIT-licensed item hooks. No test-engine runtime is bundled.
struct UiItem {
    ImGuiID id = 0, viewport = 0;
    ImRect rect;
    std::string window, label;
    bool visible = false, disabled = false, inputable = false, checked = false, checkable = false;
};
class UiItems {
    ImGuiContext *context;
    ImGuiID hook = 0;

  public:
    std::map<ImGuiID, UiItem> items;
    explicit UiItems(ImGuiContext *);
    ~UiItems();
    UiItems(const UiItems &) = delete;
    UiItems &operator=(const UiItems &) = delete;
};
} // namespace sf

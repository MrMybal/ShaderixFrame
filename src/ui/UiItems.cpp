#include "ui/UiItems.h"
#include <utility>
namespace sf {
namespace {
thread_local std::optional<std::string> dialogPath;
thread_local std::string dialogError;
thread_local std::string uiErrorCode;
} // namespace
void automatedDialogs(bool active, const std::string &path) {
    if (active) {
        dialogPath = path;
        dialogError.clear();
        uiErrorCode.clear();
    } else
        dialogPath.reset();
}
std::optional<std::string> takeAutomatedDialogPath() {
    if (!dialogPath)
        return {};
    auto value = std::exchange(*dialogPath, {});
    if (value.empty())
        rejectAutomatedUiAction("PathRequired",
                                "PathRequired: supply file_path with the automated command or input");
    return value;
}
const std::string &automatedDialogError() {
    return dialogError;
}
const std::string &automatedUiErrorCode() {
    return uiErrorCode;
}
bool automatedUiInput() {
    return dialogPath.has_value();
}
void rejectAutomatedUiAction(const std::string &code, const std::string &message) {
    uiErrorCode = code;
    dialogError = message;
}
UiItems::UiItems(ImGuiContext *ctx) : context(ctx) {
    IM_ASSERT(ctx && !ctx->TestEngine);
    ctx->TestEngine = this;
    ctx->TestEngineHookItems = true;
    ImGuiContextHook h;
    h.Type = ImGuiContextHookType_NewFramePre;
    h.UserData = this;
    h.Callback = [](ImGuiContext *, ImGuiContextHook *h) {
        static_cast<UiItems *>(h->UserData)->items.clear();
    };
    hook = ImGui::AddContextHook(ctx, &h);
}
UiItems::~UiItems() {
    ImGui::RemoveContextHook(context, hook);
    context->TestEngineHookItems = false;
    context->TestEngine = nullptr;
}
} // namespace sf
void ImGuiTestEngineHook_ItemAdd(ImGuiContext *ctx, ImGuiID id, const ImRect &box,
                                 const ImGuiLastItemData *data) {
    if (!ctx->TestEngine || !id || !ctx->CurrentWindow)
        return;
    auto *window = ctx->CurrentWindow;
    auto &row = static_cast<sf::UiItems *>(ctx->TestEngine)->items[id];
    row.id = id;
    row.rect = box;
    row.window = window->Name;
    row.viewport = window->Viewport ? window->Viewport->ID : 0;
    row.visible = box.Overlaps(window->ClipRect) && !window->SkipItems;
    row.disabled = ((data ? data->ItemFlags : ctx->CurrentItemFlags) & ImGuiItemFlags_Disabled) != 0;
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext *ctx, ImGuiID id, const char *label,
                                  ImGuiItemStatusFlags flags) {
    if (!ctx->TestEngine || !id)
        return;
    auto &items = static_cast<sf::UiItems *>(ctx->TestEngine)->items;
    auto found = items.find(id);
    if (found == items.end())
        return;
    auto &row = found->second;
    row.label = label ? label : "";
    row.inputable = (flags & ImGuiItemStatusFlags_Inputable) != 0;
    row.checkable = (flags & ImGuiItemStatusFlags_Checkable) != 0;
    row.checked = (flags & ImGuiItemStatusFlags_Checked) != 0;
}
void ImGuiTestEngineHook_Log(ImGuiContext *, const char *, ...) {}
const char *ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *ctx, ImGuiID id) {
    if (!ctx->TestEngine)
        return nullptr;
    auto &items = static_cast<sf::UiItems *>(ctx->TestEngine)->items;
    auto found = items.find(id);
    return found == items.end() ? nullptr : found->second.label.c_str();
}

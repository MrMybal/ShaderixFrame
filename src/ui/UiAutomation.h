#pragma once
#include "mcp/Automation.h"
#include "ui/Editor.h"
#include "ui/UiItems.h"
#include <deque>
namespace sf {
class UiAutomation {
    using J = cymcp::Json;
    Automation &automation;
    Editor &editor;
    UiItems recorder;
    std::vector<std::string> operations;
    struct Event {
        std::function<void(ImGuiIO &)> emit;
    };
    struct Input {
        uint64_t id = 0;
        std::string state = "idle", error, errorCode, principal, file;
        std::deque<Event> events;
        cymcp::Cancellation cancellation;
        std::set<ImGuiKey> keys;
        std::set<int> buttons;
        ImGuiID item = 0;
        std::string window, label;
        ImVec2 point{};
    } input;
    uint64_t sequence = 0;
    bool inputFrame = false, finishFrame = false;
    Editor &target(const J &) const;
    void context(Editor &, bool, const std::string & = {});
    void release();
    J queueInput(const J &, const cymcp::CallContext &);
    J inputState(const cymcp::CallContext &) const;
    J state(Editor &) const;
    void add(std::string name, std::string description, J properties, J required, bool readOnly,
             std::function<J(const J &, const cymcp::CallContext &)>);

  public:
    UiAutomation(Automation &, Editor &);
    ~UiAutomation();
    // Called after native backend updates and before ImGui::NewFrame; input never leaves this context.
    void beforeFrame();
    void afterFrame();
};
} // namespace sf

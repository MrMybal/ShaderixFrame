#include "ui/UiAutomation.h"
#include "ui/NativeControls.h"
#include "ui/UiPreferences.h"
#include <algorithm>
#include <charconv>
#include <cyui/Core.h>
#include <iomanip>
#include <sstream>

namespace sf {
namespace {
using J = cymcp::Json;
J type(const char *name) {
    return {{"type", name}};
}
J text() {
    return type("string");
}
J number() {
    return type("number");
}
J flag() {
    return type("boolean");
}
J integer() {
    return type("integer");
}
std::string itemId(ImGuiID id) {
    std::ostringstream out;
    out << std::hex << std::setw(8) << std::setfill('0') << id;
    return out.str();
}
ImGuiID parseId(const std::string &value) {
    ImGuiID id = 0;
    auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), id, 16);
    if (error != std::errc{} || end != value.data() + value.size() || !id)
        throw cymcp::Error("InvalidArguments", "Use the exact hexadecimal item ID returned by get_ui_items");
    return id;
}
ImGuiKey namedKey(const std::string &name) {
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
        if (name == ImGui::GetKeyName(ImGuiKey(key)))
            return ImGuiKey(key);
    throw cymcp::Error("InvalidArguments", "Unknown ImGui key name");
}
bool contains(const std::string &value, const std::string &query) {
    auto lower = [](std::string s) {
        for (auto &c : s)
            if (c >= 'A' && c <= 'Z')
                c += 32;
        return s;
    };
    return lower(value).find(lower(query)) != std::string::npos;
}
const char *modeNames[] = {"edit", "effects", "motion", "color", "audio", "deliver"};
} // namespace

void Editor::applyPanelPlacement(const char *name) {
    if (detachedPanels.contains(name)) {
        ImGuiWindowClass cls;
        cls.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&cls);
    }
    auto found = panelPlacements.find(name);
    if (found == panelPlacements.end())
        return;
    auto &p = found->second;
    if (p.detach) {
        ImGuiWindowClass cls;
        cls.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&cls);
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
    } else if (p.dock)
        ImGui::SetNextWindowDockID(*p.dock, ImGuiCond_Always);
    if (p.position)
        ImGui::SetNextWindowPos(*p.position, ImGuiCond_Always);
    if (p.size)
        ImGui::SetNextWindowSize(*p.size, ImGuiCond_Always);
    panelPlacements.erase(found);
}

Editor &UiAutomation::target(const J &args) const {
    Editor *result = &editor;
    for (auto id : args.value("workspace", std::vector<Id>{})) {
        if (!result->clipWorkspace || result->editedContainer != id)
            throw cymcp::Error("UnknownWorkspace",
                               "Open each clip editor before addressing its workspace path");
        result = result->clipWorkspace.get();
    }
    return *result;
}
void UiAutomation::context(Editor &e, bool active, const std::string &file) {
    if (&e == &editor)
        automatedDialogs(active, file);
    e.automationActive = active;
    e.automationPath = active ? file : "";
    if (active) {
        e.automationDispatch = [this, &e](const std::string &name, const Json &args) {
            J result;
            auto input = cymcp::parse_json(args.Dump(0), cymcp::provider_message_limit);
            try {
                if (&e == &editor)
                    result = automation.invoke(name, input);
                else {
                    // Nested drafts use the same business registry and validation, on this UI thread.
                    AutomationOptions nestedOptions;
                    nestedOptions.accessProfile = automation.accessProfile();
                    Automation nested(e.s, nestedOptions);
                    nested.allowChanges(automation.changesAllowed());
                    result = nested.invoke(name, input);
                }
            } catch (const cymcp::Error &error) {
                e.automationErrorCode = error.code;
                throw;
            }
            Json converted;
            if (!Json::Parse(result.dump(), converted))
                throw cymcp::Error("InvalidJson", "Invalid engine result");
            return converted;
        };
    } else
        e.automationDispatch = {};
    if (e.clipWorkspace)
        context(*e.clipWorkspace, active, file);
}
J UiAutomation::state(Editor &e) const {
    auto navigation = [](float zoom, sx::float2 pan) {
        return J{{"zoom", zoom}, {"pan_x", pan.x}, {"pan_y", pan.y}};
    };
    J windows = J::array(), monitors = J::array();
    for (auto &[name, open] : e.panels) {
        auto title = e.panelTitle(name.c_str());
        J row = {{"panel", name}, {"visible", open}, {"window", title}};
        if (auto *w = ImGui::FindWindowByName(title.c_str())) {
            row["active"] = w->WasActive;
            row["dock"] = itemId(w->DockId);
            row["viewport"] = itemId(w->ViewportId);
            row["position"] = {w->Pos.x, w->Pos.y};
            row["size"] = {w->Size.x, w->Size.y};
            row["focused"] = GImGui->NavWindow && GImGui->NavWindow->RootWindow == w->RootWindow;
        }
        windows.push_back(std::move(row));
    }
    int index = 0;
    for (const auto &m : ImGui::GetPlatformIO().Monitors)
        monitors.push_back({{"index", index++},
                            {"position", {m.WorkPos.x, m.WorkPos.y}},
                            {"size", {m.WorkSize.x, m.WorkSize.y}},
                            {"dpi", m.DpiScale}});
    auto themes = J::array(), locales = J::array();
    for (auto &t : cyui::builtInThemes())
        themes.push_back(t.key);
    for (auto &l : cyui::availableLocales())
        locales.push_back(l.code);
    return {{"frame", ImGui::GetFrameCount()},
            {"views",
             {{"composition", navigation(e.compositionView.zoom, e.compositionView.pan)},
              {"shader", navigation(e.graphView.zoom, e.graphView.pan)},
              {"layer", navigation(e.layerCanvas.zoomLevel(), e.layerCanvas.panOffset())},
              {"track", navigation(e.trackCanvas.zoomLevel(), e.trackCanvas.panOffset())},
              {"dsp", navigation(e.dspCanvas.navigation().zoom, e.dspCanvas.navigation().pan)},
              {"motion", navigation(e.motionView.zoom, e.motionView.pan)}}},
            {"composition_pixel_lock_percent", e.compositionPixelLock * 100},
            {"composition_pending_pixel_percent", e.pendingPixelZoom * 100},
            {"mode", modeNames[int(e.mode)]},
            {"panels", windows},
            {"monitors", monitors},
            {"theme", cyui::currentThemeKey()},
            {"locale", cyui::locale()},
            {"themes", themes},
            {"locales", locales},
            {"density", cyui::density() == cyui::Density::compact ? "compact" : "comfortable"},
            {"timeline",
             {{"pixels_per_second", e.zoom},
              {"first_visible", e.scroll},
              {"snap", e.snap},
              {"loop", e.loop},
              {"ripple", e.rippleEdits}}},
            {"selected_effect", e.selectedEffect},
            {"mix_scope", e.mixScope},
            {"mix_target", e.mixTarget},
            {"clip_editor", e.editedContainer},
            {"clip_drafts", e.hasClipDrafts()},
            {"source", e.sourcePath},
            {"error", e.error},
            {"changes_allowed", automation.changesAllowed()}};
}
void UiAutomation::add(std::string name, std::string description, J properties, J required, bool readOnly,
                       std::function<J(const J &, const cymcp::CallContext &)> handler) {
    properties["workspace"] = {{"type", "array"}, {"items", integer()}, {"maxItems", 8}};
    cymcp::Operation op;
    op.name = name;
    op.description = std::move(description);
    op.input_schema = {{"type", "object"},
                       {"properties", properties},
                       {"required", required},
                       {"additionalProperties", false}};
    op.output_schema = {{"type", "object"}, {"properties", {{"ok", flag()}}}, {"required", {"ok"}}};
    op.read_only = readOnly;
    op.handler = [handler = std::move(handler)](const J &args, const cymcp::CallContext &context) {
        auto result = handler(args, context);
        result["ok"] = true;
        return result;
    };
    automation.addHostOperation(std::move(op));
    operations.push_back(std::move(name));
}
UiAutomation::UiAutomation(Automation &a, Editor &e)
    : automation(a), editor(e), recorder(ImGui::GetCurrentContext()) {
    auto required = [](std::initializer_list<const char *> names) {
        J out = J::array();
        for (auto n : names)
            out.push_back(n);
        return out;
    };
    add("get_ui_state",
        "Inspect live editor panels, native viewport geometry, monitors, preferences and active clip "
        "workspace. UI tools require a running GUI instance.",
        J::object(), J::array(), true, [this](const J &a, const auto &) { return state(target(a)); });
    add("get_ui_native_windows",
        "List native windows of this workspace's open VST3 plugin workers. Other applications and "
        "account/browser windows are excluded.",
        J::object(), J::array(), true, [this](const J &a, const auto &) {
            auto &e = target(a);
            return nativeWindows(e.s.pluginEditors.processIds(e.s.project, false));
        });
    add("get_ui_native_controls",
        "Discover accessible controls and supported actions in an owned plugin window. Read before acting. "
        "Custom plugin surfaces may expose no accessibility controls; use their existing plugin "
        "parameters/state APIs instead.",
        {{"window", text()}, {"offset", integer()}, {"limit", integer()}}, required({"window"}), true,
        [this](const J &a, const auto &) {
            auto &e = target(a);
            return nativeControls(e.s.pluginEditors.processIds(e.s.project, false), a);
        });
    add("ui_native_control",
        "Invoke, set_value, set_range, toggle, select, expand, collapse or focus a discovered accessible "
        "plugin control. Only current unlocked plugin-worker windows are admitted. Plugin edits are captured "
        "by normal plugin state synchronization and Undo.",
        {{"window", text()}, {"id", text()}, {"action", text()}, {"text", text()}, {"value", number()}},
        required({"window", "id", "action"}), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            return nativeControl(e.s.pluginEditors.processIds(e.s.project, true), a);
        });
    add("get_ui_items",
        "Find controls in the last rendered ImGui frame, including menus, inputs and canvases. Use their "
        "stable item IDs with ui_input; open panels and menus before querying their children. Labels contain "
        "no input text values.",
        {{"window", text()},
         {"query", text()},
         {"offset", integer()},
         {"limit", integer()},
         {"visible_only", flag()},
         {"include_active_text", flag()}},
        J::array(), true, [this](const J &a, const auto &) {
            int offset = std::max(0, a.value("offset", 0)), limit = std::clamp(a.value("limit", 100), 1, 200),
                total = 0;
            auto rows = J::array();
            auto window = a.value("window", std::string{}), query = a.value("query", std::string{});
            for (auto &[id, r] : recorder.items) {
                if (!contains(r.window, window) || !contains(r.label, query) ||
                    (a.value("visible_only", true) && !r.visible))
                    continue;
                if (total++ < offset || rows.size() >= size_t(limit))
                    continue;
                rows.push_back({{"id", itemId(id)},
                                {"window", r.window},
                                {"label", r.label},
                                {"visible", r.visible},
                                {"disabled", r.disabled},
                                {"inputable", r.inputable},
                                {"checkable", r.checkable},
                                {"checked", r.checked},
                                {"rect", {r.rect.Min.x, r.rect.Min.y, r.rect.Max.x, r.rect.Max.y}}});
                auto &input = GImGui->InputTextState;
                if (a.value("include_active_text", false) && GImGui->ActiveId == id && input.ID == id &&
                    !(input.Flags & ImGuiInputTextFlags_Password))
                    rows.back()["active_text"] = std::string(input.GetText()).substr(0, 65536);
            }
            return J{{"frame", ImGui::GetFrameCount()},
                     {"items", rows},
                     {"total", total},
                     {"next_offset", offset + int(rows.size()) < total ? J(offset + rows.size()) : J()}};
        });
    add("list_ui_commands",
        "Discover the real command palette's stable command IDs, translated labels, shortcuts and current "
        "enabled state. Commands also work inside an open nested clip workspace.",
        {{"query", text()}}, J::array(), true, [this](const J &a, const auto &) {
            J rows = J::array();
            for (auto &c : target(a).commands())
                if (contains(c.id + " " + c.label, a.value("query", std::string{})))
                    rows.push_back({{"id", c.id},
                                    {"label", cyui::tr(c.label.c_str())},
                                    {"enabled", c.enabled},
                                    {"shortcut", c.shortcut}});
            return J{{"commands", rows}};
        });
    add("ui_command",
        "Execute an enabled command from list_ui_commands. Supply file_path for file/folder dialogs; omitted "
        "paths report PathRequired without blocking. Business edits retain validation, permissions and Undo.",
        {{"id", text()}, {"file_path", text()}}, required({"id"}), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            auto id = a.at("id").get<std::string>();
            for (auto &c : e.commands())
                if (c.id == id) {
                    if (!c.enabled)
                        throw cymcp::Error("Disabled",
                                           "This editor command is disabled in the current selection");
                    e.error.clear();
                    e.automationErrorCode.clear();
                    context(editor, true, a.value("file_path", std::string{}));
                    try {
                        c.run();
                    } catch (...) {
                        context(editor, false);
                        throw;
                    }
                    context(editor, false);
                    if (!e.error.empty())
                        throw cymcp::Error(
                            e.automationErrorCode.empty()
                                ? (e.error.starts_with("PathRequired") ? "PathRequired" : "OperationFailed")
                                : e.automationErrorCode,
                            e.error);
                    return J{{"executed", id}, {"state", state(e)}};
                }
            throw cymcp::Error("UnknownCommand", "Query list_ui_commands for an exact command ID");
        });
    add("ui_workspace",
        "Switch workspace mode or reset its layout. Detached panels and the visible assistant remain "
        "available. Modes: edit, effects, motion, color, audio, deliver.",
        {{"mode", text()}, {"reset_layout", flag()}}, J::array(), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            if (!e.panelSuffix.empty())
                throw cymcp::Error("Unsupported", "Modes belong to the main editor");
            if (a.contains("mode")) {
                auto name = a.at("mode").get<std::string>();
                int m = 0;
                for (; m < Editor::modeCount; ++m)
                    if (name == modeNames[m])
                        break;
                if (m == Editor::modeCount)
                    throw cymcp::Error("InvalidArguments", "Unknown workspace mode");
                bool assistant = e.panels["Assistant"];
                e.setMode(Editor::Mode(m));
                if (assistant)
                    e.showAssistant();
            }
            if (a.value("reset_layout", false)) {
                e.resetModeLayout();
                e.layoutForce = true;
                e.layoutReady = false;
            }
            return J{{"state", state(e)}};
        });
    add("ui_panel",
        "Show, close, focus, detach, position, resize or dock any editor panel. Geometry is desktop pixels "
        "from get_ui_state. dock_to tabs with another open panel; geometry is applied on the next render "
        "frame.",
        {{"panel", text()},
         {"visible", flag()},
         {"focus", flag()},
         {"detach", flag()},
         {"x", number()},
         {"y", number()},
         {"width", number()},
         {"height", number()},
         {"dock_to", text()}},
        required({"panel"}), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            auto name = a.at("panel").get<std::string>();
            if (!e.panels.contains(name))
                throw cymcp::Error("UnknownPanel", "Use a panel name from get_ui_state");
            Editor::PanelPlacement p;
            p.detach = a.value("detach", false);
            if (a.contains("x") != a.contains("y") || a.contains("width") != a.contains("height"))
                throw cymcp::Error("InvalidArguments", "Provide both coordinates and both dimensions");
            if (a.contains("x"))
                p.position = ImVec2(a["x"].get<float>(), a["y"].get<float>());
            if (a.contains("width")) {
                float w = a["width"], h = a["height"];
                if (w < 120 || w > 16384 || h < 80 || h > 16384)
                    throw cymcp::Error("InvalidArguments",
                                       "Panel dimensions are outside 120..16384 by 80..16384");
                p.size = ImVec2(w, h);
            }
            if (a.contains("dock_to")) {
                auto other = a["dock_to"].get<std::string>();
                if (!e.panels.contains(other) || other == name || p.detach)
                    throw cymcp::Error("InvalidArguments", "Choose another panel and omit detach");
                auto *w = ImGui::FindWindowByName(e.panelTitle(other.c_str()).c_str());
                if (!w || !w->DockId)
                    throw cymcp::Error("NotReady",
                                       "Open and dock the target panel before attaching another tab");
                p.dock = w->DockId;
                e.detachedPanels.erase(name);
            }
            if (p.detach)
                e.detachedPanels.insert(name);
            e.panels[name] = a.value("visible", true);
            if (e.panels[name] && a.value("focus", true))
                e.pendingPanelFocus = e.panelTitle(name.c_str());
            if (p.detach || p.dock || p.position || p.size)
                e.panelPlacements[name] = p;
            return J{{"requested", true}, {"state", state(e)}};
        });
    add("ui_view",
        "Control timeline navigation/options and composition or graph zoom/pan. view: timeline, composition, "
        "shader, layer, track, dsp, motion. pixel_percent is exact image scale (100 = 1:1); fit resets the "
        "selected view.",
        {{"view", text()},
         {"zoom", number()},
         {"pan_x", number()},
         {"pan_y", number()},
         {"fit", flag()},
         {"first_visible", number()},
         {"pixel_percent", number()},
         {"snap", flag()},
         {"loop", flag()},
         {"ripple", flag()},
         {"filmstrip", flag()},
         {"waveforms", flag()},
         {"keyframes", flag()}},
        required({"view"}), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            auto v = a.at("view").get<std::string>();
            bool fit = a.value("fit", false);
            if (v == "timeline") {
                e.setTimelineView(a.value("zoom", e.zoom), a.value("first_visible", e.scroll));
                if (fit)
                    e.fitTimeline(std::max(100.f, ImGui::GetMainViewport()->WorkSize.x - e.headerWidth - 50));
                e.snap = a.value("snap", e.snap);
                e.loop = a.value("loop", e.loop);
                e.rippleEdits = a.value("ripple", e.rippleEdits);
                e.showFilmstrip = a.value("filmstrip", e.showFilmstrip);
                e.showWaveforms = a.value("waveforms", e.showWaveforms);
                e.showKeyframes = a.value("keyframes", e.showKeyframes);
            } else if (v == "composition") {
                if (fit) {
                    e.compositionView = {};
                    e.compositionPixelLock = e.pendingPixelZoom = 0;
                }
                if (a.contains("pixel_percent")) {
                    auto value = a["pixel_percent"].get<float>();
                    if (value < 1 || value > 3200)
                        throw cymcp::Error("InvalidArguments", "pixel_percent must be 1..3200");
                    e.pendingPixelZoom = value / 100;
                }
                e.compositionView.pan = {a.value("pan_x", e.compositionView.pan.x),
                                         a.value("pan_y", e.compositionView.pan.y)};
            } else if (v == "layer" || v == "track") {
                auto &c = v == "layer" ? e.layerCanvas : e.trackCanvas;
                c.setNavigation(a.value("zoom", c.zoomLevel()),
                                {a.value("pan_x", c.panOffset().x), a.value("pan_y", c.panOffset().y)}, fit);
            } else if (v == "dsp") {
                e.dspCanvas.setNavigation(a.value("zoom", e.dspCanvas.navigation().zoom),
                                          {a.value("pan_x", 0.f), a.value("pan_y", 0.f)}, fit);
                e.channelDsp.setNavigation(a.value("zoom", e.channelDsp.navigation().zoom),
                                           {a.value("pan_x", 0.f), a.value("pan_y", 0.f)}, fit);
            } else if (v == "shader" || v == "motion") {
                auto &c = v == "shader" ? e.graphView : e.motionView;
                c.zoom = std::clamp(a.value("zoom", c.zoom), .2f, 2.5f);
                c.pan = {a.value("pan_x", c.pan.x), a.value("pan_y", c.pan.y)};
                if (fit) {
                    c = {};
                    if (v == "shader")
                        e.graphFitPending = true;
                }
            } else
                throw cymcp::Error("InvalidArguments", "Unknown view");
            return J{{"state", state(e)}};
        });
    add("ui_preferences",
        "Set and save application theme, UI density or language using the real CyUI preferences. Discover "
        "valid theme/locale keys with get_ui_state.",
        {{"theme", text()}, {"locale", text()}, {"density", text()}}, J::array(), false,
        [this](const J &a, const auto &) {
            if (a.contains("theme") &&
                std::none_of(cyui::builtInThemes().begin(), cyui::builtInThemes().end(),
                             [&](auto &t) { return t.key == a["theme"].get<std::string>(); }))
                throw cymcp::Error("InvalidArguments", "Unknown theme key");
            auto locales = cyui::availableLocales();
            if (a.contains("locale") && std::none_of(locales.begin(), locales.end(), [&](auto &l) {
                    return l.code == a["locale"].get<std::string>();
                }))
                throw cymcp::Error("InvalidArguments", "Unknown locale");
            auto density = a.value("density", std::string{});
            if (!density.empty() && density != "compact" && density != "comfortable")
                throw cymcp::Error("InvalidArguments", "Unknown density");
            if (a.contains("theme"))
                cyui::setTheme(a["theme"].get<std::string>().c_str());
            if (a.contains("locale"))
                cyui::setLocale(a["locale"].get<std::string>().c_str());
            if (!density.empty())
                cyui::setDensity(density == "compact" ? cyui::Density::compact : cyui::Density::comfortable);
            saveUiPreferences();
            return J{{"state", state(editor)}};
        });
    add("list_ui_shortcuts", "Inspect actual editable shortcut IDs and conflicting bindings.", J::object(),
        J::array(), true, [this](const J &a, const auto &) {
            auto &e = target(a);
            J rows = J::array();
            for (auto &d : Keymap::defaults())
                rows.push_back({{"id", d.id},
                                {"label", d.label},
                                {"chord", e.keymap.label(d.id)},
                                {"conflicts", e.keymap.conflicts(d.id)}});
            return J{{"shortcuts", rows}};
        });
    add("ui_shortcut",
        "Rebind an editable shortcut by stable ID and named ImGui key; key None removes its binding. reset "
        "restores all defaults. Changes use the normal keymap persistence.",
        {{"id", text()},
         {"key", text()},
         {"ctrl", flag()},
         {"shift", flag()},
         {"alt", flag()},
         {"reset", flag()}},
        J::array(), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            if (a.value("reset", false))
                e.keymap.reset();
            else {
                auto key = a.value("key", std::string{});
                if (!a.contains("id") || key.empty())
                    throw cymcp::Error("InvalidArguments", "Provide id and key");
                e.keymap.set(a["id"], key == "None" ? ImGuiKey_None : namedKey(key), a.value("ctrl", false),
                             a.value("shift", false), a.value("alt", false));
            }
            e.saveKeymap();
            return J{{"saved", true}};
        });
    add("ui_focus",
        "Select the clip/effect, mix channel and animation parameter shown by the editor. A track opens its "
        "video effect graph. Use workspace to address a nested clip editor.",
        {{"clip", integer()},
         {"effect", integer()},
         {"track", integer()},
         {"scope", text()},
         {"target", integer()},
         {"parameter", text()}},
        J::array(), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            if (a.contains("clip")) {
                auto id = a["clip"].get<Id>();
                if (id && !e.s.project.findClip(id))
                    throw cymcp::Error("InvalidArguments", "Unknown clip");
                e.s.selectClip(id);
            }
            if (a.contains("effect")) {
                int index = a["effect"];
                auto *c = e.s.project.findClip(e.s.selected);
                if (index < -1 || !c || index >= int(c->effects.size()))
                    throw cymcp::Error("InvalidArguments", "Unknown clip effect");
                e.selectedEffect = index;
                e.animationEffect = index;
            }
            if (a.contains("track")) {
                auto id = a["track"].get<Id>();
                auto *t = e.s.project.findTrack(id);
                if (!t || t->kind != TrackKind::Video)
                    throw cymcp::Error("InvalidArguments", "Choose a video track");
                e.openTrackGraph(id);
            }
            if (a.contains("scope")) {
                auto scope = a["scope"].get<std::string>();
                Id id = a.value("target", Id(0));
                if ((scope == "track" && !e.s.project.findTrack(id)) ||
                    (scope == "bus" && !e.s.project.findBus(id)) ||
                    (scope != "master" && scope != "track" && scope != "bus"))
                    throw cymcp::Error("InvalidArguments", "Unknown mix channel");
                e.mixScope = scope;
                e.mixTarget = scope == "master" ? 0 : id;
            }
            if (a.contains("parameter")) {
                e.animationKey = a["parameter"];
                e.mixAnimationKey = e.animationKey;
            }
            return J{{"state", state(e)}};
        });
    add("ui_clip_editor",
        "Open a video clip's nested editor (creating its container if needed), apply drafts, or reload saved "
        "container content. Controlled profiles refuse reloading dirty drafts. action: open, apply, reload.",
        {{"action", text()}, {"clip", integer()}}, required({"action"}), false,
        [this](const J &a, const auto &) {
            auto &e = target(a);
            auto action = a["action"].get<std::string>();
            e.error.clear();
            context(editor, true);
            try {
                if (action == "apply") {
                    if (!e.applyClipDrafts())
                        throw cymcp::Error("OperationFailed", e.error);
                } else if (action == "open")
                    e.openClipEditor(a.value("clip", e.s.selected));
                else if (action == "reload") {
                    if (automation.controlsOperations() && e.hasClipDrafts())
                        throw cymcp::Error("ConfirmationRequired",
                                           "Apply or save the nested draft before reloading");
                    e.openClipEditor(e.editedContainer, true);
                } else
                    throw cymcp::Error("InvalidArguments", "Unknown clip editor action");
            } catch (...) {
                context(editor, false);
                throw;
            }
            context(editor, false);
            if (!e.error.empty())
                throw cymcp::Error("OperationFailed", e.error);
            return J{{"state", state(e)}};
        });
    add("get_ui_clip_project",
        "Read the current nested workspace, including uncommitted local edits. Its workspace path is the "
        "ordered list of open ancestor container clip IDs.",
        J::object(), J::array(), true, [this](const J &a, const auto &) {
            auto &e = target(a);
            return J{{"project", cymcp::parse_json(e.projectWithClipDrafts().json().Dump(0),
                                                   cymcp::provider_message_limit)},
                     {"dirty", e.s.dirty}};
        });
    add("ui_clip_invoke",
        "Run a real business operation in an open nested clip workspace, using the same input schema as the "
        "root operation. Changes remain local drafts until ui_clip_editor apply. Saving and Undo work on "
        "that draft.",
        {{"operation", text()}, {"arguments", type("object")}}, required({"operation"}), false,
        [this](const J &a, const auto &) {
            auto &e = target(a);
            if (&e == &editor)
                throw cymcp::Error("InvalidArguments", "Specify a nonempty workspace path");
            auto name = a["operation"].get<std::string>();
            Json args;
            if (!Json::Parse(a.value("arguments", J::object()).dump(), args))
                throw cymcp::Error("InvalidJson", "Invalid arguments");
            e.error.clear();
            context(editor, true);
            bool ok = false;
            try {
                ok = e.action(name, args);
            } catch (...) {
                context(editor, false);
                throw;
            }
            context(editor, false);
            if (!ok)
                throw cymcp::Error("OperationFailed", e.error);
            return J{{"result", cymcp::parse_json(e.actionResult.Dump(0), cymcp::provider_message_limit)},
                     {"draft", true}};
        });
    add("ui_open_source", "Open media in the real Source monitor for inspection and source editing.",
        {{"path", text()}}, required({"path"}), false, [this](const J &a, const auto &) {
            auto &e = target(a);
            e.openSource(a["path"]);
            return J{{"state", state(e)}};
        });
    add("ui_show_help",
        "Open the application's existing local CyDoc reader at an exact topic, language and anchor.",
        {{"topic", text()}, {"language", text()}, {"anchor", text()}}, J::array(), false,
        [this](const J &a, const auto &) {
            auto &e = target(a);
            e.showManual(a.value("topic", std::string{}), a.value("language", std::string{}),
                         a.value("anchor", std::string{}));
            if (!e.help.error().empty())
                throw cymcp::Error("OperationFailed", e.help.error());
            return J{{"opened", true}};
        });
    add("ui_file_list",
        "List one explicitly supplied media/project directory to choose a path for an application operation. "
        "Nonrecursive, sorted, paginated; never reads file contents or executes a shell.",
        {{"path", text()}, {"offset", integer()}, {"limit", integer()}, {"query", text()}},
        required({"path"}), true, [this](const J &a, const auto &c) {
            auto path = std::filesystem::absolute(sx::Utf8Path(a["path"]));
            std::vector<std::filesystem::directory_entry> entries;
            bool capped = false;
            size_t scanned = 0;
            for (auto &entry : std::filesystem::directory_iterator(path)) {
                c.cancellation.check();
                if (scanned++ == 10000) {
                    capped = true;
                    break;
                }
                if (contains(cymcp::path_utf8(entry.path().filename()), a.value("query", std::string{})))
                    entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(),
                      [](auto &a, auto &b) { return a.path().filename() < b.path().filename(); });
            auto offset = std::min(size_t(std::max(0, a.value("offset", 0))), entries.size()),
                 limit = size_t(std::clamp(a.value("limit", 100), 1, 200));
            J rows = J::array();
            for (auto i = offset; i < std::min(entries.size(), offset + limit); ++i) {
                auto &entry = entries[i];
                rows.push_back({{"name", cymcp::path_utf8(entry.path().filename())},
                                {"path", cymcp::path_utf8(entry.path())},
                                {"directory", entry.is_directory()}});
            }
            return J{{"entries", rows},
                     {"total", entries.size()},
                     {"scan_capped", capped},
                     {"next_offset", offset + rows.size() < entries.size() ? J(offset + rows.size()) : J()}};
        });
    add("ui_input",
        "Interact with live native ImGui controls: click, double_click, context_click, text, key, scroll, "
        "drag. Prefer semantic/business commands. Use item ID or an exact window name and desktop x/y. "
        "Returns an asynchronous job: poll get_ui_input before the next input. file_path resolves file "
        "dialogs without blocking.",
        {{"action", text()},
         {"item", text()},
         {"window", text()},
         {"x", number()},
         {"y", number()},
         {"to_x", number()},
         {"to_y", number()},
         {"button", integer()},
         {"text", text()},
         {"key", text()},
         {"ctrl", flag()},
         {"shift", flag()},
         {"alt", flag()},
         {"submit", flag()},
         {"wheel_x", number()},
         {"wheel_y", number()},
         {"frames", integer()},
         {"file_path", text()}},
        required({"action"}), false, [this](const J &a, const auto &c) { return queueInput(a, c); });
    add("get_ui_input",
        "Inspect the latest UI input job. completed means input was delivered, not that a business edit "
        "succeeded; inspect editor state/tool results. Failed/cancelled jobs are never replayed.",
        {{"job", integer()}}, J::array(), true, [this](const J &a, const auto &c) {
            if (a.contains("job") && a["job"] != input.id)
                throw cymcp::Error("NotFound", "Only the latest UI input job is retained");
            return inputState(c);
        });
    add("ui_cancel_input",
        "Cancel the caller's pending UI input and release only its synthetic keys/buttons. Allowed after "
        "write permission is revoked; no business operation is replayed.",
        {{"job", integer()}}, J::array(), true, [this](const J &a, const auto &c) {
            inputState(c);
            if (a.contains("job") && a["job"] != input.id)
                throw cymcp::Error("NotFound", "Unknown input job");
            if (input.state == "running" || input.state == "queued") {
                release();
                input.state = "cancelled";
            }
            return inputState(c);
        });
}
UiAutomation::~UiAutomation() {
    release();
    context(editor, false);
    for (auto &name : operations)
        automation.removeHostOperation(name);
}
J UiAutomation::inputState(const cymcp::CallContext &c) const {
    if (input.id && input.principal != c.principal)
        throw cymcp::Error("AccessDenied", "UI input belongs to another caller");
    return {{"job", input.id},
            {"state", input.state},
            {"remaining_frames", input.events.size()},
            {"held_keys", input.keys.size()},
            {"held_buttons", input.buttons.size()},
            {"error", input.error},
            {"error_code", input.errorCode}};
}
void UiAutomation::release() {
    auto &io = ImGui::GetIO();
    for (auto key : input.keys)
        io.AddKeyEvent(key, false);
    for (auto button : input.buttons)
        io.AddMouseButtonEvent(button, false);
    input.keys.clear();
    input.buttons.clear();
    input.events.clear();
}
J UiAutomation::queueInput(const J &a, const cymcp::CallContext &c) {
    if (input.state == "queued" || input.state == "running")
        throw cymcp::Error("Busy", "Wait for or cancel the active UI input");
    auto action = a["action"].get<std::string>();
    if (action != "click" && action != "double_click" && action != "context_click" && action != "text" &&
        action != "key" && action != "scroll" && action != "drag")
        throw cymcp::Error("InvalidArguments", "Unknown input action");
    Input next;
    next.id = ++sequence;
    next.principal = c.principal;
    next.cancellation = c.cancellation;
    next.file = a.value("file_path", std::string{});
    if (a.contains("item")) {
        next.item = parseId(a["item"]);
        auto it = recorder.items.find(next.item);
        if (it == recorder.items.end() || !it->second.visible || it->second.disabled)
            throw cymcp::Error("NotReady",
                               "The control is missing, clipped or disabled; query get_ui_items again");
        auto &r = it->second;
        next.window = r.window;
        next.label = r.label;
        next.point = r.rect.GetCenter();
        if (action == "text" && !r.inputable)
            throw cymcp::Error("InvalidArguments", "Choose an inputable control");
    } else {
        next.window = a.value("window", std::string{});
        auto *w = ImGui::FindWindowByName(next.window.c_str());
        if (!w || !w->WasActive)
            throw cymcp::Error("NotReady", "Choose an exact active window from get_ui_state or get_ui_items");
        next.point = {a.value("x", w->InnerRect.GetCenter().x), a.value("y", w->InnerRect.GetCenter().y)};
        if (!w->Rect().Contains(next.point))
            throw cymcp::Error("InvalidArguments", "Point must be inside the chosen application window");
    }
    auto text = a.value("text", std::string{});
    if (text.size() > 65536)
        throw cymcp::Error("InvalidArguments", "Input text exceeds 64 KiB");
    int button = action == "context_click" ? 1 : a.value("button", 0);
    if (button < 0 || button > 2)
        throw cymcp::Error("InvalidArguments", "Mouse button must be 0, 1 or 2");
    if (action == "drag" && (!a.contains("to_x") || !a.contains("to_y")))
        throw cymcp::Error("InvalidArguments", "Drag requires to_x and to_y");
    ImGuiKey key = action == "key" ? namedKey(a.value("key", std::string{})) : ImGuiKey_None;
    input = std::move(next);
    input.state = "queued";
    auto event = [&](auto emit) { input.events.push_back({emit}); };
    auto keyEvent = [&](ImGuiKey k, bool down) {
        event([this, k, down](ImGuiIO &io) {
            io.AddKeyEvent(k, down);
            if (down)
                input.keys.insert(k);
            else
                input.keys.erase(k);
        });
    };
    auto mouse = [&](bool down) {
        event([this, button, down](ImGuiIO &io) {
            io.AddMouseButtonEvent(button, down);
            if (down)
                input.buttons.insert(button);
            else
                input.buttons.erase(button);
        });
    };
    event([this](ImGuiIO &io) {
        auto *w = ImGui::FindWindowByName(input.window.c_str());
        if (!w)
            throw cymcp::Error("NotReady", "Target window closed");
        ImGui::FocusWindow(w);
        io.AddFocusEvent(true);
        if (w->Viewport)
            io.AddMouseViewportEvent(w->Viewport->ID);
        io.AddMousePosEvent(input.point.x, input.point.y);
    });
    if (a.value("ctrl", false) || action == "text")
        keyEvent(ImGuiMod_Ctrl, true);
    if (a.value("shift", false))
        keyEvent(ImGuiMod_Shift, true);
    if (a.value("alt", false))
        keyEvent(ImGuiMod_Alt, true);
    if (action == "key") {
        keyEvent(key, true);
        keyEvent(key, false);
    } else if (action == "scroll") {
        float x = a.value("wheel_x", 0.f), y = a.value("wheel_y", 0.f);
        event([x, y](ImGuiIO &io) {
            io.AddMouseWheelEvent(std::clamp(x, -20.f, 20.f), std::clamp(y, -20.f, 20.f));
        });
    } else {
        mouse(true);
        if (action == "drag") {
            auto end = ImVec2(a["to_x"].get<float>(), a["to_y"].get<float>());
            int frames = std::clamp(a.value("frames", 8), 1, 60);
            auto start = input.point;
            for (int i = 1; i <= frames; ++i)
                event([this, start, end, f = float(i) / frames](ImGuiIO &io) {
                    auto p = ImLerp(start, end, f);
                    input.point = p;
                    io.AddMousePosEvent(p.x, p.y);
                });
        }
        mouse(false);
        if (action == "double_click") {
            mouse(true);
            mouse(false);
        }
        if (action == "text") {
            keyEvent(ImGuiMod_Ctrl, false);
            keyEvent(ImGuiMod_Ctrl, true);
            keyEvent(ImGuiKey_A, true);
            keyEvent(ImGuiKey_A, false);
            keyEvent(ImGuiMod_Ctrl, false);
            event([text](ImGuiIO &io) { io.AddInputCharactersUTF8(text.c_str()); });
            if (a.value("submit", false)) {
                keyEvent(ImGuiKey_Enter, true);
                keyEvent(ImGuiKey_Enter, false);
            }
        }
    }
    event([this](ImGuiIO &) { release(); });
    return inputState(c);
}
void UiAutomation::beforeFrame() {
    finishFrame = false;
    inputFrame = false;
    if (input.state != "queued" && input.state != "running")
        return;
    try {
        input.cancellation.check();
        if (!automation.changesAllowed())
            throw cymcp::Error("AccessDenied", "Application changes were disabled before input execution");
        if (input.item && input.state == "queued") {
            auto it = recorder.items.find(input.item);
            if (it == recorder.items.end() || !it->second.visible || it->second.disabled ||
                it->second.window != input.window || it->second.label != input.label)
                throw cymcp::Error("NotReady", "Target control changed before execution");
            input.point = it->second.rect.GetCenter();
        }
        if (input.state == "queued") {
            editor.error.clear();
            editor.automationErrorCode.clear();
            for (auto *child = editor.clipWorkspace.get(); child; child = child->clipWorkspace.get())
                if (input.window.find(child->panelSuffix) != std::string::npos) {
                    child->error.clear();
                    child->automationErrorCode.clear();
                }
        }
        auto &io = ImGui::GetIO();
        if (auto *w = ImGui::FindWindowByName(input.window.c_str()); w && w->Viewport)
            io.AddMouseViewportEvent(w->Viewport->ID);
        io.AddMousePosEvent(input.point.x, input.point.y);
        input.state = "running";
        inputFrame = true;
        context(editor, true, input.file);
        if (!input.events.empty()) {
            auto e = std::move(input.events.front());
            input.events.pop_front();
            e.emit(ImGui::GetIO());
        }
        finishFrame = input.events.empty();
    } catch (const std::exception &e) {
        input.error = cymcp::redact_error_message(e.what());
        if (auto *sdk = dynamic_cast<const cymcp::Error *>(&e))
            input.errorCode = sdk->code;
        input.state = input.errorCode == "Cancelled" ? "cancelled" : "failed";
        release();
        context(editor, false);
    }
}
void UiAutomation::afterFrame() {
    if (!inputFrame)
        return;
    if (!automatedDialogError().empty()) {
        editor.error = automatedDialogError();
        editor.automationErrorCode = automatedUiErrorCode();
    }
    for (auto *child = editor.clipWorkspace.get(); child; child = child->clipWorkspace.get())
        if (input.window.find(child->panelSuffix) != std::string::npos && !child->error.empty()) {
            editor.error = child->error;
            editor.automationErrorCode = child->automationErrorCode;
        }
    if (!editor.error.empty()) {
        input.error = editor.error;
        input.errorCode = editor.automationErrorCode;
        input.state = "failed";
        release();
    } else if (finishFrame)
        input.state = "completed";
    context(editor, false);
    inputFrame = false;
}
} // namespace sf

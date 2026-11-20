#include "ui/Assistant.h"
#include "ui/UiItems.h"
#include <commdlg.h>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <shellapi.h>
namespace sf {
namespace {
std::vector<std::filesystem::path> attachments() {
    if (auto path = takeAutomatedDialogPath()) {
        if (path->empty())
            throw cymcp::Error("PathRequired", automatedDialogError());
        return {sx::Utf8Path(*path)};
    }
    wchar_t path[32768]{};
    OPENFILENAMEW o{sizeof(o)};
    o.hwndOwner = GetActiveWindow();
    o.lpstrFilter = L"Attachments\0*.*\0\0";
    o.lpstrFile = path;
    o.nMaxFile = 32768;
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&o))
        return {path};
    return {};
}
std::string T(const char *text) {
    return cyui::trLabel(text);
}
std::string modelIdentifier(const cymcp::Json &row) {
    if (row.is_string())
        return row.get<std::string>();
    if (row.is_object())
        for (auto key : {"id", "model"})
            if (row.contains(key) && row[key].is_string())
                return row[key].get<std::string>();
    return {};
}
} // namespace
Assistant::Assistant(Automation &a, AutomationOptions o) : automation(a), options(std::move(o)) {
    cyui::registerTranslations(
        "fr",
        {{"Assistant", "Assistant"},
         {"Assistant settings", "Paramètres de l’assistant"},
         {"Detect providers", "Détecter les fournisseurs"},
         {"Provider ID", "Identifiant du fournisseur"},
         {"Model ID", "Identifiant du modèle"},
         {"Base URL", "URL de base"},
         {"Executable", "Exécutable"},
         {"Process arguments (JSON)", "Arguments du processus (JSON)"},
         {"API key environment variable", "Variable d’environnement de la clé API"},
         {"AI may use application tools", "L’IA peut utiliser les outils de l’application"},
         {"Allow application changes", "Autoriser les modifications de l’application"},
         {"Save conversation history", "Enregistrer l’historique de conversation"},
         {"History ID", "Identifiant de l’historique"},
         {"Context budget (KiB, 0 = off)", "Budget de contexte (Kio, 0 = désactivé)"},
         {"Connect / New chat", "Connecter / Nouveau chat"},
         {"Restore saved chat", "Restaurer le chat enregistré"},
         {"Reconnect", "Reconnecter"},
         {"Disconnect", "Déconnecter"},
         {"Start local MCP", "Démarrer le MCP local"},
         {"Stop local MCP", "Arrêter le MCP local"},
         {"Copy connection file path", "Copier le chemin du fichier de connexion"},
         {"Copy MCP address", "Copier l’adresse MCP"},
         {"MCP access profile (restart required)", "Profil d’accès MCP (redémarrage requis)"},
         {"Restart the application to apply the saved profile.", "Redémarrez l’application pour appliquer le profil enregistré."},
         {"Local controlled", "Local contrôlé"}, {"Token only", "Jeton seul"}, {"Protected", "Protégé"},
         {"Connect allowed remote tools", "Connecter les outils distants autorisés"},
         {"Configuration applies to the next connection.",
          "La configuration s’applique à la prochaine connexion."}},
        "Français");
    cymcp::ChatPanelOptions p;
    p.pick_attachments = attachments;
    p.open_settings = [this] { settings = true; };
    p.open_link = [](const std::string &url) {
        if (url.starts_with("https://") || url.starts_with("http://"))
            ShellExecuteW(GetActiveWindow(), L"open", sx::Widen(url).c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL);
    };
    panel = std::make_unique<cymcp::ChatPanel>(controller, std::move(p));
    cyui::registerTranslations("fr",
                               {{"Models", "Modèles"},
                                {"Providers", "Fournisseurs"},
                                {"Advanced connection", "Connexion avancée"},
                                {"History and context", "Historique et contexte"},
                                {"Permissions", "Autorisations"},
                                {"Other applications", "Autres applications"},
                                {"Allow replacing unsaved project once",
                                 "Autoriser une fois le remplacement du projet non enregistré"},
                                {"Allow replacing once", "Autoriser un remplacement"}},
                               "Français");
}
Assistant::~Assistant() {
    automation.stop();
    detectionCancellation.cancel();
    controller.cancel();
    if (detection.valid())
        detection.wait();
    panel.reset();
}
void Assistant::poll() {
    controller.poll();
    if (detection.valid() && detection.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            connections = detection.get();
        } catch (const std::exception &e) {
            error = cymcp::redact_error_message(e.what());
        }
    }
}
void Assistant::connect(bool restore) {
    error.clear();
    cymcp::ChatConnection connection;
    connection.provider_id = provider;
    connection.model = model;
    connection.provider = selectedConfig;
    if (!baseUrl.empty())
        connection.provider.settings["base_url"] = baseUrl;
    if (!executable.empty())
        connection.provider.process.executable = sx::Utf8Path(executable);
    connection.provider.process.arguments = cymcp::parse_json(arguments).get<std::vector<std::string>>();
    if (!keyEnvironment.empty()) {
        auto key = std::getenv(keyEnvironment.c_str());
        connection.provider.api_key = key ? key : "";
    }
    if (appTools)
        connection.provider.tools = automation.chatTools();
    connection.provider.settings["deferred_tools"] = options.compact;
    connection.system_prompt =
        "You assist inside ShaderixFrame. Use application tools to inspect and edit the actual current "
        "sequence. Read before editing. Respect refusals and preserve unsaved work. Never claim an action "
        "succeeded without its tool result. "
        "Discover UI commands and panels with get_ui_state/list_ui_commands. Use ui_input only for "
        "visible controls lacking semantic operations, poll get_ui_input and inspect the resulting state. "
        "Use workspace clip-ID paths for nested drafts and apply them explicitly before final saving. "
        "Large results must be read using their resource handles; never "
        "replay a mutation to recover an expired result.";
    connection.automatic_model = true;
    connection.restore = restore;
    if (saveHistory || restore) {
        connection.history_directory = options.privateDirectory / "conversations";
        connection.history_id = historyId;
    }
    connection.context.max_bytes = size_t(std::max(0, contextKiB)) * 1024;
    connection.performance = automation.performance();
    controller.open(std::move(connection));
}
void Assistant::draw() {
    poll();
    if (settings) {
        ImGui::SetNextItemOpen(true);
        settings = false;
    }
    auto label = T("Assistant settings");
    if (ImGui::CollapsingHeader(label.c_str())) {
        ImGui::BeginChild(
            "AssistantConfiguration",
            {0, std::min(cyui::dp(340), std::max(cyui::dp(120), ImGui::GetContentRegionAvail().y * .55f))},
            ImGuiChildFlags_Borders);
        try {
            if (ImGui::Button(T("Detect providers").c_str()) &&
                (!detection.valid() ||
                 detection.wait_for(std::chrono::seconds(0)) == std::future_status::ready))
                detection = std::async(std::launch::async, [this] {
                    cymcp::CallContext context;
                    context.cancellation = detectionCancellation;
                    return providers->discover({}, context);
                });
            for (auto &c : connections) {
                ImGui::PushID(c.provider_id.c_str());
                if (ImGui::Selectable((c.name + " (" + c.state + ")").c_str(), provider == c.provider_id)) {
                    provider = c.provider_id;
                    selectedConfig = c.config;
                    keyEnvironment = c.api_key_env;
                    selectedModels = c.models;
                    baseUrl = selectedConfig.settings.value("base_url", std::string{});
                    executable = cymcp::path_utf8(selectedConfig.process.executable);
                    arguments = cymcp::Json(selectedConfig.process.arguments).dump();
                    model.clear();
                }
                ImGui::PopID();
            }
            auto field = [&](const char *caption, const char *id, std::string &value) {
                auto text = cyui::tr(caption);
                cyui::property(text.c_str(), text.c_str());
                ImGui::InputText(id, &value);
            };
            if (cyui::beginProperties("AssistantConnection")) {
                cyui::property(cyui::tr("Providers").c_str());
                if (ImGui::BeginCombo("##providers", provider.c_str())) {
                    for (const auto &row : providers->describe()) {
                        auto id = row.at("id").get<std::string>();
                        auto name = row.at("name").get<std::string>();
                        if (ImGui::Selectable(name.c_str(), provider == id)) {
                            provider = id;
                            selectedConfig = {};
                            selectedModels = cymcp::Json::array();
                            const auto &defaults = row.at("defaults");
                            selectedConfig.settings = defaults.at("settings");
                            baseUrl = defaults.value("base_url", std::string{});
                            keyEnvironment = defaults.value("credential_env", std::string{});
                            executable.clear();
                            arguments = "[]";
                            model.clear();
                        }
                    }
                    ImGui::EndCombo();
                }
                field("Model ID", "##model", model);
                cyui::property(cyui::tr("Models").c_str());
                if (ImGui::BeginCombo("##models", model.c_str())) {
                    const auto &models =
                        controller.state().provider_id == provider && controller.state().connected
                            ? controller.state().models
                            : selectedModels;
                    for (const auto &row : models) {
                        auto id = modelIdentifier(row);
                        if (!id.empty() && ImGui::Selectable(id.c_str(), model == id))
                            model = id;
                    }
                    ImGui::EndCombo();
                }
                field("Base URL", "##baseurl", baseUrl);
                cyui::endProperties();
            }
            ImGui::Checkbox(T("AI may use application tools").c_str(), &appTools);
            const std::pair<cymcp::LocalAccessProfile, const char *> profiles[] = {
                {cymcp::LocalAccessProfile::free, "Free"},
                {cymcp::LocalAccessProfile::local_controlled, "Local controlled"},
                {cymcp::LocalAccessProfile::token_only, "Token only"},
                {cymcp::LocalAccessProfile::protected_access, "Protected"}};
            const char *selected = "Free";
            std::optional<cymcp::LocalAccessProfile> requestedProfile;
            for (const auto &[value, name] : profiles)
                if (value == options.accessProfile) selected = name;
            auto profileLabel = cyui::tr("MCP access profile (restart required)");
            ImGui::TextWrapped("%s", profileLabel.c_str());
            ImGui::SetNextItemWidth(-1);
            auto profileId = ImGui::GetID("##mcp-access-profile");
            bool profileOpen = ImGui::BeginCombo("##mcp-access-profile", cyui::tr(selected).c_str());
            // BeginCombo has no ItemInfo hook in this ImGui revision; expose its real label.
            if (GImGui->TestEngineHookItems)
                ImGuiTestEngineHook_ItemInfo(GImGui, profileId, profileLabel.c_str(), 0);
            if (profileOpen) {
                for (const auto &[value, name] : profiles) {
                    if (ImGui::Selectable(T(name).c_str(), value == options.accessProfile)) {
                        if (automatedUiInput() && automation.controlsOperations())
                            rejectAutomatedUiAction("AccessDenied", "MCP profile changes require the user's own action");
                        else
                            requestedProfile = value;
                    }
                }
                ImGui::EndCombo();
            }
            if (requestedProfile) {
                options.saveProfile(*requestedProfile);
                options.accessProfile = *requestedProfile;
            }
            if (options.accessProfile != automation.accessProfile())
                ImGui::TextWrapped("%s", cyui::tr("Restart the application to apply the saved profile.").c_str());
            bool writes = automation.changesAllowed();
            ImGui::BeginDisabled(!automation.controlsOperations());
            if (ImGui::Checkbox(T("Allow application changes").c_str(), &writes)) {
                if (automatedUiInput() && writes)
                    rejectAutomatedUiAction("AccessDenied",
                                            "Application write access requires the user's own action");
                else
                    automation.allowChanges(writes);
            }
            ImGui::EndDisabled();
            {
                cyui::ScopedDisabled disabled(controller.state().busy);
                if (cyui::primaryButton(T("Connect / New chat").c_str()))
                    connect(false);
                if (ImGui::Button(T("Restore saved chat").c_str()))
                    connect(true);
                if (ImGui::Button(T("Reconnect").c_str()))
                    controller.reconnect();
                ImGui::SameLine();
                if (ImGui::Button(T("Disconnect").c_str()))
                    controller.disconnect();
            }
            if (ImGui::CollapsingHeader(T("Advanced connection").c_str()) &&
                cyui::beginProperties("AssistantAdvanced")) {
                field("Provider ID", "##provider-id", provider);
                field("Executable", "##executable", executable);
                field("Process arguments (JSON)", "##arguments", arguments);
                field("API key environment variable", "##key-env", keyEnvironment);
                cyui::endProperties();
            }
            if (ImGui::CollapsingHeader(T("History and context").c_str())) {
                ImGui::Checkbox(T("Save conversation history").c_str(), &saveHistory);
                if (cyui::beginProperties("AssistantHistory")) {
                    field("History ID", "##history", historyId);
                    cyui::property(cyui::tr("Context budget (KiB, 0 = off)").c_str(),
                                   cyui::tr("Context budget (KiB, 0 = off)").c_str());
                    ImGui::InputInt("##context-kib", &contextKiB);
                    contextKiB = std::clamp(contextKiB, 0, 1024 * 1024);
                    cyui::endProperties();
                }
            }
            if (automation.controlsOperations() && ImGui::CollapsingHeader(T("Permissions").c_str())) {
                ImGui::TextWrapped("%s", cyui::tr("Allow replacing unsaved project once").c_str());
                if (ImGui::Button(T("Allow replacing once").c_str())) {
                    if (automatedUiInput())
                        rejectAutomatedUiAction(
                            "ConfirmationRequired",
                            "Replacing unsaved work requires the user's own confirmation");
                    else
                        automation.confirmDiscard();
                }
            }
            if (automation.connectionFile().empty()) {
                if (ImGui::Button(T("Start local MCP").c_str()))
                    automation.startHttp();
            } else {
                if (ImGui::Button(T("Stop local MCP").c_str()))
                    automation.stopHttp();
                ImGui::SameLine();
                if (ImGui::Button(T("Copy connection file path").c_str()))
                    ImGui::SetClipboardText(cymcp::path_utf8(automation.connectionFile()).c_str());
                ImGui::TextWrapped("%s", automation.connectionUrl().c_str());
                if (ImGui::Button(T("Copy MCP address").c_str()))
                    ImGui::SetClipboardText(automation.connectionUrl().c_str());
            }
            if (ImGui::CollapsingHeader(T("Other applications").c_str())) {
                if (ImGui::Button(T("Connect allowed remote tools").c_str()))
                    automation.discoverRemoteTools();
                ImGui::TextWrapped("%s", automation.remoteStatus().c_str());
            }
            ImGui::TextWrapped("%s", cyui::tr("Configuration applies to the next connection.").c_str());
        } catch (const std::exception &e) {
            error = cymcp::redact_error_message(e.what());
        }
        ImGui::EndChild();
    }
    if (!error.empty())
        cyui::callout(cyui::Tone::error, error.c_str());
    panel->draw("ShaderixFrameChat");
}
} // namespace sf

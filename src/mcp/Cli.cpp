#include "mcp/Cli.h"
#include "mcp/Automation.h"
#include <fstream>
#include <iostream>
#include <mfapi.h>

namespace sf {
namespace {
std::atomic<bool> consoleStopping = false;
BOOL WINAPI consoleControl(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
        consoleStopping = true;
        return TRUE;
    }
    return FALSE;
}
} // namespace
int runCli(const std::vector<std::string> &arguments, const std::filesystem::path &root) {
    using J = cymcp::Json;
    auto print = [](const J &value, bool diagnostic = false) {
        auto text = value.dump() + "\n";
        size_t offset = 0;
        while (offset < text.size()) {
            DWORD written = 0;
            if (!WriteFile(GetStdHandle(diagnostic ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE),
                           text.data() + offset, DWORD(std::min<size_t>(32768, text.size() - offset)),
                           &written, nullptr) ||
                !written)
                break;
            offset += written;
        }
    };
    bool runtime = false;
    int resultCode = 0;
    try {
        std::string command = "help", name, payload = "{}", argsFile, project, save, config, connect, info;
        bool writes = false, compact = false, confirm = false;
        int port = 0;
        bool commandSeen = false;
        for (const auto &argument : arguments) {
            if (argument.starts_with("--") && argument.ends_with("="))
                throw cymcp::Error("InvalidArguments", "An option value is required");
            if (argument == "--allow-changes")
                writes = true;
            else if (argument == "--confirm-discard")
                confirm = true;
            else if (argument == "--compact")
                compact = true;
            else if (argument == "--warp")
                SetEnvironmentVariableW(L"SF_FORCE_WARP", L"1");
            else if (argument.starts_with("--json="))
                payload = argument.substr(7);
            else if (argument.starts_with("--args="))
                argsFile = argument.substr(7);
            else if (argument.starts_with("--project="))
                project = argument.substr(10);
            else if (argument.starts_with("--save="))
                save = argument.substr(7);
            else if (argument.starts_with("--automation-config="))
                config = argument.substr(20);
            else if (argument.starts_with("--connect="))
                connect = argument.substr(10);
            else if (argument.starts_with("--info="))
                info = argument.substr(7);
            else if (argument.starts_with("--port=")) {
                auto value = argument.substr(7);
                if (value.empty() || value.size() > 5 ||
                    value.find_first_not_of("0123456789") != std::string::npos)
                    throw cymcp::Error("InvalidArguments", "Port must be an integer from 0 to 65535");
                port = std::stoi(value);
                if (port > 65535)
                    throw cymcp::Error("InvalidArguments", "Port must be an integer from 0 to 65535");
            } else if (argument == "--help" || argument == "-h")
                command = "help", commandSeen = true;
            else if (!commandSeen)
                command = argument, commandSeen = true;
            else if (name.empty() && !argument.starts_with("-"))
                name = argument;
            else
                throw cymcp::Error("InvalidArguments", "Unknown CLI argument");
        }
        if (command == "help") {
            print({{"ok", true},
                   {"usage",
                    "ShaderixFrameCLI help | list | describe NAME | invoke NAME --json=JSON | mcp | serve"},
                   {"options",
                    {"--project=FILE", "--save=FILE", "--args=JSON_FILE", "--allow-changes",
                     "--confirm-discard", "--connect=PRIVATE_DESCRIPTOR", "--info=PRIVATE_DESCRIPTOR",
                     "--automation-config=FILE", "--compact", "--warp", "--port=0"}},
                   {"notes", "Use ShaderixFrame.exe --cli with the same arguments. A connected call uses the "
                             "open instance's permissions. Standalone mutations require --allow-changes; use "
                             "--save for durable project changes."}});
            return 0;
        }
        if (command != "list" && command != "describe" && command != "invoke" && command != "mcp" &&
            command != "serve")
            throw cymcp::Error("InvalidArguments", "Unknown command; use help");
        if ((command == "invoke" || command == "describe") && name.empty())
            throw cymcp::Error("InvalidArguments", "An operation name is required");
        if (command != "invoke" && command != "describe" && !name.empty())
            throw cymcp::Error("InvalidArguments", "This command does not accept an operation name");
        if (!argsFile.empty()) {
            std::ifstream file(sx::Utf8Path(argsFile), std::ios::binary);
            if (!file)
                throw cymcp::Error("FileError", "Cannot read arguments file");
            payload.assign(std::istreambuf_iterator<char>(file), {});
        }
        J args = cymcp::parse_json(payload, cymcp::provider_message_limit);
        if (!connect.empty()) {
            if (command == "mcp" || command == "serve" || !project.empty() || !save.empty() || writes ||
                confirm)
                throw cymcp::Error("InvalidArguments",
                                   "Connected mode uses only the instance state and permissions");
            std::ifstream file(sx::Utf8Path(connect), std::ios::binary);
            if (!file)
                throw cymcp::Error("FileError", "Cannot read private connection descriptor");
            auto descriptor = cymcp::parse_json(std::string(std::istreambuf_iterator<char>(file), {}));
            auto url = descriptor.at("url").get<std::string>();
            auto parsed = cymcp::parse_url(url);
            if (!parsed.loopback || parsed.scheme != "http")
                throw cymcp::Error("InvalidArguments", "Instance descriptor must use loopback HTTP");
            cymcp::HttpRequest auth;
            auth.url = url + "/v1";
            auth.method = "POST";
            auth.body = "{\"method\":\"describe\"}";
            auth.headers = {{"Authorization", "Bearer " + descriptor.at("token").get<std::string>()},
                            {"Content-Type", "application/json"}};
            auto authentication = cymcp::HttpClient().perform(auth);
            if (authentication.status == 401 || authentication.status == 403)
                throw cymcp::Error("AuthenticationRequired",
                                   "The instance rejected the connection credential");
            cymcp::McpClient client(url + "/mcp", descriptor.at("token").get<std::string>());
            client.connect();
            if (command == "list")
                print({{"ok", true}, {"operations", client.list_tools()}});
            else if (command == "describe")
                print({{"ok", true}, {"operation", client.get_tool(name)}});
            else {
                auto response = client.call_tool(name, args);
                if (response.value("isError", false)) {
                    auto text = response.at("content").at(0).value("text", std::string{});
                    auto detail = J::parse(text, nullptr, false);
                    if (detail.is_object() && detail.contains("code"))
                        throw cymcp::Error(detail.at("code"), detail.value("message", std::string{}));
                    throw cymcp::Error("OperationFailed", text);
                }
                print({{"ok", true}, {"result", response.value("structuredContent", response)}});
            }
            return 0;
        }
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        MFStartup(MF_VERSION);
        runtime = true;
        {
            Session session;
            if (!project.empty()) {
                session.project = Project::load(project);
                session.projectPath = project;
            }
            if (command == "invoke" || command == "mcp" || command == "serve") {
                session.renderer.initialize();
                session.renderer.library.scan(root);
            }
            Automation automation(session, AutomationOptions::load(sx::Utf8Path(config)));
            automation.allowChanges(writes);
            if (confirm)
                automation.confirmDiscard();
            if (command == "list")
                print({{"ok", true}, {"operations", automation.registry()->describe()}});
            else if (command == "describe")
                print({{"ok", true}, {"operation", automation.registry()->describe(name)}});
            else if (command == "invoke") {
                auto value = automation.invoke(name, args);
                // A standalone process owns its jobs: retain it until all exports finish.
                for (;;) {
                    bool active = false;
                    for (const auto &job : session.exports.list())
                        active = active || job.active();
                    if (!active)
                        break;
                    session.pollJobs();
                    Sleep(10);
                }
                for (const auto &job : session.exports.list())
                    if (job.state == ExportState::Failed)
                        throw cymcp::Error("ExportFailed",
                                           "Background export failed; inspect the export destination");
                if (!save.empty()) {
                    if (!writes)
                        throw cymcp::Error("AccessDenied", "--save requires --allow-changes");
                    automation.invoke("save_project", {{"path", save}});
                }
                print({{"ok", true}, {"result", value}});
            } else {
                if (command == "serve") {
                    auto actual = automation.startHttp(port, sx::Utf8Path(info));
                    print({{"ok", true},
                           {"port", actual},
                           {"connectionFile", cymcp::path_utf8(automation.connectionFile())}});
                }
                std::atomic<bool> done = false;
                std::exception_ptr failure;
                std::thread input;
                if (command == "mcp")
                    input = std::thread([&] {
                        try {
                            automation.serveStdio(compact);
                        } catch (...) {
                            failure = std::current_exception();
                        }
                        done = true;
                    });
                consoleStopping = false;
                SetConsoleCtrlHandler(consoleControl, TRUE);
                try {
                    while (!done && !consoleStopping) {
                        automation.pump();
                        session.pollJobs();
                        Sleep(5);
                    }
                } catch (...) {
                    automation.stop();
                    if (input.joinable()) {
                        CancelSynchronousIo(input.native_handle());
                        input.join();
                    }
                    SetConsoleCtrlHandler(consoleControl, FALSE);
                    throw;
                }
                if (input.joinable() && consoleStopping)
                    CancelSynchronousIo(input.native_handle());
                automation.stop();
                if (input.joinable())
                    input.join();
                if (failure)
                    std::rethrow_exception(failure);
                SetConsoleCtrlHandler(consoleControl, FALSE);
            }
        }
    } catch (const std::exception &error) {
        auto sdk = dynamic_cast<const cymcp::Error *>(&error);
        auto code = sdk ? sdk->code : "OperationFailed";
        resultCode = code == "AccessDenied" || code == "ConfirmationRequired" ? 3
                     : code == "InvalidArguments" || code == "InvalidJson" || code == "InvalidSchema" ||
                             code == "ValidationError" || code == "InvalidParameters" ||
                             code == "PathRequired"
                         ? 2
                     : code == "AuthenticationRequired" || code == "Unauthorized" ? 4
                                                                                  : 1;
        J failure = {{"ok", false},
                     {"error", {{"code", code}, {"message", cymcp::redact_error_message(error.what())}}}};
        print(failure);
        print(failure, true);
    }
    if (runtime) {
        MFShutdown();
        CoUninitialize();
    }
    return resultCode;
}
} // namespace sf

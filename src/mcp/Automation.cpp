#include "mcp/Automation.h"
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace sf {
using J = cymcp::Json;
cymcp::LocalAccessProfile AutomationOptions::parseProfile(const std::string &name) {
    if (name == "free") return cymcp::LocalAccessProfile::free;
    if (name == "local_controlled") return cymcp::LocalAccessProfile::local_controlled;
    if (name == "token_only") return cymcp::LocalAccessProfile::token_only;
    if (name == "protected" || name == "protected_access") return cymcp::LocalAccessProfile::protected_access;
    throw cymcp::Error("InvalidArguments", "Unknown MCP access profile");
}
const char *AutomationOptions::profileName(cymcp::LocalAccessProfile profile) {
    switch (profile) {
    case cymcp::LocalAccessProfile::free: return "free";
    case cymcp::LocalAccessProfile::local_controlled: return "local_controlled";
    case cymcp::LocalAccessProfile::token_only: return "token_only";
    default: return "protected";
    }
}
void AutomationOptions::saveProfile(cymcp::LocalAccessProfile profile) const {
    auto path = configurationFile.empty() ? privateDirectory / "automation.json" : configurationFile;
    J data = J::object();
    if (std::filesystem::exists(path)) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw cymcp::Error("FileError", "Cannot read automation configuration");
        data = cymcp::parse_json(std::string(std::istreambuf_iterator<char>(input), {}));
        if (!data.is_object()) throw cymcp::Error("InvalidOptions", "Configuration must be an object");
    }
    data["access_profile"] = profileName(profile);
    if (!std::filesystem::exists(path.parent_path())) cymcp::private_directory(path.parent_path());
    auto temporary = path;
    temporary += L"." + sx::Widen(cymcp::random_token(16)) + L".pending";
    try {
        auto text = data.dump(2) + "\n";
        auto stream = cymcp::open_private_output_file(temporary);
        if (fwrite(text.data(), 1, text.size(), stream.get()) != text.size() || fflush(stream.get()))
            throw cymcp::Error("FileError", "Cannot save MCP access profile");
        stream.reset();
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw cymcp::Error("FileError", "Cannot publish MCP access profile");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
AutomationOptions AutomationOptions::load(const std::filesystem::path &file) {
    AutomationOptions result;
    wchar_t local[32768]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768))
        result.privateDirectory = std::filesystem::path(local) / L"ShaderixFrame" / L"Assistant";
    else
        result.privateDirectory = std::filesystem::temp_directory_path() / L"ShaderixFrame-Assistant";
    result.configurationFile = std::filesystem::absolute(file.empty() ? result.privateDirectory / "automation.json" : file);
    if (file.empty() && !std::filesystem::exists(result.configurationFile))
        return result;
    std::ifstream stream(result.configurationFile, std::ios::binary);
    if (!stream)
        throw cymcp::Error("FileError", "Cannot read automation configuration");
    std::string text((std::istreambuf_iterator<char>(stream)), {});
    auto data = cymcp::parse_json(text);
    if (!data.is_object()) throw cymcp::Error("InvalidOptions", "Configuration must be an object");
    if (data.contains("access_profile")) {
        result.accessProfile = parseProfile(data.at("access_profile").get<std::string>());
        result.explicitProfile = true;
    }
    result.compact = data.value("compact", result.compact);
    result.spool = data.value("disk_spooling", result.spool);
    result.notifications = data.value("notifications", result.notifications);
    result.metrics = data.value("performance", result.metrics);
    result.inlineBytes = data.value("inline_bytes", result.inlineBytes);
    result.chunkBytes = data.value("chunk_bytes", result.chunkBytes);
    result.memoryBytes = data.value("memory_bytes", result.memoryBytes);
    result.diskBytes = data.value("disk_bytes", result.diskBytes);
    result.retentionSeconds = data.value("retention_seconds", result.retentionSeconds);
    result.cacheSeconds = data.value("cache_seconds", result.cacheSeconds);
    if (data.contains("private_directory"))
        result.privateDirectory = sx::Utf8Path(data.at("private_directory").get<std::string>());
    result.remoteServers = data.value("remote_servers", J::array());
    if (result.retentionSeconds < 1 || result.cacheSeconds < 0 || result.chunkBytes < 1024 ||
        result.chunkBytes > 1024 * 1024 || result.memoryBytes < result.chunkBytes ||
        result.inlineBytes < 1024 || result.inlineBytes > 1024 * 1024 ||
        result.diskBytes < result.chunkBytes || !result.remoteServers.is_array())
        throw cymcp::Error("InvalidOptions", "Invalid automation quotas or retention");
    return result;
}
namespace {
bool readOnly(const std::string &name) {
    return name.starts_with("get_") || name.starts_with("list_") || name.starts_with("inspect_") ||
           name.starts_with("validate_") || name == "status" || name == "export_status" ||
           name == "mask_tracking_status" || name == "media_status";
}
J native(const Json &data) {
    return cymcp::parse_json(data.Dump(0), cymcp::provider_message_limit);
}
Json legacy(const J &data) {
    Json result;
    if (!Json::Parse(data.dump(), result))
        throw cymcp::Error("InvalidJson", "Cannot convert application arguments");
    return result;
}
// Use the actual inherited Windows handles even when the executable has a GUI subsystem.
struct LegacyInitialization {
    std::mutex mutex;
    std::map<std::string, std::string> versions;
};
class InputBuffer : public std::streambuf {
    LegacyInitialization &legacy;
    std::string pending, ready;
    int_type underflow() override {
        size_t newline;
        while ((newline = pending.find('\n')) == std::string::npos) {
            char data[4096];
            DWORD count = 0;
            if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), data, sizeof(data), &count, nullptr) || !count)
                return traits_type::eof();
            pending.append(data, count);
            if (pending.size() > cymcp::provider_message_limit)
                throw cymcp::Error("MessageTooLarge", "MCP request exceeds the memory guard");
        }
        ready = pending.substr(0, newline + 1);
        pending.erase(0, newline + 1);
        auto request = J::parse(ready, nullptr, false);
        if (request.is_object() && request.value("method", J()) == "initialize" && request.contains("id") &&
            request.value("params", J::object()).is_object()) {
            auto version = request["params"].value("protocolVersion", std::string{});
            if (version == "2025-03-26" || version == "2024-11-05") {
                {
                    std::lock_guard lock(legacy.mutex);
                    legacy.versions[request["id"].dump()] = version;
                }
                request["params"]["protocolVersion"] = "2025-06-18";
                ready = request.dump() + "\n";
            }
        }
        setg(ready.data(), ready.data(), ready.data() + ready.size());
        return traits_type::to_int_type(*gptr());
    }

  public:
    explicit InputBuffer(LegacyInitialization &state) : legacy(state) {}
};
class OutputBuffer : public std::streambuf {
    LegacyInitialization &legacy;
    std::string pending;
    std::streamsize write(const char *data, std::streamsize size) {
        std::streamsize total = 0;
        while (total < size) {
            DWORD count = 0;
            if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), data + total,
                           DWORD(std::min<std::streamsize>(size - total, 1024 * 1024)), &count, nullptr) ||
                !count)
                break;
            total += count;
        }
        return total;
    }
    std::streamsize xsputn(const char *data, std::streamsize size) override {
        std::lock_guard lock(legacy.mutex);
        if (legacy.versions.empty() && pending.empty())
            return write(data, size);
        pending.append(data, size);
        size_t newline;
        while ((newline = pending.find('\n')) != std::string::npos) {
            auto line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            auto response = J::parse(line, nullptr, false);
            if (response.is_object() && response.contains("id")) {
                auto found = legacy.versions.find(response["id"].dump());
                if (found != legacy.versions.end()) {
                    if (response.contains("result") && response["result"].contains("protocolVersion")) {
                        response["result"]["protocolVersion"] = found->second;
                        line = response.dump();
                    }
                    legacy.versions.erase(found);
                }
            }
            line += '\n';
            if (write(line.data(), line.size()) != line.size())
                return 0;
        }
        return size;
    }
    int_type overflow(int_type c) override {
        if (traits_type::eq_int_type(c, traits_type::eof()))
            return traits_type::not_eof(c);
        char value = traits_type::to_char_type(c);
        return xsputn(&value, 1) == 1 ? c : traits_type::eof();
    }

  public:
    explicit OutputBuffer(LegacyInitialization &state) : legacy(state) {}
};
} // namespace
struct Automation::Impl {
    Session &session;
    Protocol protocol;
    AutomationOptions options;
    std::thread::id owner = std::this_thread::get_id();
    std::atomic<bool> writes = false, stopping = false;
    bool discardOnce = false, discardAlways = false;
    std::shared_ptr<cymcp::Registry> operations;
    std::shared_ptr<cymcp::ResourceRegistry> resources;
    std::shared_ptr<cymcp::ChangeFeed> changes;
    std::shared_ptr<cymcp::PerformanceMonitor> monitor;
    std::unique_ptr<cymcp::ToolGateway> gateway;
    std::unique_ptr<cymcp::McpServer> server, compactServer;
    std::unique_ptr<cymcp::LocalHttpServer> http;
    std::filesystem::path descriptor;
    std::string descriptorFingerprint, httpUrl;
    std::mutex mutex, remoteMutex;
    std::deque<std::function<void()>> pending;
    std::future<void> remoteFuture;
    cymcp::Cancellation remoteCancellation;
    std::string remoteMessage = "Not connected";
    std::string snapshot;
    std::map<std::string, std::function<J(const J &, const cymcp::CallContext &)>> hostHandlers;
    cymcp::Clock::time_point nextSnapshot{};
    Impl(Session &s, AutomationOptions o) : session(s), protocol(s), options(std::move(o)) {
        const auto access = cymcp::local_access_settings(options.accessProfile);
        operations = std::make_shared<cymcp::Registry>(
            access.control_operations ? cymcp::Policy([this](const auto &, const std::string &name, const auto &) {
                return !stopping &&
                               (operations->describe(name).at("annotations").value("readOnlyHint", false) ||
                                writes)
                           ? cymcp::Decision::allow
                           : cymcp::Decision::deny;
            }) : cymcp::Policy::allow_all());
        if (options.notifications) {
            changes = std::make_shared<cymcp::ChangeFeed>();
            operations->set_change_feed(changes);
        }
        if (options.metrics) {
            cymcp::PerformanceOptions p;
            p.jsonl_file =
                options.privateDirectory / ("performance-" + std::to_string(GetCurrentProcessId()) + "-" +
                                            cymcp::random_token(16) + ".jsonl");
            monitor = std::make_shared<cymcp::PerformanceMonitor>(p);
            operations->set_performance_monitor(monitor);
        }
        cymcp::ResourceOptions ro;
        ro.chunk_bytes = options.chunkBytes;
        ro.max_stored_bytes = options.memoryBytes;
        ro.result_ttl = std::chrono::seconds(options.retentionSeconds);
        ro.max_disk_bytes = options.diskBytes;
        if (options.spool)
            ro.disk_directory = options.privateDirectory / "results";
        resources = std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all(), ro);
        resources->set_change_feed(changes);
        for (const auto &tool : native(protocol.tools())) {
            cymcp::Operation op;
            op.name = tool.at("name");
            op.description = tool.at("description");
            op.input_schema = tool.at("inputSchema");
            op.output_schema = {
                {"type", "object"}, {"properties", {{"ok", {{"type", "boolean"}}}}}, {"required", {"ok"}}};
            if (op.name == "status") {
                for (auto key : {"project"})
                    op.output_schema["properties"][key] = {{"type", "object"}};
                for (auto key : {"playing", "dirty"})
                    op.output_schema["properties"][key] = {{"type", "boolean"}};
                op.output_schema["properties"]["time"] = {{"type", "number"}};
                op.output_schema["properties"]["version"] = {{"type", "string"}};
                op.output_schema["properties"]["exports"] = {{"type", "array"},
                                                             {"items", {{"type", "object"}}}};
                op.output_schema["properties"]["selected"] = {{"type", "integer"}};
                op.output_schema["properties"]["selection"] = {{"type", "array"},
                                                               {"items", {{"type", "integer"}}}};
                op.output_schema["required"] = {"ok",      "project", "time",     "playing",  "dirty",
                                                "version", "exports", "selected", "selection"};
            } else if (op.name == "get_history") {
                for (auto key : {"past", "future"})
                    op.output_schema["properties"][key] = {{"type", "array"},
                                                           {"items", {{"type", "string"}}}};
                op.output_schema["required"] = {"ok", "past", "future"};
            }
            op.read_only = readOnly(op.name);
            auto name = op.name;
            op.handler = [this, name](const J &args, const cymcp::CallContext &context) {
                return dispatch(name, args, context);
            };
            operations->add(std::move(op));
        }
        cymcp::ToolGatewayOptions go;
        go.inline_bytes = options.inlineBytes;
        go.resources = ro;
        gateway = std::make_unique<cymcp::ToolGateway>(operations, go, resources);
        cymcp::McpServerOptions mo;
        mo.page_size = 1000;
        mo.cache_ttl = std::chrono::seconds(options.cacheSeconds);
        mo.resources = resources;
        mo.changes = changes;
        mo.performance = monitor;
        server = std::make_unique<cymcp::McpServer>(*operations, "ShaderixFrame", mo);
        compactServer = std::make_unique<cymcp::McpServer>(*gateway->tools(), "ShaderixFrame compact", mo);
        refresh();
    }
    J business(const std::string &name, const J &args, const cymcp::CallContext &context) {
        context.cancellation.check();
        if (stopping ||
            (cymcp::local_access_settings(options.accessProfile).control_operations &&
             !operations->describe(name).at("annotations").value("readOnlyHint", false) && !writes))
            throw cymcp::Error("AccessDenied", "Application changes are disabled");
        if (cymcp::local_access_settings(options.accessProfile).control_operations && session.dirty &&
            (name == "new_project" || name == "open_project" || name == "restore_version")) {
            if (!discardOnce && !discardAlways)
                throw cymcp::Error("ConfirmationRequired",
                                   "Save the current project or explicitly allow replacing unsaved work once "
                                   "in Assistant settings; standalone CLI supports --confirm-discard");
            discardOnce = false;
        }
        J result;
        try {
            auto host = hostHandlers.find(name);
            result = host != hostHandlers.end() ? host->second(args, context)
                                                : native(protocol.execute(name, legacy(args)));
        } catch (const cymcp::Error &) {
            throw;
        } catch (const std::exception &e) {
            throw cymcp::Error("OperationFailed", e.what());
        }
        refresh();
        return result;
    }
    J dispatch(std::string name, J args, cymcp::CallContext context) {
        if (std::this_thread::get_id() == owner)
            return business(name, args, context);
        auto promise = std::make_shared<std::promise<J>>();
        auto future = promise->get_future();
        {
            std::lock_guard lock(mutex);
            if (pending.size() >= 128 || stopping)
                throw cymcp::Error("Busy", "Editor call queue unavailable");
            pending.push_back([this, promise, name = std::move(name), args = std::move(args), context] {
                try {
                    promise->set_value(business(name, args, context));
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        while (future.wait_for(std::chrono::milliseconds(25)) != std::future_status::ready) {
            context.cancellation.check();
            if (stopping)
                throw cymcp::Error("Cancelled", "Editor is closing");
        }
        return future.get();
    }
    void refresh() {
        auto text = session.project.json().Dump(2);
        if (text == snapshot)
            return;
        snapshot = std::move(text);
        auto bytes = std::make_shared<const std::string>(snapshot);
        cymcp::Resource r;
        r.uri = "shaderixframe://project";
        r.name = "Current sequence";
        r.description = "Immutable current project JSON; follow resource chunks for complete content";
        r.mime_type = "application/json";
        r.size = bytes->size();
        r.read = [bytes](size_t offset, size_t length, const auto &context) {
            context.cancellation.check();
            return bytes->substr(std::min(offset, bytes->size()), length);
        };
        if (resources->readable(r.uri))
            resources->replace(std::move(r));
        else
            resources->add(std::move(r));
    }
};
Automation::Automation(Session &s, AutomationOptions o) : impl(std::make_unique<Impl>(s, std::move(o))) {}
Automation::~Automation() {
    stop();
}
void Automation::addHostOperation(cymcp::Operation operation) {
    if (std::this_thread::get_id() != impl->owner)
        throw cymcp::Error("WrongThread", "Register host operations on the editor thread");
    auto name = operation.name;
    if (!(name.starts_with("ui_") || name.starts_with("get_ui_") || name.starts_with("list_ui_")))
        throw cymcp::Error("InvalidOperation", "Editor extensions require a UI namespace");
    if (impl->hostHandlers.contains(name))
        throw cymcp::Error("DuplicateOperation", "Host operation already registered");
    auto handler = operation.handler;
    operation.handler = [this, name](const J &args, const cymcp::CallContext &context) {
        return impl->dispatch(name, args, context);
    };
    impl->operations->add(std::move(operation));
    impl->hostHandlers.emplace(name, std::move(handler));
}
void Automation::removeHostOperation(const std::string &name) {
    if (std::this_thread::get_id() != impl->owner)
        throw cymcp::Error("WrongThread", "Remove host operations on the editor thread");
    if (impl->hostHandlers.erase(name))
        impl->operations->remove(name);
}
std::shared_ptr<cymcp::Registry> Automation::registry() const {
    return impl->operations;
}
std::shared_ptr<const cymcp::Registry> Automation::chatTools() const {
    return impl->options.compact ? impl->gateway->tools() : impl->operations;
}
std::shared_ptr<cymcp::PerformanceMonitor> Automation::performance() const {
    return impl->monitor;
}
void Automation::allowChanges(bool value) {
    impl->writes = value;
}
bool Automation::changesAllowed() const {
    return !controlsOperations() || impl->writes;
}
cymcp::LocalAccessProfile Automation::accessProfile() const { return impl->options.accessProfile; }
bool Automation::controlsOperations() const {
    return cymcp::local_access_settings(accessProfile()).control_operations;
}
std::string Automation::connectionUrl() const { return impl->http ? impl->httpUrl : ""; }
void Automation::confirmDiscard(bool persistent) {
    impl->discardOnce = true;
    impl->discardAlways = persistent;
}
void Automation::pump() {
    if (std::this_thread::get_id() != impl->owner)
        throw cymcp::Error("WrongThread", "Pump must run on the editor thread");
    std::deque<std::function<void()>> work;
    {
        std::lock_guard lock(impl->mutex);
        work.swap(impl->pending);
    }
    for (auto &call : work)
        call();
    if (cymcp::Clock::now() >= impl->nextSnapshot) {
        impl->refresh();
        impl->nextSnapshot = cymcp::Clock::now() + std::chrono::milliseconds(500);
    }
}
void Automation::stop() {
    if (!impl || impl->stopping.exchange(true))
        return;
    impl->remoteCancellation.cancel();
    impl->server->cancel_all();
    impl->compactServer->cancel_all();
    {
        std::lock_guard lock(impl->mutex);
        impl->pending.clear();
    }
    stopHttp();
    if (impl->remoteFuture.valid())
        impl->remoteFuture.wait();
}
void Automation::serveStdio(bool compact) {
    LegacyInitialization compatibility;
    InputBuffer input(compatibility);
    OutputBuffer output(compatibility);
    std::istream in(&input);
    std::ostream out(&output);
    (compact ? impl->compactServer : impl->server)->serve_stdio(in, out, 4);
}
int Automation::startHttp(int port, const std::filesystem::path &file) {
    if (impl->http)
        throw cymcp::Error("Busy", "Local MCP server is already running");
    const auto access = cymcp::local_access_settings(accessProfile());
    auto token = access.require_token ? cymcp::random_token() : std::string{};
    auto handler = [this](const J &request, const cymcp::CallContext &context) {
            return request.contains("jsonrpc") ? impl->server->handle(request, context)
                                               : cymcp::dispatch_app(*impl->operations, request, context);
        };
    auto http = access.require_token ? std::make_unique<cymcp::LocalHttpServer>(token, handler)
                                    : std::make_unique<cymcp::LocalHttpServer>(cymcp::LocalHttpServer::UnauthenticatedLoopback{}, handler);
    auto actual = http->start(port);
    auto path = file.empty() ? impl->options.privateDirectory /
                                   ("instance-" + std::to_string(GetCurrentProcessId()) + ".json")
                             : file;
    path = std::filesystem::absolute(path);
    if (std::filesystem::exists(path))
        throw cymcp::Error("FileExists", "Connection descriptor already exists; choose a new private file");
    if (!std::filesystem::exists(path.parent_path()))
        cymcp::private_directory(path.parent_path());
    auto url = "http://127.0.0.1:" + std::to_string(actual);
    J connection = {{"url", url}, {"access_profile", AutomationOptions::profileName(accessProfile())},
                    {"pid", GetCurrentProcessId()}};
    if (access.require_token) connection["token"] = token;
    auto text = connection.dump();
    auto temporary = path;
    temporary += L"." + sx::Widen(cymcp::random_token(16)) + L".pending";
    if (temporary.parent_path() != path.parent_path())
        throw cymcp::Error("UnsafePath", "Invalid connection descriptor staging path");
    try {
        auto stream = cymcp::open_private_output_file(temporary);
        if (fwrite(text.data(), 1, text.size(), stream.get()) != text.size() || fflush(stream.get()) != 0)
            throw cymcp::Error("FileError", "Cannot write private connection descriptor");
        stream.reset();
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
            throw cymcp::Error("FileError",
                               "Cannot publish connection descriptor without replacing an existing file");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    impl->descriptor = path;
    impl->descriptorFingerprint = cymcp::sha256(text);
    impl->httpUrl = url + "/mcp";
    impl->http = std::move(http);
    return actual;
}
void Automation::stopHttp() {
    impl->http.reset();
    if (!impl->descriptor.empty()) {
        std::error_code error;
        try {
            if (cymcp::sha256_file(impl->descriptor) == impl->descriptorFingerprint)
                std::filesystem::remove(impl->descriptor, error);
        } catch (...) {
        }
        impl->descriptor.clear();
        impl->descriptorFingerprint.clear();
    }
}
const std::filesystem::path &Automation::connectionFile() const {
    return impl->descriptor;
}
J Automation::invoke(const std::string &name, const J &args, const cymcp::CallContext &c) {
    return impl->operations->invoke(name, args, c);
}
void Automation::discoverRemoteTools() {
    if (impl->remoteFuture.valid() &&
        impl->remoteFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        throw cymcp::Error("Busy", "Remote discovery is running");
    impl->remoteFuture = std::async(std::launch::async, [this] {
        try {
            size_t added = 0;
            for (const auto &configured : impl->options.remoteServers) {
                auto alias = configured.at("id").get<std::string>();
                auto allowed = configured.value("allow_tools", std::vector<std::string>{});
                auto uris = configured.value("allow_resources", std::vector<std::string>{});
                if (allowed.empty() && uris.empty())
                    continue;
                cymcp::McpClientOptions options;
                options.max_cache_ttl = std::chrono::seconds(impl->options.cacheSeconds);
                options.performance = impl->monitor;
                std::shared_ptr<cymcp::McpClient> client;
                if (configured.contains("url")) {
                    auto env = configured.value("token_env", std::string{});
                    const char *token = env.empty() ? nullptr : std::getenv(env.c_str());
                    client = std::make_shared<cymcp::McpClient>(configured.at("url").get<std::string>(),
                                                                token ? token : "", options);
                } else {
                    cymcp::ProcessOptions process;
                    process.executable = sx::Utf8Path(configured.at("executable").get<std::string>());
                    process.arguments = configured.value("arguments", std::vector<std::string>{});
                    client = std::make_shared<cymcp::McpClient>(process, options);
                }
                client->connect(impl->remoteCancellation);
                if (impl->options.notifications) {
                    try {
                        client->start_notifications(
                            {{"toolsListChanged", true}, {"resourcesListChanged", true}},
                            impl->remoteCancellation);
                    } catch (const cymcp::Error &e) {
                        if (e.code != "Unsupported")
                            throw;
                    }
                }
                for (const auto &tool : client->list_tools(impl->remoteCancellation)) {
                    auto name = tool.at("name").get<std::string>();
                    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
                        continue;
                    cymcp::Operation op;
                    op.name = "remote." + alias + "." + name;
                    op.description = tool.value("description", name);
                    op.input_schema = tool.at("inputSchema");
                    op.output_schema = {
                        {"type", "object"},
                        {"properties",
                         {{"remote",
                           {{"type", "object"},
                            {"properties",
                             {{"structuredContent", tool.value("outputSchema", J{{"type", "object"}})}}}}}}},
                        {"required", {"remote"}}};
                    op.read_only = tool.value("annotations", J::object()).value("readOnlyHint", false);
                    auto mutating = !op.read_only;
                    op.handler = [client, name, mutating](const J &args, const cymcp::CallContext &c) {
                        auto result = client->call_tool(name, args, c.cancellation);
                        if (mutating)
                            client->clear_cache();
                        if (result.value("isError", false))
                            throw cymcp::Error("RemoteToolError", result.value("content", J::array()).dump());
                        return J{{"remote", result}};
                    };
                    // Remote names cannot claim local read-only prefixes. Their mutation permission is
                    // explicit.
                    auto nameKey = op.name;
                    bool exists = false;
                    try {
                        impl->operations->describe(nameKey);
                        exists = true;
                    } catch (const cymcp::Error &) {
                    }
                    if (exists)
                        impl->operations->replace(std::move(op));
                    else
                        impl->operations->add(std::move(op));
                    ++added;
                }
                if (!uris.empty()) {
                    auto accepted = std::make_shared<std::set<std::string>>(uris.begin(), uris.end());
                    auto guard = std::make_shared<std::mutex>();
                    cymcp::Operation op;
                    op.name = "remote." + alias + ".read_resource";
                    op.description = "Read only the host-selected remote resources and chunk links returned "
                                     "by those resources";
                    op.read_only = true;
                    op.input_schema = {{"type", "object"},
                                       {"properties", {{"uri", {{"type", "string"}}}}},
                                       {"required", {"uri"}},
                                       {"additionalProperties", false}};
                    op.output_schema = {{"type", "object"}};
                    op.handler = [client, accepted, guard](const J &args, const cymcp::CallContext &context) {
                        auto uri = args.at("uri").get<std::string>();
                        {
                            std::lock_guard lock(*guard);
                            if (!accepted->contains(uri))
                                throw cymcp::Error("AccessDenied",
                                                   "Remote resource is not selected by the host");
                        }
                        auto result = client->read_resource(uri, context.cancellation);
                        {
                            std::lock_guard lock(*guard);
                            auto add = [&](const J &object, const char *key) {
                                if (object.is_object() && object.contains(key) && object[key].is_string() &&
                                    accepted->size() < 10000)
                                    accepted->insert(object[key].get<std::string>());
                            };
                            if (result.contains("_meta"))
                                add(result["_meta"].value("com.cymcp/chunk", J::object()), "nextUri");
                            for (const auto &content : result.value("contents", J::array())) {
                                auto text = content.value("text", std::string{});
                                auto manifest = J::parse(text, nullptr, false);
                                if (manifest.is_object() &&
                                    manifest.value("type", std::string{}) == "cymcp.resource.v1")
                                    add(manifest, "firstUri");
                            }
                        }
                        return J{{"resource", result}};
                    };
                    bool exists = false;
                    try {
                        impl->operations->describe(op.name);
                        exists = true;
                    } catch (const cymcp::Error &) {
                    }
                    if (exists)
                        impl->operations->replace(std::move(op));
                    else
                        impl->operations->add(std::move(op));
                    ++added;
                }
            }
            std::lock_guard lock(impl->remoteMutex);
            impl->remoteMessage = std::to_string(added) + " remote tools connected";
        } catch (const std::exception &e) {
            std::lock_guard lock(impl->remoteMutex);
            impl->remoteMessage = cymcp::redact_error_message(e.what());
        }
    });
}
std::string Automation::remoteStatus() const {
    std::lock_guard lock(impl->remoteMutex);
    return impl->remoteMessage;
}
} // namespace sf

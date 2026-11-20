#include "mcp/Automation.h"
#include <cymcp/chat_ui.hpp>
#include <cymcp/context.hpp>
#include <fstream>
#include <iostream>

using J = cymcp::Json;
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Pass fixture base URL and private test directory");
        auto directory = std::filesystem::absolute(sx::Utf8Path(argv[2]));
        std::filesystem::create_directories(directory);
        auto migrationFile = directory / "profile-migration.json";
        {
            std::ofstream output(migrationFile);
            output << R"({"compact":false,"custom":{"preserve":42}})";
        }
        auto migration = sf::AutomationOptions::load(migrationFile);
        require(migration.accessProfile == cymcp::LocalAccessProfile::free && !migration.explicitProfile,
                "Configurations without an explicit profile migrate to Free");
        for (auto profile : {cymcp::LocalAccessProfile::free, cymcp::LocalAccessProfile::local_controlled,
                             cymcp::LocalAccessProfile::token_only, cymcp::LocalAccessProfile::protected_access}) {
            migration.saveProfile(profile);
            auto reloaded = sf::AutomationOptions::load(migrationFile);
            require(reloaded.explicitProfile && reloaded.accessProfile == profile && !reloaded.compact,
                    "An explicit profile persists without changing unrelated configuration");
            std::ifstream input(migrationFile);
            require(J::parse(input).at("custom").at("preserve") == 42, "Preserve unknown configuration fields");
        }
        sf::Session session;
        auto options = sf::AutomationOptions::load();
        options.accessProfile = cymcp::LocalAccessProfile::protected_access;
        options.privateDirectory = directory;
        options.metrics = true;
        options.spool = true;
        options.inlineBytes = 1024;
        options.chunkBytes = 32768;
        options.retentionSeconds = 5;
        auto remoteOps = std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
        remoteOps->add({"echo",
                        "Echo the supplied fixture value",
                        {{"type", "object"}},
                        {{"type", "object"}},
                        true,
                        [](const J &a, const auto &) { return a; }});
        remoteOps->add({"fail",
                        "Report a remote business failure",
                        {{"type", "object"}},
                        {{"type", "object"}},
                        true,
                        [](const J &, const auto &) -> J {
                            throw cymcp::Error("FixtureError", "Remote error fixture");
                        }});
        auto remoteChanges = std::make_shared<cymcp::ChangeFeed>();
        remoteOps->set_change_feed(remoteChanges);
        auto remoteResources = std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all());
        remoteResources->set_change_feed(remoteChanges);
        auto resource = [](std::string text) {
            cymcp::Resource r;
            r.uri = "fixture://text";
            r.name = "Fixture resource";
            r.size = text.size();
            r.cache_ttl = std::chrono::seconds(30);
            r.read = [text](size_t offset, size_t length, const auto &) {
                return text.substr(offset, length);
            };
            return r;
        };
        remoteResources->add(resource("before"));
        cymcp::McpServerOptions remoteOptions;
        remoteOptions.resources = remoteResources;
        remoteOptions.changes = remoteChanges;
        cymcp::McpServer remote(*remoteOps, "Remote test fixture", remoteOptions);
        auto token = cymcp::random_token();
        _putenv_s("SF_ASSISTANT_TEST_TOKEN", token.c_str());
        cymcp::LocalHttpServer remoteHttp(
            token, [&](const J &request, const auto &context) { return remote.handle(request, context); });
        auto port = remoteHttp.start();
        options.remoteServers = J::array({{{"id", "fixture"},
                                           {"url", "http://127.0.0.1:" + std::to_string(port) + "/mcp"},
                                           {"token_env", "SF_ASSISTANT_TEST_TOKEN"},
                                           {"allow_tools", {"echo", "fail"}},
                                           {"allow_resources", {"fixture://text"}}}});
        sf::Automation automation(session, options);
        auto existing = directory / ("existing-" + cymcp::random_token(16) + ".json");
        {
            std::ofstream f(existing);
            f << "Preserve this existing file";
        }
        bool fileRefused = false;
        try {
            automation.startHttp(0, existing);
        } catch (const cymcp::Error &) {
            fileRefused = true;
        }
        {
            std::ifstream f(existing);
            std::string text((std::istreambuf_iterator<char>(f)), {});
            require(fileRefused && text == "Preserve this existing file",
                    "Never overwrite an existing descriptor destination");
        }
        std::filesystem::remove(existing);
        auto owned = directory / ("owned-" + cymcp::random_token(16) + ".json");
        automation.startHttp(0, owned);
        {
            std::ofstream f(owned);
            f << "User replacement";
        }
        automation.stopHttp();
        require(std::filesystem::exists(owned), "Do not remove a descriptor changed by its owner");
        std::filesystem::remove(owned);
        require(automation.registry()->size() == 163, "Preserve the original 163 tools");
        bool denied = false;
        try {
            automation.invoke("set_sequence", {{"name", "Forbidden"}});
        } catch (const cymcp::Error &e) {
            denied = e.code == "AccessDenied";
        }
        require(denied, "Writes must be denied by default");
        automation.allowChanges(true);
        auto pending = std::async(std::launch::async, [&] {
            return automation.invoke("set_sequence", {{"name", "Thread dispatched"}});
        });
        while (pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            automation.pump();
            Sleep(2);
        }
        pending.get();
        require(session.project.name == "Thread dispatched",
                "Worker calls must edit the actual project on its owner thread");
        automation.invoke("undo", J::object());
        require(session.project.name != "Thread dispatched", "Real Undo must be preserved");
        automation.invoke("set_sequence", {{"name", "Sensitive body sentinel"}});
        auto grep =
            automation.chatTools()->invoke("cy_resources_grep", {{"text", "Sensitive body sentinel"}}).dump();
        require(grep.find("Sensitive body sentinel") != std::string::npos && grep.size() < 4096,
                "Native grep must return targeted text rather than the whole project");
        denied = false;
        try {
            automation.invoke("new_project", J::object());
        } catch (const cymcp::Error &e) {
            denied = e.code == "ConfirmationRequired";
        }
        require(denied, "Replacing unsaved work needs explicit confirmation");
        automation.confirmDiscard();
        automation.invoke("new_project", {{"name", "Fixture"}});
        auto tools = automation.chatTools();
        require(tools->size() == 5, "Compact discovery must retain five SDK gateway tools");
        auto schema = tools->invoke("cy_tools_describe", {{"name", "set_sequence"}});
        require(schema.contains("outputSchema"), "Discover full output schemas");
        session.project.name = std::string(1200000, 'X');
        automation.pump();
        cymcp::CallContext owner;
        owner.principal = "fixture-owner";
        auto large = tools->invoke("cy_tools_call", {{"name", "status"}}, owner);
        require(large.contains("resource"), "Large results must produce a complete resource handle");
        auto handle = large.at("resource").at("uri").get<std::string>();
        auto manifest = J::parse(tools->invoke("cy_resources_read", {{"uri", handle}}, owner)
                                     .at("contents")
                                     .at(0)
                                     .at("text")
                                     .get<std::string>());
        auto next = manifest.at("firstUri").get<std::string>();
        std::string full;
        while (!next.empty()) {
            auto chunk = tools->invoke("cy_resources_read", {{"uri", next}}, owner);
            full += chunk.at("contents").at(0).at("text").get<std::string>();
            next = chunk.at("_meta").at("com.cymcp/chunk").value("nextUri", std::string{});
        }
        require(J::parse(full).at("project").at("name").get<std::string>().size() == 1200000,
                "Recover the entire large result, not its preview");
        require(!std::filesystem::is_empty(directory / "results"),
                "Large results must use the opted-in private disk spool");
        denied = false;
        try {
            tools->invoke("cy_resources_read", {{"uri", handle}});
        } catch (const cymcp::Error &) {
            denied = true;
        }
        require(denied, "Result handles are principal-private");
        auto metrics = automation.performance()->snapshot().dump();
        require(metrics.find("Sensitive body sentinel") == std::string::npos &&
                    metrics.find("fixture-owner") == std::string::npos,
                "Metrics must contain metadata only");
        automation.discoverRemoteTools();
        auto deadline = cymcp::Clock::now() + std::chrono::seconds(15);
        while (automation.registry()->size() < 166 && cymcp::Clock::now() < deadline) {
            automation.pump();
            Sleep(5);
        }
        require(automation.registry()->size() == 166, automation.remoteStatus().c_str());
        automation.allowChanges(false);
        require(automation.invoke("remote.fixture.echo", {{"value", 42}})
                        .at("remote")
                        .at("structuredContent")
                        .at("value") == 42,
                "Explicitly permitted remote read works with local writes disabled");
        bool remoteError = false;
        try {
            automation.invoke("remote.fixture.fail", J::object());
        } catch (const cymcp::Error &e) {
            remoteError = e.code == "RemoteToolError";
        }
        require(remoteError, "Remote business errors must reach the caller");
        auto remoteRead = [&] {
            return automation.invoke("remote.fixture.read_resource", {{"uri", "fixture://text"}})
                .at("resource")
                .at("contents")
                .at(0)
                .at("text")
                .get<std::string>();
        };
        require(remoteRead() == "before", "Read a selected remote resource");
        remoteResources->replace(resource("after"));
        deadline = cymcp::Clock::now() + std::chrono::seconds(4);
        std::string latest;
        while ((latest = remoteRead()) != "after" && cymcp::Clock::now() < deadline)
            Sleep(20);
        require(latest == "after",
                "Notification must invalidate the cached resource before its 30-second TTL");
        automation.allowChanges(true);
        cymcp::ChatController chat;
        cymcp::ChatConnection connection;
        connection.provider_id = "openai-compatible";
        connection.model = "fixture-model";
        connection.provider.settings = {{"base_url", argv[1]}, {"deferred_tools", false}};
        connection.provider.tools = automation.chatTools();
        connection.history_directory = directory / "conversations";
        connection.history_id = "integration";
        // A complete compact search/describe/call turn must fit without splitting tool pairs.
        connection.context.max_bytes = 16384;
        auto wait = [&] {
            auto until = cymcp::Clock::now() + std::chrono::seconds(20);
            while (chat.state().busy && cymcp::Clock::now() < until) {
                automation.pump();
                chat.poll();
                Sleep(3);
            }
            require(!chat.state().busy, "Chat timeout");
        };
        chat.open(connection);
        wait();
        require(chat.state().connected, chat.state().error_message.c_str());
        chat.send("change sequence");
        bool partial = false;
        deadline = cymcp::Clock::now() + std::chrono::seconds(20);
        while (chat.state().busy && cymcp::Clock::now() < deadline) {
            automation.pump();
            chat.poll();
            for (const auto &m : chat.state().messages)
                if (m.role == "assistant" && !m.markdown.empty() && chat.state().busy)
                    partial = true;
            Sleep(3);
        }
        require(!chat.state().busy, "Streamed tool turn timeout");
        require(chat.state().error_code.empty(), chat.state().error_message.c_str());
        require(partial, "Streamed text must reach the controller before turn completion");
        require(session.project.name == "Assistant fixture edit",
                "An API tool call must edit the real application");
        require(chat.state().error_code.empty(), chat.state().error_message.c_str());
        chat.send("remote fixture");
        wait();
        require(chat.state().error_code.empty(), chat.state().error_message.c_str());
        auto transcript = chat.state().messages;
        bool remoteSuccessInChat = false, remoteErrorInChat = false;
        for (const auto &message : transcript) {
            remoteSuccessInChat |= message.markdown.find("chat-remote-value") != std::string::npos;
            remoteErrorInChat |= message.markdown.find("RemoteToolError") != std::string::npos;
        }
        require(remoteSuccessInChat && remoteErrorInChat,
                "Remote tool output and errors must reach the actual chat transcript");
        chat.send("cancel fixture");
        deadline = cymcp::Clock::now() + std::chrono::seconds(10);
        while (chat.state().busy && cymcp::Clock::now() < deadline) {
            automation.pump();
            chat.poll();
            bool text = false;
            for (const auto &m : chat.state().messages)
                text = text || (m.role == "assistant" && m.markdown == "Starting");
            if (text) {
                chat.cancel();
                break;
            }
            Sleep(3);
        }
        wait();
        require(chat.state().phase == "interrupted", "Chat cancellation must preserve the partial turn");
        for (int turn = 0; turn < 8; ++turn) {
            chat.send("long fixture " + std::string(8000, 'x'));
            wait();
            require(chat.state().error_code.empty(), chat.state().error_message.c_str());
        }
        chat.disconnect();
        wait();
        connection.restore = true;
        chat.open(connection);
        wait();
        require(chat.state().messages.size() >= 20,
                "Restore complete history after context reduction, including cancelled turn, without replay");
        chat.send("error fixture");
        wait();
        require(chat.state().phase == "failed" && !chat.state().error_code.empty(),
                "Connection/generation errors must be visible");
        chat.disconnect();
        wait();
        // The default Free profile must use the same chat path without granting write access.
        sf::AutomationOptions freeOptions;
        sf::Session freeSession;
        sf::Automation freeAutomation(freeSession, freeOptions);
        connection.restore = false;
        connection.history_directory.clear();
        connection.history_id.clear();
        connection.provider.tools = freeAutomation.chatTools();
        chat.open(connection);
        wait();
        require(chat.state().connected, chat.state().error_message.c_str());
        chat.send("change sequence");
        deadline = cymcp::Clock::now() + std::chrono::seconds(20);
        while (chat.state().busy && cymcp::Clock::now() < deadline) {
            freeAutomation.pump();
            chat.poll();
            Sleep(3);
        }
        require(!chat.state().busy && chat.state().error_code.empty(), "Free chat tool turn failed");
        require(freeSession.project.name == "Assistant fixture edit",
                "Free chat edits the actual sequence without allowChanges or confirmation");
        chat.disconnect();
        wait();
        Sleep(5100);
        denied = false;
        try {
            tools->invoke("cy_resources_read", {{"uri", handle}}, owner);
        } catch (const cymcp::Error &) {
            denied = true;
        }
        require(denied, "Expired results fail without re-executing the command");
        automation.stop();
        std::cout << "Assistant integration: owner thread, policy, undo, confirmation, compact schemas, "
                     "complete private chunks, TTL, metadata, remote tools/errors, API streaming/tool "
                     "call/cancellation/history/error passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

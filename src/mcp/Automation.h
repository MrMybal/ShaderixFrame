#pragma once
#include "mcp/Protocol.h"
#include <cymcp/changes.hpp>
#include <cymcp/mcp.hpp>
#include <cymcp/performance.hpp>

namespace sf {
struct AutomationOptions {
    cymcp::LocalAccessProfile accessProfile = cymcp::LocalAccessProfile::free;
    std::filesystem::path configurationFile;
    bool explicitProfile = false;
    bool compact = true, spool = false, notifications = true, metrics = false;
    size_t inlineBytes = 16384, chunkBytes = 32768, memoryBytes = 64 * 1024 * 1024;
    size_t diskBytes = 512 * 1024 * 1024;
    int retentionSeconds = 600, cacheSeconds = 30;
    std::filesystem::path privateDirectory;
    cymcp::Json remoteServers = cymcp::Json::array();
    static AutomationOptions load(const std::filesystem::path & = {});
    static cymcp::LocalAccessProfile parseProfile(const std::string &);
    static const char *profileName(cymcp::LocalAccessProfile);
    void saveProfile(cymcp::LocalAccessProfile) const;
};
class Automation {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Automation(Session &, AutomationOptions = AutomationOptions::load());
    ~Automation();
    std::shared_ptr<cymcp::Registry> registry() const;
    std::shared_ptr<const cymcp::Registry> chatTools() const;
    std::shared_ptr<cymcp::PerformanceMonitor> performance() const;
    void allowChanges(bool);
    bool changesAllowed() const;
    cymcp::LocalAccessProfile accessProfile() const;
    bool controlsOperations() const;
    std::string connectionUrl() const;
    void confirmDiscard(bool persistent = false);
    void pump();
    void stop();
    void serveStdio(bool compact = false);
    int startHttp(int port = 0, const std::filesystem::path &descriptor = {});
    void stopHttp();
    const std::filesystem::path &connectionFile() const;
    cymcp::Json invoke(const std::string &, const cymcp::Json &, const cymcp::CallContext & = {});
    void discoverRemoteTools();
    std::string remoteStatus() const;
    // Host-owned operations use the same policy, schemas and owning-thread queue.
    void addHostOperation(cymcp::Operation);
    void removeHostOperation(const std::string &);
};
} // namespace sf

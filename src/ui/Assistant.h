#pragma once
#include "mcp/Automation.h"
#include <cymcp/chat_ui.hpp>
namespace sf {
class Assistant {
    Automation &automation;
    std::shared_ptr<cymcp::ProviderRegistry> providers = std::make_shared<cymcp::ProviderRegistry>();
    cymcp::ChatController controller{providers};
    std::unique_ptr<cymcp::ChatPanel> panel;
    std::future<std::vector<cymcp::DiscoveredConnection>> detection;
    std::vector<cymcp::DiscoveredConnection> connections;
    cymcp::Cancellation detectionCancellation;
    cymcp::ProviderConfig selectedConfig;
    cymcp::Json selectedModels = cymcp::Json::array();
    std::string provider = "ollama", model, baseUrl, executable, arguments = "[]", keyEnvironment,
                historyId = "default", error;
    AutomationOptions options;
    bool saveHistory = false, settings = true, appTools = true;
    int contextKiB = 0;
    void connect(bool restore);

  public:
    Assistant(Automation &, AutomationOptions);
    ~Assistant();
    void draw();
    void poll();
};
} // namespace sf

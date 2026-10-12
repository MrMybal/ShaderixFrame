#pragma once
#include "core/Project.h"
#include <cydaw/plugins/PluginIsolation.h>
#include <windows.h>
namespace sf {
class IsolatedAudio {
    HANDLE file = INVALID_HANDLE_VALUE, mapping = nullptr, processHandle = nullptr;
    std::filesystem::path ipc;
    cydaw::plugins::SharedPluginAudio *shared = nullptr;
    uint64_t sequence = 0;
    std::string kind;
    std::map<unsigned, float> overflowControls;
    void await(uint64_t value, bool command = false, unsigned timeout = 2000);
    void command(cydaw::plugins::SharedPluginCommand command);

  public:
    IsolatedAudio(const std::string &kind, const std::string &path);
    ~IsolatedAudio();
    IsolatedAudio(const IsolatedAudio &) = delete;
    std::map<std::string, float> parameters();
    std::map<std::string, std::string> parameterNames() const;
    std::string state();
    void restoreState(const std::string &base64);
    void setParameter(unsigned index, float value);
    void showEditor(bool visible);
    void process(double time, unsigned frames, std::vector<float> &data,
                 const std::map<std::string, Parameter> &params);
    unsigned latency() const;
    void terminateForTest();
};
bool externalAudio(const std::string &id);
bool externalParameter(const std::string &key);
} // namespace sf

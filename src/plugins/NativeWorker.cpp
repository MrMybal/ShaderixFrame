#include "ShaderixFramePlugin.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cydaw/plugins/PluginIsolation.h>
#include <windows.h>
using namespace cydaw::plugins;
int wmain(int argc, wchar_t **argv) {
    const wchar_t *plugin = nullptr, *ipc = nullptr;
    for (int i = 1; i + 1 < argc; ++i) {
        if (wcscmp(argv[i], L"--realtime-plugin") == 0)
            plugin = argv[++i];
        else if (wcscmp(argv[i], L"--shared-file") == 0)
            ipc = argv[++i];
    }
    if (!plugin || !ipc)
        return 2;
    HANDLE file = CreateFileW(ipc, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return 3;
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    auto *shared = mapping ? static_cast<SharedPluginAudio *>(
                                 MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedPluginAudio)))
                           : nullptr;
    if (!shared) {
        if (mapping)
            CloseHandle(mapping);
        CloseHandle(file);
        return 4;
    }
    auto error = [&](const char *message) {
        snprintf(shared->statusMessage, sizeof(shared->statusMessage), "%s", message);
        std::atomic_ref(shared->state).store(uint32_t(SharedPluginState::error), std::memory_order_release);
    };
    HMODULE module = nullptr;
    void *instance = nullptr;
    const SfPluginV1 *api = nullptr;
    int result = 0;
    try {
        if (shared->magic != workerProtocolMagic || shared->protocolVersion != workerProtocolVersion)
            throw "Incompatible IPC protocol";
        module = LoadLibraryExW(plugin, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module)
            throw "Cannot read plugin DLL";
        auto entry = reinterpret_cast<SfPluginEntry>(GetProcAddress(module, "shaderixframe_plugin_v1"));
        if (!entry)
            throw "Native SDK entry point is missing";
        api = entry();
        if (!api || api->version != 1 || api->size != sizeof(SfPluginV1) || api->parameter_count > 64 ||
            api->latency_samples > 480000 || !api->create || !api->destroy || !api->process ||
            !api->set_parameter || !api->parameter_name || !api->parameter_default)
            throw "ABI native incompatible";
        instance = api->create(48000, 2048);
        if (!instance)
            throw "Plugin creation failed";
        shared->latencySamples = api->latency_samples;
        shared->parameterCount = api->parameter_count;
        for (uint32_t i = 0; i < api->parameter_count; ++i) {
            auto &p = shared->parameters[i];
            p.index = i;
            p.flags = parameterAutomatable;
            p.value = std::clamp(api->parameter_default(i), 0.f, 1.f);
            snprintf(p.name, sizeof(p.name), "%s", api->parameter_name(i));
            api->set_parameter(instance, i, p.value);
        }
        std::atomic_ref(shared->state).store(uint32_t(SharedPluginState::ready), std::memory_order_release);
        uint64_t automation = 0;
        while (!std::atomic_ref(shared->shutdownRequested).load(std::memory_order_acquire)) {
            bool worked = false;
            auto request = std::atomic_ref(shared->requestSequence).load(std::memory_order_acquire);
            if (request != std::atomic_ref(shared->responseSequence).load(std::memory_order_relaxed)) {
                worked = true;
                auto current = std::atomic_ref(shared->automationSequence).load(std::memory_order_acquire);
                if (current != automation) {
                    for (uint32_t i = 0; i < std::min(shared->automationCount, 64u); ++i) {
                        auto &p = shared->automation[i];
                        if (p.index >= api->parameter_count)
                            throw "Unknown plugin parameter";
                        api->set_parameter(instance, p.index, std::clamp(p.value, 0.f, 1.f));
                        shared->parameters[p.index].value = p.value;
                    }
                    automation = current;
                }
                if (!shared->frameCount || shared->frameCount > 2048)
                    throw "Invalid audio block";
                if (!api->process(instance, shared->input, shared->input + 2048, shared->output,
                                  shared->output + 2048, shared->frameCount))
                    throw "Plugin processing failed";
                shared->outputFrameCount = shared->frameCount;
                std::atomic_ref(shared->responseSequence).store(request, std::memory_order_release);
            }
            auto command = std::atomic_ref(shared->commandSequence).load(std::memory_order_acquire);
            if (command != std::atomic_ref(shared->commandResponseSequence).load(std::memory_order_relaxed)) {
                worked = true;
                shared->commandResult =
                    shared->command == uint32_t(SharedPluginCommand::refreshParameters) ? 0 : 1;
                if (shared->command == uint32_t(SharedPluginCommand::setParameter) &&
                    shared->commandParameterIndex < api->parameter_count &&
                    std::isfinite(shared->commandParameterValue) && shared->commandParameterValue >= 0 &&
                    shared->commandParameterValue <= 1) {
                    api->set_parameter(instance, shared->commandParameterIndex,
                                       shared->commandParameterValue);
                    shared->parameters[shared->commandParameterIndex].value = shared->commandParameterValue;
                    shared->commandResult = 0;
                }
                if (shared->commandResult)
                    snprintf(shared->commandStatus, sizeof(shared->commandStatus),
                             "Unsupported native command");
                std::atomic_ref(shared->commandResponseSequence).store(command, std::memory_order_release);
            }
            std::atomic_ref(shared->heartbeat).fetch_add(1, std::memory_order_relaxed);
            Sleep(worked ? 0 : 1);
        }
    } catch (const char *message) {
        error(message);
        result = 5;
    } catch (...) {
        error("Exception in native plugin");
        result = 6;
    }
    if (api && instance)
        api->destroy(instance);
    if (module)
        FreeLibrary(module);
    UnmapViewOfFile(shared);
    CloseHandle(mapping);
    CloseHandle(file);
    return result;
}

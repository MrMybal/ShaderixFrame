#include "plugins/IsolatedAudio.h"
#include "core/Binary.h"
#include "media/Media.h"
#include <atomic>
namespace sf {
using namespace cydaw::plugins;
bool externalAudio(const std::string &id) {
    return id == "vst3" || id == "native";
}
bool externalParameter(const std::string &key) {
    if (key.size() < 2 || key[0] != 'p' || key.size() > 4 ||
        key.find_first_not_of("0123456789", 1) != std::string::npos)
        return false;
    return std::stoi(key.substr(1)) < 512;
}
void IsolatedAudio::await(uint64_t value, bool command, unsigned timeout) {
    auto deadline = GetTickCount64() + timeout;
    auto &counter = command ? shared->commandResponseSequence : shared->responseSequence;
    while (std::atomic_ref(counter).load(std::memory_order_acquire) != value) {
        if (WaitForSingleObject(processHandle, 0) == WAIT_OBJECT_0)
            throw std::runtime_error("Le processus plugin a quitté pendant le traitement");
        if (std::atomic_ref(shared->state).load(std::memory_order_acquire) ==
            uint32_t(SharedPluginState::error))
            throw std::runtime_error(shared->statusMessage);
        if (GetTickCount64() > deadline)
            throw std::runtime_error("Le plugin isolé ne répond plus");
        Sleep(0);
    }
}
IsolatedAudio::IsolatedAudio(const std::string &kind, const std::string &path) : kind(kind) {
    try {
        if (!externalAudio(kind) || path.empty() || !std::filesystem::exists(sx::Utf8Path(path)))
            throw std::runtime_error("Plugin natif/VST3 introuvable");
        wchar_t temporary[MAX_PATH], name[MAX_PATH];
        GetTempPathW(MAX_PATH, temporary);
        if (!GetTempFileNameW(temporary, L"sfp", 0, name))
            throw std::runtime_error("Transport plugin indisponible");
        ipc = name;
        file = CreateFileW(ipc.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Création IPC échouée");
        mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, sizeof(SharedPluginAudio), nullptr);
        if (!mapping)
            throw std::runtime_error("Mapping plugin échoué");
        shared = static_cast<SharedPluginAudio *>(
            MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedPluginAudio)));
        if (!shared)
            throw std::runtime_error("Vue IPC échouée");
        memset(shared, 0, sizeof(*shared));
        shared->magic = workerProtocolMagic;
        shared->protocolVersion = workerProtocolVersion;
        shared->inputChannels = shared->outputChannels = 2;
        std::atomic_ref(shared->state).store(uint32_t(SharedPluginState::loading), std::memory_order_release);
        wchar_t executable[32768];
        GetModuleFileNameW(nullptr, executable, 32768);
        auto helper = std::filesystem::path(executable).parent_path() /
                      (kind == "native" ? L"ShaderixFramePluginWorker.exe" : L"CyDAWPluginWorker.exe");
        auto quote = [](const std::wstring &value) {
            if (value.find(L'"') != std::wstring::npos)
                throw std::runtime_error("Chemin plugin invalide");
            return L"\"" + value + L"\"";
        };
        std::wstring command = quote(helper.wstring()) + L" --realtime-plugin " + quote(sx::Widen(path)) +
                               L" --shared-file " + quote(ipc.wstring()) +
                               L" --sample-rate 48000 --block-size 2048 --channels 2";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &startup, &process))
            throw std::runtime_error("Lancement de l'hôte plugin échoué");
        processHandle = process.hProcess;
        CloseHandle(process.hThread);
        auto deadline = GetTickCount64() + 15000;
        while (std::atomic_ref(shared->state).load(std::memory_order_acquire) ==
               uint32_t(SharedPluginState::loading)) {
            if (WaitForSingleObject(processHandle, 0) == WAIT_OBJECT_0 || GetTickCount64() > deadline)
                throw std::runtime_error("Chargement du plugin isolé échoué");
            Sleep(1);
        }
        if (std::atomic_ref(shared->state).load(std::memory_order_acquire) !=
            uint32_t(SharedPluginState::ready))
            throw std::runtime_error(shared->statusMessage);
    } catch (...) {
        if (shared)
            UnmapViewOfFile(shared);
        if (mapping)
            CloseHandle(mapping);
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
        if (processHandle) {
            TerminateProcess(processHandle, 1);
            CloseHandle(processHandle);
        }
        if (!ipc.empty())
            DeleteFileW(ipc.c_str());
        throw;
    }
}
IsolatedAudio::~IsolatedAudio() {
    if (shared)
        std::atomic_ref(shared->shutdownRequested).store(1, std::memory_order_release);
    if (processHandle) {
        if (WaitForSingleObject(processHandle, 750) == WAIT_TIMEOUT) {
            TerminateProcess(processHandle, 1);
            WaitForSingleObject(processHandle, 750);
        }
        CloseHandle(processHandle);
    }
    if (shared)
        UnmapViewOfFile(shared);
    if (mapping)
        CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    if (!ipc.empty())
        DeleteFileW(ipc.c_str());
}
std::map<std::string, float> IsolatedAudio::parameters() {
    command(SharedPluginCommand::refreshParameters);
    std::map<std::string, float> result;
    if (shared->parameterCount > maximumSharedPluginParameters)
        throw std::runtime_error("Métadonnées plugin trop grandes");
    for (uint32_t i = 0; i < shared->parameterCount; ++i) {
        auto p = shared->parameters[i];
        if (p.index >= maximumSharedPluginParameters || !std::isfinite(p.value) || p.value < 0 || p.value > 1)
            throw std::runtime_error("Métadonnées plugin invalides");
        result["p" + std::to_string(p.index)] = p.value;
    }
    return result;
}
void IsolatedAudio::command(SharedPluginCommand operation) {
    shared->command = uint32_t(operation);
    auto value = std::atomic_ref(shared->commandSequence).fetch_add(1, std::memory_order_release) + 1;
    await(value, true,
          operation == SharedPluginCommand::showEditor || operation == SharedPluginCommand::getState ||
                  operation == SharedPluginCommand::setState
              ? 15000
              : 2000);
    if (shared->commandResult)
        throw std::runtime_error(shared->commandStatus);
}
std::map<std::string, std::string> IsolatedAudio::parameterNames() const {
    std::map<std::string, std::string> result;
    for (unsigned i = 0; i < std::min(shared->parameterCount, maximumSharedPluginParameters); ++i) {
        const auto &p = shared->parameters[i];
        result["p" + std::to_string(p.index)] = std::string(p.name, strnlen_s(p.name, sizeof(p.name)));
    }
    return result;
}
std::string IsolatedAudio::state() {
    if (kind != "vst3")
        return {};
    command(SharedPluginCommand::getState);
    if (shared->stateBytes > maximumSharedPluginStateBytes)
        throw std::runtime_error("État VST3 supérieur à 4 Mio");
    return encodeBase64({shared->stateData, shared->stateBytes});
}
void IsolatedAudio::restoreState(const std::string &text) {
    if (text.empty())
        return;
    if (kind != "vst3")
        throw std::runtime_error("État opaque disponible pour VST3 uniquement");
    auto bytes = decodeBase64(text);
    memcpy(shared->stateData, bytes.data(), bytes.size());
    shared->stateBytes = unsigned(bytes.size());
    command(SharedPluginCommand::setState);
    overflowControls.clear();
}
void IsolatedAudio::setParameter(unsigned index, float value) {
    if (index >= maximumSharedPluginParameters || !std::isfinite(value) || value < 0 || value > 1)
        throw std::runtime_error("Contrôle plugin hors plage");
    shared->commandParameterIndex = index;
    shared->commandParameterValue = value;
    command(SharedPluginCommand::setParameter);
}
void IsolatedAudio::showEditor(bool visible) {
    if (kind != "vst3")
        throw std::runtime_error("Cette API native n'expose pas de fenêtre");
    command(visible ? SharedPluginCommand::showEditor : SharedPluginCommand::hideEditor);
}
void IsolatedAudio::process(double time, unsigned frames, std::vector<float> &data,
                            const std::map<std::string, Parameter> &params) {
    for (unsigned offset = 0; offset < frames; offset += 2048) {
        auto count = std::min(2048u, frames - offset);
        shared->automationCount = 0;
        for (auto &[key, param] : params) {
            if (!externalParameter(key))
                throw std::runtime_error("Paramètre plugin invalide");
            if (shared->automationCount >= maximumSharedAutomationParameters) {
                unsigned index = unsigned(std::stoul(key.substr(1)));
                float value = std::clamp(param.at(time + offset / 48000.), 0.f, 1.f);
                if (!overflowControls.contains(index) || overflowControls[index] != value) {
                    setParameter(index, value);
                    overflowControls[index] = value;
                }
                continue;
            }
            auto &automation = shared->automation[shared->automationCount++];
            automation.index = std::stoul(key.substr(1));
            automation.value = param.at(time + offset / 48000.);
        }
        std::atomic_ref(shared->automationSequence).fetch_add(1, std::memory_order_release);
        for (unsigned frame = 0; frame < count; ++frame)
            for (unsigned channel = 0; channel < 2; ++channel)
                shared->input[sharedAudioIndex(channel, frame)] = data[(offset + frame) * 2 + channel];
        shared->frameCount = count;
        std::atomic_ref(shared->requestSequence).store(++sequence, std::memory_order_release);
        await(sequence);
        if (shared->outputFrameCount != count)
            throw std::runtime_error("Taille de sortie du plugin invalide");
        for (unsigned frame = 0; frame < count; ++frame)
            for (unsigned channel = 0; channel < 2; ++channel) {
                float sample = shared->output[sharedAudioIndex(channel, frame)];
                if (!std::isfinite(sample))
                    throw std::runtime_error("Plugin : sortie non finie");
                data[(offset + frame) * 2 + channel] = sample;
            }
    }
}
unsigned IsolatedAudio::latency() const {
    return std::atomic_ref(shared->latencySamples).load(std::memory_order_acquire);
}
void IsolatedAudio::terminateForTest() {
    TerminateProcess(processHandle, 91);
    WaitForSingleObject(processHandle, 1000);
}
} // namespace sf

#include "plugins/IsolatedOfx.h"
#include <atomic>
#include <set>
namespace sf {
std::string ofxBinary(const std::string &text) {
    auto path = std::filesystem::absolute(sx::Utf8Path(text)).lexically_normal();
    if (std::filesystem::is_directory(path)) {
        auto inner = path / L"Contents" / L"Win64";
        if (!std::filesystem::is_directory(inner))
            throw std::runtime_error("Bundle OFX Windows x64 requis");
        std::filesystem::path found;
        for (auto &item : std::filesystem::directory_iterator(inner))
            if (item.is_regular_file() && item.path().extension() == L".ofx") {
                if (!found.empty())
                    throw std::runtime_error("Plusieurs binaires OFX : choisir le fichier .ofx");
                found = item.path();
            }
        path = found;
    }
    if (path.empty() || !std::filesystem::is_regular_file(path) || path.extension() != L".ofx")
        throw std::runtime_error("Binaire .ofx Windows x64 introuvable");
    return sx::Narrow(path.wstring());
}
void IsolatedOfx::cleanup() {
    if (shared)
        std::atomic_ref(shared->quit).store(1, std::memory_order_release);
    if (process) {
        if (WaitForSingleObject(process, 750) == WAIT_TIMEOUT) {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, 750);
        }
        CloseHandle(process);
        process = nullptr;
    }
    if (shared) {
        UnmapViewOfFile(shared);
        shared = nullptr;
    }
    if (mapping) {
        CloseHandle(mapping);
        mapping = nullptr;
    }
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
    }
    if (!ipc.empty())
        DeleteFileW(ipc.c_str());
}
void IsolatedOfx::await(uint64_t value, unsigned timeout) {
    auto deadline = GetTickCount64() + timeout;
    while (value ? std::atomic_ref(shared->response).load(std::memory_order_acquire) != value
                 : std::atomic_ref(shared->ready).load(std::memory_order_acquire) == 0) {
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
            throw std::runtime_error("Le plugin OFX a quitté son processus");
        if (GetTickCount64() > deadline)
            throw std::runtime_error("Le plugin OFX ne répond plus");
        Sleep(1);
    }
    if ((value && shared->result) || (!value && shared->ready != 1))
        throw std::runtime_error(
            std::string("OFX : ") +
            std::string(shared->message, strnlen_s(shared->message, sizeof(shared->message))));
}
IsolatedOfx::IsolatedOfx(const std::string &path, const std::string &identifier, unsigned width,
                         unsigned height) {
    try {
        auto binary = ofxBinary(path);
        if (!width || !height || ofxBytes(width, height) > 512ull * 1024 * 1024 ||
            identifier.size() >= sizeof(shared->identifier))
            throw std::runtime_error("OFX : taille de transport hors limites (512 Mio)");
        wchar_t temp[MAX_PATH], name[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        if (!GetTempFileNameW(temp, L"sfo", 0, name))
            throw std::runtime_error("IPC OFX indisponible");
        ipc = name;
        file = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Fichier IPC OFX");
        auto bytes = ofxBytes(width, height);
        mapping =
            CreateFileMappingW(file, nullptr, PAGE_READWRITE, DWORD(bytes >> 32), DWORD(bytes), nullptr);
        if (!mapping)
            throw std::runtime_error("Mapping OFX");
        shared = static_cast<OfxShared *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size_t(bytes)));
        if (!shared)
            throw std::runtime_error("Vue IPC OFX");
        *shared = OfxShared{};
        shared->width = width;
        shared->height = height;
        memcpy(shared->identifier, identifier.c_str(), identifier.size() + 1);
        wchar_t exe[32768];
        GetModuleFileNameW(nullptr, exe, 32768);
        auto worker = std::filesystem::path(exe).parent_path() / L"ShaderixFrameOfxWorker.exe";
        auto quote = [](const std::wstring &s) {
            if (s.find(L'"') != std::wstring::npos)
                throw std::runtime_error("Chemin OFX invalide");
            return L"\"" + s + L"\"";
        };
        auto command =
            quote(worker.wstring()) + L" " + quote(ipc.wstring()) + L" " + quote(sx::Widen(binary));
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION info{};
        if (!CreateProcessW(worker.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &startup, &info))
            throw std::runtime_error("Lancement du worker OFX");
        process = info.hProcess;
        CloseHandle(info.hThread);
        await(0);
    } catch (...) {
        cleanup();
        throw;
    }
}
Json IsolatedOfx::metadata() const {
    Json result;
    if (!Json::Parse(std::string(shared->metadata, strnlen_s(shared->metadata, sizeof(shared->metadata))),
                     result) ||
        !result.IsObject())
        throw std::runtime_error("Métadonnées OFX invalides");
    return result;
}
FloatImage IsolatedOfx::render(const FloatImage &image, const Effect &effect, double time, double fps,
                               double duration) {
    if (image.width != shared->width || image.height != shared->height ||
        image.rgba.size() != size_t(image.width) * image.height * 4)
        throw std::runtime_error("Dimensions OFX incohérentes");
    auto *input = reinterpret_cast<float *>(shared + 1);
    memcpy(input, image.rgba.data(), image.rgba.size() * 4);
    Json values = Json::Object();
    for (auto &[key, p] : effect.params)
        values[key] = p.at(time);
    auto text = values.Dump(0);
    if (text.size() >= sizeof(shared->parameters))
        throw std::runtime_error("Paramètres OFX trop grands");
    memcpy(shared->parameters, text.c_str(), text.size() + 1);
    shared->time = time * fps;
    shared->fps = fps;
    shared->duration = duration * fps;
    std::atomic_ref(shared->request).store(++sequence, std::memory_order_release);
    await(sequence);
    FloatImage result = image;
    memcpy(result.rgba.data(), input + image.rgba.size(), image.rgba.size() * 4);
    for (float value : result.rgba)
        if (!std::isfinite(value) || std::abs(value) > 65504)
            throw std::runtime_error("OFX : sortie non finie ou hors plage half");
    return result;
}
void IsolatedOfx::terminateForTest() {
    TerminateProcess(process, 91);
    WaitForSingleObject(process, 1000);
}
void validateOfx(const Effect &effect) {
    if (effect.code.empty() || effect.graph["identifier"].S().empty() ||
        !effect.graph["parameters"].IsArray() || effect.graph["parameters"].Size() > 512)
        throw std::runtime_error("Description OFX invalide");
    std::set<std::string> seen;
    for (auto &p : effect.graph["parameters"].Elements()) {
        auto key = p["key"].S();
        if (!seen.insert(key).second || key.empty() || !std::isfinite(p["minimum"].Num()) ||
            !std::isfinite(p["maximum"].Num()) || p["minimum"].Num() > p["maximum"].Num())
            throw std::runtime_error("Contrôle OFX invalide");
    }
    for (auto &[key, value] : effect.params) {
        bool found = false;
        for (auto &p : effect.graph["parameters"].Elements())
            if (key == p["key"].S()) {
                found = true;
                auto check = [&](float v) {
                    if (!std::isfinite(v) || v < p["minimum"].Num() || v > p["maximum"].Num())
                        throw std::runtime_error("Paramètre OFX hors plage : " + key);
                };
                check(value.value);
                for (auto &k : value.keys)
                    check(k.value);
            }
        if (!found)
            throw std::runtime_error("Contrôle OFX inconnu");
    }
}
} // namespace sf

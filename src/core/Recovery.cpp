#include "core/Recovery.h"
#include "core/AtomicFile.h"
#include <fstream>

namespace sf {
static std::string processToken(HANDLE process) {
    FILETIME created{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exit, &kernel, &user))
        return {};
    return std::to_string((uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime);
}
static Json readRecovery(const std::filesystem::path &file) {
    std::string text, error;
    Json json;
    if (!sx::ReadTextFile(sx::Narrow(file.wstring()), text) || !Json::Parse(text, json, &error))
        throw std::runtime_error("Fichier de récupération illisible");
    Project::fromJson(json);
    if (!json["recovery"].IsObject())
        throw std::runtime_error("Métadonnées de récupération manquantes");
    return json;
}
void Recovery::rotate() {
    static std::atomic<unsigned> serial = 0;
    current = directory / (L"session-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(serial++) + L".sframe");
}
void Recovery::configure(std::filesystem::path path) {
    if (!path.empty())
        std::filesystem::create_directories(path);
    directory = std::move(path);
    if (enabled())
        rotate();
}
void Recovery::capture(const Project &p, const std::string &originalPath, double time) {
    if (!enabled())
        return;
    p.validate();
    auto json = p.json();
    auto &meta = json["recovery"];
    meta = Json::Object();
    meta["path"] = originalPath;
    meta["time"] = std::clamp(time, 0., p.duration);
    meta["processId"] = sx::u32(GetCurrentProcessId());
    meta["processToken"] = processToken(GetCurrentProcess());
    AtomicFile output(sx::Narrow(current.wstring()));
    {
        std::ofstream file(output.path(), std::ios::binary);
        file << json.Dump(2);
        file.flush();
        if (!file)
            throw std::runtime_error("Écriture de la récupération échouée");
    }
    output.commit();
}
void Recovery::preserve(const Project &p, const std::string &path, double time) {
    capture(p, path, time);
    if (enabled())
        rotate();
}
void Recovery::clear() {
    if (enabled())
        std::filesystem::remove(current);
}
std::vector<RecoveryEntry> Recovery::list() const {
    std::vector<RecoveryEntry> entries;
    if (!enabled())
        return entries;
    for (auto &file : std::filesystem::directory_iterator(directory)) {
        if (file.path().extension() != L".sframe")
            continue;
        try {
            auto json = readRecovery(file.path());
            auto &meta = json["recovery"];
            HANDLE process =
                OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, meta["processId"].U());
            bool active = process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT &&
                          processToken(process) == meta["processToken"].S();
            if (process)
                CloseHandle(process);
            if (!active)
                entries.push_back({file.path(), json["name"].S(), meta["path"].S(), meta["time"].Num()});
        } catch (...) {
            // Leave corrupt files on disk for manual inspection; never destroy a recovery silently.
        }
    }
    std::sort(entries.begin(), entries.end(), [](auto &a, auto &b) {
        return std::filesystem::last_write_time(a.file) > std::filesystem::last_write_time(b.file);
    });
    return entries;
}
Project Recovery::restore(const RecoveryEntry &entry) const {
    readRecovery(entry.file);
    return Project::load(sx::Narrow(entry.file.wstring()));
}
void Recovery::discard(const RecoveryEntry &entry) const {
    auto resolved = std::filesystem::weakly_canonical(entry.file);
    if (resolved.parent_path() != std::filesystem::weakly_canonical(directory) ||
        resolved.extension() != L".sframe")
        throw std::runtime_error("Chemin de récupération invalide");
    std::filesystem::remove(resolved);
}
} // namespace sf

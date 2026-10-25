#pragma once
#include "core/Common.h"
#include <atomic>
#include <windows.h>

namespace sf {
// Keep the destination untouched until all writers have successfully closed.
class AtomicFile {
    std::filesystem::path destination, temporary;

  public:
    explicit AtomicFile(const std::string &path) : destination(sx::Utf8Path(path)) {
        static std::atomic<unsigned> serial = 0;
        for (int attempt = 0; attempt < 100; ++attempt) {
            temporary = destination.parent_path() /
                        (destination.stem().wstring() + L".pending-" +
                         std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) +
                         L"-" + std::to_wstring(serial++) + destination.extension().wstring());
            HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                CloseHandle(file);
                return;
            }
            if (GetLastError() != ERROR_FILE_EXISTS)
                break;
        }
        throw std::runtime_error("Unable to create the temporary output file");
    }
    AtomicFile(const AtomicFile &) = delete;
    AtomicFile &operator=(const AtomicFile &) = delete;
    ~AtomicFile() {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
    }
    const std::filesystem::path &path() const { return temporary; }
    void commit() {
        if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Unable to finalize the output file");
    }
};
} // namespace sf

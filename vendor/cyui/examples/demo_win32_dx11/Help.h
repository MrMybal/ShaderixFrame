#pragma once
#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <string_view>

namespace demo_help {
// CyDoc's route contract: file URI #/locale/page[/section].
inline std::string encode(std::string_view text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            out += char(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
inline std::filesystem::path readerPath() {
    std::wstring executable(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
    if (!size || size >= executable.size()) return {};
    executable.resize(size);
    return std::filesystem::path(executable).parent_path() / L"Documentation" / L"Documentation.html";
}
inline std::wstring url(const std::filesystem::path& reader, std::string_view locale,
                        std::string_view page = "overview", std::string_view section = {}) {
    std::wstring uri(65536, L'\0');
    DWORD length = DWORD(uri.size());
    if (FAILED(UrlCreateFromPathW(std::filesystem::absolute(reader).c_str(), uri.data(), &length, 0))) return {};
    uri.resize(length);
    const std::string route = "#/" + encode(locale) + "/" + encode(page) +
        (section.empty() ? "" : "/" + encode(section));
    uri.append(route.begin(), route.end());
    return uri;
}
inline bool open(std::string_view locale, std::string_view page = "overview", std::string_view section = {}) {
    const auto reader = readerPath();
    std::error_code error;
    if (reader.empty() || !std::filesystem::is_regular_file(reader, error) ||
        !std::filesystem::is_regular_file(reader.parent_path() / L"DocData" / L"data.js", error) ||
        !std::filesystem::is_regular_file(reader.parent_path() / L"DocData" / L"viewer.js", error)) return false;
    const auto uri = url(reader, locale, page, section);
    return !uri.empty() && reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}
} // namespace demo_help

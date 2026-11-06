#include "ui/Help.h"
#include "core/Json.h"
#include "../../.CyDocBuild/vendor/cydoc/cydoc_link.hpp"
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <set>

namespace sf {
HelpDocumentation::HelpDocumentation(std::function<bool(const std::string &)> launch) : opener(std::move(launch)) {
    if (!opener) opener = [](const std::string &url) {
        auto wide = sx::Widen(url);
        return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    };
}
std::string HelpDocumentation::fileUrl(const std::filesystem::path &file) {
    auto path = std::filesystem::absolute(file).wstring();
    DWORD count = 4096;
    std::wstring result(count, L'\0');
    auto status = UrlCreateFromPathW(path.c_str(), result.data(), &count, 0);
    if (status == E_POINTER) {
        result.resize(count);
        status = UrlCreateFromPathW(path.c_str(), result.data(), &count, 0);
    }
    if (FAILED(status)) return {};
    result.resize(count);
    return sx::Narrow(result);
}
std::string HelpDocumentation::url(const std::filesystem::path &application, const std::string &requested,
                                   const std::string &language, const std::string &anchor, State state) {
    failure.clear();
    const auto root = application / "Documentation";
    std::string raw;
    sx::Json manifest;
    if (!std::filesystem::is_regular_file(root / "Documentation.html") ||
        !sx::ReadTextFile(sx::Narrow((root / "DocData/manifest.json").wstring()), raw) ||
        !sx::Json::Parse(raw, manifest) || manifest["schemaVersion"].I() != 1) {
        failure = "CyDoc help is unavailable. Restore the complete Documentation folder from the application package.";
        return {};
    }
    std::string page = requested.empty() ? "welcome" : requested, section = anchor;
    std::set<std::string> visited;
    while (manifest["aliases"].Has(page)) {
        if (!visited.insert(page).second) { failure = "Invalid help alias"; return {}; }
        auto value = manifest["aliases"][page].S();
        auto split = value.find('#');
        page = value.substr(0, split);
        if (section.empty() && split != std::string::npos) section = value.substr(split + 1);
    }
    std::string locale = language == "fr" ? "fr" : "en";
    const sx::Json *found = nullptr, *fallback = nullptr;
    for (const auto &candidate : manifest["pages"].Elements()) if (candidate["id"].S() == page) {
        if (candidate["locale"].S() == locale) found = &candidate;
        if (candidate["locale"].S() == "en") fallback = &candidate;
    }
    if (!found) found = fallback;
    if (!found) { failure = "Unknown help page: " + page; return {}; }
    if (!section.empty()) {
        bool exists = false;
        for (const auto &heading : (*found)["headings"].Elements()) exists |= heading["id"].S() == section;
        for (const auto &id : (*found)["anchors"].Elements()) exists |= id.S() == section;
        if (!exists) { failure = "Unknown help section: " + section; return {}; }
    }
    for (const auto &[key, value] : state) if (key == "media") {
        if ((*found)["html"].S().find("data-cydoc-id=\"" + value + "\"") == std::string::npos) {
            failure = "Unknown help media: " + value; return {};
        }
    }
    auto base = fileUrl(root / "Documentation.html");
    if (base.empty()) { failure = "Cannot create the local help URL"; return {}; }
    std::sort(state.begin(), state.end());
    return cydoc_native::link(base, locale, page, section, state);
}
bool HelpDocumentation::open(const std::filesystem::path &application, const std::string &page,
                             const std::string &locale, const std::string &anchor, State state) {
    auto link = url(application, page, locale, anchor, std::move(state));
    if (link.empty()) return false;
    if (!opener(link)) { failure = "Windows could not open the local CyDoc reader in your browser."; return false; }
    return true;
}
std::string HelpDocumentation::topicForPanel(const std::string &name) {
    // Child canvas and detached/clip window suffixes retain their parent panel's public name.
    for (auto [panel, topic] : {std::pair{"Track Graph", "track-graphs"}, {"Layer Graph", "layers"},
            {"Shader Graph", "shaders"}, {"HLSL", "shaders"}, {"DSP Graph", "dsp"},
            {"dspCanvas", "dsp"}, {"Track Effects", "devices"}, {"Mixer & Audio Graph", "mixer"},
            {"Motion Design", "motion"}, {"Color Grading", "grading"}, {"Video Scopes", "scopes"},
            {"Clip Editor", "containers"}, {"Timeline", "timeline"}, {"Composition", "composition"},
            {"Source", "source"}, {"Media & Effects", "media"}, {"Shaderixel Library", "library"},
            {"Effects Browser", "library"}, {"Animation", "animation"}, {"Markers", "markers"},
            {"Subtitles", "titles"}, {"Camera Angles", "multicam"}, {"Exports", "export"},
            {"Plugins & MCP", "plugins"}, {"Keyboard Shortcuts", "shortcuts"}, {"History", "recovery"}})
        if (name.find(panel) != std::string::npos) return topic;
    return "welcome";
}
} // namespace sf

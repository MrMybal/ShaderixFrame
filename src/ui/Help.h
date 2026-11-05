#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace sf {
// Native boundary to the exported CyDoc reader. The reader owns search, media,
// language switching and reading state; the editor only creates local routes.
class HelpDocumentation {
    std::string failure;
    std::function<bool(const std::string &)> opener;
  public:
    using State = std::vector<std::pair<std::string, std::string>>;
    explicit HelpDocumentation(std::function<bool(const std::string &)> launch = {});
    static std::string fileUrl(const std::filesystem::path &file);
    std::string url(const std::filesystem::path &application, const std::string &page = "welcome",
                    const std::string &locale = "en", const std::string &anchor = {}, State state = {});
    bool open(const std::filesystem::path &application, const std::string &page = "welcome",
              const std::string &locale = "en", const std::string &anchor = {}, State state = {});
    static std::string topicForPanel(const std::string &name);
    const std::string &error() const { return failure; }
    void setOpener(std::function<bool(const std::string &)> launch) { opener = std::move(launch); }
};
} // namespace sf

#pragma once
// Example host services. CyUI modules never depend on SDL or an OS SDK.
#include <SDL3/SDL.h>
#include <imgui.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace platform {
namespace fs = std::filesystem;
inline fs::path path(const std::string &value) {
    return fs::u8path(value);
}
inline std::string utf8(const fs::path &value) {
    const auto bytes = value.u8string();
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
struct Clipboard {
    std::vector<fs::path> files;
    std::vector<unsigned char> image;
    std::string imageName, html;
};
Clipboard nativeClipboard();
Clipboard clipboard();
std::vector<fs::path> fileUris(const std::string &text);
bool writeFile(const fs::path &, const void *, std::size_t);
class Dialogs {
    struct Result;
    std::shared_ptr<Result> pending;
    std::function<void(const std::vector<fs::path> &)> complete;
    static void SDLCALL received(void *, const char *const *, int);

  public:
    bool busy() const {
        return bool(pending);
    }
    void choose(SDL_Window *, bool save, bool multiple, bool markdown, const std::string &name,
                std::function<void(const std::vector<fs::path> &)>);
    void poll(); // Deliver callbacks on the UI thread, outside the ImGui frame.
};
class Host {
  public:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    bool closeRequested = false;
    std::vector<fs::path> dropped;
    Host() = default;
    Host(const Host &) = delete;
    Host &operator=(const Host &) = delete;
    ~Host();
    bool init(const char *, bool hidden);
    bool initImGui();
    void events();
    void newFrame();
    bool render();
    bool capture(const fs::path &);
    float scale() const;

  private:
    bool backend = false;
};
} // namespace platform

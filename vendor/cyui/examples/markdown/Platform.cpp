#include "Platform.h"
#include <cyui/Core.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <algorithm>
#include <fstream>
#include <mutex>
#include <random>
#include <stdexcept>

namespace platform {
struct Dialogs::Result {
    std::mutex mutex;
    bool ready = false;
    std::string error, name;
    std::vector<fs::path> files;
};
void SDLCALL Dialogs::received(void *data, const char *const *files, int) {
    std::unique_ptr<std::shared_ptr<Result>> owner(static_cast<std::shared_ptr<Result> *>(data));
    auto &result = **owner;
    std::lock_guard<std::mutex> lock(result.mutex);
    if (!files)
        result.error = SDL_GetError();
    else
        for (auto file = files; *file; ++file)
            result.files.push_back(path(*file));
    result.ready = true;
}
void Dialogs::choose(SDL_Window *window, bool save, bool multiple, bool markdown, const std::string &name,
                     std::function<void(const std::vector<fs::path> &)> callback) {
    if (busy())
        return;
    static const SDL_DialogFileFilter md[]{{"Markdown", "md;markdown;txt"}, {"All files", "*"}};
    static const SDL_DialogFileFilter all[]{{"All files", "*"}};
    pending = std::make_shared<Result>();
    pending->name = name;
    complete = std::move(callback);
    auto *data = new std::shared_ptr<Result>(pending);
    if (save)
        SDL_ShowSaveFileDialog(received, data, window, markdown ? md : all, markdown ? 2 : 1,
                               pending->name.c_str());
    else
        SDL_ShowOpenFileDialog(received, data, window, markdown ? md : all, markdown ? 2 : 1,
                               pending->name.empty() ? nullptr : pending->name.c_str(), multiple);
}
void Dialogs::poll() {
    if (!pending)
        return;
    std::vector<fs::path> files;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(pending->mutex);
        if (!pending->ready)
            return;
        files = std::move(pending->files);
        error = pending->error;
    }
    pending.reset();
    auto callback = std::move(complete);
    complete = {};
    if (!error.empty())
        throw std::runtime_error(error);
    if (callback)
        callback(files);
}
std::vector<fs::path> fileUris(const std::string &text) {
    std::vector<fs::path> files;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto end = text.find_first_of("\r\n", at);
        auto line = text.substr(at, end == std::string::npos ? end : end - at);
        at = end == std::string::npos ? text.size() : end + 1;
        if (line.rfind("file://", 0) != 0)
            continue;
        line.erase(0, 7);
        if (line.rfind("localhost/", 0) == 0)
            line.erase(0, 9);
        if (line.empty() || line.front() != '/')
            continue;
        std::string decoded;
        const auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        };
        bool valid = true;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] != '%')
                decoded += line[i];
            else if (i + 2 < line.size() && digit(line[i + 1]) >= 0 && digit(line[i + 2]) >= 0) {
                const auto c = char(digit(line[i + 1]) * 16 + digit(line[i + 2]));
                if (!c) {
                    valid = false;
                    break;
                }
                decoded += c;
                i += 2;
            } else {
                valid = false;
                break;
            }
        }
        if (!valid)
            continue;
#if defined(_WIN32)
        if (decoded.size() > 2 && decoded[2] == ':')
            decoded.erase(0, 1);
#endif
        files.push_back(path(decoded));
    }
    return files;
}
Clipboard clipboard() {
    auto result = nativeClipboard();
    const auto read = [](const char *mime, std::size_t limit) {
        std::vector<unsigned char> bytes;
        if (!SDL_HasClipboardData(mime))
            return bytes;
        std::size_t size = 0;
        auto *data = static_cast<unsigned char *>(SDL_GetClipboardData(mime, &size));
        if (data && size <= limit)
            bytes.assign(data, data + size);
        SDL_free(data);
        return bytes;
    };
    if (result.files.empty()) {
        auto bytes = read("text/uri-list", 16 * 1024 * 1024);
        if (bytes.empty())
            bytes = read("x-special/gnome-copied-files", 16 * 1024 * 1024);
        result.files = fileUris({bytes.begin(), bytes.end()});
    }
    if (result.image.empty())
        for (const auto &format :
             {std::pair{"image/png", "clipboard.png"}, std::pair{"image/bmp", "clipboard.bmp"},
              std::pair{"image/jpeg", "clipboard.jpg"}}) {
            result.image = read(format.first, 64 * 1024 * 1024);
            if (!result.image.empty()) {
                result.imageName = format.second;
                break;
            }
        }
    if (result.html.empty()) {
        const auto html = read("text/html", 16 * 1024 * 1024);
        result.html.assign(html.begin(), html.end());
    }
    const auto begin = result.html.find("<!--StartFragment-->"), end = result.html.find("<!--EndFragment-->");
    if (begin != std::string::npos && end != std::string::npos && end > begin)
        result.html = result.html.substr(begin + 20, end - begin - 20);
    return result;
}
bool writeFile(const fs::path &destination, const void *data, std::size_t size) {
    // Exclusively-created sibling directory: no collisions, same-filesystem rename.
    fs::path temporary;
    try {
        const auto target = fs::absolute(destination);
        std::random_device random;
        for (int n = 0; n < 16; ++n) {
            const auto candidate = target.parent_path() / (".cyui-save-" + std::to_string(random()) + "-" +
                                                           std::to_string(random()));
            if (fs::create_directory(candidate)) {
                temporary = candidate;
                break;
            }
        }
        if (temporary.empty())
            return false;
        const auto file = temporary / "content";
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output.write(static_cast<const char *>(data), std::streamsize(size));
        output.close();
        const bool ok = bool(output) && SDL_RenamePath(utf8(file).c_str(), utf8(target).c_str());
        std::error_code ignored;
        fs::remove(file, ignored);
        fs::remove(temporary, ignored);
        return ok;
    } catch (...) {
        if (!temporary.empty()) {
            std::error_code ignored;
            fs::remove(temporary / "content", ignored);
            fs::remove(temporary, ignored);
        }
        return false;
    }
}
Host::~Host() {
    if (backend) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    if (ImGui::GetCurrentContext())
        ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
bool Host::init(const char *title, bool hidden) {
    if (!SDL_Init(SDL_INIT_VIDEO))
        return false;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (hidden)
        flags |= SDL_WINDOW_HIDDEN;
    window = SDL_CreateWindow(title, 1280, 900, flags);
    if (!window)
        return false;
    SDL_SetWindowMinimumSize(window, 900, 660);
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
        return false;
    SDL_SetRenderVSync(renderer, hidden ? 0 : 1);
    return true;
}
bool Host::initImGui() {
    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer))
        return false;
    if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
        ImGui_ImplSDL3_Shutdown();
        return false;
    }
    backend = true;
    return true;
}
float Host::scale() const {
    // macOS window coordinates already use points.
#if defined(__APPLE__)
    return 1.f;
#else
    return std::max(1.f, SDL_GetWindowDisplayScale(window));
#endif
}
void Host::events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (backend)
            ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            closeRequested = true;
        if (event.type == SDL_EVENT_DROP_FILE && event.drop.data)
            dropped.push_back(path(event.drop.data));
        if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED && ImGui::GetCurrentContext())
            cyui::applyDpi(scale());
    }
}
void Host::newFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}
bool Host::render() {
    const auto color = cyui::vec(cyui::palette().bg0);
    SDL_SetRenderDrawColorFloat(renderer, color.x, color.y, color.z, 1.f);
    if (!SDL_RenderClear(renderer))
        return false;
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    return true;
}
bool Host::capture(const fs::path &file) {
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface)
        return false;
    const bool ok = SDL_SaveBMP(surface, utf8(file).c_str());
    SDL_DestroySurface(surface);
    return ok;
}
} // namespace platform

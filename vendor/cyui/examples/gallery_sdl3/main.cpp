#include "Platform.h"
#include <SDL3/SDL_main.h>
#include <cyui/Core.h>
#include <cyui/Gallery.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    std::string capture, theme = "mid-grey";
    int tab = 0;
    bool comfortable = false;
    for (int n = 1; n < argc; ++n) {
        const std::string arg = argv[n];
        const auto next = [&]() { return n + 1 < argc ? std::string(argv[++n]) : std::string(); };
        if (arg == "--capture")
            capture = next();
        else if (arg == "--theme")
            theme = next();
        else if (arg == "--tab")
            tab = std::atoi(next().c_str());
        else if (arg == "--comfortable")
            comfortable = true;
        else
            return 2;
    }
    platform::Host host;
    if (!host.init("CyUI - Gallery [multiplatform]", !capture.empty())) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    cyui::Config config;
    config.themeKey = theme.c_str();
    config.dpiScale = host.scale();
    config.density = comfortable ? cyui::Density::comfortable : cyui::Density::compact;
    cyui::apply(config);
    cyui::loadFonts();
    if (!host.initImGui())
        return 1;
    // Host-owned sample texture, shared by the image, viewport and document pages.
    std::vector<ImU32> pixels(480 * 240);
    const auto &palette = cyui::palette();
    const ImU32 colors[]{palette.bg2, palette.accent, palette.info, palette.success};
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 480; ++x)
            pixels[y * 480 + x] = colors[(x / 120 + y / 80) % 4];
    SDL_Surface *surface = SDL_CreateSurfaceFrom(480, 240, SDL_PIXELFORMAT_RGBA32, pixels.data(), 480 * 4);
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
        surface ? SDL_CreateTextureFromSurface(host.renderer, surface) : nullptr, SDL_DestroyTexture);
    SDL_DestroySurface(surface);
    if (!texture)
        return 1;
    cyui::GalleryResources resources;
    resources.texture = ImTextureID(reinterpret_cast<std::uintptr_t>(texture.get()));
    resources.width = 480;
    resources.height = 240;
    resources.imagePreviews.fill(resources.texture);
    bool open = true;
    int frames = 0;
    while (open) {
        host.events();
        if (host.closeRequested)
            break;
        host.newFrame();
        ImGui::NewFrame();
        const auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Once);
        cyui::showGallery(&open, tab, resources);
        ImGui::SetWindowPos(cyui::trLabel("CyUI Gallery").c_str(), viewport->WorkPos);
        ImGui::SetWindowSize(cyui::trLabel("CyUI Gallery").c_str(), viewport->WorkSize);
        ImGui::Render();
        if (!host.render())
            return 1;
        if (!capture.empty() && ++frames >= 44)
            return host.capture(platform::path(capture)) ? 0 : 1;
        if (!SDL_RenderPresent(host.renderer))
            return 1;
    }
    return 0;
}

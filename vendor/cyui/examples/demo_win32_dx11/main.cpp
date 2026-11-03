// CyUI demo: a complete application shell (menu bar, top bar, sidebar,
// status bar, dock space, panels) with the CyUI gallery and theme editor.
// Win32 + Direct3D 11, from Dear ImGui's own example.

#include <cyui/CyUI.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <d3d11.h>
#include <tchar.h>
#include <windows.h>

#include <shellapi.h>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#if defined(CYUI_DEMO_CEF)
#include <cyui/adapters/WebCEF.h>
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
using cyui::tr;
using cyui::trLabel;

ID3D11Device* gDevice {};
ID3D11DeviceContext* gContext {};
IDXGISwapChain* gSwapChain {};
ID3D11RenderTargetView* gTarget {};
ID3D11ShaderResourceView* gGalleryTexture {};
std::array<ID3D11ShaderResourceView*, 4> gImagePreviews {};
ID3D11Texture2D* gWebTexture {};
ID3D11ShaderResourceView* gWebView {};
int gWebWidth=0,gWebHeight=0;
// Replaced/closed web textures wait here until after the next Present, so no draw list built
// before the swap can still reference a released view.
std::vector<IUnknown*> gWebRetired;
void retireWeb(){if(gWebView){gWebRetired.push_back(gWebView);gWebView=nullptr;}if(gWebTexture){gWebRetired.push_back(gWebTexture);gWebTexture=nullptr;}gWebWidth=gWebHeight=0;}
void releaseRetiredWeb(){for(auto*resource:gWebRetired)resource->Release();gWebRetired.clear();}

ImTextureID uploadWeb(int width,int height,const void* pixels) {
    if(width!=gWebWidth||height!=gWebHeight){
        retireWeb();
        D3D11_TEXTURE2D_DESC desc{};desc.Width=UINT(width);desc.Height=UINT(height);desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(gDevice->CreateTexture2D(&desc,nullptr,&gWebTexture)))return 0;
        if(FAILED(gDevice->CreateShaderResourceView(gWebTexture,nullptr,&gWebView)))return 0;
        gWebWidth=width;gWebHeight=height;
    }
    gContext->UpdateSubresource(gWebTexture,0,nullptr,pixels,UINT(width*4),0);
    return ImTextureID(reinterpret_cast<std::uintptr_t>(gWebView));
}
void createGalleryTexture() {
    std::vector<ImU32> pixels(256*256);const auto&p=cyui::palette();
    for(int y=0;y<256;++y)for(int x=0;x<256;++x){const float k=float(x)/255;const float signal=.5f+.5f*std::sin(float(x*x+y*y)*.0007f);const auto color=cyui::mix(cyui::mix(p.bg0,p.info,k),p.accent,signal*.65f);pixels[std::size_t(y)*256+x]=color;}
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=256;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=256*4;ID3D11Texture2D* texture=nullptr;
    if(SUCCEEDED(gDevice->CreateTexture2D(&desc,&data,&texture))){if(FAILED(gDevice->CreateShaderResourceView(texture,nullptr,&gGalleryTexture)))gGalleryTexture=nullptr;texture->Release();}
}
// Synthetic host-owned preview images. These pixel colors are image data, not UI colors.
// No file decoder, third-party artwork or image-processing engine is required by CyUI.
void createImagePreviews()
{
    constexpr int width = 256, height = 160;
    std::array<std::vector<ImVec4>, 4> images;
    for (auto& image : images)
        image.resize(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = float(x) / (width - 1), v = float(y) / (height - 1);
            ImVec4 pixel{.17f + .48f * v, .38f + .35f * v, .64f + .18f * v, 1};
            const float sun = std::hypot(u - .76f, (v - .22f) * .625f);
            if (sun < .065f)
                pixel = {.98f, .85f, .58f, 1};
            const float farRidge = .45f + .08f * std::sin(u * 9) + .04f * std::sin(u * 22);
            const float nearRidge = .62f + .08f * std::sin(u * 7 + 1) + .035f * std::sin(u * 18);
            if (v > farRidge)
                pixel = {.22f + .09f * u, .38f + .12f * u, .44f + .09f * u, 1};
            if (v > nearRidge)
                pixel = {.07f + .1f * u, .24f + .15f * u, .27f + .1f * u, 1};
            if (v > .78f + .015f * std::sin(u * 12)) {
                const float reflection = .5f + .5f * std::sin(v * 190 + u * 15);
                pixel = {.12f + .07f * reflection, .34f + .08f * reflection, .46f + .09f * reflection, 1};
            }
            const auto index = std::size_t(y * width + x);
            images[0][index] = pixel;
            images[1][index] = {std::clamp(pixel.x * 1.18f + .045f, 0.f, 1.f),
                                std::clamp(pixel.y * 1.04f + .015f, 0.f, 1.f), pixel.z * .83f, 1};
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            ImVec4 blurred{};
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const auto& pixel = images[1][std::size_t(std::clamp(y + dy, 0, height - 1) * width +
                                                             std::clamp(x + dx, 0, width - 1))];
                    blurred.x += pixel.x / 25;
                    blurred.y += pixel.y / 25;
                    blurred.z += pixel.z / 25;
                }
            blurred.w = 1;
            const auto index = std::size_t(y * width + x);
            images[2][index] = blurred;
            const float u = float(x) / (width - 1) - .5f, v = float(y) / (height - 1) - .5f;
            const float vignette = 1 - .65f * (u * u + v * v);
            images[3][index] = {blurred.x * vignette, blurred.y * vignette, blurred.z * vignette, 1};
        }
    }
    for (std::size_t i = 0; i < images.size(); ++i) {
        std::vector<ImU32> pixels;
        pixels.reserve(width * height);
        for (const auto& pixel : images[i])
            pixels.push_back(ImGui::ColorConvertFloat4ToU32(pixel));
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width; desc.Height = height; desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = pixels.data(); data.SysMemPitch = width * 4;
        ID3D11Texture2D* texture = nullptr;
        if (SUCCEEDED(gDevice->CreateTexture2D(&desc, &data, &texture))) {
            if (FAILED(gDevice->CreateShaderResourceView(texture, nullptr, &gImagePreviews[i])))
                gImagePreviews[i] = nullptr;
            texture->Release();
        }
    }
}
UINT gResizeWidth {}, gResizeHeight {};
bool gOccluded = false;

bool createTarget()
{
    ID3D11Texture2D* back {};
    if (FAILED(gSwapChain->GetBuffer(0, IID_PPV_ARGS(&back))))
        return false;
    const auto result = gDevice->CreateRenderTargetView(back, nullptr, &gTarget);
    back->Release();
    if (FAILED(result))
        gTarget = nullptr;
    return gTarget != nullptr;
}

void releaseTarget()
{
    if (gTarget != nullptr) {
        gTarget->Release();
        gTarget = nullptr;
    }
}

bool createDevice(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL level {};
    auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                                &gSwapChain, &gDevice, &level, &gContext);
    if (result == DXGI_ERROR_UNSUPPORTED)
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                               &gSwapChain, &gDevice, &level, &gContext);
    if (result != S_OK)
        return false;
    return createTarget();
}

void releaseDevice()
{
    if(gGalleryTexture)gGalleryTexture->Release();
    for (auto* preview : gImagePreviews)
        if (preview) preview->Release();
    retireWeb();
    releaseRetiredWeb();
    releaseTarget();
    if (gSwapChain != nullptr)
        gSwapChain->Release();
    if (gContext != nullptr)
        gContext->Release();
    if (gDevice != nullptr)
        gDevice->Release();
}

// Writes the back buffer to a 32-bit BMP (the --capture option, for checks).
bool captureBackBuffer(const wchar_t* path)
{
    ID3D11Texture2D* back {};
    if (FAILED(gSwapChain->GetBuffer(0, IID_PPV_ARGS(&back))))
        return false;
    D3D11_TEXTURE2D_DESC desc {};
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ID3D11Texture2D* staging {};
    if (FAILED(gDevice->CreateTexture2D(&desc, nullptr, &staging))) {
        back->Release();
        return false;
    }
    gContext->CopyResource(staging, back);
    back->Release();
    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (FAILED(gContext->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        staging->Release();
        return false;
    }
    const auto width = static_cast<int>(desc.Width);
    const auto height = static_cast<int>(desc.Height);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    for (auto y = 0; y < height; ++y) {
        const auto* row = static_cast<const unsigned char*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch;
        for (auto x = 0; x < width; ++x) {
            auto* out = &pixels[(static_cast<std::size_t>(y) * width + x) * 4];
            out[0] = row[x * 4 + 2];
            out[1] = row[x * 4 + 1];
            out[2] = row[x * 4 + 0];
            out[3] = 255;
        }
    }
    gContext->Unmap(staging, 0);
    staging->Release();
    BITMAPFILEHEADER file {};
    BITMAPINFOHEADER info {};
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = -height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    FILE* out {};
    if (_wfopen_s(&out, path, L"wb") != 0 || out == nullptr)
        return false;
    std::fwrite(&file, sizeof(file), 1, out);
    std::fwrite(&info, sizeof(info), 1, out);
    std::fwrite(pixels.data(), 1, pixels.size(), out);
    std::fclose(out);
    return true;
}

LRESULT WINAPI windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            gResizeWidth = LOWORD(lParam);
            gResizeHeight = HIWORD(lParam);
        }
        return 0;
    case WM_DPICHANGED: {
        // Moved to a monitor with another scale: restyle, take the suggested size.
        if (ImGui::GetCurrentContext() != nullptr)
            cyui::applyDpi(static_cast<float>(HIWORD(wParam)) / 96.0f);
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                       suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---- The application's interface ---------------------------------------------

struct App {
    int page = 0;
    cyui::GalleryResources resources;
    int galleryTab = -1;
    bool gallery = true;
    bool layoutChecked = false;
    bool themeEditor = false;
    bool imguiDemo = false;
    bool playing = false;
    float tempo = 120.0f;
    char search[128] {};

    void menuBar()
    {
        if (!ImGui::BeginMainMenuBar())
            return;
        if (ImGui::BeginMenu(trLabel("File").c_str())) {
            ImGui::MenuItem(trLabel("New").c_str(), "Ctrl+N");
            ImGui::MenuItem(trLabel("Open...").c_str(), "Ctrl+O");
            ImGui::MenuItem(trLabel("Save").c_str(), "Ctrl+S");
            ImGui::Separator();
            if (ImGui::MenuItem(trLabel("Quit").c_str(), "Alt+F4"))
                ::PostQuitMessage(0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(trLabel("View").c_str())) {
            ImGui::MenuItem(trLabel("CyUI gallery").c_str(), nullptr, &gallery);
            ImGui::MenuItem(trLabel("Theme editor").c_str(), nullptr, &themeEditor);
            ImGui::MenuItem(trLabel("Dear ImGui demo").c_str(), nullptr, &imguiDemo);
            ImGui::Separator();
            if (ImGui::BeginMenu(trLabel("Theme").c_str())) {
                for (const auto& theme : cyui::builtInThemes())
                    if (ImGui::MenuItem(trLabel(theme.name.c_str()).c_str(), nullptr, theme.key == cyui::currentThemeKey()))
                        cyui::setTheme(theme.key.c_str());
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void topBar()
    {
        if (cyui::beginTopBar("##topBar")) {
            static int mode = 0;
            const char* modes[] { "Studio", "Mixer", "Library" };
            cyui::segmented("##mode", modes, 3, &mode);
            cyui::toolbarSeparator();
            if (cyui::iconButton(playing ? cyui::icon::pause : cyui::icon::play, "Play / pause (Space)", playing,
                                 cyui::palette().success))
                playing = !playing;
            ImGui::SameLine(0.0f, cyui::dp(2.0f));
            cyui::iconButton(cyui::icon::stop, "Stop");
            ImGui::SameLine(0.0f, cyui::dp(2.0f));
            cyui::iconButton(cyui::icon::record, "Record (R)");
            cyui::toolbarSeparator();
            cyui::dragValue("##tempo", &tempo, 0.1f, 20.0f, 300.0f, "%.2f BPM", cyui::dp(100.0f));
            ImGui::SameLine();
            cyui::alignRight(cyui::dp(240.0f));
            cyui::searchField("##search", search, sizeof(search), "Search", cyui::dp(240.0f));
        }
        cyui::endTopBar();
    }

    void statusBar()
    {
        if (cyui::beginStatusBar("##statusBar")) {
            cyui::statusText(cyui::Tone::neutral, tr("Ready").c_str());
            ImGui::SameLine(0.0f, cyui::dp(18.0f));
            cyui::statusText(cyui::Tone::success, tr("Audio: 48 kHz, 256 samples").c_str());
            char fps[32];
            std::snprintf(fps, sizeof(fps), "%.0f fps", ImGui::GetIO().Framerate);
            ImGui::SameLine();
            cyui::ScopedFont font(nullptr, 0.86f);
            cyui::alignRight(ImGui::CalcTextSize(fps).x);
            cyui::textFaint(tr("%s").c_str(), fps);
        }
        cyui::endStatusBar();
    }

    void sidebar()
    {
        if (cyui::beginSidebar("##sidebar")) {
            cyui::sidebarHeader("CYUI DEMO", "1.1");
            cyui::sidebarGroup("WORKSPACE");
            if (cyui::sidebarItem(cyui::icon::home, trLabel("Home").c_str(), page == 0))
                page = 0;
            if (cyui::sidebarItem(cyui::icon::library, trLabel("Library").c_str(), page == 1, "12"))
                page = 1;
            if (cyui::sidebarItem(cyui::icon::equalizer, trLabel("Mixer").c_str(), page == 2))
                page = 2;
            cyui::sidebarGroup("SYSTEM");
            if (cyui::sidebarItem(cyui::icon::settings, trLabel("Settings").c_str(), page == 3))
                page = 3;
        }
        cyui::endSidebar();
    }

    void panels()
    {
        if (ImGui::Begin(trLabel("Inspector").c_str())) {
            cyui::panelHeader(tr("Inspector").c_str(), "Kick drum - Track 1");
            cyui::divider();
            if (cyui::beginSection(trLabel("Track").c_str())) {
                static float volume = -3.0f, pan = 0.0f;
                static bool mute = false;
                if (cyui::beginProperties("##track")) {
                    cyui::property(tr("Volume").c_str());
                    cyui::sliderBar("##volume", &volume, -60.0f, 6.0f, "%.1f dB", -1.0f);
                    cyui::property(tr("Pan").c_str());
                    cyui::sliderBar("##pan", &pan, -1.0f, 1.0f, "%.2f", -1.0f, cyui::palette().info);
                    cyui::property(tr("Mute").c_str());
                    cyui::toggleSwitch("##mute", &mute);
                    cyui::endProperties();
                }
                cyui::endSection();
            }
        }
        ImGui::End();

        if (ImGui::Begin(trLabel("Devices").c_str())) {
            if (cyui::beginToolbar("##devicesToolbar")) {
                cyui::iconButton(cyui::icon::add, "Add a device");
                ImGui::SameLine(0.0f, cyui::dp(2.0f));
                cyui::iconButton(cyui::icon::presets, "Presets");
                cyui::toolbarSeparator();
                cyui::badge(tr("3 DEVICES").c_str());
            }
            cyui::endToolbar();
            cyui::space(6.0f);
            static float drive = 0.4f, tone = 0.6f, mix = 1.0f;
            if (cyui::beginCard("##saturator", tr("Saturator").c_str())) {
                cyui::knob("##drive", &drive, 0.0f, 1.0f, 0.4f, { 36.0f, 0, "Drive" });
                ImGui::SameLine();
                cyui::knob("##tone", &tone, 0.0f, 1.0f, 0.5f, { 36.0f, cyui::palette().info, "Tone" });
                ImGui::SameLine();
                cyui::knob("##mix", &mix, 0.0f, 1.0f, 1.0f, { 36.0f, cyui::palette().success, "Mix" });
            }
            cyui::endCard();
        }
        ImGui::End();

        if (ImGui::Begin(trLabel("Browser").c_str())) {
            cyui::emptyState(tr("Nothing here yet").c_str(), tr("Drop files to add them to the library").c_str(), cyui::icon::folder);
        }
        ImGui::End();
    }

    // The first layout: gallery in the middle, inspector on the right,
    // devices and browser under.
    static void buildLayout(const ImGuiID dockId)
    {
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);
        ImGuiID centre = dockId;
        const auto right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.26f, nullptr, &centre);
        const auto bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, nullptr, &centre);
        ImGui::DockBuilderDockWindow("CyUI Gallery", centre);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Devices", bottom);
        ImGui::DockBuilderDockWindow("Browser", bottom);
        ImGui::DockBuilderFinish(dockId);
    }

    void frame()
    {
        menuBar();
        topBar();
        statusBar();
        sidebar();
        const auto dockId = cyui::dockSpace();
        // Once, at start: the saved layout (imgui.ini) when there is one.
        if (!layoutChecked) {
            layoutChecked = true;
            if (ImGui::DockBuilderGetNode(dockId) == nullptr || ImGui::DockBuilderGetNode(dockId)->IsLeafNode())
                buildLayout(dockId);
        }
        panels();
        if (gallery)
            cyui::showGallery(&gallery, galleryTab,resources);
        if (themeEditor)
            cyui::showThemeEditor(&themeEditor);
        if (imguiDemo)
            ImGui::ShowDemoWindow(&imguiDemo);
        cyui::renderNotifications();
    }
};

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
#if defined(CYUI_DEMO_CEF)
    CefMainArgs cefArgs(GetModuleHandleW(nullptr));auto cefApp=cyui::webCEFApp();const int cefChild=CefExecuteProcess(cefArgs,cefApp,nullptr);if(cefChild>=0)return cefChild;
    bool cefStarted=false;
#endif
    // --capture out.bmp [--theme key] [--comfortable] [--tab n]: renders a few
    // frames, writes the window to a BMP and quits.
    std::wstring capturePath,webUrl;
    bool cefNoSandbox=false, galleryOnly=false, imageGraph=false;
    std::string themeKey = "mid-grey";
    std::string language = "en";
    auto comfortable = false;
    auto tab = -1;
    {
        auto count = 0;
        auto** args = ::CommandLineToArgvW(::GetCommandLineW(), &count);
        for (auto i = 1; i < count; ++i) {
            const std::wstring arg = args[i];
            const auto next = [&]() { return i + 1 < count ? std::wstring(args[++i]) : std::wstring(); };
            if(arg==L"--web-url")webUrl=next();
            else if(arg==L"--gallery-only")galleryOnly=true;
            else if(arg==L"--image-graph")imageGraph=true;
            else if(arg==L"--locale") {
                language.clear();
                for (const auto c : next()) {
                    if (c > 127) { language = "?"; break; }
                    language += static_cast<char>(c);
                }
            }
            else if(arg==L"--cef-no-sandbox")cefNoSandbox=true; // Accepted; the demo is never sandboxed.
            else if (arg == L"--capture")
                capturePath = next();
            else if (arg == L"--theme") {
                const auto key = next();
                for (const auto c : key)
                    themeKey += static_cast<char>(c);
                themeKey.erase(0, themeKey.size() - key.size());
            } else if (arg == L"--comfortable")
                comfortable = true;
            else if (arg == L"--tab")
                tab = _wtoi(next().c_str());
        }
        ::LocalFree(args);
    }

    ImGui_ImplWin32_EnableDpiAwareness();
    const auto dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc { sizeof(wc), CS_CLASSDC, windowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                     L"CyUIDemo", nullptr };
    ::RegisterClassExW(&wc);
    const auto hwnd = ::CreateWindowW(wc.lpszClassName, L"CyUI demo", WS_OVERLAPPEDWINDOW, 100, 100,
                                      static_cast<int>(1440 * dpiScale), static_cast<int>((galleryOnly ? 760 : 900) * dpiScale), nullptr, nullptr,
                                      wc.hInstance, nullptr);
    if (!createDevice(hwnd)) {
        releaseDevice();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    // CyUI: the theme, then the fonts.
    cyui::Config config;
    config.themeKey = themeKey.c_str();
    config.density = comfortable ? cyui::Density::comfortable : cyui::Density::compact;
    config.dpiScale = dpiScale;
    cyui::apply(config);
    cyui::setLocale(language.c_str());
    cyui::loadFonts();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(gDevice, gContext);

    if (!capturePath.empty())
        io.IniFilename = nullptr;

    createGalleryTexture();
    createImagePreviews();
    App app;
    app.resources.texture=ImTextureID(reinterpret_cast<std::uintptr_t>(gGalleryTexture));app.resources.width=app.resources.height=256;
    for (std::size_t i = 0; i < gImagePreviews.size(); ++i)
        app.resources.imagePreviews[i] = ImTextureID(reinterpret_cast<std::uintptr_t>(gImagePreviews[i]));
    app.resources.previewImageGraph = imageGraph;
#if defined(CYUI_DEMO_CEF)
    std::unique_ptr<cyui::WebBackend> web;
    if(!webUrl.empty()){
        // No sandbox info is passed to CefExecuteProcess/CefInitialize, so CEF runs unsandboxed
        // whatever --cef-no-sandbox says; say so explicitly (docs/WEB.md "Process sandbox").
        (void)cefNoSandbox;
        CefSettings settings;settings.windowless_rendering_enabled=true;settings.no_sandbox=true;CefString(&settings.root_cache_path)=std::filesystem::absolute("build/cef-demo-profile").string();
        cefStarted=CefInitialize(cefArgs,settings,cefApp,nullptr);
        if(cefStarted){const int count=WideCharToMultiByte(CP_UTF8,0,webUrl.data(),int(webUrl.size()),nullptr,0,nullptr,nullptr);std::string url(std::size_t(count),'\0');WideCharToMultiByte(CP_UTF8,0,webUrl.data(),int(webUrl.size()),url.data(),count,nullptr,nullptr);cyui::WebCEFOptions options;options.parent=hwnd;options.url=url;options.policy.origins={cyui::webOrigin(url)};options.upload=uploadWeb;options.release=[](ImTextureID texture){if(texture==ImTextureID(reinterpret_cast<std::uintptr_t>(gWebView)))retireWeb();};web=cyui::createWebCEF(options);app.resources.web=web.get();if(tab<0)tab=19;}
    }
#else
    (void)webUrl;(void)cefNoSandbox;
#endif
    app.galleryTab = tab;
    app.resources.previewCommands = !capturePath.empty() && tab == 16;
    cyui::notify(tr("Welcome to the CyUI demo").c_str(), cyui::Tone::accent);
    auto done = false;
    auto frames = 0;
    const auto captureStart=GetTickCount64();
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        // Hidden capture windows still need to render their requested frames.
        // Otherwise, minimised or locked interactive windows can wait.
        if (capturePath.empty() && gOccluded && gSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        gOccluded = false;
        if (gResizeWidth != 0 && gResizeHeight != 0) {
            releaseTarget();
            const auto resized = gSwapChain->ResizeBuffers(0, gResizeWidth, gResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            gResizeWidth = gResizeHeight = 0;
            if (FAILED(resized) || !createTarget()) {
                ::OutputDebugStringA("CyUI demo: could not resize the swap chain\n");
                break;
            }
        }

#if defined(CYUI_DEMO_CEF)
        if(cefStarted)CefDoMessageLoopWork();if(web)web->beginFrame();
#endif
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (galleryOnly) {
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSizeConstraints(viewport->WorkSize, viewport->WorkSize);
            cyui::showGallery(nullptr, app.galleryTab, app.resources);
        } else {
            app.frame();
        }
#if defined(CYUI_DEMO_CEF)
        if(web)web->endFrame();
#endif
        ImGui::Render();

        const auto ground = cyui::vec(cyui::palette().bg0);
        const float clear[4] { ground.x, ground.y, ground.z, 1.0f };
        gContext->OMSetRenderTargets(1, &gTarget, nullptr);
        gContext->ClearRenderTargetView(gTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#if defined(CYUI_DEMO_CEF)
        const bool browserCaptureReady=!web||(web->texture()&&GetTickCount64()-captureStart>=1500);
#else
        const bool browserCaptureReady=true;(void)captureStart;
#endif
        if (!capturePath.empty() && ++frames >= 40 && browserCaptureReady) {
            captureBackBuffer(capturePath.c_str());
            done = true;
        }
        const auto presented = gSwapChain->Present(1, 0);
        releaseRetiredWeb();
        gOccluded = presented == DXGI_STATUS_OCCLUDED;
        if (presented == DXGI_ERROR_DEVICE_REMOVED || presented == DXGI_ERROR_DEVICE_RESET) {
            ::OutputDebugStringA("CyUI demo: the graphics device was lost\n");
            break;
        }
    }

#if defined(CYUI_DEMO_CEF)
    if(web){web->close();const auto start=GetTickCount64();while(!web->closed()&&GetTickCount64()-start<5000){CefDoMessageLoopWork();MsgWaitForMultipleObjects(0,nullptr,FALSE,5,QS_ALLINPUT);}web.reset();}if(cefStarted)CefShutdown();
#endif
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    releaseDevice();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

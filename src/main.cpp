#include "ui/Editor.h"
#include <atomic>
#include <deque>
#include <fstream>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <mfapi.h>
#include <mutex>
#include <shellapi.h>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
sf::Com<IDXGISwapChain> swapChain;
sf::Com<ID3D11RenderTargetView> backTarget;
sf::Com<ID3D11Device> device;
sf::Com<ID3D11DeviceContext> context;
sf::Editor *editor = nullptr;
unsigned pendingWidth = 0, pendingHeight = 0;
bool closing = false;
void target() {
    sf::Com<ID3D11Texture2D> t;
    sf::check(swapChain->GetBuffer(0, IID_PPV_ARGS(&t)), "Swapchain");
    sf::check(device->CreateRenderTargetView(t.Get(), nullptr, &backTarget), "Fenêtre GPU");
}
LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM a, LPARAM b) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(w, msg, a, b))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (a != SIZE_MINIMIZED) {
            pendingWidth = LOWORD(b);
            pendingHeight = HIWORD(b);
        }
        return 0;
    case WM_CLOSE:
        closing = true;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DROPFILES: {
        auto drop = reinterpret_cast<HDROP>(a);
        UINT count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t p[32768];
            DragQueryFileW(drop, i, p, 32768);
            if (editor)
                editor->drop(sx::Narrow(p));
        }
        DragFinish(drop);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((a & 0xfff0) == SC_KEYMENU)
            return 0;
    }
    return DefWindowProcW(w, msg, a, b);
}
void output(const sf::Json &j) {
    if (j.IsNull())
        return;
    std::string text = j.Dump(0) + "\n";
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text.data(), DWORD(text.size()), &written, nullptr);
}
void capture(const std::string &path) {
    sf::Com<ID3D11Texture2D> t;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&t));
    D3D11_TEXTURE2D_DESC d;
    t->GetDesc(&d);
    unsigned w = d.Width, h = d.Height;
    d.BindFlags = 0;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    sf::Com<ID3D11Texture2D> stage;
    sf::check(device->CreateTexture2D(&d, nullptr, &stage), "Capture UI");
    context->CopyResource(stage.Get(), t.Get());
    D3D11_MAPPED_SUBRESOURCE mapped;
    sf::check(context->Map(stage.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Capture pixels");
    sf::Image image;
    image.width = w;
    image.height = h;
    image.pixels.resize(size_t(w) * h * 4);
    for (unsigned y = 0; y < h; ++y) {
        auto *dst = image.pixels.data() + size_t(y) * w * 4;
        auto *src = static_cast<unsigned char *>(mapped.pData) + size_t(y) * mapped.RowPitch;
        for (unsigned x = 0; x < w; ++x) {
            dst[x * 4] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4];
            dst[x * 4 + 3] = 255;
        }
    }
    context->Unmap(stage.Get(), 0);
    sf::writePng(image, path);
}
} // namespace
int main() {
    bool mcp = false, hidden = false;
    std::string snapshot, projectPath, view, recoveryDir, viewportReport;
    bool viewportRendered = false, viewportClosed = false;
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--mcp")
            mcp = true;
        else if (a == L"--hidden")
            hidden = true;
        else if (a == L"--warp")
            SetEnvironmentVariableW(L"SF_FORCE_WARP", L"1");
        else if (a.starts_with(L"--ui-snapshot="))
            snapshot = sx::Narrow(a.substr(14));
        else if (a.starts_with(L"--viewport-smoke="))
            viewportReport = sx::Narrow(a.substr(17));
        else if (a.starts_with(L"--view="))
            view = sx::Narrow(a.substr(7));
        else if (a.starts_with(L"--recovery-dir="))
            recoveryDir = sx::Narrow(a.substr(15));
        else if (a.ends_with(L".sframe"))
            projectPath = sx::Narrow(a);
    }
    LocalFree(argv);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    HWND window = nullptr;
    std::thread input;
    std::atomic<bool> eof = false, stop = false;
    std::mutex mutex;
    std::deque<std::string> requests;
    int exitCode = 0;
    try {
        wchar_t exe[32768];
        GetModuleFileNameW(nullptr, exe, 32768);
        auto root = std::filesystem::path(exe).parent_path();
        WNDCLASSEXW cls{sizeof(cls),
                        CS_CLASSDC,
                        proc,
                        0,
                        0,
                        GetModuleHandleW(nullptr),
                        nullptr,
                        LoadCursorW(nullptr, IDC_ARROW),
                        nullptr,
                        nullptr,
                        L"ShaderixFrameWindow",
                        nullptr};
        RegisterClassExW(&cls);
        ImGui_ImplWin32_EnableDpiAwareness();
        window = CreateWindowW(cls.lpszClassName, L"ShaderixFrame — Montage vidéo & VFX", WS_OVERLAPPEDWINDOW,
                               100, 70, 1600, 980, nullptr, nullptr, cls.hInstance, nullptr);
        if (!window)
            throw std::runtime_error("Création fenêtre échouée");
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL level;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            GetEnvironmentVariableW(L"SF_FORCE_WARP", nullptr, 0) ? D3D_DRIVER_TYPE_WARP
                                                                  : D3D_DRIVER_TYPE_HARDWARE,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &swapChain, &device, &level, &context);
        if (FAILED(hr))
            sf::check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                                    D3D11_SDK_VERSION, &desc, &swapChain, &device, &level,
                                                    &context),
                      "Direct3D fenêtre");
        target();
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        if (!hidden || !viewportReport.empty())
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        std::string ini = sx::Narrow((root / L"workspace.ini").wstring());
        io.IniFilename = !hidden && snapshot.empty() ? ini.c_str() : nullptr;
        sf::applyFrameTheme();
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(device.Get(), context.Get());
        if (hidden && !viewportReport.empty())
            ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport *vp) {
                ShowWindow(static_cast<HWND>(vp->PlatformHandle), SW_HIDE);
            };
        sf::Session session;
        if (!mcp && snapshot.empty())
            session.media.enableBackground(root / "cache/audio");
        if (!recoveryDir.empty())
            session.recovery.configure(sx::Utf8Path(recoveryDir));
        else if (!hidden && snapshot.empty())
            session.recovery.configure(root / "recovery");
        session.renderer.initialize(device.Get(), context.Get());
        session.renderer.library.scan(root);
        if (!projectPath.empty())
            session.project = sf::Project::load(projectPath);
        else if (std::filesystem::exists(root / "examples/FirstLight.sframe"))
            session.project = sf::Project::load(sx::Narrow((root / "examples/FirstLight.sframe").wstring()));
        session.projectPath =
            projectPath.empty() ? ""
                                : sx::Narrow(std::filesystem::absolute(sx::Utf8Path(projectPath)).wstring());
        if (projectPath.empty())
            session.time = 2;
        if (!session.project.tracks.empty() && !session.project.tracks.front().clips.empty())
            session.selectClip(session.project.tracks.front().clips.front().id);
        for (auto &t : session.project.tracks)
            for (auto &c : t.clips)
                for (auto &e : c.effects)
                    if (e.code.empty() && e.graph.IsNull())
                        if (auto *d = session.renderer.library.find(e.id))
                            e.code = d->code;
        sf::Editor ui(session, root);
        editor = &ui;
        sf::Protocol protocol(session);
        if (mcp) {
            input = std::thread([&] {
                std::string buffered;
                char chunk[4096];
                DWORD count = 0;
                HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
                while (!stop && ReadFile(in, chunk, sizeof(chunk), &count, nullptr) && count) {
                    buffered.append(chunk, count);
                    size_t at;
                    while ((at = buffered.find('\n')) != std::string::npos) {
                        std::string line = buffered.substr(0, at);
                        buffered.erase(0, at + 1);
                        if (!line.empty()) {
                            std::lock_guard lock(mutex);
                            requests.push_back(std::move(line));
                        }
                    }
                    if (buffered.size() > 8 * 1024 * 1024)
                        break;
                }
                eof = true;
            });
        }
        if (!hidden)
            ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
        DragAcceptFiles(window, TRUE);
        int frames = 0;
        int ticks = 0;
        while (!closing) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT)
                    closing = true;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (closing)
                break;
            if (mcp) {
                std::deque<std::string> incoming;
                {
                    std::lock_guard lock(mutex);
                    incoming.swap(requests);
                }
                for (auto &line : incoming) {
                    sf::Json j;
                    std::string err;
                    if (sf::Json::Parse(line, j, &err))
                        output(protocol.respond(j));
                    else {
                        sf::Json e = sf::Json::Object();
                        e["jsonrpc"] = "2.0";
                        e["id"] = {};
                        e["error"] = sf::Json::Object();
                        e["error"]["code"] = -32700;
                        e["error"]["message"] = "Parse error";
                        output(e);
                    }
                }
                if (eof) {
                    std::lock_guard lock(mutex);
                    if (requests.empty())
                        closing = true;
                }
            }
            if (pendingWidth && pendingHeight) {
                context->OMSetRenderTargets(0, nullptr, nullptr);
                backTarget.Reset();
                sf::check(swapChain->ResizeBuffers(0, pendingWidth, pendingHeight, DXGI_FORMAT_UNKNOWN, 0),
                          "Redimensionnement");
                pendingWidth = pendingHeight = 0;
                target();
            }
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            ui.draw();
            if (++ticks == 3 && !view.empty()) {
                if (view == "clip")
                    ui.showClipEditor();
                if (view == "grading")
                    ui.showGrading();
                const char *panel = view == "layers"       ? "Graphe des calques"
                                    : view == "graph"      ? "Graphe VFX"
                                    : view == "grading"    ? "Étalonnage"
                                    : view == "scopes"     ? "Scopes vidéo"
                                    : view == "hlsl"       ? "HLSL"
                                    : view == "audio"      ? "Mixeur & graphe audio"
                                    : view == "dsp"        ? "Graphe DSP"
                                    : view == "motion"     ? "Motion design"
                                    : view == "library"    ? "Bibliothèque Shaderixel"
                                    : view == "clip"       ? "Éditeur de clip"
                                    : view == "animations" ? "Animations"
                                    : view == "plugins"    ? "Plugins & MCP"
                                    : view == "exports"    ? "Exports"
                                                           : "Timeline";
                ui.openPanel(panel);
                ImGui::SetWindowFocus(panel);
                if (view == "dsp")
                    ui.showDspGraph();
            }
            if (!viewportReport.empty() && ticks == 3 && view != "clip") {
                auto *composition = ImGui::FindWindowByName("Composition");
                ImGui::SetWindowDock(composition, 0, ImGuiCond_Always);
                ImGui::SetWindowPos("Composition", {1760, 100}, ImGuiCond_Always);
                ImGui::SetWindowSize("Composition", {600, 400}, ImGuiCond_Always);
            }
            ImGui::Render();
            float bg[] = {.05f, .06f, .075f, 1};
            auto *rt = backTarget.Get();
            context->OMSetRenderTargets(1, &rt, nullptr);
            context->ClearRenderTargetView(rt, bg);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (!snapshot.empty() && ++frames == 10) {
                capture(snapshot);
                closing = true;
            }
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
            if (!viewportReport.empty()) {
                auto &platform = ImGui::GetPlatformIO();
                if (ticks >= 8 && ticks < 14)
                    for (int i = 1; i < platform.Viewports.Size; ++i) {
                        auto *vp = platform.Viewports[i];
                        viewportRendered |= IsWindow(static_cast<HWND>(vp->PlatformHandle)) &&
                                            vp->RendererUserData && vp->DrawData &&
                                            vp->DrawData->TotalVtxCount > 0;
                    }
                if (ticks == 14)
                    for (int i = 1; i < platform.Viewports.Size; ++i)
                        SendMessageW(static_cast<HWND>(platform.Viewports[i]->PlatformHandle), WM_CLOSE, 0,
                                     0);
                if (ticks == 20) {
                    unsigned live = 0;
                    for (int i = 1; i < platform.Viewports.Size; ++i)
                        live += IsWindow(static_cast<HWND>(platform.Viewports[i]->PlatformHandle)) ? 1 : 0;
                    viewportClosed = live == 0 && !closing;
                    sf::Json report = sf::Json::Object();
                    report["nativeWindowRendered"] = viewportRendered;
                    report["secondaryClosePreservesMain"] = viewportClosed;
                    report["remainingViewports"] = platform.Viewports.Size;
                    report["liveSecondaryWindows"] = live;
                    report["mainCloseRequested"] = closing;
                    report["compositionVisible"] = ImGui::FindWindowByName("Composition")->Active;
                    std::ofstream f(sx::Utf8Path(viewportReport), std::ios::binary);
                    f << report.Dump(2);
                    if (!f || !viewportRendered || !viewportClosed)
                        exitCode = 1;
                    closing = true;
                }
            }
            swapChain->Present(hidden ? 0 : 1, 0);
            if (hidden)
                Sleep(10);
        }
        // Finish cancellation and release worker COM/GPU objects before runtime shutdown.
        session.exports.shutdown();
        session.syncPluginEditors(true);
        if (session.dirty || ui.hasClipDrafts())
            session.recovery.capture(ui.projectWithClipDrafts(), session.projectPath, session.time);
        else
            session.recovery.clear();
        editor = nullptr;
        stop = true;
        if (input.joinable()) {
            CancelSynchronousIo(input.native_handle());
            input.join();
        }
        ImGui::DestroyPlatformWindows();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    } catch (const std::exception &e) {
        exitCode = 1;
        std::ofstream log("shaderixframe-error.log", std::ios::binary);
        log << e.what();
        if (!hidden && !mcp)
            MessageBoxW(window, sx::Widen(e.what()).c_str(), L"ShaderixFrame", MB_ICONERROR);
        stop = true;
        if (input.joinable()) {
            CancelSynchronousIo(input.native_handle());
            input.join();
        }
    }
    backTarget.Reset();
    swapChain.Reset();
    context.Reset();
    device.Reset();
    if (window)
        DestroyWindow(window);
    MFShutdown();
    CoUninitialize();
    return exitCode;
}

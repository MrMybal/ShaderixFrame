#include "ui/EnglishPanels.h"
#include "ui/Editor.h"
#include "ui/UiPreferences.h"
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
    sf::check(device->CreateRenderTargetView(t.Get(), nullptr, &backTarget), "GPU window");
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
    case WM_DPICHANGED: {
        const auto* rect=reinterpret_cast<RECT*>(b);
        SetWindowPos(w,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;
    }
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
    std::string manualTopic, manualLanguage, helpAnchor, helpMedia, helpNote, helpReport;
    std::string uiTheme, uiDensity, uiLocale;float uiScale=0;
    int snapshotFrames = 10, startMode = -1;
    std::string modeReport, modeIni;
    sf::Json modeResults = sf::Json::Array();
    bool modeFailure = false;
    float timelineZoom = 0, viewZoom = 0;
    double timelineStart = 0, startTime = -1;
    std::string sourceMedia;
    bool prerender = false, compare = false;
    int selectClip = -1, clipMenu = -1;
    std::string paletteQuery;
    int browserKind = -1;
    bool palette = false;
    int guides = 0;
    bool viewportRendered = false, viewportClosed = false;
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if(a.starts_with(L"--ui-theme="))uiTheme=sx::Narrow(a.substr(11));
        else if(a.starts_with(L"--ui-density="))uiDensity=sx::Narrow(a.substr(13));
        else if(a.starts_with(L"--help-topic="))manualTopic=sx::Narrow(a.substr(13));
        else if(a.starts_with(L"--help-language="))manualLanguage=sx::Narrow(a.substr(16));
        else if(a.starts_with(L"--help-section="))helpAnchor=sx::Narrow(a.substr(15));
        else if(a.starts_with(L"--help-media="))helpMedia=sx::Narrow(a.substr(13));
        else if(a.starts_with(L"--help-note="))helpNote=sx::Narrow(a.substr(12));
        else if(a.starts_with(L"--help-url-report="))helpReport=sx::Narrow(a.substr(18));
        else if(a.starts_with(L"--manual-topic="))manualTopic=sx::Narrow(a.substr(15));
        else if(a.starts_with(L"--manual-language="))manualLanguage=sx::Narrow(a.substr(18));
        else if(a.starts_with(L"--ui-locale="))uiLocale=sx::Narrow(a.substr(12));
        else if(a.starts_with(L"--ui-scale="))uiScale=std::clamp(float(_wtof(a.substr(11).c_str())),.5f,4.f);
        else if (a == L"--mcp")
            mcp = true;
        else if (a == L"--hidden")
            hidden = true;
        else if (a == L"--warp")
            SetEnvironmentVariableW(L"SF_FORCE_WARP", L"1");
        else if (a.starts_with(L"--ui-snapshot="))
            snapshot = sx::Narrow(a.substr(14));
        else if (a.starts_with(L"--viewport-smoke="))
            viewportReport = sx::Narrow(a.substr(17));
        else if (a.starts_with(L"--mode-smoke="))
            modeReport = sx::Narrow(a.substr(13));
        else if (a.starts_with(L"--snapshot-frames="))
            snapshotFrames = std::clamp(_wtoi(a.substr(18).c_str()), 2, 2000);
        else if (a.starts_with(L"--mode="))
            startMode = std::clamp(_wtoi(a.substr(7).c_str()), 0, sf::Editor::modeCount - 1);
        else if (a.starts_with(L"--timeline-zoom="))
            timelineZoom = float(_wtof(a.substr(16).c_str()));
        else if (a.starts_with(L"--timeline-start="))
            timelineStart = _wtof(a.substr(17).c_str());
        else if (a.starts_with(L"--browser="))
            browserKind = _wtoi(a.substr(10).c_str());
        else if (a.starts_with(L"--palette=")) {
            palette = true;
            paletteQuery = sx::Narrow(a.substr(10));
        } else if (a.starts_with(L"--clip-menu="))
            clipMenu = _wtoi(a.substr(12).c_str());
        else if (a.starts_with(L"--select-clip="))
            selectClip = _wtoi(a.substr(14).c_str());
        else if (a == L"--compare")
            compare = true;
        else if (a.starts_with(L"--guides="))
            guides = _wtoi(a.substr(9).c_str());
        else if (a == L"--prerender")
            prerender = true;
        else if (a.starts_with(L"--source="))
            sourceMedia = sx::Narrow(a.substr(9));
        else if (a.starts_with(L"--time="))
            startTime = _wtof(a.substr(7).c_str());
        else if (a.starts_with(L"--view-zoom="))
            viewZoom = float(_wtof(a.substr(12).c_str()));
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
        window = CreateWindowW(cls.lpszClassName, L"ShaderixFrame - Video Editing & VFX", WS_OVERLAPPEDWINDOW,
                               100, 70, 1600, 980, nullptr, nullptr, cls.hInstance, nullptr);
        if (!window)
            throw std::runtime_error("Window creation failed");
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
                      "Window Direct3D");
        target();
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        if (!hidden || !viewportReport.empty() || !modeReport.empty())
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        std::string ini = sx::Narrow((root / L"workspace.ini").wstring());
        if (!hidden && snapshot.empty())
            sf::migrateEnglishWorkspace(ini);
        io.IniFilename = !hidden && snapshot.empty() ? ini.c_str() : nullptr;
        // The workspace smoke keeps its own settings next to its report.
        if (!modeReport.empty()) {
            modeIni = modeReport + ".ini";
            io.IniFilename = modeIni.c_str();
        }
        sf::applyFrameTheme();
        sf::initializeUiPreferences(!hidden&&snapshot.empty()?root/"ui-preferences.json":std::filesystem::path{},ImGui_ImplWin32_GetDpiScaleForHwnd(window));
        if(!uiTheme.empty())cyui::setTheme(uiTheme.c_str());
        if(!uiDensity.empty())cyui::setDensity(uiDensity=="comfortable"?cyui::Density::comfortable:cyui::Density::compact);
        if(!uiLocale.empty())cyui::setLocale(uiLocale.c_str());
        io.ConfigDpiScaleViewports=true;
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(device.Get(), context.Get());
        if (hidden && (!viewportReport.empty() || !modeReport.empty()))
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
        if (startTime >= 0)
            session.time = std::min(startTime, session.project.duration);
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
        if (!modeReport.empty())
            ui.setPanelSettingsFile(sx::Utf8Path(modeReport + ".panels.json"));
        if (startMode >= 0)
            ui.setMode(sf::Editor::Mode(startMode));
        if (timelineZoom > 0)
            ui.setTimelineView(timelineZoom, timelineStart);
        if (viewZoom > 0)
            ui.setCompositionPixelZoom(viewZoom / 100);

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
            cyui::applyDpi(uiScale>0?uiScale:ImGui_ImplWin32_GetDpiScaleForHwnd(window));
            ImGui::NewFrame();
            ui.draw();
            ++ticks;
            // After the first frames, when the workspace mode has set its panels.
            if (ticks == 2 && selectClip >= 0)
                ui.selectClipAt(selectClip);
            if (ticks == 10 && clipMenu >= 0)
                ui.openClipMenuAt(clipMenu);
            if (ticks == 3 && browserKind >= 0)
                ui.setBrowserKind(browserKind);
            if (ticks == 10 && palette)
                ui.openPalette(paletteQuery);
            if (ticks == 3 && guides)
                ui.setGuides(guides & 7, guides >> 3);
            if (ticks == 20 && compare) {
                ui.takeSnapshot();
                ui.setWipe(.42f);
            }
            if (ticks == 3 && prerender)
                ui.startRenderCache();
            if (ticks == 3 && !sourceMedia.empty())
                ui.openSource(sx::Narrow(std::filesystem::absolute(sx::Utf8Path(sourceMedia)).wstring()));
            if (ticks == 3 && (view == "manual" || !manualTopic.empty() || !helpReport.empty())) {
                sf::HelpDocumentation::State state;
                if (!helpMedia.empty()) state.emplace_back("media", helpMedia);
                if (!helpNote.empty()) state.emplace_back("note", helpNote);
                if (!helpReport.empty()) {
                    sf::Json report = sf::Json::Object();
                    auto url = ui.helpUrl(manualTopic.empty() ? "welcome" : manualTopic,
                                          manualLanguage.empty() ? "en" : manualLanguage, helpAnchor, state);
                    report["url"] = url;
                    report["error"] = ui.documentation().error();
                    std::ofstream stream(sx::Utf8Path(helpReport), std::ios::binary);
                    stream << report.Dump(2);
                    if (!stream || url.empty()) exitCode = 1;
                    closing = true;
                } else if (!hidden) ui.showManual(manualTopic, manualLanguage, helpAnchor, state);
            }
            if (ticks == 3 && !view.empty() && view != "manual") {
                if (view == "clip")
                    ui.showClipEditor();
                if (view == "grading")
                    ui.showGrading();
                const char *panel = view == "layers"       ? "Layer Graph"
                                    : view == "graph"      ? "Shader Graph"
                                    : view == "grading"    ? "Color Grading"
                                    : view == "scopes"     ? "Video Scopes"
                                    : view == "hlsl"       ? "HLSL"
                                    : view == "audio"      ? "Mixer & Audio Graph"
                                    : view == "dsp"        ? "DSP Graph"
                                    : view == "motion"     ? "Motion Design"
                                    : view == "library"    ? "Shaderixel Library"
                                    : view == "clip"       ? "Clip Editor"
                                    : view == "animations" ? "Animation"
                                    : view == "plugins"    ? "Plugins & MCP"
                                    : view == "exports"    ? "Exports"
                                    : view == "markers"    ? "Markers"
                                    : view == "history"    ? "History"
                                    : view == "subtitles"  ? "Subtitles"
                                    : view == "angles"     ? "Camera Angles"
                                    : view == "shortcuts"  ? "Keyboard Shortcuts"
                                    : view == "track-effects" ? "Track Effects"
                                    : view == "track-graph" ? "Track Graph"
                                    : view == "effects-browser" ? "Effects Browser"
                                                           : "Timeline";
                ui.openPanel(panel);
                ImGui::SetWindowFocus(panel);
                if (view == "dsp")
                    ui.showDspGraph();
            }
            // Workspace smoke: Composition detached to its own native window, then two rounds of the six
            // modes. Each mode must show a docked timeline and keep the detached window where it is.
            if (!modeReport.empty()) {
                if (ticks == 2) {
                    sf::Json start = sf::Json::Object();
                    start["storedAtStart"] = ui.storedModeLayouts();
                    modeResults.Push(start);
                }
                if (ticks == 3) {
                    ImGui::SetWindowDock(ImGui::FindWindowByName("Composition"), 0, ImGuiCond_Always);
                    ImGui::SetWindowPos("Composition", {1760, 100}, ImGuiCond_Always);
                    ImGui::SetWindowSize("Composition", {600, 400}, ImGuiCond_Always);
                }
                if (ticks >= 12 && ticks < 12 + 6 * 12 && (ticks - 12) % 6 == 5) {
                    auto *timeline = ImGui::FindWindowByName("Timeline");
                    auto *composition = ImGui::FindWindowByName("Composition");
                    bool docked = timeline && timeline->Active && timeline->DockIsActive;
                    bool detached = !composition || !composition->Active ||
                                    (!composition->DockIsActive && composition->Viewport != ImGui::GetMainViewport());
                    sf::Json visit = sf::Json::Object();
                    visit["mode"] = int(ui.currentMode());
                    visit["timelineDocked"] = docked;
                    visit["compositionDetached"] = detached;
                    modeResults.Push(visit);
                    modeFailure = modeFailure || !docked || !detached;
                }
                if (ticks >= 12 && ticks < 12 + 6 * 12 && (ticks - 12) % 6 == 0)
                    ui.setMode(sf::Editor::Mode(((ticks - 12) / 6 + 1) % sf::Editor::modeCount));
                if (ticks == 12 + 6 * 12 + 2) {
                    sf::Json report = sf::Json::Object();
                    report["visits"] = modeResults;
                    report["storedLayouts"] = ui.storedModeLayouts();
                    report["ok"] = !modeFailure && ui.storedModeLayouts() >= sf::Editor::modeCount - 1;
                    std::ofstream f(sx::Utf8Path(modeReport), std::ios::binary);
                    f << report.Dump(2);
                    if (!f || !report["ok"].B())
                        exitCode = 1;
                    closing = true;
                }
            }
            if (!viewportReport.empty() && ticks == 3 && view != "clip") {
                auto *composition = ImGui::FindWindowByName("Composition");
                ImGui::SetWindowDock(composition, 0, ImGuiCond_Always);
                ImGui::SetWindowPos("Composition", {1760, 100}, ImGuiCond_Always);
                ImGui::SetWindowSize("Composition", {600, 400}, ImGuiCond_Always);
            }
            cyui::renderNotifications();
            ImGui::Render();
            float bg[] = {.05f, .06f, .075f, 1};
            auto *rt = backTarget.Get();
            context->OMSetRenderTargets(1, &rt, nullptr);
            context->ClearRenderTargetView(rt, bg);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (!snapshot.empty() && frames < snapshotFrames && snapshotFrames > 10)
                Sleep(25); // background thumbnails get time to arrive before the capture
            if (!snapshot.empty() && ++frames == snapshotFrames) {
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

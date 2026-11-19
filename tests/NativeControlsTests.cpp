#include "ui/NativeControls.h"
#include <atomic>
#include <future>
#include <iostream>
#include <objbase.h>
#include <thread>
static std::atomic<int> invocations = 0;
static LRESULT CALLBACK proc(HWND w, UINT message, WPARAM a, LPARAM b) {
    if (message == WM_COMMAND && LOWORD(a) == 101 && HIWORD(a) == BN_CLICKED)
        ++invocations;
    return DefWindowProcW(w, message, a, b);
}
int main() {
    auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW cls{};
    cls.lpfnWndProc = proc;
    cls.hInstance = instance;
    cls.lpszClassName = L"ShaderixFrameAutomationFixture";
    RegisterClassW(&cls);
    auto root = CreateWindowW(cls.lpszClassName, L"Hidden native automation fixture", WS_OVERLAPPEDWINDOW, 0,
                              0, 400, 240, nullptr, nullptr, instance, nullptr);
    auto edit = CreateWindowW(L"EDIT", L"Before", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 20, 20, 250, 30, root,
                              reinterpret_cast<HMENU>(100), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Invoke fixture", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 60, 150, 30, root,
                  reinterpret_cast<HMENU>(101), instance, nullptr);
    // Keep the fixture hidden throughout. UIA operates on its semantic providers, never on desktop input.
    auto worker = std::async(std::launch::async, [root] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        try {
            auto windows = sf::nativeWindows({GetCurrentProcessId()});
            std::string token;
            for (auto &w : windows.at("windows"))
                if (w.at("title") == "Hidden native automation fixture")
                    token = w.at("window");
            if (token.empty())
                throw std::runtime_error("Fixture window not found");
            bool denied = false;
            try {
                sf::nativeControls({}, {{"window", token}});
            } catch (const cymcp::Error &e) {
                denied = e.code == "AccessDenied";
            }
            if (!denied)
                throw std::runtime_error("Unowned native window was accepted");
            auto rows = sf::nativeControls({GetCurrentProcessId()}, {{"window", token}}).at("controls");
            std::string input, button;
            for (auto &r : rows) {
                if (r.at("control_type") == 50004)
                    input = r.at("id").get<std::string>(); // UIA_EditControlTypeId
                if (r.at("name") == "Invoke fixture")
                    button = r.at("id").get<std::string>();
                if (r.contains("value"))
                    throw std::runtime_error("Native discovery must not expose input contents");
            }
            if (input.empty() || button.empty())
                throw std::runtime_error("Native controls not found");
            sf::nativeControl({GetCurrentProcessId()},
                              {{"window", token}, {"id", input}, {"action", "set_value"}, {"text", "After"}});
            sf::nativeControl({GetCurrentProcessId()},
                              {{"window", token}, {"id", button}, {"action", "invoke"}});
            CoUninitialize();
            return std::string{};
        } catch (const std::exception &e) {
            CoUninitialize();
            return std::string(e.what());
        }
    });
    auto deadline = GetTickCount64() + 15000;
    while (worker.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
           GetTickCount64() < deadline) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(1);
    }
    auto error = worker.get();
    for (int i = 0; i < 100 && invocations == 0; ++i) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(2);
    }
    wchar_t value[64]{};
    GetWindowTextW(edit, value, 64);
    bool ok = error.empty() && std::wstring(value) == L"After" && invocations > 0 && !IsWindowVisible(root);
    DestroyWindow(root);
    UnregisterClassW(cls.lpszClassName, instance);
    if (!ok) {
        std::cerr << "Native UI fixture: " << error << "; invocations=" << invocations << "\n";
        return 1;
    }
    std::cout << "Hidden native UIA fixture: scoped discovery, denied foreign window, set_value and real "
                 "button invocation passed\n";
}

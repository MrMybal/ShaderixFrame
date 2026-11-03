#include "cyui/adapters/WebCEF.h"
#if defined(OS_WIN)
#include <windows.h>
#endif
#if defined(OS_MAC)
#include "include/wrapper/cef_library_loader.h"
#endif
int main(int argc, char **argv) {
#if defined(OS_MAC)
    CefScopedLibraryLoader loader;
    if (!loader.LoadInHelper())
        return 1;
#endif
#if defined(OS_WIN)
    (void)argc;
    (void)argv;
    CefMainArgs args(GetModuleHandleW(nullptr));
#else
    CefMainArgs args(argc, argv);
#endif
    // No sandbox info: on Windows these subprocesses run unsandboxed, matching the browser
    // process (settings.no_sandbox = true). A sandboxed Windows host is built as a DLL launched
    // by CEF's bootstrap(c).exe, which passes sandbox_info to RunWinMain/RunConsoleMain for both
    // CefExecuteProcess and CefInitialize; on macOS the helper bundle initializes
    // CefScopedSandboxContext first. See docs/WEB.md "Process sandbox".
    const auto result = CefExecuteProcess(args, cyui::webCEFApp(), nullptr);
    return result >= 0 ? result : 2;
}

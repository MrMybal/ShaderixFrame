#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "Platform.h"
namespace platform {
Clipboard nativeClipboard() {
    Clipboard result;
    if (!OpenClipboard(nullptr))
        return result;
    if (auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP))) {
        const auto count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
        for (UINT n = 0; n < count; ++n) {
            std::wstring file(DragQueryFileW(drop, n, nullptr, 0) + 1, L'\0');
            DragQueryFileW(drop, n, file.data(), UINT(file.size()));
            file.pop_back();
            result.files.emplace_back(file);
        }
    }
    const auto data = GetClipboardData(RegisterClipboardFormatW(L"HTML Format"));
    const auto size = data ? GlobalSize(data) : 0;
    if (size && size <= 16 * 1024 * 1024) {
        if (auto *bytes = static_cast<const char *>(GlobalLock(data))) {
            std::size_t length = 0;
            while (length < size && bytes[length])
                ++length;
            result.html.assign(bytes, length);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return result; // SDL supplies PNG and DIB/BMP images.
}
} // namespace platform

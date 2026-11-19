#include "ui/NativeControls.h"
#include <objbase.h>
#include <oleauto.h>

#include <UIAutomation.h>
#include <algorithm>
#include <charconv>
#include <set>
#include <string>
#include <wrl/client.h>
namespace sf {
namespace {
using J = cymcp::Json;
template <class T> using Ptr = Microsoft::WRL::ComPtr<T>;
void checked(HRESULT hr) {
    if (FAILED(hr))
        throw cymcp::Error(
            "NativeControlUnavailable",
            "The native control does not expose this accessibility operation or is no longer available");
}
std::string narrow(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
std::wstring widen(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
    if (!n)
        throw cymcp::Error("InvalidArguments", "Text must be UTF-8");
    std::wstring out(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), out.data(), n);
    return out;
}
std::string name(IUIAutomationElement *e) {
    BSTR value = nullptr;
    checked(e->get_CurrentName(&value));
    std::wstring text = value ? std::wstring(value, SysStringLen(value)) : L"";
    SysFreeString(value);
    return narrow(text);
}
std::string token(HWND w) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    return std::to_string(pid) + ":" + std::to_string(reinterpret_cast<uintptr_t>(w));
}
HWND window(const std::vector<DWORD> &allowed, const J &a) {
    auto value = a.at("window").get<std::string>();
    auto split = value.find(':');
    uintptr_t raw = 0;
    DWORD claimed = 0;
    if (split == std::string::npos)
        throw cymcp::Error("InvalidArguments", "Use the window token returned by get_ui_native_windows");
    auto p = std::from_chars(value.data(), value.data() + split, claimed);
    auto h = std::from_chars(value.data() + split + 1, value.data() + value.size(), raw);
    auto w = reinterpret_cast<HWND>(raw);
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (p.ec != std::errc{} || p.ptr != value.data() + split || h.ec != std::errc{} ||
        h.ptr != value.data() + value.size() || !IsWindow(w) || GetAncestor(w, GA_ROOT) != w ||
        pid != claimed || std::find(allowed.begin(), allowed.end(), pid) == allowed.end())
        throw cymcp::Error("AccessDenied",
                           "The window is not owned by a permitted plugin editor in this workspace");
    return w;
}
Ptr<IUIAutomation> client() {
    Ptr<IUIAutomation2> extended;
    checked(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&extended)));
    checked(extended->put_ConnectionTimeout(1000));
    checked(extended->put_TransactionTimeout(2000));
    Ptr<IUIAutomation> result;
    checked(extended.As(&result));
    return result;
}
std::string runtime(IUIAutomationElement *e) {
    SAFEARRAY *array = nullptr;
    if (FAILED(e->GetRuntimeId(&array)) || !array)
        return {};
    LONG low = 0, high = -1;
    SafeArrayGetLBound(array, 1, &low);
    SafeArrayGetUBound(array, 1, &high);
    std::string out;
    for (LONG i = low; i <= high && i < low + 64; ++i) {
        int n = 0;
        SafeArrayGetElement(array, &i, &n);
        if (!out.empty())
            out += '.';
        out += std::to_string(n);
    }
    SafeArrayDestroy(array);
    return out;
}
std::vector<Ptr<IUIAutomationElement>> elements(IUIAutomation *uia, HWND w) {
    Ptr<IUIAutomationElement> root;
    checked(uia->ElementFromHandle(w, &root));
    Ptr<IUIAutomationCondition> condition;
    checked(uia->CreateTrueCondition(&condition));
    Ptr<IUIAutomationElementArray> all;
    checked(root->FindAll(TreeScope_Subtree, condition.Get(), &all));
    int count = 0;
    checked(all->get_Length(&count));
    std::vector<Ptr<IUIAutomationElement>> result;
    std::set<std::string> seen;
    for (int i = 0; i < std::min(count, 4096); ++i) {
        Ptr<IUIAutomationElement> e;
        checked(all->GetElement(i, &e));
        seen.insert(runtime(e.Get()));
        result.push_back(e);
    }
    // Hidden standard child windows may be absent from the UIA tree. Resolve their real
    // providers by HWND as well; this also covers native controls in plugin-owned dialogs.
    std::vector<HWND> children;
    EnumChildWindows(
        w,
        [](HWND child, LPARAM data) -> BOOL {
            auto &list = *reinterpret_cast<std::vector<HWND> *>(data);
            if (list.size() == 4096)
                return FALSE;
            list.push_back(child);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&children));
    for (auto child : children) {
        Ptr<IUIAutomationElement> e;
        if (SUCCEEDED(uia->ElementFromHandle(child, &e)) && e) {
            auto id = runtime(e.Get());
            if (!id.empty() && seen.insert(id).second)
                result.push_back(e);
        }
    }
    return result;
}
Ptr<IUIAutomationElement> find(const std::vector<Ptr<IUIAutomationElement>> &all, const std::string &id) {
    for (auto &e : all) {
        if (runtime(e.Get()) == id)
            return e;
    }
    throw cymcp::Error("NotFound", "Native control changed; query get_ui_native_controls again");
}
template <class T> Ptr<T> pattern(IUIAutomationElement *e, PATTERNID id) {
    Ptr<T> out;
    checked(e->GetCurrentPatternAs(id, IID_PPV_ARGS(&out)));
    return out;
}
} // namespace
J nativeWindows(const std::vector<DWORD> &allowed) {
    struct Data {
        const std::vector<DWORD> &pids;
        J rows = J::array();
    } data{allowed};
    EnumWindows(
        [](HWND w, LPARAM raw) -> BOOL {
            auto &d = *reinterpret_cast<Data *>(raw);
            DWORD pid = 0;
            GetWindowThreadProcessId(w, &pid);
            if (std::find(d.pids.begin(), d.pids.end(), pid) == d.pids.end())
                return TRUE;
            wchar_t text[1024]{};
            GetWindowTextW(w, text, 1024);
            RECT r{};
            GetWindowRect(w, &r);
            d.rows.push_back({{"window", token(w)},
                              {"title", narrow(text)},
                              {"visible", bool(IsWindowVisible(w))},
                              {"rect", {r.left, r.top, r.right, r.bottom}}});
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data));
    return {{"windows", data.rows}};
}
J nativeControls(const std::vector<DWORD> &allowed, const J &a) {
    auto w = window(allowed, a);
    auto uia = client();
    auto all = elements(uia.Get(), w);
    int count = int(all.size());
    int offset = std::clamp(a.value("offset", 0), 0, 4096), limit = std::clamp(a.value("limit", 100), 1, 200);
    J rows = J::array();
    for (int i = offset; i < std::min({count, offset + limit, 4096}); ++i) {
        auto e = all[size_t(i)];
        BOOL enabled = false, password = false;
        CONTROLTYPEID kind = 0;
        RECT rect{};
        e->get_CurrentIsEnabled(&enabled);
        e->get_CurrentIsPassword(&password);
        e->get_CurrentControlType(&kind);
        e->get_CurrentBoundingRectangle(&rect);
        J patterns = J::array();
        for (auto [id, label] : {std::pair{UIA_InvokePatternId, "invoke"},
                                 {UIA_ValuePatternId, "set_value"},
                                 {UIA_RangeValuePatternId, "set_range"},
                                 {UIA_TogglePatternId, "toggle"},
                                 {UIA_SelectionItemPatternId, "select"},
                                 {UIA_ExpandCollapsePatternId, "expand/collapse"}}) {
            Ptr<IUnknown> p;
            if (SUCCEEDED(e->GetCurrentPattern(id, &p)) && p)
                patterns.push_back(label);
        }
        rows.push_back({{"id", runtime(e.Get())},
                        {"name", name(e.Get())},
                        {"enabled", bool(enabled)},
                        {"password", bool(password)},
                        {"control_type", kind},
                        {"rect", {rect.left, rect.top, rect.right, rect.bottom}},
                        {"actions", patterns}});
    }
    return {
        {"controls", rows},
        {"total", count},
        {"scan_capped", count > 4096},
        {"next_offset", offset + int(rows.size()) < std::min(count, 4096) ? J(offset + rows.size()) : J()}};
}
J nativeControl(const std::vector<DWORD> &allowed, const J &a) {
    auto w = window(allowed, a);
    auto uia = client();
    auto all = elements(uia.Get(), w);
    auto e = find(all, a.at("id"));
    BOOL enabled = false;
    checked(e->get_CurrentIsEnabled(&enabled));
    if (!enabled)
        throw cymcp::Error("Disabled", "Native control is disabled");
    auto action = a.at("action").get<std::string>();
    if (action == "invoke")
        checked(pattern<IUIAutomationInvokePattern>(e.Get(), UIA_InvokePatternId)->Invoke());
    else if (action == "set_value") {
        auto value = widen(a.value("text", std::string{}));
        if (value.size() > 65536)
            throw cymcp::Error("InvalidArguments", "Native input exceeds 64 KiB");
        auto edit = pattern<IUIAutomationValuePattern>(e.Get(), UIA_ValuePatternId);
        auto bstr = SysAllocStringLen(value.data(), UINT(value.size()));
        if (!bstr)
            throw cymcp::Error("MemoryLimit", "Cannot allocate native input");
        auto result = edit->SetValue(bstr);
        SysFreeString(bstr);
        checked(result);
    } else if (action == "set_range")
        checked(pattern<IUIAutomationRangeValuePattern>(e.Get(), UIA_RangeValuePatternId)
                    ->SetValue(a.at("value").get<double>()));
    else if (action == "toggle")
        checked(pattern<IUIAutomationTogglePattern>(e.Get(), UIA_TogglePatternId)->Toggle());
    else if (action == "select")
        checked(pattern<IUIAutomationSelectionItemPattern>(e.Get(), UIA_SelectionItemPatternId)->Select());
    else if (action == "expand")
        checked(pattern<IUIAutomationExpandCollapsePattern>(e.Get(), UIA_ExpandCollapsePatternId)->Expand());
    else if (action == "collapse")
        checked(
            pattern<IUIAutomationExpandCollapsePattern>(e.Get(), UIA_ExpandCollapsePatternId)->Collapse());
    else if (action == "focus")
        checked(e->SetFocus());
    else
        throw cymcp::Error("InvalidArguments", "Unknown native accessibility action");
    return {{"delivered", true}, {"window", token(w)}, {"id", runtime(e.Get())}};
}
} // namespace sf

#pragma once
#include <cymcp/core.hpp>
#include <vector>
#include <windows.h>
namespace sf {
// Only windows of explicitly owned plugin-worker processes are admitted by the host.
cymcp::Json nativeWindows(const std::vector<DWORD> &);
cymcp::Json nativeControls(const std::vector<DWORD> &, const cymcp::Json &);
cymcp::Json nativeControl(const std::vector<DWORD> &, const cymcp::Json &);
} // namespace sf

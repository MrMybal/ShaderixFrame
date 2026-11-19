#include "Platform.h"
namespace platform {
Clipboard nativeClipboard() {
    return {};
} // Linux uses SDL's MIME clipboard API.
} // namespace platform

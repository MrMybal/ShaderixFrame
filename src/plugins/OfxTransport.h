#pragma once
#include <cstdint>
namespace sf {
struct OfxShared {
    uint32_t magic = 0x53464f58, version = 1, width = 0, height = 0;
    alignas(8) uint64_t request = 0, response = 0;
    uint32_t ready = 0, quit = 0, result = 0, reserved = 0;
    double time = 0, fps = 30, duration = 1;
    char identifier[1024]{}, message[1024]{}, metadata[65536]{}, parameters[65536]{};
    // Followed by two straight RGBA float images, top-down, width*height*4 each.
};
inline uint64_t ofxBytes(unsigned w, unsigned h) {
    return sizeof(OfxShared) + uint64_t(w) * h * 32;
}
} // namespace sf

#include "Raster.h"
#define STBI_MAX_DIMENSIONS 16384
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <cstdint>
#include <memory>
namespace raster {
Image decode(const unsigned char *data, std::size_t size) {
    Image result;
    int w = 0, h = 0, channels = 0;
    if (!data || !size || size > 64 * 1024 * 1024 ||
        !stbi_info_from_memory(data, int(size), &w, &h, &channels) || w <= 0 || h <= 0 || w > 16384 ||
        h > 16384 || std::uint64_t(w) * h > 32 * 1024 * 1024)
        return result;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(data, int(size), &w, &h, &channels, 4), stbi_image_free);
    if (pixels) {
        result.width = w;
        result.height = h;
        result.pixels.assign(pixels.get(), pixels.get() + std::size_t(w) * h * 4);
    }
    return result;
}
std::vector<unsigned char> png(const unsigned char *pixels, int width, int height) {
    std::vector<unsigned char> out;
    if (!pixels || width <= 0 || height <= 0 || width > 16384 || height > 16384)
        return out;
    stbi_write_png_to_func(
        [](void *ctx, void *data, int length) {
            auto &bytes = *static_cast<std::vector<unsigned char> *>(ctx);
            auto *begin = static_cast<unsigned char *>(data);
            bytes.insert(bytes.end(), begin, begin + length);
        },
        &out, width, height, 4, pixels, width * 4);
    return out;
}
} // namespace raster

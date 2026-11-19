#pragma once
#include <cstddef>
#include <vector>
namespace raster {
struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> pixels;
};
Image decode(const unsigned char *, std::size_t);
std::vector<unsigned char> png(const unsigned char *, int width, int height);
} // namespace raster

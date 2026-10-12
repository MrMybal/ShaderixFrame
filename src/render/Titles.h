#pragma once
#include "media/Media.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

namespace sf {
class TitleRasterizer {
    struct Cached {
        std::string signature;
        Image image;
    };
    Com<ID2D1Factory> drawing;
    Com<IDWriteFactory> writing;
    Com<IWICImagingFactory> imaging;
    std::map<Id, Cached> cache;

  public:
    const Image &at(const Clip &clip, double time, unsigned width, unsigned height, int projectHeight);
    void retain(const Project &project);
};
} // namespace sf

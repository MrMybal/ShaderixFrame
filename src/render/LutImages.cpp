#include "render/Grading.h"
namespace sf {
Json loadLut(const std::filesystem::path &path, const std::string &layout) {
    if (layout != "auto" && layout != "rg" && layout != "atlas")
        throw std::runtime_error("LUT layout: auto, rg or atlas");
    if (path.extension() == L".cube" || path.extension() == L".CUBE")
        return loadCube(path);
    MediaAsset asset;
    asset.open(sx::Narrow(path.wstring()));
    auto &image = asset.frame;
    if (!image.width || !image.height || uint64_t(image.width) * image.height > 4 * 1024 * 1024)
        throw std::runtime_error("Empty LUT image or more than four million pixels");
    unsigned n = unsigned(std::llround(std::cbrt(double(image.width) * image.height))), columns = 0;
    bool atlas = layout != "rg" && n >= 2 && n <= 65 &&
                 uint64_t(n) * n * n == uint64_t(image.width) * image.height && image.width % n == 0 &&
                 image.height % n == 0;
    if (atlas)
        columns = image.width / n;
    else {
        if (layout == "atlas")
            throw std::runtime_error("LUT atlas: N cubed pixels arranged in N by N tiles");
        if (image.width != image.height)
            throw std::runtime_error("A 2D RG LUT must be square");
        n = std::min(65u, image.width);
    }
    if (n < 2)
        throw std::runtime_error("At least two LUT levels required");
    Json lut = Json::Object();
    lut["dimension"] = 3;
    lut["size"] = n;
    lut["sourceDimension"] = 2;
    lut["layout"] = atlas ? "atlas" : "rg";
    lut["title"] = sx::Narrow(path.filename().wstring());
    lut["min"] = Json::Array();
    lut["max"] = Json::Array();
    for (int c = 0; c < 3; ++c) {
        lut["min"].Push(0);
        lut["max"].Push(1);
    }
    lut["data"] = Json::Array();
    for (unsigned z = 0; z < n; ++z)
        for (unsigned y = 0; y < n; ++y)
            for (unsigned x = 0; x < n; ++x) {
                unsigned px = atlas ? (z % columns) * n + x
                                    : unsigned(std::llround(x * double(image.width - 1) / (n - 1)));
                unsigned py = atlas ? (z / columns) * n + y
                                    : unsigned(std::llround(y * double(image.height - 1) / (n - 1)));
                size_t at = (size_t(py) * image.width + px) * 4;
                for (int c = 0; c < 3; ++c)
                    lut["data"].Push(image.rgba.empty() ? image.pixels[at + 2 - c] / 255.f
                                                        : image.rgba[at + c]);
            }
    auto grade = makeGrade();
    grade.grading["lut"] = lut;
    validateGrade(grade);
    return lut;
}
} // namespace sf

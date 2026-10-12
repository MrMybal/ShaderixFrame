#include "core/AtomicFile.h"
#include "media/Media.h"
#include "render/Color.h"
#include <DirectXPackedVector.h>
#include <fstream>
#include <set>
#include <sstream>
#include <wincodec.h>
namespace sf {
using DirectX::PackedVector::XMConvertFloatToHalf;
using DirectX::PackedVector::XMConvertHalfToFloat;
static void dimensions(unsigned w, unsigned h, size_t count) {
    if (!w || !h || uint64_t(w) * h > 67108864 || count != uint64_t(w) * h * 4)
        throw std::runtime_error("Pixels haute précision invalides");
}
void writePng16(const FloatImage &i, const std::string &path) {
    dimensions(i.width, i.height, i.rgba.size());
    std::vector<uint16_t> pixels(i.rgba.size());
    for (size_t n = 0; n < pixels.size(); n += 4) {
        auto c = convertColor({i.rgba[n], i.rgba[n + 1], i.rgba[n + 2]}, i.colorSpace, "srgb");
        for (int k = 0; k < 4; ++k) {
            float v = k == 3 ? i.rgba[n + 3] : c[k];
            if (!std::isfinite(v))
                throw std::runtime_error("PNG : valeur non finie");
            pixels[n + k] = uint16_t(std::lround(std::clamp(v, 0.f, 1.f) * 65535));
        }
    }
    AtomicFile output(path);
    Com<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)),
          "WIC 16 bits");
    Com<IWICStream> stream;
    check(factory->CreateStream(&stream), "Flux PNG");
    check(stream->InitializeFromFilename(output.path().c_str(), GENERIC_WRITE), "Fichier PNG16");
    Com<IWICBitmapEncoder> encoder;
    check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "Encodeur PNG16");
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Initialisation PNG16");
    Com<IWICBitmapFrameEncode> frame;
    Com<IPropertyBag2> options;
    check(encoder->CreateNewFrame(&frame, &options), "Image PNG16");
    check(frame->Initialize(options.Get()), "Frame PNG16");
    check(frame->SetSize(i.width, i.height), "Taille PNG16");
    auto format = GUID_WICPixelFormat64bppRGBA;
    check(frame->SetPixelFormat(&format), "Format PNG16");
    if (format != GUID_WICPixelFormat64bppRGBA)
        throw std::runtime_error("Encodeur PNG sans RGBA16");
    check(frame->WritePixels(i.height, i.width * 8, UINT(pixels.size() * 2),
                             reinterpret_cast<BYTE *>(pixels.data())),
          "Pixels PNG16");
    check(frame->Commit(), "Commit PNG16");
    check(encoder->Commit(), "Finalisation PNG16");
    frame.Reset();
    encoder.Reset();
    stream.Reset();
    output.commit();
}
template <class T> static void put(std::ostream &s, T x) {
    s.write(reinterpret_cast<const char *>(&x), sizeof(x));
}
static void word(std::ostream &s, const std::string &x) {
    s.write(x.c_str(), x.size() + 1);
}
static void attribute(std::ostream &s, const std::string &name, const std::string &type,
                      const std::string &data) {
    word(s, name);
    word(s, type);
    put(s, uint32_t(data.size()));
    s.write(data.data(), data.size());
}
template <class T> static std::string bytes(T x) {
    return std::string(reinterpret_cast<char *>(&x), sizeof(x));
}
void writeExr(const FloatImage &i, const std::string &path) {
    dimensions(i.width, i.height, i.rgba.size());
    AtomicFile output(path);
    std::ofstream file(output.path(), std::ios::binary);
    if (!file)
        throw std::runtime_error("Création EXR échouée");
    put(file, uint32_t(20000630));
    put(file, uint32_t(2));
    std::ostringstream channels(std::ios::binary);
    for (auto name : {"A", "B", "G", "R"}) {
        word(channels, name);
        put(channels, int32_t(1));
        put(channels, uint32_t(0));
        put(channels, int32_t(1));
        put(channels, int32_t(1));
    }
    channels.put(0);
    attribute(file, "channels", "chlist", channels.str());
    attribute(file, "compression", "compression", std::string(1, 0));
    int32_t box[] = {0, 0, int32_t(i.width - 1), int32_t(i.height - 1)};
    auto bounds = std::string(reinterpret_cast<char *>(box), sizeof(box));
    attribute(file, "dataWindow", "box2i", bounds);
    attribute(file, "displayWindow", "box2i", bounds);
    attribute(file, "lineOrder", "lineOrder", std::string(1, 0));
    attribute(file, "pixelAspectRatio", "float", bytes(1.f));
    attribute(file, "screenWindowCenter", "v2f", std::string(8, 0));
    attribute(file, "screenWindowWidth", "float", bytes(1.f));
    const bool rec2020 = i.colorSpace == "linear2020";
    float chroma709[] = {.64f, .33f, .30f, .60f, .15f, .06f, .3127f, .3290f},
          chroma2020[] = {.708f, .292f, .170f, .797f, .131f, .046f, .3127f, .3290f};
    attribute(file, "chromaticities", "chromaticities",
              std::string(reinterpret_cast<char *>(rec2020 ? chroma2020 : chroma709), 32));
    attribute(file, "shaderixframeColorSpace", "string", rec2020 ? "linear2020" : "linear709");
    attribute(file, "comments", "string", "RGBA half, linear, premultiplied alpha");
    file.put(0);
    auto start = uint64_t(file.tellp()) + i.height * 8ull;
    uint32_t rowBytes = i.width * 8;
    for (unsigned y = 0; y < i.height; ++y)
        put(file, start + y * (8ull + rowBytes));
    std::vector<uint16_t> row(i.width * 4);
    for (unsigned y = 0; y < i.height; ++y) {
        for (unsigned x = 0; x < i.width; ++x) {
            size_t n = (size_t(y) * i.width + x) * 4;
            auto rgb = convertColor({i.rgba[n], i.rgba[n + 1], i.rgba[n + 2]}, i.colorSpace,
                                    rec2020 ? "linear2020" : "linear709");
            float alpha = i.rgba[n + 3];
            float values[] = {alpha, rgb.z * alpha, rgb.y * alpha, rgb.x * alpha};
            for (unsigned k = 0; k < 4; ++k) {
                if (!std::isfinite(values[k]) || std::abs(values[k]) > 65504)
                    throw std::runtime_error("EXR half : valeur hors limites");
                row[k * i.width + x] = XMConvertFloatToHalf(values[k]);
            }
        }
        put(file, int32_t(y));
        put(file, rowBytes);
        file.write(reinterpret_cast<char *>(row.data()), rowBytes);
    }
    file.close();
    if (!file)
        throw std::runtime_error("Écriture EXR échouée");
    output.commit();
}
template <class T> static T get(std::istream &s) {
    T value{};
    if (!s.read(reinterpret_cast<char *>(&value), sizeof(value)))
        throw std::runtime_error("EXR tronqué");
    return value;
}
static std::string word(std::istream &s) {
    std::string r;
    for (int i = 0; i < 256; ++i) {
        int c = s.get();
        if (c == EOF)
            throw std::runtime_error("En-tête EXR tronqué");
        if (!c)
            return r;
        r += char(c);
    }
    throw std::runtime_error("Attribut EXR trop long");
}
Image readExr(const std::string &path) {
    std::ifstream file(sx::Utf8Path(path), std::ios::binary);
    if (get<uint32_t>(file) != 20000630 || get<uint32_t>(file) != 2)
        throw std::runtime_error("EXR : scanline simple requis");
    std::vector<std::pair<std::string, int>> channels;
    int32_t box[4] = {};
    bool window = false, compression = false;
    std::string space = "linear709";
    for (unsigned attrs = 0; attrs < 128; ++attrs) {
        auto name = word(file);
        if (name.empty())
            break;
        auto type = word(file);
        auto size = get<uint32_t>(file);
        if (size > 65536)
            throw std::runtime_error("En-tête EXR trop grand");
        std::string data(size, 0);
        if (!file.read(data.data(), size))
            throw std::runtime_error("EXR tronqué");
        std::istringstream value(data, std::ios::binary);
        if (name == "compression") {
            compression = size == 1 && data[0] == 0;
            if (!compression)
                throw std::runtime_error("EXR compressé : décompression non disponible");
        }
        if (name == "dataWindow") {
            if (size != 16 || type != "box2i")
                throw std::runtime_error("Fenêtre EXR invalide");
            memcpy(box, data.data(), 16);
            window = true;
        }
        if (name == "channels") {
            for (;;) {
                auto channel = word(value);
                if (channel.empty())
                    break;
                int pixel = get<int32_t>(value);
                get<uint32_t>(value);
                if (get<int32_t>(value) != 1 || get<int32_t>(value) != 1 || (pixel != 1 && pixel != 2) ||
                    channels.size() >= 4)
                    throw std::runtime_error("EXR : RGB/RGBA half ou float sans sous-échantillonnage requis");
                if (channel != "R" && channel != "G" && channel != "B" && channel != "A")
                    throw std::runtime_error("Canal EXR non pris en charge");
                for (auto &c : channels)
                    if (c.first == channel)
                        throw std::runtime_error("Canal EXR dupliqué");
                channels.emplace_back(channel, pixel);
            }
        }
        if (name == "shaderixframeColorSpace" && (data == "linear709" || data == "linear2020"))
            space = data;
        if (name == "chromaticities" && size == 32) {
            float red;
            memcpy(&red, data.data(), 4);
            if (std::abs(red - .708f) < .001f)
                space = "linear2020";
        }
        if (attrs == 127)
            throw std::runtime_error("Trop d'attributs EXR");
    }
    if (!window || !compression || box[2] < box[0] || box[3] < box[1] || channels.size() < 3 ||
        std::none_of(
            channels.begin(), channels.end(), [](auto &c) { return c.first == "R"; }) ||
        std::none_of(
            channels.begin(), channels.end(), [](auto &c) { return c.first == "G"; }) ||
        std::none_of(channels.begin(), channels.end(), [](auto &c) { return c.first == "B"; }))
        throw std::runtime_error("EXR RGB invalide");
    uint64_t w = int64_t(box[2]) - box[0] + 1, h = int64_t(box[3]) - box[1] + 1;
    if (w * h > 67108864 || w > 65536 || h > 65536)
        throw std::runtime_error("Dimensions EXR hors limites");
    Image image;
    image.width = unsigned(w);
    image.height = unsigned(h);
    image.colorSpace = space;
    image.rgba.resize(size_t(w * h * 4), 0);
    image.pixels.resize(image.rgba.size());
    for (size_t n = 3; n < image.rgba.size(); n += 4)
        image.rgba[n] = 1;
    std::vector<uint64_t> offsets(h);
    for (auto &offset : offsets)
        offset = get<uint64_t>(file);
    auto headerEnd = uint64_t(file.tellg());
    file.seekg(0, std::ios::end);
    auto fileSize = uint64_t(file.tellg());
    unsigned bytesPerPixel = 0;
    for (auto &c : channels)
        bytesPerPixel += c.second == 1 ? 2 : 4;
    std::set<int32_t> rows;
    for (auto offset : offsets) {
        if (offset < headerEnd || offset > fileSize || w * bytesPerPixel + 8 > fileSize - offset)
            throw std::runtime_error("Offset EXR invalide");
        file.seekg(offset);
        int32_t y = get<int32_t>(file);
        if (y < box[1] || y > box[3] || !rows.insert(y).second || get<uint32_t>(file) != w * bytesPerPixel)
            throw std::runtime_error("Scanline EXR invalide");
        for (auto &[channel, pixel] : channels) {
            int k = channel == "R" ? 0 : channel == "G" ? 1 : channel == "B" ? 2 : 3;
            for (unsigned x = 0; x < w; ++x) {
                float v = pixel == 1 ? XMConvertHalfToFloat(get<uint16_t>(file)) : get<float>(file);
                if (!std::isfinite(v))
                    throw std::runtime_error("EXR : pixel non fini");
                image.rgba[(size_t(y - box[1]) * w + x) * 4 + k] = v;
            }
        }
    }
    for (size_t n = 0; n < image.rgba.size(); n += 4) {
        float a = image.rgba[n + 3];
        if (a < 0 || a > 1)
            throw std::runtime_error("Alpha EXR hors limites");
        for (int k = 0; k < 3; ++k)
            image.rgba[n + k] = a > 0 ? image.rgba[n + k] / a : 0;
        auto rgb = convertColor({image.rgba[n], image.rgba[n + 1], image.rgba[n + 2]}, space, "srgb");
        for (int k = 0; k < 3; ++k)
            image.pixels[n + 2 - k] = BYTE(std::clamp(rgb[k], 0.f, 1.f) * 255 + .5f);
        image.pixels[n + 3] = BYTE(a * 255 + .5f);
    }
    return image;
}
} // namespace sf

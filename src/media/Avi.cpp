#include "core/AtomicFile.h"
#include "media/Media.h"
#include "render/Renderer.h"
#include <fstream>
namespace sf {
bool readFrameAviAudio(const std::string &path, const std::function<void(const float *, size_t)> &write) {
    std::ifstream file(sx::Utf8Path(path), std::ios::binary);
    if (!file)
        return false;
    char header[12];
    file.read(header, 12);
    if (!file || memcmp(header, "RIFF", 4) || memcmp(header + 8, "AVI ", 4))
        return false;
    bool ours = false;
    uint64_t moviEnd = 0;
    while (file) {
        char kind[4];
        uint32_t length = 0;
        file.read(kind, 4);
        file.read(reinterpret_cast<char *>(&length), 4);
        if (!file)
            return false;
        auto begin = file.tellg();
        if (memcmp(kind, "LIST", 4) == 0 && length >= 4) {
            char type[4];
            file.read(type, 4);
            if (memcmp(type, "INFO", 4) == 0 && length <= 4096) {
                std::string info(length - 4, '\0');
                file.read(info.data(), info.size());
                if (info.find("ShaderixFrame08") != std::string::npos)
                    ours = true;
            } else if (memcmp(type, "movi", 4) == 0) {
                if (!ours)
                    return false;
                moviEnd = uint64_t(begin) + length;
                break;
            }
        }
        file.seekg(begin + std::streamoff(length + (length & 1)));
    }
    if (!moviEnd)
        return false;
    while (uint64_t(file.tellg()) < moviEnd) {
        char kind[4];
        uint32_t length = 0;
        file.read(kind, 4);
        file.read(reinterpret_cast<char *>(&length), 4);
        if (!file || length > moviEnd - uint64_t(file.tellg()))
            throw std::runtime_error("Bloc AVI tronqué");
        if (memcmp(kind, "01wb", 4) == 0) {
            if (length % 4)
                throw std::runtime_error("Bloc PCM AVI invalide");
            std::array<int16_t, 2048> pcm;
            std::array<float, 2048> converted;
            for (uint32_t at = 0; at < length;) {
                uint32_t bytes = std::min(uint32_t(pcm.size() * 2), length - at);
                file.read(reinterpret_cast<char *>(pcm.data()), bytes);
                if (!file)
                    throw std::runtime_error("Audio AVI tronqué");
                for (unsigned n = 0; n < bytes / 2; ++n)
                    converted[n] = pcm[n] / 32768.f;
                write(converted.data(), bytes / 2);
                at += bytes;
            }
        } else
            file.seekg(length, std::ios::cur);
        if (length & 1)
            file.seekg(1, std::ios::cur);
    }
    return true;
}
void exportAvi(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path,
               const std::function<bool(float)> &progress) {
    p.validate();
    int total = int(std::ceil(p.duration * p.fps));
    uint32_t bytes = uint32_t(p.width) * p.height * 4;
    if (double(total) * (bytes + 192000. / p.fps + 64) + 4096 > 2147483647)
        throw std::runtime_error(
            "AVI RGB sans compression : limite de 2 Gio ; choisir MP4 ou une séquence PNG");
    if (progress && !progress(0))
        throw ExportCancelled();
    AtomicFile destination(path);
    std::ofstream file(destination.path(), std::ios::binary);
    if (!file)
        throw std::runtime_error("Création AVI échouée");
    auto u32 = [&](uint32_t value) { file.write(reinterpret_cast<const char *>(&value), 4); };
    auto u16 = [&](uint16_t value) { file.write(reinterpret_cast<const char *>(&value), 2); };
    auto four = [&](const char *value) { file.write(value, 4); };
    auto start = [&](const char *kind, const char *type) {
        four(kind);
        auto pos = file.tellp();
        u32(0);
        if (type)
            four(type);
        return pos;
    };
    auto end = [&](std::streampos size) {
        auto current = file.tellp();
        file.seekp(size);
        u32(uint32_t(current - size) - 4);
        file.seekp(current);
    };
    auto riff = start("RIFF", "AVI ");
    auto hdrl = start("LIST", "hdrl");
    four("avih");
    u32(56);
    u32(1000000 / p.fps);
    u32(bytes * p.fps + 192000);
    u32(0);
    u32(0x10);
    u32(total);
    u32(0);
    u32(2);
    u32(bytes);
    u32(p.width);
    u32(p.height);
    for (int i = 0; i < 4; ++i)
        u32(0);
    auto streamHeader = [&](bool video) {
        four("strh");
        u32(56);
        four(video ? "vids" : "auds");
        four(video ? "DIB " : "\0\0\0\0");
        u32(0);
        u16(0);
        u16(0);
        u32(0);
        u32(1);
        u32(video ? p.fps : 48000);
        u32(0);
        u32(video ? total : uint32_t(uint64_t(total) * 48000 / p.fps));
        u32(video ? bytes : 192000 / p.fps);
        u32(0xffffffff);
        u32(video ? 0 : 4);
        u16(0);
        u16(0);
        u16(video ? p.width : 0);
        u16(video ? p.height : 0);
    };
    auto video = start("LIST", "strl");
    streamHeader(true);
    four("strf");
    u32(40);
    u32(40);
    u32(p.width);
    u32(uint32_t(-p.height));
    u16(1);
    u16(32);
    u32(0);
    u32(bytes);
    for (int i = 0; i < 4; ++i)
        u32(0);
    end(video);
    auto audio = start("LIST", "strl");
    streamHeader(false);
    four("strf");
    u32(16);
    u16(1);
    u16(2);
    u32(48000);
    u32(192000);
    u16(4);
    u16(16);
    end(audio);
    end(hdrl);
    auto info = start("LIST", "INFO");
    four("ISFT");
    u32(16);
    file.write("ShaderixFrame08\0\0", 16);
    end(info);
    auto movi = start("LIST", "movi");
    auto base = movi + std::streamoff(4);
    struct Index {
        char id[4];
        uint32_t flags, offset, size;
    };
    std::vector<Index> index;
    auto chunk = [&](const char *name, const void *data, uint32_t length) {
        Index entry;
        memcpy(entry.id, name, 4);
        entry.flags = name[2] == 'd' ? 0x10 : 0;
        entry.offset = uint32_t(file.tellp() - base);
        entry.size = length;
        index.push_back(entry);
        four(name);
        u32(length);
        file.write(static_cast<const char *>(data), length);
        if (length & 1)
            file.put(0);
    };
    uint64_t previous = 0;
    media.resetAudio();
    for (int frame = 0; frame < total; ++frame) {
        if (progress && !progress(float(frame) / total))
            throw ExportCancelled();
        renderer.render(p, media, double(frame) / p.fps, p.width, p.height);
        auto image = renderer.readback();
        chunk("00db", image.pixels.data(), bytes);
        uint64_t next = uint64_t(frame + 1) * 48000 / p.fps;
        std::vector<float> mix;
        media.mix(p, previous / 48000., unsigned(next - previous), mix);
        std::vector<int16_t> pcm(mix.size());
        for (size_t n = 0; n < mix.size(); ++n)
            pcm[n] = int16_t(mix[n] * 32767);
        chunk("01wb", pcm.data(), uint32_t(pcm.size() * 2));
        previous = next;
        if (!file)
            throw std::runtime_error("Écriture AVI échouée");
    }
    end(movi);
    four("idx1");
    u32(uint32_t(index.size() * sizeof(Index)));
    file.write(reinterpret_cast<const char *>(index.data()), index.size() * sizeof(Index));
    end(riff);
    file.close();
    if (!file)
        throw std::runtime_error("Finalisation AVI échouée");
    if (progress && !progress(1))
        throw ExportCancelled();
    destination.commit();
}
} // namespace sf

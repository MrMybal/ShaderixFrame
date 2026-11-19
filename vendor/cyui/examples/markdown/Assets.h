#pragma once
#include <cyui/modules/DocumentIO.h>
#include "Platform.h"
#include "Raster.h"
#include <charconv>
#include <fstream>
#include <map>
#include <unordered_map>
#include <stdexcept>

namespace native_assets {
namespace fs = std::filesystem;
inline std::vector<unsigned char> read(const fs::path &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0 || input.tellg() > 64 * 1024 * 1024)
        throw std::runtime_error("Unable to import file (64 MiB maximum)");
    std::vector<unsigned char> bytes(std::size_t(input.tellg()));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())))
        throw std::runtime_error("Unable to read attachment");
    return bytes;
}
inline std::string escaped(const std::string &text) {
    std::string out;
    for (char c : text) {
        if (c == '[' || c == ']' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}
struct Asset {
    cyui::ItemId id = 0;
    std::string name, uri;
    fs::path sourcePath;
    std::vector<unsigned char> bytes;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture{nullptr, SDL_DestroyTexture};
    cyui::ImageData image;
    cyui::TranscriptAttachment attachment() const {
        return {id, name, uri, bytes.size(), image.texture != 0};
    }
};
class Store {
    SDL_Renderer *renderer;
    cyui::ItemId next = 1;
    std::unordered_map<std::string, std::shared_ptr<Asset>> items;
    std::unordered_map<std::string, std::string> local;
    void decode(Asset &asset) {
        const auto decoded = raster::decode(asset.bytes.data(), asset.bytes.size());
        if (decoded.pixels.empty())
            return;
        SDL_Surface *surface =
            SDL_CreateSurfaceFrom(decoded.width, decoded.height, SDL_PIXELFORMAT_RGBA32,
                                  const_cast<unsigned char *>(decoded.pixels.data()), decoded.width * 4);
        if (!surface)
            return;
        asset.texture.reset(SDL_CreateTextureFromSurface(renderer, surface));
        SDL_DestroySurface(surface);
        if (asset.texture) {
            SDL_SetTextureBlendMode(asset.texture.get(), SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(asset.texture.get(), SDL_SCALEMODE_LINEAR);
            asset.image = {ImTextureID(reinterpret_cast<std::uintptr_t>(asset.texture.get())), decoded.width,
                           decoded.height};
        }
    }

  public:
    explicit Store(SDL_Renderer *target) : renderer(target) {}
    Asset &add(std::string name, std::vector<unsigned char> bytes) {
        auto asset = std::make_shared<Asset>();
        asset->id = next++;
        asset->name = std::move(name);
        asset->bytes = std::move(bytes);
        auto extension = platform::utf8(platform::path(asset->name).extension());
        if (extension.size() > 12 ||
            extension.find_first_not_of(".abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
            extension.clear();
        asset->uri = "asset://" + std::to_string(asset->id) + extension;
        decode(*asset);
        auto *result = asset.get();
        items.emplace(asset->uri, std::move(asset));
        return *result;
    }
    Asset &import(const fs::path &path) {
        auto &asset = add(platform::utf8(path.filename()), read(path));
        asset.sourcePath = path;
        return asset;
    }
    const Asset *find(const std::string &uri) const {
        auto it = items.find(uri);
        return it == items.end() ? nullptr : it->second.get();
    }
    std::shared_ptr<const Asset> retain(const std::string &uri) const { return items.at(uri); }
    cyui::ImageData image(const std::string &uri) const {
        if (const auto *asset = find(uri))
            return asset->image;
        auto known = local.find(uri);
        if (known != local.end())
            if (const auto *asset = find(known->second))
                return asset->image;
        return {};
    }
    void prepare(const cyui::Document &document, const fs::path &base) {
        for (const auto &link : document.destinations()) {
            if (!link.image || find(link.target) || local.count(link.target) ||
                link.target.find("://") != std::string::npos)
                continue;
            try {
                local[link.target] = import(base / platform::path(link.target)).uri;
            } catch (...) {
                local[link.target] = "";
            }
        }
    }
    // Imported URIs can also occur in a chat message or an unfinished draft.
    static void retainReferences(const std::string &text, std::unordered_set<cyui::ItemId> &keep) {
        std::size_t at = 0;
        while ((at = text.find("asset://", at)) != std::string::npos) {
            at += 8;
            cyui::ItemId id{};
            if (std::from_chars(text.data() + at, text.data() + text.size(), id).ec == std::errc{})
                keep.insert(id);
        }
    }
    void resetDocument(const std::unordered_set<cyui::ItemId> &keep) {
        local.clear();
        for (auto it = items.begin(); it != items.end();) {
            if (!keep.count(it->second->id))
                it = items.erase(it);
            else
                ++it;
        }
    }
    std::string markup(const Asset &asset) const {
        return std::string(asset.image.texture ? "![" : "[") + escaped(asset.name) + "](" + asset.uri + ")";
    }
    template <class Target> std::string rewrite(const cyui::Document &doc, Target target) const {
        // Several links can resolve to the same reference definition.
        std::map<std::size_t, std::pair<std::size_t, std::string>> edits;
        for (const auto &link : doc.destinations()) {
            if (edits.count(link.begin))
                continue;
            const auto known = local.find(link.target);
            if (const auto *asset = find(known == local.end() ? link.target : known->second)) {
                auto replacement = target(link.target, *asset);
                if (replacement != link.target)
                    edits.emplace(link.begin, std::make_pair(link.end, std::move(replacement)));
            }
        }
        std::string out;
        const auto &source = doc.markdown();
        std::size_t at = 0;
        for (const auto &[begin, edit] : edits) {
            out.append(source, at, begin - at);
            out += edit.second;
            at = edit.first;
        }
        out.append(source, at, std::string::npos);
        return out;
    }
    // Keep relative source images available to chat after switching documents.
    std::string chatMarkdown(const cyui::Document &doc) const {
        return rewrite(doc, [](const std::string &, const Asset &asset) { return asset.uri; });
    }
    std::string exportMarkdown(const cyui::Document &doc, const fs::path &destination) const {
        std::unordered_map<cyui::ItemId, std::string> written;
        return rewrite(doc, [&](const std::string &original, const Asset &asset) {
            std::error_code error;
            if (local.count(original)) {
                const auto relative = platform::path(original).lexically_normal();
                if (relative.is_relative() && !relative.empty() && *relative.begin() != ".." &&
                    fs::equivalent(destination.parent_path() / relative, asset.sourcePath, error))
                    return original;
            }
            auto cached = written.find(asset.id);
            if (cached != written.end())
                return cached->second;
            const auto name = "cyui-" + asset.uri.substr(8);
            const auto path = destination.parent_path() / "assets" / platform::path(name);
            fs::create_directories(path.parent_path());
            auto safe = path;
            int suffix = 0;
            while (fs::exists(safe) && read(safe) != asset.bytes) {
                safe = path.parent_path() / platform::path(platform::utf8(path.stem()) + "-" +
                           std::to_string(++suffix) + platform::utf8(path.extension()));
            }
            if (!fs::exists(safe)) {
                std::ofstream output(safe, std::ios::binary);
                output.write(reinterpret_cast<const char *>(asset.bytes.data()),
                             std::streamsize(asset.bytes.size()));
                output.close();
                if (!output)
                    throw std::runtime_error("Unable to save attachment");
            }
            return written[asset.id] = std::string("assets/") + platform::utf8(safe.filename());
        });
    }
    Asset &sample() {
        const auto &p = cyui::palette();
        const ImU32 colors[]{p.bg2, p.accent, p.info, p.success};
        std::vector<ImU32> pixels(480 * 240);
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 480; ++x)
                pixels[y * 480 + x] = colors[(x / 120 + y / 80) % 4];
        auto bytes = raster::png(reinterpret_cast<const unsigned char *>(pixels.data()), 480, 240);
        return add("palette.png", std::move(bytes));
    }
};
} // namespace native_assets

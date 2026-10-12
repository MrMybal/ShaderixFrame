#include "media/Media.h"
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mfapi.h>
#include <thread>

namespace sf {
struct MediaCache::Background {
    struct Request {
        std::string source;
        double time = 0;
        uint64_t generation = 0;
        bool load = false;
        std::string decodePath;
    };
    struct Result {
        std::string source;
        uint64_t generation;
        std::unique_ptr<MediaAsset> asset;
        Image frame;
        std::string error;
        bool load;
    };
    std::mutex mutex;
    std::condition_variable wake;
    std::filesystem::path directory;
    bool stopping = false;
    uint64_t generation = 0;
    std::deque<Request> loads;
    std::map<std::string, Request> frames;
    std::map<std::string, std::string> proxies;
    std::deque<Result> ready;
    // Launch only after every field accessed by run() has been initialized.
    std::thread worker;
    Background(std::filesystem::path path) : directory(std::move(path)), worker([this] { run(); }) {}
    ~Background() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
            loads.clear();
            frames.clear();
        }
        wake.notify_all();
        if (worker.joinable())
            worker.join();
    }
    void run() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HRESULT mf = SUCCEEDED(hr) ? MFStartup(MF_VERSION) : hr;
        std::map<std::string, std::unique_ptr<MediaAsset>> decoders;
        uint64_t decoderGeneration = 0;
        for (;;) {
            Request request;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return stopping || !loads.empty() || !frames.empty(); });
                if (stopping)
                    break;
                if (!loads.empty()) {
                    request = loads.front();
                    loads.pop_front();
                } else {
                    auto it = frames.begin();
                    request = it->second;
                    frames.erase(it);
                }
            }
            if (request.generation != decoderGeneration) {
                decoders.clear();
                decoderGeneration = request.generation;
            }
            Result result;
            result.source = request.source;
            result.generation = request.generation;
            result.load = request.load;
            try {
                check(mf, "Runtime média en arrière-plan");
                auto decode = request.decodePath.empty() ? request.source : request.decodePath;
                auto it = decoders.find(decode);
                if (it == decoders.end()) {
                    if (decoders.size() >= 8)
                        decoders.erase(decoders.begin());
                    auto asset = std::make_unique<MediaAsset>();
                    asset->open(decode, directory);
                    it = decoders.emplace(decode, std::move(asset)).first;
                }
                auto &asset = *it->second;
                if (request.load) {
                    result.asset = std::make_unique<MediaAsset>();
                    auto &copy = *result.asset;
                    copy.path = asset.path;
                    copy.video = asset.video;
                    copy.image = asset.image;
                    copy.hasAudio = asset.hasAudio;
                    copy.duration = asset.duration;
                    copy.peakCeiling = asset.peakCeiling;
                    copy.pcm = std::move(asset.pcm);
                    copy.diskPcm = std::move(asset.diskPcm);
                    copy.peaks = std::move(asset.peaks);
                    copy.frame = asset.at(0);
                    if (!copy.frame.pixels.empty()) {
                        auto &thumb = copy.thumbnail;
                        thumb.width = 160;
                        thumb.height = std::max(1u, 160 * copy.frame.height / copy.frame.width);
                        thumb.pixels.resize(size_t(thumb.width) * thumb.height * 4);
                        for (unsigned y = 0; y < thumb.height; ++y)
                            for (unsigned x = 0; x < thumb.width; ++x)
                                memcpy(thumb.pixels.data() + (y * thumb.width + x) * 4,
                                       copy.frame.pixels.data() +
                                           (size_t(y) * copy.frame.height / thumb.height * copy.frame.width +
                                            size_t(x) * copy.frame.width / thumb.width) *
                                               4,
                                       4);
                    }
                } else
                    result.frame = asset.at(request.time);
            } catch (const std::exception &error) {
                result.error = error.what();
            }
            {
                std::lock_guard lock(mutex);
                if (result.generation == generation && !stopping)
                    ready.push_back(std::move(result));
            }
        }
        decoders.clear();
        if (SUCCEEDED(mf))
            MFShutdown();
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};
MediaCache::MediaCache() = default;
MediaCache::~MediaCache() = default;
void MediaCache::enableBackground(const std::filesystem::path &directory) {
    if (background)
        return;
    diskCacheDirectory = directory;
    assets.clear();
    resetAudio();
    background = std::make_unique<Background>(directory);
}
void MediaCache::poll() {
    if (!background)
        return;
    std::deque<Background::Result> results;
    {
        std::lock_guard lock(background->mutex);
        results.swap(background->ready);
    }
    for (auto &result : results) {
        if (result.generation != background->generation)
            continue;
        auto it = assets.find(result.source);
        if (it == assets.end())
            continue;
        if (!result.error.empty()) {
            it->second->error = result.error;
            it->second->loading = false;
            continue;
        }
        if (result.load)
            it->second = std::move(result.asset);
        else
            it->second->frame = std::move(result.frame);
    }
}
MediaAsset &MediaCache::get(const std::string &source) {
    if (!background)
        return getBlocking(source);
    poll();
    auto it = assets.find(source);
    if (it == assets.end()) {
        auto asset = std::make_unique<MediaAsset>();
        asset->path = source;
        asset->loading = true;
        it = assets.emplace(source, std::move(asset)).first;
        try {
            auto cached = proxyPath(source);
            if (std::filesystem::is_regular_file(cached))
                useProxy(source, sx::Narrow(cached.wstring()));
        } catch (const std::filesystem::filesystem_error &) {
            // The decoder reports missing/inaccessible sources through the normal result queue.
        }
        {
            std::lock_guard lock(background->mutex);
            background->loads.push_back({source, 0, background->generation, true});
        }
        background->wake.notify_one();
    }
    if (!it->second->error.empty())
        throw std::runtime_error(it->second->error);
    return *it->second;
}
const Image &MediaCache::frame(const std::string &source, double time) {
    auto &asset = get(source);
    if (!background)
        return asset.at(time);
    if (!asset.loading && asset.video && !asset.image) {
        std::lock_guard lock(background->mutex);
        auto proxy = background->proxies.find(source);
        background->frames[source] = {source, time, background->generation, false,
                                      proxiesEnabled && proxy != background->proxies.end() ? proxy->second
                                                                                           : std::string{}};
        background->wake.notify_one();
    }
    return asset.frame;
}
bool MediaCache::pending() const {
    return std::any_of(assets.begin(), assets.end(), [](const auto &entry) { return entry.second->loading; });
}
void MediaCache::clear() {
    if (background) {
        std::lock_guard lock(background->mutex);
        ++background->generation;
        background->loads.clear();
        background->frames.clear();
        background->ready.clear();
    }
    assets.clear();
    resetAudio();
}
std::filesystem::path MediaCache::proxyPath(const std::string &source) const {
    if (!background)
        throw std::runtime_error("Chemin proxy requis hors de la prévisualisation interactive");
    auto file = sx::Utf8Path(source);
    auto signature = std::filesystem::weakly_canonical(file).wstring() + L"|" +
                     std::to_wstring(std::filesystem::file_size(file)) + L"|" +
                     std::to_wstring(std::filesystem::last_write_time(file).time_since_epoch().count());
    uint64_t hash = 14695981039346656037ull;
    for (wchar_t c : signature) {
        hash ^= uint64_t(c);
        hash *= 1099511628211ull;
    }
    auto path = background->directory.parent_path() / L"proxies" / (std::to_wstring(hash) + L".mp4");
    std::filesystem::create_directories(path.parent_path());
    return path;
}
void MediaCache::useProxy(const std::string &source, const std::string &path) {
    if (!background)
        return;
    std::lock_guard lock(background->mutex);
    background->proxies[source] = path;
}
} // namespace sf

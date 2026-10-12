#include "ui/ClipPreviews.h"
#include <mfapi.h>
namespace sf {
ClipPreviews::~ClipPreviews() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
        pending.clear();
    }
    wake.notify_all();
    if (worker.joinable())
        worker.join();
}
void ClipPreviews::clear() {
    std::lock_guard lock(mutex);
    pending.clear();
    ready.clear();
    scheduled.clear();
    textures.clear();
}
void ClipPreviews::run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    try {
        Renderer renderer;
        renderer.initialize();
        renderer.library = library;
        MediaCache media;
        for (;;) {
            Request request;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return stopping || !pending.empty(); });
                if (stopping)
                    break;
                request = std::move(pending.front());
                pending.pop_front();
            }
            Image image;
            try {
                renderer.clearHistory();
                renderer.render(request.project, media, std::max(0., request.time - 1. / request.project.fps),
                                128, 72);
                renderer.render(request.project, media, request.time, 128, 72);
                image = renderer.readback();
            } catch (...) { /* Invalid shader is reported by the normal composition inspector. */
            }
            {
                std::lock_guard lock(mutex);
                if (ready.size() >= 128) {
                    scheduled.erase(ready.begin()->first);
                    ready.erase(ready.begin());
                }
                ready[request.key] = std::move(image);
            }
        }
    } catch (...) {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    MFShutdown();
    CoUninitialize();
}
ID3D11ShaderResourceView *ClipPreviews::get(const Project &project, const Clip &clip, double time,
                                            Renderer &owner) {
    if (!prepared.contains(clip.id)) {
        Project isolated;
        isolated.width = project.width;
        isolated.height = project.height;
        isolated.fps = project.fps;
        isolated.timeOrigin = project.timeOrigin;
        isolated.duration = std::max(.01, clip.start + clip.duration);
        isolated.workingSpace = project.workingSpace;
        isolated.toneMap = project.toneMap;
        isolated.exposure = project.exposure;
        isolated.backgroundAlpha = 1;
        isolated.nextId = clip.id + 1;
        auto &track = isolated.addTrack(TrackKind::Video);
        auto item = clip;
        item.link = 0;
        item.transition.enabled = false;
        item.widget = Json();
        track.clips = {item};
        isolated.nextId = std::max(isolated.nextId, clip.id + 1);
        for (auto &e : track.clips.front().effects)
            if (e.code.empty() && e.graph.IsNull())
                if (auto *d = owner.library.find(e.id))
                    e.code = d->code;
        auto signature = std::to_string(std::hash<std::string>{}(isolated.json().Dump(0)));
        prepared.emplace(clip.id, std::make_pair(std::move(isolated), std::move(signature)));
    }
    auto &[isolated, signature] = prepared.at(clip.id);
    auto key = signature + ":" + std::to_string(int64_t(time * project.fps));
    if (auto it = textures.find(key); it != textures.end())
        return it->second.Get();
    std::optional<Image> finished;
    {
        std::lock_guard lock(mutex);
        if (auto it = ready.find(key); it != ready.end()) {
            finished = std::move(it->second);
            ready.erase(it);
            scheduled.erase(key);
        } else if (!stopping && !scheduled.contains(key) && pending.size() < 64 && requestsThisFrame < 8) {
            scheduled.insert(key);
            pending.push_back({key, isolated, time});
            ++requestsThisFrame;
            wake.notify_one();
        }
    }
    if (!worker.joinable() && !stopping) {
        library = owner.library;
        worker = std::thread([this] { run(); });
    }
    if (finished && !finished->pixels.empty()) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = finished->width;
        d.Height = finished->height;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_IMMUTABLE;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{finished->pixels.data(), d.Width * 4, 0};
        Com<ID3D11Texture2D> texture;
        Com<ID3D11ShaderResourceView> view;
        if (SUCCEEDED(owner.gpu()->CreateTexture2D(&d, &pixels, &texture)) &&
            SUCCEEDED(owner.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &view))) {
            if (textures.size() >= 256)
                textures.erase(textures.begin());
            textures[key] = std::move(view);
            return textures[key].Get();
        }
    }
    if (finished) {
        if (textures.size() >= 256)
            textures.erase(textures.begin());
        textures[key].Reset();
    }
    return nullptr;
}
} // namespace sf

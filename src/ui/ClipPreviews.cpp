#include "ui/ClipPreviews.h"
#include <cmath>
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
    sequence.reset();
    sequenceSignature.clear();
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
                // Newest first: the cells on screen now matter more than those scrolled past.
                request = std::move(pending.back());
                pending.pop_back();
            }
            Image image;
            try {
                auto &project = *request.project;
                renderer.clearHistory();
                renderer.render(project, media, std::max(0., request.time - 1. / project.fps), request.width,
                                request.height);
                renderer.render(project, media, request.time, request.width, request.height);
                image = renderer.readback();
            } catch (...) { /* Invalid shader is reported by the normal composition inspector. */
            }
            {
                std::lock_guard lock(mutex);
                if (ready.size() >= 192) {
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
ID3D11ShaderResourceView *ClipPreviews::fetch(const std::string &key, const std::shared_ptr<const Project> &project,
                                              double time, Renderer &owner) {
    if (auto it = textures.find(key); it != textures.end()) {
        it->second.used = clock;
        return it->second.view.Get();
    }
    std::optional<Image> finished;
    {
        std::lock_guard lock(mutex);
        if (auto it = ready.find(key); it != ready.end()) {
            finished = std::move(it->second);
            ready.erase(it);
            scheduled.erase(key);
        } else if (!stopping && !scheduled.contains(key) && requestsThisFrame < 12) {
            if (pending.size() >= 96) {
                scheduled.erase(pending.front().key);
                pending.pop_front();
            }
            unsigned width = thumbnailWidth;
            unsigned height = std::max(24u, unsigned(std::lround(double(width) * project->height / project->width)));
            scheduled.insert(key);
            pending.push_back({key, project, time, width, height});
            ++requestsThisFrame;
            wake.notify_one();
        }
    }
    if (!worker.joinable() && !stopping) {
        library = owner.library;
        worker = std::thread([this] { run(); });
    }
    if (!finished)
        return nullptr;
    // Least recently used texture goes first.
    if (textures.size() >= 512) {
        auto oldest = textures.begin();
        for (auto it = textures.begin(); it != textures.end(); ++it)
            if (it->second.used < oldest->second.used)
                oldest = it;
        textures.erase(oldest);
    }
    auto &slot = textures[key];
    slot.used = clock;
    if (!finished->pixels.empty()) {
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
        if (SUCCEEDED(owner.gpu()->CreateTexture2D(&d, &pixels, &texture)))
            owner.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &slot.view);
    }
    return slot.view.Get();
}
ID3D11ShaderResourceView *ClipPreviews::get(const Project &project, const Clip &clip, double time,
                                            Renderer &owner) {
    // Cheap description of what changes a thumbnail most often; parameter values are caught by the
    // periodic re-signature.
    std::size_t fingerprint = std::hash<std::string>{}(clip.name + "|" + clip.source + "|" + clip.title.text);
    auto mixIn = [&](double value) { fingerprint = fingerprint * 1099511628211ull ^ std::hash<double>{}(value); };
    for (double value : {clip.start, clip.duration, clip.in, clip.speed, double(clip.effects.size()),
                         double(clip.params.size()), double(clip.generator), double(clip.composition.Size()),
                         double(project.width), double(project.height), double(project.fps), project.timeOrigin,
                         double(project.exposure)})
        mixIn(value);
    fingerprint ^= std::hash<std::string>{}(project.workingSpace + project.toneMap);
    auto found = prepared.find(clip.id);
    bool stale = found == prepared.end() || found->second.fingerprint != fingerprint || clock - found->second.checked >= 15;
    if (stale) {
        auto isolated = std::make_shared<Project>();
        isolated->width = project.width;
        isolated->height = project.height;
        isolated->fps = project.fps;
        isolated->timeOrigin = project.timeOrigin;
        isolated->duration = std::max(.01, clip.start + clip.duration);
        isolated->workingSpace = project.workingSpace;
        isolated->toneMap = project.toneMap;
        isolated->exposure = project.exposure;
        isolated->backgroundAlpha = 1;
        isolated->nextId = clip.id + 1;
        auto &track = isolated->addTrack(TrackKind::Video);
        auto item = clip;
        item.link = 0;
        item.enabled = true; // a disabled clip still shows its pictures, dimmed by the timeline
        item.transition.enabled = false;
        item.widget = Json();
        track.clips = {item};
        isolated->nextId = std::max(isolated->nextId, clip.id + 1);
        for (auto &e : track.clips.front().effects)
            if (e.code.empty() && e.graph.IsNull())
                if (auto *d = owner.library.find(e.id))
                    e.code = d->code;
        auto signature = std::to_string(std::hash<std::string>{}(isolated->json().Dump(0)));
        auto &entry = prepared[clip.id];
        if (entry.signature != signature) {
            entry.project = std::move(isolated);
            entry.signature = std::move(signature);
        }
        entry.fingerprint = fingerprint;
        entry.checked = clock;
    }
    auto &entry = prepared.at(clip.id);
    auto &isolated = entry.project;
    auto &signature = entry.signature;
    auto frame = std::llround(time * project.fps);
    return fetch(signature + ":" + std::to_string(frame), isolated, double(frame) / project.fps, owner);
}
ID3D11ShaderResourceView *ClipPreviews::getSequence(const Project &project, double time, Renderer &owner) {
    if (!sequence || (clock % 8 == 0 && sequenceChecked != clock)) {
        sequenceChecked = clock;
        auto signature = "sequence:" + std::to_string(std::hash<std::string>{}(project.json().Dump(0)));
        if (signature != sequenceSignature || !sequence) {
            auto copy = std::make_shared<Project>(project);
            copy->backgroundAlpha = 1;
            sequence = std::move(copy);
            sequenceSignature = std::move(signature);
        }
    }
    auto frame = std::llround(time * project.fps);
    return fetch(sequenceSignature + ":" + std::to_string(frame), sequence, double(frame) / project.fps, owner);
}
} // namespace sf

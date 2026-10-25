#include "ui/RenderCache.h"
#include <cmath>
#include <mfapi.h>
namespace sf {
void RenderCache::start(const Project &p, const EffectLibrary &effects, std::string signature, double begin,
                        double end, unsigned w, unsigned h, std::size_t budget) {
    clear();
    fps = std::max(1, p.fps);
    first = int(std::floor(std::max(0., begin) * fps + 1e-6));
    int last = int(std::ceil(std::min(end, p.duration) * fps - 1e-6));
    std::size_t frameBytes = std::size_t(w) * h * 4;
    int fit = int(std::min<std::size_t>(budget / std::max<std::size_t>(1, frameBytes), 1 << 20));
    int count = std::clamp(last - first, 0, fit);
    if (!count || !w || !h)
        return;
    project = std::make_shared<const Project>(p);
    library = effects;
    width = w;
    height = h;
    key = std::move(signature);
    frames.assign(size_t(count), {});
    stopping = false;
    running = true;
    worker = std::thread([this] { run(); });
}
void RenderCache::clear() {
    stopping = true;
    if (worker.joinable())
        worker.join();
    running = false;
    std::lock_guard lock(mutex);
    frames.clear();
    done = 0;
    key.clear();
    uploaded.Reset();
    uploadedIndex = -1;
}
void RenderCache::run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    try {
        Renderer renderer;
        renderer.initialize();
        renderer.library = library;
        MediaCache media;
        for (int index = 0; index < int(frames.size()) && !stopping; ++index) {
            renderer.render(*project, media, double(first + index) / fps, width, height);
            auto image = renderer.readback();
            std::lock_guard lock(mutex);
            frames[size_t(index)] = std::move(image);
            ++done;
        }
    } catch (...) { /* A frame that cannot render ends the pre-render; playback renders it live. */
    }
    running = false;
    MFShutdown();
    CoUninitialize();
}
bool RenderCache::ready(int index) const {
    std::lock_guard lock(mutex);
    return index >= 0 && index < int(frames.size()) && !frames[size_t(index)].pixels.empty();
}
ID3D11ShaderResourceView *RenderCache::frame(double time, ID3D11Device *device) {
    int index = int(std::floor(time * fps + 1e-6)) - first;
    if (index == uploadedIndex && uploaded)
        return uploaded.Get();
    std::lock_guard lock(mutex);
    if (index < 0 || index >= int(frames.size()) || frames[size_t(index)].pixels.empty())
        return nullptr;
    auto &image = frames[size_t(index)];
    D3D11_TEXTURE2D_DESC d{};
    d.Width = image.width;
    d.Height = image.height;
    d.MipLevels = d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA pixels{image.pixels.data(), d.Width * 4, 0};
    Com<ID3D11Texture2D> texture;
    uploaded.Reset();
    uploadedIndex = -1;
    if (FAILED(device->CreateTexture2D(&d, &pixels, &texture)) ||
        FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &uploaded)))
        return nullptr;
    uploadedIndex = index;
    return uploaded.Get();
}
} // namespace sf

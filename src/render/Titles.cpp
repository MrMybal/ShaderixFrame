#include "render/Titles.h"

namespace sf {
void TitleRasterizer::retain(const Project &project) {
    std::erase_if(cache, [&](const auto &entry) {
        auto *clip = project.findClip(entry.first);
        return !clip || !clip->title.enabled;
    });
}
const Image &TitleRasterizer::at(const Clip &clip, double time, unsigned width, unsigned height,
                                 int projectHeight) {
    if (!clip.title.enabled || !width || !height || projectHeight <= 0)
        throw std::runtime_error("Invalid title dimensions");
    float size = std::clamp(clip.param("font_size", time, 48), 4.f, 1024.f) * height / projectHeight;
    float red = std::clamp(clip.param("text_r", time, 1), 0.f, 1.f);
    float green = std::clamp(clip.param("text_g", time, 1), 0.f, 1.f);
    float blue = std::clamp(clip.param("text_b", time, 1), 0.f, 1.f);
    float alpha = std::clamp(clip.param("text_a", time, 1), 0.f, 1.f);
    float outline = std::clamp(clip.param("text_outline", time, 0), 0.f, 16.f) * height / projectHeight;
    float shadow = std::clamp(clip.param("text_shadow", time, 0), 0.f, 1.f);
    float shadowX = clip.param("text_shadow_x", time, 4) * height / projectHeight;
    float shadowY = clip.param("text_shadow_y", time, 4) * height / projectHeight;
    float blur = std::clamp(clip.param("text_shadow_blur", time, 0), 0.f, 32.f) * height / projectHeight;
    // Box behind the text: opacity, colour and padding around the text's own bounds.
    float box = std::clamp(clip.param("text_box", time, 0), 0.f, 1.f);
    float boxRed = std::clamp(clip.param("text_box_r", time, 0), 0.f, 1.f);
    float boxGreen = std::clamp(clip.param("text_box_g", time, 0), 0.f, 1.f);
    float boxBlue = std::clamp(clip.param("text_box_b", time, 0), 0.f, 1.f);
    float padding = std::clamp(clip.param("text_box_padding", time, 12), 0.f, 200.f) * height / projectHeight;
    // Shadow and outline colours (black by default), and the share of the text already written.
    float shadowRed = std::clamp(clip.param("text_shadow_r", time, 0), 0.f, 1.f);
    float shadowGreen = std::clamp(clip.param("text_shadow_g", time, 0), 0.f, 1.f);
    float shadowBlue = std::clamp(clip.param("text_shadow_b", time, 0), 0.f, 1.f);
    float outlineRed = std::clamp(clip.param("text_outline_r", time, 0), 0.f, 1.f);
    float outlineGreen = std::clamp(clip.param("text_outline_g", time, 0), 0.f, 1.f);
    float outlineBlue = std::clamp(clip.param("text_outline_b", time, 0), 0.f, 1.f);
    auto text = sx::Widen(clip.title.text);
    auto written = UINT32(std::lround(std::clamp(clip.param("text_reveal", time, 1), 0.f, 1.f) * text.size()));
    Json key = Json::Array();
    for (auto value :
         {Json(clip.title.text), Json(clip.title.font), Json(clip.title.alignment), Json(clip.title.bold),
          Json(clip.title.italic), Json(width), Json(height), Json(size), Json(red), Json(green), Json(blue),
          Json(alpha), Json(outline), Json(shadow), Json(shadowX), Json(shadowY), Json(blur), Json(box),
          Json(boxRed), Json(boxGreen), Json(boxBlue), Json(padding), Json(shadowRed), Json(shadowGreen),
          Json(shadowBlue), Json(outlineRed), Json(outlineGreen), Json(outlineBlue), Json(written)})
        key.Push(value);
    auto signature = key.Dump(0);
    auto &cached = cache[clip.id];
    if (cached.signature == signature)
        return cached.image;
    if (!drawing) {
        check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, drawing.GetAddressOf()),
              "Direct2D titles");
    }
    if (!writing) {
        check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                  reinterpret_cast<IUnknown **>(writing.GetAddressOf())),
              "DirectWrite titles");
    }
    if (!imaging) {
        check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(imaging.GetAddressOf())),
              "WIC titles");
    }
    Com<IWICBitmap> bitmap;
    check(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap),
          "Title surface");
    Com<ID2D1RenderTarget> target;
    auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    check(drawing->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target), "Title rendering");
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    Com<IDWriteTextFormat> format;
    auto font = sx::Widen(clip.title.font);
    check(writing->CreateTextFormat(font.c_str(), nullptr,
                                    clip.title.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                    clip.title.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                    DWRITE_FONT_STRETCH_NORMAL, size, L"fr-FR", &format),
          "Title font");
    format->SetTextAlignment(clip.title.alignment == 0   ? DWRITE_TEXT_ALIGNMENT_LEADING
                             : clip.title.alignment == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING
                                                         : DWRITE_TEXT_ALIGNMENT_CENTER);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    Com<IDWriteTextLayout> layout;
    check(writing->CreateTextLayout(text.c_str(), UINT32(text.size()), format.Get(), width * .9f,
                                    height * .9f, &layout),
          "Title layout");
    Com<ID2D1SolidColorBrush> brush;
    check(target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush), "Title color");
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    if (box > 0) {
        DWRITE_TEXT_METRICS metrics{};
        layout->GetMetrics(&metrics);
        float left = width * .05f + metrics.left - padding, top = height * .05f + metrics.top - padding;
        auto area = D2D1::RoundedRect(
            D2D1::RectF(left, top, left + metrics.width + 2 * padding, top + metrics.height + 2 * padding),
            padding * .6f, padding * .6f);
        Com<ID2D1SolidColorBrush> fill;
        check(target->CreateSolidColorBrush(D2D1::ColorF(boxRed, boxGreen, boxBlue, box * alpha), &fill),
              "Title background");
        target->FillRoundedRectangle(area, fill.Get());
    }
    // Typewriter: the characters not written yet keep their place but draw with a transparent brush,
    // in the shadow and the outline too, so the text does not move while it appears.
    Com<ID2D1SolidColorBrush> hidden;
    if (written < text.size()) {
        check(target->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0), &hidden), "Text goes here");
        layout->SetDrawingEffect(hidden.Get(), {written, UINT32(text.size()) - written});
    }
    Com<ID2D1SolidColorBrush> dark, ring;
    check(target->CreateSolidColorBrush(D2D1::ColorF(shadowRed, shadowGreen, shadowBlue, 1), &dark), "Title shadow");
    check(target->CreateSolidColorBrush(D2D1::ColorF(outlineRed, outlineGreen, outlineBlue, 1), &ring),
          "Title outline");
    auto origin = D2D1::Point2F(width * .05f, height * .05f);
    if (shadow > 0) {
        dark->SetOpacity(shadow * alpha);
        target->DrawTextLayout(D2D1::Point2F(origin.x + shadowX, origin.y + shadowY), layout.Get(),
                               dark.Get());
        if (blur > 0) {
            dark->SetOpacity(shadow * alpha / 12);
            for (int ring = 1; ring <= 2; ++ring)
                for (int n = 0; n < 12; ++n) {
                    float angle = n * 6.2831853f / 12;
                    target->DrawTextLayout(
                        D2D1::Point2F(origin.x + shadowX + std::cos(angle) * blur * ring / 2,
                                      origin.y + shadowY + std::sin(angle) * blur * ring / 2),
                        layout.Get(), dark.Get());
                }
        }
    }
    if (outline > 0) {
        ring->SetOpacity(alpha);
        for (int n = 0; n < 24; ++n) {
            float angle = n * 6.2831853f / 24;
            target->DrawTextLayout(
                D2D1::Point2F(origin.x + std::cos(angle) * outline, origin.y + std::sin(angle) * outline),
                layout.Get(), ring.Get());
        }
    }
    target->DrawTextLayout(D2D1::Point2F(width * .05f, height * .05f), layout.Get(), brush.Get(),
                           D2D1_DRAW_TEXT_OPTIONS_CLIP);
    check(target->EndDraw(), "Title finalization");
    Image result;
    result.width = width;
    result.height = height;
    result.pixels.resize(size_t(width) * height * 4);
    check(bitmap->CopyPixels(nullptr, width * 4, UINT(result.pixels.size()), result.pixels.data()),
          "Title pixels");
    // The shader pipeline uses straight alpha. Grayscale coverage stays in the alpha channel.
    for (size_t i = 0; i < result.pixels.size(); i += 4) {
        unsigned a = result.pixels[i + 3];
        for (int channel = 0; channel < 3; ++channel)
            result.pixels[i + channel] =
                a ? BYTE(std::min(255u, (unsigned(result.pixels[i + channel]) * 255 + a / 2) / a)) : 0;
    }
    cached.image = std::move(result);
    cached.signature = std::move(signature);
    return cached.image;
}
} // namespace sf

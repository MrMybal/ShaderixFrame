#include "ui/Editor.h"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
namespace sf {
static Json target(Id clip, int effect) {
    Json a = Json::Object();
    a["clip"] = clip;
    a["effect"] = effect;
    return a;
}
void Editor::captureGradeReference(const Image &im) {
    if (!im.width || !im.height || im.pixels.empty())
        throw std::runtime_error("Empty reference");
    D3D11_TEXTURE2D_DESC d{};
    d.Width = im.width;
    d.Height = im.height;
    d.MipLevels = d.ArraySize = 1;
    d.SampleDesc.Count = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{im.pixels.data(), im.width * 4, 0};
    Com<ID3D11Texture2D> texture;
    check(s.renderer.gpu()->CreateTexture2D(&d, &data, &texture), "Reference image");
    check(s.renderer.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &gradeReference),
          "Reference view");
}
void Editor::gradeComparisonControls() {
    ImGui::SetNextItemWidth(210);
    ImGui::Combo("Compare", &gradeComparison,
                 "Graded Image\0Bypass Grading\0Reference / Grade\0Before / After\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("Split", &gradeSplit, 0, 1, "%.2f");
    if (ImGui::Button("Store Reference")) {
        try {
            captureGradeReference(s.renderer.readback());
            gradeComparison = 2;
        } catch (const std::exception &ex) {
            error = ex.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Reference...")) {
        auto path = dialog(false, L"Reference Image\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.exr\0All\0*.*\0\0",
                           nullptr);
        if (!path.empty())
            try {
                s.media.get(path);
                pendingGradeReference = path;
            } catch (const std::exception &ex) {
                error = ex.what();
            }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Reference"))
        gradeReference.Reset();
    ImGui::SameLine();
    ImGui::Checkbox("Show Selection", &gradeMatte);
}
void Editor::gradePreview(ImVec2 origin, float width, float height) {
    auto *draw = ImGui::GetWindowDrawList();
    ID3D11ShaderResourceView *other = gradeComparison == 2                  ? gradeReference.Get()
                                      : gradeComparison == 3 && gradeBefore ? gradeBefore->view()
                                                                            : nullptr;
    if (other) {
        float split = origin.x + width * gradeSplit;
        draw->PushClipRect(origin, {split, origin.y + height}, true);
        draw->AddImage((ImTextureID)(intptr_t)other, origin, {origin.x + width, origin.y + height});
        draw->PopClipRect();
        draw->AddLine({split, origin.y}, {split, origin.y + height}, IM_COL32(255, 180, 90, 255), 2);
        draw->AddText({origin.x + 8, origin.y + 8}, IM_COL32_WHITE,
                      gradeComparison == 2 ? "REFERENCE" : "BEFORE");
        draw->AddText({split + 8, origin.y + 8}, IM_COL32_WHITE, "AFTER");
    }
    if (!gradePick)
        return;
    draw->AddText({origin.x + 8, origin.y + height - 22}, IM_COL32(255, 190, 90, 255),
                  gradePick == 1 ? "NEUTRAL EYEDROPPER - click image - Esc cancels"
                                 : "HSL EYEDROPPER - click image - Esc cancels");
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        gradePick = 0;
        return;
    }
    if (!ImGui::IsMouseHoveringRect(origin, {origin.x + width, origin.y + height}))
        return;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return;
    auto *clip = s.project.findClip(s.selected);
    auto &image = s.renderer.capturedGradeInput;
    if (!clip || !clip->active(s.time) || selectedEffect < 0 || selectedEffect >= int(clip->effects.size()) ||
        clip->effects[selectedEffect].id != "grading" || image.rgba.empty())
        return;
    auto mouse = ImGui::GetIO().MousePos;
    float u = (mouse.x - origin.x) / width - .5f - clip->param("x", s.time, 0),
          v = (mouse.y - origin.y) / height - .5f - clip->param("y", s.time, 0);
    float aspect = float(s.project.width) / s.project.height,
          a = -clip->param("rotation", s.time, 0) * .01745329252f, scale = clip->param("scale", s.time, 1);
    float x = (std::cos(a) * u * aspect - std::sin(a) * v) / scale / aspect + .5f,
          y = (std::sin(a) * u * aspect + std::cos(a) * v) / scale + .5f;
    if (x < 0 || y < 0 || x >= 1 || y >= 1)
        return;
    unsigned px = std::min(image.width - 1, unsigned(x * image.width)),
             py = std::min(image.height - 1, unsigned(y * image.height));
    ColorRGB rgb{};
    unsigned count = 0;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            int xx = int(px) + dx, yy = int(py) + dy;
            if (xx < 0 || yy < 0 || xx >= int(image.width) || yy >= int(image.height))
                continue;
            auto i = (size_t(yy) * image.width + xx) * 4;
            if (image.rgba[i + 3] <= .001f)
                continue;
            auto c = convertColor({image.rgba[i], image.rgba[i + 1], image.rgba[i + 2]}, image.colorSpace,
                                  "linear709");
            for (int k = 0; k < 3; ++k)
                rgb[k] += c[k];
            ++count;
        }
    if (!count)
        return;
    rgb = rgb / float(count);
    if (gradePick == 1) {
        auto args = target(clip->id, selectedEffect);
        args["r"] = rgb.x;
        args["g"] = rgb.y;
        args["b"] = rgb.z;
        action("neutral_grade", args);
    } else {
        auto encoded = convertColor(rgb, "linear709", "srgb");
        float h, sat, val;
        ImGui::ColorConvertRGBtoHSV(std::max(0.f, encoded.x), std::max(0.f, encoded.y),
                                    std::max(0.f, encoded.z), h, sat, val);
        auto before = s.project;
        clip->effects[selectedEffect].params["hue"] = {h};
        clip->effects[selectedEffect].params["qualifier"] = {1};
        s.history.checkpoint(before, "Color Grading");
        s.dirty = true;
        s.renderer.clearHistory();
    }
    gradePick = 0;
}
void Editor::gradingPanel() {
    if (!pendingGradeReference.empty())
        try {
            auto &asset = s.media.get(pendingGradeReference);
            if (!asset.loading) {
                captureGradeReference(s.media.frame(pendingGradeReference, 0));
                pendingGradeReference.clear();
                gradeComparison = 2;
            }
        } catch (const std::exception &ex) {
            error = ex.what();
            pendingGradeReference.clear();
        }
    if (auto *timeline = ImGui::FindWindowByName(panelTitle("Timeline").c_str());
        timeline && timeline->DockId)
        ImGui::SetNextWindowDockID(timeline->DockId, ImGuiCond_FirstUseEver);
    if (!panels["Color Grading"] || !beginPanel("Color Grading"))
        return;
    gradeComparisonControls();
    if (!pendingGradeReference.empty())
        ImGui::TextDisabled("Loading reference...");
    auto *clip = s.project.findClip(s.selected);
    if (!clip || s.project.owner(clip->id)->kind != TrackKind::Video) {
        ImGui::TextWrapped(
            "Select a video clip. Each correction keeps its own masks, curves and LUT.");
        ImGui::End();
        return;
    }
    ImGui::BeginDisabled(s.project.owner(clip->id)->locked);
    auto args = target(clip->id, selectedEffect);
    if (ImGui::Button("+ Correction")) {
        args["name"] = "Correction " + std::to_string(clip->effects.size() + 1);
        if (!action("add_grade", args)) {
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        selectedEffect = int(clip->effects.size() - 1);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s - clip corrections - RGBA16F", clip->name.c_str());
    if (ImGui::SmallButton("Open Layer Graph"))
        openPanel("Layer Graph");
    // These nodes are the actual ordered clip stack. Clicking a node selects its correction.
    ImGui::BeginChild("Grade graph", {0, 72}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    auto nodePosition = ImGui::GetCursorScreenPos();
    ImGui::Button("Source", {95, 44});
    ImVec2 previous{nodePosition.x + 95, nodePosition.y + 22};
    for (int i = 0; i < int(clip->effects.size()); ++i) {
        auto &effect = clip->effects[i];
        ImGui::SameLine(0, 32);
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddBezierCubic(previous, {previous.x + 15, previous.y},
                                                   {pos.x - 15, pos.y + 22}, {pos.x, pos.y + 22},
                                                   IM_COL32(239, 139, 58, 255), 2);
        bool wasSelected = i == selectedEffect;
        ImGui::PushID(i);
        if (wasSelected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.43f, .25f, .12f, 1));
        auto label = effect.id == "grading"
                         ? static_cast<const Effect &>(effect).grading["name"].S("Correction")
                         : effect.id;
        if (!effect.enabled)
            label += " (bypass)";
        std::string display = label;
        if (ImGui::CalcTextSize(display.c_str()).x > 130) {
            while (!display.empty() && ImGui::CalcTextSize((display + "…").c_str()).x > 130) {
                size_t end = display.size() - 1;
                while (end > 0 && (static_cast<unsigned char>(display[end]) & 0xc0) == 0x80)
                    --end;
                display.resize(end);
            }
            display += "…";
        }
        if (ImGui::Button(display.c_str(), {150, 44}))
            selectedEffect = i;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", label.c_str());
        if (wasSelected)
            ImGui::PopStyleColor();
        ImGui::PopID();
        previous = {pos.x + 150, pos.y + 22};
    }
    ImGui::EndChild();
    if (selectedEffect < 0 || selectedEffect >= int(clip->effects.size()) ||
        clip->effects[selectedEffect].id != "grading") {
        ImGui::TextUnformatted("Click a correction in the stack or add one.");
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }
    auto &effect = clip->effects[selectedEffect];
    args = target(clip->id, selectedEffect);
    bool enabled = effect.enabled;
    if (ImGui::Checkbox("Enable", &enabled)) {
        auto a = args;
        a["enabled"] = enabled;
        if (!action("set_effect", a)) {
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
    }
    ImGui::SameLine();
    if (effectControls(clip->id, selectedEffect)) {
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }
    ImGui::SameLine();
    std::string name = static_cast<const Effect &>(effect).grading["name"].S("Correction");
    ImGui::SetNextItemWidth(170);
    if (ImGui::InputText("Name", &name, ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto a = args;
        a["settings"] = Json::Object();
        a["settings"]["name"] = name;
        if (!action("set_grade", a)) {
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
    }
    auto value = [&](const char *k) {
        auto it = effect.params.find(k);
        if (it != effect.params.end())
            return it->second.at(s.time);
        for (auto &p : gradeControls())
            if (std::string(p.key) == k)
                return p.value;
        return 0.f;
    };
    auto assign = [&](const char *k, float f) {
        auto &p = effect.params[k];
        if (p.keys.empty())
            p.value = f;
        else
            p.setKey({snapTime(s.time, s.project.fps, true), f, Curve::Smooth});
        s.dirty = true;
        s.renderer.clearHistory();
    };
    auto control = [&](const char *key) {
        auto &all = gradeControls();
        auto it = std::find_if(all.begin(), all.end(), [&](auto &p) { return std::string(p.key) == key; });
        if (it == all.end())
            return;
        ImGui::PushID(key);
        float f = value(key), lo = it->minimum, hi = it->maximum;
        if (std::string(key) == "sat_min")
            hi = value("sat_max");
        if (std::string(key) == "sat_max")
            lo = value("sat_min");
        if (std::string(key) == "luma_min")
            hi = value("luma_max");
        if (std::string(key) == "luma_max")
            lo = value("luma_min");
        ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x - 190));
        bool changed = ImGui::SliderFloat(it->label, &f, lo, hi, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemActivated())
            s.history.checkpoint(s.project, "Color Grading");
        if (changed)
            assign(key, f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            animationKey = key;
            animationEffect = selectedEffect;
            openPanel("Animation");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("K")) {
            s.history.checkpoint(s.project, "Color Grading");
            effect.params.try_emplace(key, Parameter{it->value})
                .first->second.setKey({snapTime(s.time, s.project.fps, true), f, Curve::Smooth});
            s.dirty = true;
            animationKey = key;
            animationEffect = selectedEffect;
        }
        if (auto it = effect.params.find(key); it != effect.params.end() && !it->second.keys.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("◆");
        }
        ImGui::PopID();
    };
    control("intensity");
    if (ImGui::BeginTabBar("Grade tools")) {
        if (ImGui::BeginTabItem("Primaries")) {
            if (ImGui::Button("Neutral eyedropper")) {
                gradePick = 1;
                gradeMatte = false;
                gradeComparison = 0;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("sample before this correction, 5x5 average");
            for (auto key : {"temperature", "tint", "wb_r", "wb_g", "wb_b", "exposure", "contrast", "pivot",
                             "blacks", "whites", "saturation", "vibrance"})
                control(key);
            ImGui::TextWrapped("Creative balance relative to 6500 K. Wheels and primaries work in "
                               "linear Rec.709; HDR and negative values are preserved.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Wheels")) {
            const char *labels[] = {"Shadows", "Midtones", "Highlights", "Global Offset"};
            const char *keys[][3] = {{"shadow_r", "shadow_g", "shadow_b"},
                                     {"mid_r", "mid_g", "mid_b"},
                                     {"high_r", "high_g", "high_b"},
                                     {"offset_r", "offset_g", "offset_b"}};
            if (ImGui::BeginTable("Color wheels", 4, ImGuiTableFlags_SizingStretchSame))
                for (int w = 0; w < 4; ++w) {
                    ImGui::TableNextColumn();
                    ImGui::PushID(w);
                    ImGui::TextUnformatted(labels[w]);
                    auto pos = ImGui::GetCursorScreenPos();
                    float radius = std::min(55.f, std::max(24.f, ImGui::GetContentRegionAvail().x * .43f));
                    ImVec2 center{pos.x + radius + 3, pos.y + radius + 3};
                    ImGui::InvisibleButton("wheel", {radius * 2 + 6, radius * 2 + 6});
                    auto *d = ImGui::GetWindowDrawList();
                    for (int i = 0; i < 96; ++i) {
                        float a = i * 6.2831853f / 96, b = (i + 1) * 6.2831853f / 96, r, g, bb;
                        ImGui::ColorConvertHSVtoRGB(i / 96.f, .75f, .7f, r, g, bb);
                        d->AddTriangleFilled(
                            center, {center.x + radius * std::cos(a), center.y + radius * std::sin(a)},
                            {center.x + radius * std::cos(b), center.y + radius * std::sin(b)},
                            ImGui::ColorConvertFloat4ToU32({r, g, bb, 1}));
                    }
                    float r = value(keys[w][0]), g = value(keys[w][1]), b = value(keys[w][2]),
                          lum = (r + g + b) / 3;
                    float hx = (r - (g + b) / 2) * .5f, hy = (g - b) * .4330127f;
                    d->AddCircle({center.x + hx * radius, center.y + hy * radius}, 4, IM_COL32_WHITE, 12, 2);
                    if (ImGui::IsItemActivated())
                        s.history.checkpoint(s.project, "Color Grading");
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
                        auto m = ImGui::GetIO().MousePos;
                        float x = (m.x - center.x) / radius, y = (m.y - center.y) / radius,
                              len = std::hypot(x, y);
                        if (len > 1) {
                            x /= len;
                            y /= len;
                        }
                        assign(keys[w][0], std::clamp(lum + x, -1.f, 1.f));
                        assign(keys[w][1], std::clamp(lum - x * .5f + y * .8660254f, -1.f, 1.f));
                        assign(keys[w][2], std::clamp(lum - x * .5f - y * .8660254f, -1.f, 1.f));
                    }
                    float old = lum;
                    bool changed = ImGui::SliderFloat("Luminance", &lum, -.5f, .5f, "%.3f");
                    if (ImGui::IsItemActivated())
                        s.history.checkpoint(s.project, "Color Grading");
                    if (changed)
                        for (auto key : keys[w])
                            assign(key, std::clamp(value(key) + lum - old, -1.f, 1.f));
                    if (ImGui::Button("Reset")) {
                        s.history.checkpoint(s.project, "Color Grading");
                        for (auto key : keys[w])
                            assign(key, 0);
                    }
                    for (auto key : keys[w])
                        control(key);
                    ImGui::PopID();
                }
            if (ImGui::GetCurrentTable())
                ImGui::EndTable();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Curves")) {
            const char *names[] = {"master", "r", "g", "b", "hue_hue", "hue_sat", "hue_luma", "luma_sat"};
            ImGui::SetNextItemWidth(220);
            ImGui::Combo("Curve", &gradeCurveIndex,
                         "Master\0Red\0Green\0Blue\0Hue -> Hue\0Hue -> Saturation\0Hue -> "
                         "luminance\0Luminance → saturation\0");
            const char *key = names[gradeCurveIndex];
            auto points = static_cast<const Effect &>(effect).grading["curves"][key];
            float defaultY = gradeCurveIndex == 4 ? 0 : gradeCurveIndex >= 5 ? 1 : 0;
            if (points.IsNull()) {
                points = Json::Array();
                for (int i = 0; i < 2; ++i) {
                    Json p = Json::Array();
                    p.Push(i);
                    p.Push(gradeCurveIndex < 4 ? float(i) : defaultY);
                    points.Push(p);
                }
            }
            auto pos = ImGui::GetCursorScreenPos();
            ImVec2 size{std::max(160.f, ImGui::GetContentRegionAvail().x - 12), 170};
            ImGui::InvisibleButton("Curve canvas", size);
            bool hovered = ImGui::IsItemHovered();
            auto *d = ImGui::GetWindowDrawList();
            d->AddRectFilled(pos, pos + size, IM_COL32(17, 20, 28, 255));
            float low = gradeCurveIndex == 4 ? -.5f : 0, high = gradeCurveIndex == 4   ? .5f
                                                                : gradeCurveIndex >= 5 ? 3.f
                                                                                       : 1;
            auto screen = [&](float x, float y) {
                return ImVec2(pos.x + x * size.x, pos.y + (high - y) / (high - low) * size.y);
            };
            for (int i = 1; i < 4; ++i) {
                d->AddLine(pos + ImVec2(size.x * i / 4, 0), pos + ImVec2(size.x * i / 4, size.y),
                           IM_COL32(65, 69, 80, 100));
                d->AddLine(pos + ImVec2(0, size.y * i / 4), pos + ImVec2(size.x, size.y * i / 4),
                           IM_COL32(65, 69, 80, 100));
            }
            auto mouse = ImGui::GetIO().MousePos;
            int nearest = -1;
            float distance = 100;
            for (size_t i = 0; i < points.Size(); ++i) {
                auto p = screen(points.At(i).At(0).F(), points.At(i).At(1).F());
                float length = std::hypot(p.x - mouse.x, p.y - mouse.y);
                if (length < distance) {
                    distance = length;
                    nearest = int(i);
                }
                d->AddCircleFilled(p, 4, IM_COL32(255, 174, 90, 255));
                if (i) {
                    auto prev = screen(points.At(i - 1).At(0).F(), points.At(i - 1).At(1).F());
                    d->AddLine(prev, p, IM_COL32(239, 139, 58, 255), 2);
                }
            }
            std::vector<std::pair<float, float>> values;
            for (auto &p : points.Elements())
                values.emplace_back(p.At(0).F(), p.At(1).F());
            bool update = false;
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && distance < 10) {
                s.history.checkpoint(s.project, "Color Grading");
                gradeCurvePoint = nearest;
                gradeGestureClip = clip->id;
                gradeGestureEffect = selectedEffect;
            }
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && distance >= 10 &&
                values.size() < 32) {
                float x = std::clamp((mouse.x - pos.x) / size.x, .001f, .999f);
                float y = std::clamp(high - (mouse.y - pos.y) / size.y * (high - low), low, high);
                if (std::none_of(values.begin(), values.end(),
                                 [&](auto p) { return std::abs(p.first - x) < .001f; })) {
                    s.history.checkpoint(s.project, "Color Grading");
                    values.emplace_back(x, y);
                    std::sort(values.begin(), values.end());
                    update = true;
                }
            }
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && distance < 10 && nearest > 0 &&
                nearest + 1 < int(values.size())) {
                s.history.checkpoint(s.project, "Color Grading");
                values.erase(values.begin() + nearest);
                update = true;
            }
            if (gradeCurvePoint >= 0 && gradeGestureClip == clip->id &&
                gradeGestureEffect == selectedEffect && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                gradeCurvePoint < int(values.size())) {
                auto &p = values[gradeCurvePoint];
                if (gradeCurvePoint > 0 && gradeCurvePoint + 1 < int(values.size()))
                    p.first =
                        std::clamp((mouse.x - pos.x) / size.x, values[gradeCurvePoint - 1].first + .0001f,
                                   values[gradeCurvePoint + 1].first - .0001f);
                p.second = std::clamp(high - (mouse.y - pos.y) / size.y * (high - low), low, high);
                if (gradeCurveIndex >= 4 && gradeCurveIndex <= 6 &&
                    (gradeCurvePoint == 0 || gradeCurvePoint + 1 == int(values.size())))
                    values.front().second = values.back().second = p.second;
                update = true;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                gradeCurvePoint = -1;
            if (update) {
                Json curve = Json::Array();
                for (auto [x, y] : values) {
                    Json p = Json::Array();
                    p.Push(x);
                    p.Push(y);
                    curve.Push(p);
                }
                effect.grading["curves"][key] = curve;
                s.dirty = true;
                s.renderer.clearHistory();
            }
            ImGui::TextDisabled("Double-click: add - right-click: remove - drag: move - "
                                "HDR extrapolation of RGB curves");
            if (ImGui::Button("Reset Curve")) {
                s.history.checkpoint(s.project, "Color Grading");
                auto curves = static_cast<const Effect &>(effect).grading["curves"];
                Json clean = Json::Object();
                for (auto &[k, v] : curves.Items())
                    if (k != key)
                        clean[k] = v;
                effect.grading["curves"] = clean;
                s.dirty = true;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Selection & Mask")) {
            bool qualifier = value("qualifier") > .5f;
            if (ImGui::Checkbox("HSL Selection", &qualifier)) {
                s.history.checkpoint(s.project, "Color Grading");
                assign("qualifier", qualifier ? 1.f : 0.f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Color Eyedropper")) {
                gradePick = 2;
                gradeMatte = false;
                gradeComparison = 0;
            }
            for (auto k : {"hue", "hue_width", "hue_soft", "sat_min", "sat_max", "luma_min", "luma_max",
                           "qual_soft", "qual_invert"})
                control(k);
            int mode = int(std::round(value("mask_mode")));
            if (ImGui::Combo("Local mask", &mode, "None\0Rectangle\0Ellipse\0Polygon\0")) {
                s.history.checkpoint(s.project, "Color Grading");
                if (mode == 3 && static_cast<const Effect &>(effect).grading["maskPoints"].IsNull()) {
                    Json points = Json::Array();
                    for (auto p :
                         {ImVec2(-.5f, -.5f), ImVec2(.5f, -.5f), ImVec2(.5f, .5f), ImVec2(-.5f, .5f)}) {
                        Json j = Json::Array();
                        j.Push(p.x);
                        j.Push(p.y);
                        points.Push(j);
                    }
                    effect.grading["maskPoints"] = points;
                }
                assign("mask_mode", float(mode));
            }
            for (auto k : {"mask_x", "mask_y", "mask_width", "mask_height", "mask_feather", "mask_rotation",
                           "mask_invert"})
                control(k);
            if (mode == 3) {
                auto points = static_cast<const Effect &>(effect).grading["maskPoints"];
                Json replacement = Json::Array();
                bool changed = false;
                for (size_t i = 0; i < points.Size(); ++i) {
                    ImGui::PushID(int(i));
                    float xy[] = {points.At(i).At(0).F(), points.At(i).At(1).F()};
                    bool edit = ImGui::SliderFloat2("Vertex", xy, -1, 1);
                    if (ImGui::IsItemActivated())
                        s.history.checkpoint(s.project, "Color Grading");
                    changed |= edit;
                    Json p = Json::Array();
                    p.Push(xy[0]);
                    p.Push(xy[1]);
                    replacement.Push(p);
                    ImGui::PopID();
                }
                if (changed) {
                    effect.grading["maskPoints"] = replacement;
                    s.dirty = true;
                }
                if (points.Size() < 16 && ImGui::Button("Add Vertex")) {
                    s.history.checkpoint(s.project, "Color Grading");
                    Json p = Json::Array();
                    p.Push(0);
                    p.Push(.5f);
                    replacement.Push(p);
                    effect.grading["maskPoints"] = replacement;
                    s.dirty = true;
                }
            }
            ImGui::TextWrapped("Mask and qualifier multiply this correction's strength. "
                               "Alpha and pixels outside the mask stay intact. Tracking calculates "
                               "translation and requires a high-contrast region in video media.");
            if (ImGui::Button("Track to Clip End")) {
                auto a = args;
                a["begin"] = s.time;
                a["end"] = clip->start + clip->duration;
                a["radius"] = 24;
                action("start_mask_tracking", a);
            }
            auto status = s.tracking.status();
            ImGui::SameLine();
            ImGui::Text("%s · %.0f%%", status["state"].S().c_str(), status["progress"].F() * 100);
            if (status["state"].S() == "completed") {
                if (ImGui::Button("Apply tracking")) {
                    Json a = Json::Object();
                    a["job"] = status["job"];
                    action("apply_mask_tracking", a);
                }
            }
            if (status["state"].S() == "running" && ImGui::Button("Cancel Tracking"))
                action("cancel_mask_tracking");
            if (!status["error"].S().empty())
                ImGui::TextWrapped("%s", status["error"].S().c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("LUT & looks")) {
            ImGui::Combo("Image LUT layout", &gradeLutLayout, "Automatic\0 3D atlas\0 2D RG map\0");
            if (ImGui::Button("Import .cube / 2D image LUT…")) {
                auto path = dialog(false, L"LUT Cube\0*.cube;*.png;*.tif;*.exr\0\0", L"cube");
                if (!path.empty()) {
                    auto a = args;
                    a["path"] = path;
                    a["layout"] = gradeLutLayout == 1 ? "atlas" : gradeLutLayout == 2 ? "rg" : "auto";
                    if (!action("import_grade_lut", a)) {
                        ImGui::EndTabItem();
                        ImGui::EndTabBar();
                        ImGui::EndDisabled();
                        ImGui::End();
                        return;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove LUT")) {
                auto a = args;
                a["settings"] = Json::Object();
                a["settings"]["lut"] = {};
                action("set_grade", a);
            }
            const auto &settings = static_cast<const Effect &>(effect).grading;
            auto &lut = settings["lut"];
            if (!lut.IsNull())
                ImGui::Text("%s - %dD LUT - size %d - embedded in project",
                            lut["title"].S("Untitled").c_str(), lut["dimension"].I(), lut["size"].I());
            const char *spaces[] = {"linear709", "srgb", "linear2020", "logc3-awg3-ei800"};
            for (auto key : {"lutInput", "lutOutput"}) {
                int current = 0;
                for (int i = 0; i < 4; ++i)
                    if (settings[key].S("linear709") == spaces[i])
                        current = i;
                if (ImGui::Combo(key == std::string("lutInput") ? "LUT Input Space" : "LUT Output Space",
                                 &current,
                                 "Linear Rec.709\0sRGB\0Linear Rec.2020\0ARRI LogC3 / AWG3 - EI800\0")) {
                    auto a = args;
                    a["settings"] = Json::Object();
                    a["settings"][key] = spaces[current];
                    if (!action("set_grade", a)) {
                        ImGui::EndTabItem();
                        ImGui::EndTabBar();
                        ImGui::EndDisabled();
                        ImGui::End();
                        return;
                    }
                }
            }
            control("lut_intensity");
            ImGui::TextWrapped("3D LUT: trilinear interpolation, red changes fastest. The .cube domain "
                               "clamps the input; other corrections preserve HDR. "
                               "Color spaces must match those intended by the LUT author.");
            if (ImGui::BeginCombo("Preset", "Choose…")) {
                for (auto name :
                     {"Neutral", "Warm", "Cool", "Black and White", "Cyan shadows / warm highlights"})
                    if (ImGui::Selectable(name)) {
                        auto a = args;
                        a["name"] = name;
                        action("apply_grade_preset", a);
                    }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Save .sgrade…")) {
                auto path = dialog(true, L"ShaderixFrame Color Grade\0*.sgrade\0\0", L"sgrade");
                if (!path.empty()) {
                    auto a = args;
                    a["path"] = path;
                    action("save_grade_preset", a);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load .sgrade…")) {
                auto path = dialog(false, L"ShaderixFrame Color Grade\0*.sgrade\0\0", L"sgrade");
                if (!path.empty()) {
                    auto a = args;
                    a["path"] = path;
                    action("load_grade_preset", a);
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndDisabled();
    ImGui::End();
}
void Editor::scopesPanel() {
    if (auto *properties = ImGui::FindWindowByName(panelTitle("Properties").c_str());
        properties && properties->DockId)
        ImGui::SetNextWindowDockID(properties->DockId, ImGuiCond_FirstUseEver);
    if (!panels["Video Scopes"]) {
        gradeScopesVisible = false;
        return;
    }
    gradeScopesVisible = ImGui::Begin(panelTitle("Video Scopes").c_str(), &panels["Video Scopes"]);
    if (!gradeScopesVisible) {
        ImGui::End();
        return;
    }
    ImGui::Combo("Measure", &gradeScopeIndex,
                 "Luma waveform\0RGB parade\0Vectorscope\0RGB histogram\0");
    auto pos = ImGui::GetCursorScreenPos();
    ImVec2 size{std::max(100.f, ImGui::GetContentRegionAvail().x),
                std::max(120.f, ImGui::GetContentRegionAvail().y - 80)};
    size.y = std::min(size.y, 350.f);
    if (gradeScopeIndex == 2)
        size.x = size.y = std::min(size.x, size.y);
    ImGui::InvisibleButton("Scopes", size);
    auto *d = ImGui::GetWindowDrawList();
    d->AddRectFilled(pos, pos + size, IM_COL32(10, 13, 19, 255));
    for (int i = 0; i < 5; ++i)
        d->AddLine(pos + ImVec2(0, size.y * i / 4), pos + ImVec2(size.x, size.y * i / 4),
                   IM_COL32(90, 95, 110, 60));
    if (gradeScopeIndex < 3) {
        auto &bins = gradeScopeIndex == 0   ? gradeScopes.waveform
                     : gradeScopeIndex == 1 ? gradeScopes.parade
                                            : gradeScopes.vectorscope;
        unsigned nx = gradeScopeIndex == 1 ? 128 : 256, ny = gradeScopeIndex == 2 ? 256 : 128,
                 channels = gradeScopeIndex == 1 ? 3 : 1;
        unsigned maximum = bins.empty() ? 0 : *std::max_element(bins.begin(), bins.end());
        if (maximum)
            for (unsigned k = 0; k < channels; ++k)
                for (unsigned y = 0; y < ny; ++y)
                    for (unsigned x = 0; x < nx; ++x) {
                        unsigned count = bins[k * nx * ny + y * nx + x];
                        if (!count)
                            continue;
                        float alpha = std::clamp(std::log2(float(count) + 1) / std::log2(float(maximum) + 1),
                                                 .15f, 1.f);
                        ImU32 color = gradeScopeIndex == 1
                                          ? (k == 0   ? IM_COL32(245, 70, 60, int(alpha * 255))
                                             : k == 1 ? IM_COL32(80, 230, 115, int(alpha * 255))
                                                      : IM_COL32(80, 150, 255, int(alpha * 255)))
                                          : IM_COL32(120, 220, 185, int(alpha * 255));
                        ImVec2 a = pos + ImVec2(size.x * (k + float(x) / nx) / channels, size.y * y / ny),
                               b = pos +
                                   ImVec2(size.x * (k + float(x + 1) / nx) / channels, size.y * (y + 1) / ny);
                        d->AddRectFilled(a, b, color);
                    }
        if (gradeScopeIndex == 2) {
            d->AddCircle(pos + size * .5f, std::min(size.x, size.y) * .4f, IM_COL32(130, 135, 145, 100));
            d->AddLine(pos + ImVec2(size.x * .5f, 0), pos + ImVec2(size.x * .5f, size.y),
                       IM_COL32(130, 135, 145, 100));
        }
    } else if (!gradeScopes.histogram.empty())
        for (int k = 0; k < 3; ++k) {
            unsigned maximum = *std::max_element(gradeScopes.histogram.begin(), gradeScopes.histogram.end());
            if (!maximum)
                break;
            ImU32 colors[] = {IM_COL32(245, 70, 60, 220), IM_COL32(80, 230, 115, 220),
                              IM_COL32(80, 150, 255, 220)};
            for (int x = 1; x < 256; ++x) {
                auto h = [&](int xx) {
                    return std::log2(float(gradeScopes.histogram[k * 256 + xx]) + 1) /
                           std::log2(float(maximum) + 1);
                };
                d->AddLine(pos + ImVec2(size.x * (x - 1) / 255, size.y * (1 - h(x - 1))),
                           pos + ImVec2(size.x * x / 255, size.y * (1 - h(x))), colors[k], 1.5f);
            }
        }
    ImGui::Text("%u samples - min %.4f - max %.4f", gradeScopes.samples, gradeScopes.minimum,
                gradeScopes.maximum);
    ImGui::Text("Below Zero: %u - Above White: %u", gradeScopes.below, gradeScopes.above);
    ImGui::TextWrapped("Scopes after SDR display / tone mapping. Min/max: linear Rec.709. "
                       "Limited sampling, 4 Hz refresh.");
    ImGui::End();
}
} // namespace sf

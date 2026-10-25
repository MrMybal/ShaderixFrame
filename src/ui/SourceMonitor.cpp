// Media bin and source monitor: browse the imported media, mark a span with in/out points and place
// it on the timeline with an insert or overwrite edit (three-point editing).
#include "ui/Editor.h"
#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <cmath>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <set>

namespace sf {
using namespace style;
std::string timecode(double time, int fps);

// Decoded at most every 40 ms and when the pointer moved by a sixtieth of the medium, so skimming
// stays light; a medium that cannot open is not retried while the pointer stays on it.
ID3D11ShaderResourceView *Editor::skimFrame(const std::string &path, double fraction) {
    if (skimPath != path) {
        skimPath = path;
        skimView.Reset();
        skimTime = -1;
        skimAsset = std::make_unique<MediaAsset>();
        try {
            skimAsset->open(path, {}, false);
        } catch (...) {
            skimAsset.reset();
        }
    }
    if (!skimAsset || !skimAsset->video || skimAsset->image || skimAsset->duration <= 0)
        return nullptr;
    double t = std::clamp(fraction, 0., 1.) * std::max(0., skimAsset->duration - .05);
    bool moved = skimTime < 0 || std::abs(t - skimTime) > skimAsset->duration / 60;
    if (moved && ImGui::GetTime() - skimDecoded > .04) {
        try {
            auto &image = skimAsset->at(t);
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = image.width;
            desc.Height = image.height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA pixels{image.pixels.data(), image.width * 4, 0};
            Com<ID3D11Texture2D> texture;
            Com<ID3D11ShaderResourceView> view;
            if (!image.pixels.empty() && SUCCEEDED(s.renderer.gpu()->CreateTexture2D(&desc, &pixels, &texture)) &&
                SUCCEEDED(s.renderer.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &view)))
                skimView = view;
            skimTime = t;
            skimDecoded = ImGui::GetTime();
        } catch (...) {
            skimAsset.reset();
            return nullptr;
        }
    }
    return skimView.Get();
}
ID3D11ShaderResourceView *Editor::mediaThumbnail(const std::string &path) {
    try {
        auto &asset = s.media.get(path);
        if (asset.thumbnail.pixels.empty())
            return nullptr;
        auto found = thumbnails.find(path);
        if (found == thumbnails.end()) {
            if (thumbnails.size() >= 128)
                thumbnails.erase(thumbnails.begin());
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = asset.thumbnail.width;
            desc.Height = asset.thumbnail.height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA pixels{asset.thumbnail.pixels.data(), asset.thumbnail.width * 4, 0};
            Com<ID3D11Texture2D> texture;
            Com<ID3D11ShaderResourceView> view;
            if (FAILED(s.renderer.gpu()->CreateTexture2D(&desc, &pixels, &texture)) ||
                FAILED(s.renderer.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &view)))
                return nullptr;
            found = thumbnails.emplace(path, std::move(view)).first;
        }
        return found->second.Get();
    } catch (...) {
        return nullptr;
    }
}

// Match frame: the selected clip's medium opens in Source at the frame under the playhead, its span marked.
void Editor::matchFrame() {
    auto *clip = s.project.findClip(s.selected);
    if (!clip || clip->source.empty())
        return;
    openSource(clip->source);
    sourceIn = clip->in;
    sourceOut = clip->in + clip->duration * clip->speed;
    sourceTime = std::clamp(clip->sourceTime(std::clamp(s.time, clip->start, clip->start + clip->duration)), sourceIn,
                            std::max(sourceIn, sourceOut - 1. / s.project.fps));
    sourcePlaying = false;
}

void Editor::openSource(const std::string &path) {
    if (sourcePath != path) {
        sourcePath = path;
        sourceAsset.reset();
        sourceFrame.Reset();
        sourceFrameTime = -1;
        sourceTime = sourceIn = sourceOut = 0;
        sourceVideo = sourceAudio = true;
    }
    openPanel("Source");
}

void Editor::sourceEdit(bool insert, bool fit) {
    if (sourcePath.empty())
        return;
    Json a = Json::Object();
    a["path"] = sourcePath;
    a["in"] = sourceIn;
    if (sourceOut > sourceIn)
        a["out"] = sourceOut;
    a["start"] = s.time;
    a["mode"] = fit ? "fit" : insert ? "insert" : "overwrite";
    a["video"] = sourceVideo;
    a["audio"] = sourceAudio;
    // Target tracks chosen in the timeline headers, when still valid.
    auto usable = [&](Id id, TrackKind kind) {
        auto *track = s.project.findTrack(id);
        return track && track->kind == kind && !track->locked;
    };
    if (usable(targetVideo, TrackKind::Video))
        a["video_track"] = targetVideo;
    if (usable(targetAudio, TrackKind::Audio))
        a["audio_track"] = targetAudio;
    action("edit_source", a);
}

// Bin: every medium imported in the project, as cards; double click opens it in the source monitor.
void Editor::mediaBin() {
    std::vector<std::string> media = s.project.bin;
    for (auto &track : s.project.tracks)
        for (auto &clip : track.clips)
            if (!clip.source.empty() && std::find(media.begin(), media.end(), clip.source) == media.end())
                media.push_back(clip.source);
    if (ImGui::Button("+ Bin")) {
        auto path = dialog(false, L"Media\0*.mp4;*.mov;*.mkv;*.avi;*.wmv;*.wav;*.mp3;*.flac;*.aac;*.png;*.jpg;*.jpeg;*.bmp\0All\0*.*\0\0",
                           nullptr);
        if (!path.empty()) {
            auto absolute = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(path)).wstring());
            if (std::find(s.project.bin.begin(), s.project.bin.end(), absolute) == s.project.bin.end()) {
                s.history.checkpoint(s.project, "Source editing");
                s.project.bin.push_back(absolute);
                s.dirty = true;
            }
            openSource(absolute);
        }
    }
    tooltip("Add media to the bin without placing it; open it in Source next");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove Unused")) {
        if (action("remove_unused_media"))
            say(std::to_string(actionResult["removed"].Size()) + " media file(s) removed from the bin");
    }
    tooltip("Remove media unused by any clip from the bin (files stay on disk)");
    ImGui::SameLine();
    ImGui::Checkbox("Proxys auto", &autoProxies);
    tooltip("Prepare a 720p proxy for videos over 1440 lines on import");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##binSearch", "Search…", &binFilter);
    if (media.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("Import video, images or audio to start editing.");
        return;
    }
    float card = 128, width = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, int((width + 6) / (card + 6)));
    int column = 0;
    for (auto &path : media) {
        std::string name = sx::Narrow(sx::Utf8Path(path).filename().wstring());
        if (!binFilter.empty()) {
            auto lower = [](std::string text) {
                for (auto &c : text)
                    c = char(std::tolower(static_cast<unsigned char>(c)));
                return text;
            };
            if (lower(name).find(lower(binFilter)) == std::string::npos)
                continue;
        }
        if (column++ % columns)
            ImGui::SameLine(0, 6);
        ImGui::PushID(path.c_str());
        ImGui::BeginGroup();
        auto at = ImGui::GetCursorScreenPos();
        float imageHeight = card * 9 / 16;
        ImGui::InvisibleButton("##card", {card, imageHeight + 34});
        bool hovered = ImGui::IsItemHovered();
        if (hovered && ImGui::IsMouseDoubleClicked(0))
            openSource(path);
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("MEDIA", path.c_str(), path.size() + 1);
            ImGui::TextUnformatted(name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem("binCard")) {
            if (ImGui::MenuItem("Open in Source"))
                openSource(path);
            // A preview proxy is prepared from a clip that uses the medium.
            Id user = 0;
            for (auto &track : s.project.tracks)
                for (auto &clip : track.clips)
                    if (clip.source == path && track.kind == TrackKind::Video && !user)
                        user = clip.id;
            if (ImGui::MenuItem("Prepare Preview Proxy", nullptr, false, user != 0)) {
                Json a = Json::Object();
                a["clip"] = user;
                action("prepare_proxy", a);
            }
            if (!user && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Place the video media on the timeline first");
            bool used = false;
            for (auto &track : s.project.tracks)
                for (auto &clip : track.clips)
                    used = used || clip.source == path;
            if (ImGui::MenuItem("Remove from Bin", nullptr, false, !used)) {
                s.history.checkpoint(s.project, "Source editing");
                std::erase(s.project.bin, path);
                s.dirty = true;
            }
            if (used)
                ImGui::TextDisabled("Used in Sequence");
            ImGui::EndPopup();
        }
        auto *dl = ImGui::GetWindowDrawList();
        ImVec2 imageMax = at + ImVec2(card, imageHeight);
        dl->AddRectFilled(at, at + ImVec2(card, imageHeight + 34), hovered ? bg3 : bg2, 4);
        dl->AddRectFilled(at, imageMax, IM_COL32(0, 0, 0, 255), 4, ImDrawFlags_RoundCornersTop);
        std::string kind = "OFFLINE";
        double length = 0;
        bool loading = false, audioOnly = false, sound = false;
        unsigned width = 0, height = 0;
        try {
            auto &asset = s.media.get(path);
            loading = asset.loading;
            sound = asset.hasAudio;
            if (asset.video || asset.image) {
                width = asset.frame.width;
                height = asset.frame.height;
            }
            length = asset.image ? 0 : asset.duration;
            audioOnly = !asset.video;
            kind = asset.image ? "IMAGE" : asset.video && asset.hasAudio ? "VIDEO + AUDIO" : asset.video ? "VIDEO" : "AUDIO";
        } catch (...) {
        }
        // Skimming: the pointer across the card runs through the medium.
        ID3D11ShaderResourceView *skimmed = nullptr;
        float skimAt = (ImGui::GetIO().MousePos.x - at.x) / card;
        if (hovered && !audioOnly && !loading && kind != "IMAGE" && !ImGui::IsMouseDragging(0))
            skimmed = skimFrame(path, skimAt);
        if (skimmed) {
            dl->AddImage((ImTextureID)(intptr_t)skimmed, at, imageMax);
            float x = at.x + std::clamp(skimAt, 0.f, 1.f) * card;
            dl->AddLine({x, at.y}, {x, imageMax.y}, accent, 1.5f);
            smallText(dl, at + ImVec2(4, imageHeight - 15), text, timecode(skimTime, s.project.fps).c_str(), .72f);
        } else if (auto *thumbnail = mediaThumbnail(path))
            dl->AddImage((ImTextureID)(intptr_t)thumbnail, at, imageMax);
        else
            centredLabel(dl, at, imageMax, audioOnly ? icon::audio : loading ? "…" : icon::video);
        if (path == sourcePath)
            dl->AddRect(at, at + ImVec2(card, imageHeight + 34), accent, 4, 0, 1.5f);
        clippedText(dl, at + ImVec2(5, imageHeight + 3), card - 10, text, name.c_str(), .8f);
        std::string details = kind + (length > 0 ? "  ·  " + timecode(length, s.project.fps) : "");
        clippedText(dl, at + ImVec2(5, imageHeight + 18), card - 10, textDim, details.c_str(), .7f);
        if (hovered && !ImGui::IsMouseDragging(0) && ImGui::BeginTooltip()) {
            // The medium in full: kind, picture size, length, sound, file size and uses in the sequence.
            ImGui::PushFont(boldFont(), 0);
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopFont();
            ImGui::TextDisabled("%s", path.c_str());
            ImGui::Separator();
            ImGui::Text("%s", kind.c_str());
            if (width && height)
                ImGui::Text("%u × %u px", width, height);
            if (length > 0)
                ImGui::Text("Duration %s", timecode(length, s.project.fps).c_str());
            if (sound)
                ImGui::Text("Stereo 48 kHz Audio");
            std::error_code missing;
            auto bytes = std::filesystem::file_size(sx::Utf8Path(path), missing);
            if (!missing)
                ImGui::Text("%.1f Mo", double(bytes) / (1024. * 1024.));
            int uses = 0;
            for (auto &track : s.project.tracks)
                for (auto &clip : track.clips)
                    uses += clip.source == path;
            if (uses)
                ImGui::Text("Used by %d clip%s", uses, uses > 1 ? "s" : "");
            else
                ImGui::TextDisabled("Not Yet in Sequence");
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Double-click: Source  ·  drag: timeline");
}

void Editor::centredLabel(ImDrawList *dl, ImVec2 min, ImVec2 max, const char *label) {
    auto extent = ImGui::CalcTextSize(label);
    dl->AddText((min + max - extent) * .5f, textDim, label);
}

// Source monitor: the picture of the opened medium at its own time, in/out marks and the edits.
void Editor::sourcePanel() {
    // A hidden monitor, or the sequence starting to play, stops the source playback.
    auto stopPlayback = [&] {
        if (sourceSound.playing())
            sourceSound.stop();
        sourcePlaying = false;
    };
    if (!panels["Source"] || !beginPanel("Source")) {
        stopPlayback();
        return;
    }
    if (s.playing)
        stopPlayback();
    if (sourcePath.empty()) {
        ImGui::TextWrapped("Double-click media in the bin (Media & Effects) to open it here, mark "
                           "in and out points, then insert or overwrite at the timeline playhead.");
        ImGui::End();
        return;
    }
    auto &io = ImGui::GetIO();
    double length = 0;
    bool hasVideo = false, hasAudio = false, still = false;
    std::vector<float> peaks;
    float ceiling = 1;
    try {
        auto &shared = s.media.get(sourcePath);
        length = shared.image ? 6 : shared.duration;
        hasVideo = shared.video;
        hasAudio = shared.hasAudio;
        still = shared.image;
        peaks = shared.peaks;
        ceiling = std::max(shared.peakCeiling, 1e-6f);
    } catch (const std::exception &e) {
        ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "Media Offline: %s", e.what());
        ImGui::End();
        return;
    }
    double frame = 1. / s.project.fps;
    double out = sourceOut > sourceIn ? sourceOut : length;
    // At normal speed the medium's sound plays and drives the clock; faster, the picture plays alone.
    bool withSound = sourcePlaying && !still && hasAudio && sourceAudio && sourceSpeed == 1 && sourceSound.available();
    if (withSound && !sourceSound.playing()) {
        if (sourceSoundPath != sourcePath) {
            sourceSoundProject = Project{};
            sourceSoundProject.duration = std::max(length, frame);
            auto &track = sourceSoundProject.addTrack(TrackKind::Audio);
            track.id = 0x7ffffff0; // distinct from the sequence's tracks in the shared mixer state
            Clip clip;
            clip.id = 0x7ffffff1;
            clip.source = sourcePath;
            clip.duration = std::max(length, frame);
            track.clips = {clip};
            sourceSoundPath = sourcePath;
        }
        s.playing = false;
        s.media.resetAudio();
        sourceSound.start(sourceTime);
    }
    if (!withSound && sourceSound.playing())
        sourceSound.stop();
    if (sourcePlaying && !still) {
        if (withSound && sourceSound.playing()) {
            try {
                sourceSound.pump(sourceSoundProject, s.media);
                sourceTime = sourceSound.time();
            } catch (const std::exception &e) {
                error = e.what();
                sourcePlaying = false;
            }
        } else
            sourceTime += io.DeltaTime * sourceSpeed;
        if (sourceTime >= out - frame) {
            sourceTime = out - frame;
            sourcePlaying = false;
            sourceSound.stop();
        }
    }
    sourceTime = std::clamp(sourceTime, 0., std::max(0., length - frame));
    // Picture: a private asset decodes the monitor's frames without seeking the sequence's readers.
    if (hasVideo && !sourceAsset) {
        try {
            sourceAsset = std::make_unique<MediaAsset>();
            sourceAsset->open(sourcePath, {}, false);
        } catch (const std::exception &e) {
            error = e.what();
            sourceAsset.reset();
        }
    }
    if (sourceAsset && std::abs(sourceFrameTime - sourceTime) > 1e-6) {
        try {
            auto &image = sourceAsset->at(sourceTime);
            if (!image.pixels.empty()) {
                if (!sourceFrame || sourceFrameWidth != image.width || sourceFrameHeight != image.height) {
                    D3D11_TEXTURE2D_DESC desc{};
                    desc.Width = image.width;
                    desc.Height = image.height;
                    desc.MipLevels = desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                    desc.SampleDesc.Count = 1;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    sourceTexture.Reset();
                    sourceFrame.Reset();
                    if (SUCCEEDED(s.renderer.gpu()->CreateTexture2D(&desc, nullptr, &sourceTexture)))
                        s.renderer.gpu()->CreateShaderResourceView(sourceTexture.Get(), nullptr, &sourceFrame);
                    sourceFrameWidth = image.width;
                    sourceFrameHeight = image.height;
                }
                if (sourceTexture)
                    s.renderer.gpuContext()->UpdateSubresource(sourceTexture.Get(), 0, nullptr, image.pixels.data(),
                                                               image.width * 4, 0);
            }
            sourceFrameTime = sourceTime;
        } catch (const std::exception &e) {
            error = e.what();
            sourceFrameTime = sourceTime;
        }
    }
    // Header.
    auto name = sx::Narrow(sx::Utf8Path(sourcePath).filename().wstring());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s%s", still ? "image" : timecode(length, s.project.fps).c_str(),
                        sourceFrameWidth ? ("  ·  " + std::to_string(sourceFrameWidth) + " × " +
                                            std::to_string(sourceFrameHeight)).c_str()
                                         : "");
    // Picture area.
    ImVec2 available = ImGui::GetContentRegionAvail();
    float pictureHeight = std::max(60.f, available.y - 34 - ImGui::GetFrameHeightWithSpacing() * 2 - 12);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##sourcePicture", {available.x, pictureHeight});
    auto *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, origin + ImVec2(available.x, pictureHeight), IM_COL32(0, 0, 0, 255));
    if (sourceFrame && sourceFrameWidth) {
        float ratio = float(sourceFrameWidth) / sourceFrameHeight;
        float w = available.x, h = pictureHeight;
        if (w / h > ratio)
            w = h * ratio;
        else
            h = w / ratio;
        ImVec2 at = origin + ImVec2((available.x - w) * .5f, (pictureHeight - h) * .5f);
        dl->AddImage((ImTextureID)(intptr_t)sourceFrame.Get(), at, at + ImVec2(w, h));
    } else
        centredLabel(dl, origin, origin + ImVec2(available.x, pictureHeight), hasAudio ? icon::audio : icon::video);
    // Dragging the picture places the marked span where it is dropped on the timeline.
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("SOURCE", sourcePath.c_str(), sourcePath.size() + 1);
        ImGui::Text("%s  ·  %s", name.c_str(), timecode(out - sourceIn, s.project.fps).c_str());
        ImGui::EndDragDropSource();
    }
    // Scrub bar with the in/out span and the sound's peaks.
    ImVec2 barMin = ImGui::GetCursorScreenPos(), barSize{available.x, 34};
    ImGui::InvisibleButton("##sourceScrub", barSize);
    sourceBarMin = barMin;
    sourceBarSize = barSize;
    auto barX = [&](double t) { return barMin.x + float(t / std::max(length, frame) * barSize.x); };
    // The in and out marks are handles; elsewhere the bar scrubs the picture.
    bool nearIn = std::abs(io.MousePos.x - barX(sourceIn)) < 6, nearOut = std::abs(io.MousePos.x - barX(out)) < 6;
    if (ImGui::IsItemHovered() && (nearIn || nearOut))
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActivated())
        sourceMarkDrag = nearOut && (!nearIn || io.MousePos.x > barX(sourceIn)) ? 2 : nearIn ? 1 : 0;
    if (ImGui::IsItemActive()) {
        double t = std::clamp(std::round((io.MousePos.x - barMin.x) / barSize.x * length / frame) * frame, 0., length);
        if (sourceMarkDrag == 1)
            sourceIn = std::min(t, out - frame);
        else if (sourceMarkDrag == 2) {
            sourceOut = std::max(t, sourceIn + frame);
            out = sourceOut;
        } else
            sourceTime = std::clamp(t, 0., std::max(0., length - frame));
        if (sourceMarkDrag)
            ImGui::SetTooltip("%s %s", sourceMarkDrag == 1 ? "Input" : "Output",
                              timecode(sourceMarkDrag == 1 ? sourceIn : sourceOut, s.project.fps).c_str());
    } else
        sourceMarkDrag = 0;
    dl->AddRectFilled(barMin, barMin + barSize, bg1, 3);
    if (!peaks.empty() && length > 0) {
        float mid = barMin.y + barSize.y * .5f;
        for (float x = 0; x < barSize.x; x += 2) {
            size_t index = size_t(x / barSize.x * length * 100);
            float peak = index < peaks.size() ? std::min(1.f, peaks[index] / ceiling) : 0;
            dl->AddLine({barMin.x + x, mid - peak * 13}, {barMin.x + x, mid + peak * 13}, alpha(style::audio, .55f));
        }
    }
    dl->AddRectFilled({barX(sourceIn), barMin.y}, {barX(out), barMin.y + barSize.y}, alpha(accent, .18f));
    dl->AddRectFilled({barX(sourceIn) - 1, barMin.y}, {barX(sourceIn) + 2, barMin.y + barSize.y}, accent);
    dl->AddRectFilled({barX(out) - 2, barMin.y}, {barX(out) + 1, barMin.y + barSize.y}, accent);
    dl->AddLine({barX(sourceTime), barMin.y}, {barX(sourceTime), barMin.y + barSize.y}, text, 2);
    // Transport, marks and edits.
    ImGui::BeginDisabled(still);
    if (iconButton(icon::previous, "Previous Frame [Left]"))
        sourceTime = std::max(0., sourceTime - frame);
    ImGui::SameLine(0, 2);
    bool focusedPanel = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
    auto play = [&] {
        if (sourceTime >= out - frame * 1.5)
            sourceTime = sourceIn; // from the in point when the span was played to its end
        sourcePlaying = true;
    };
    if (iconButton(sourcePlaying ? icon::pause : icon::play, "Play / Pause [Space] - L: faster, K: stop",
                   sourcePlaying, style::play) ||
        (focusedPanel && ImGui::IsKeyPressed(ImGuiKey_Space, false))) {
        if (sourcePlaying)
            sourcePlaying = false;
        else {
            sourceSpeed = 1;
            play();
        }
    }
    if (focusedPanel && ImGui::IsKeyPressed(ImGuiKey_K, false))
        sourcePlaying = false;
    if (focusedPanel && ImGui::IsKeyPressed(ImGuiKey_L, false)) {
        sourceSpeed = sourcePlaying ? std::min(4.f, sourceSpeed * 2) : 1.f;
        play();
    }
    if (focusedPanel && ImGui::IsKeyPressed(ImGuiKey_J, false)) {
        sourcePlaying = false;
        sourceTime = std::max(0., sourceTime - 1);
    }
    ImGui::SameLine(0, 2);
    if (iconButton(icon::next, "Next Frame [→]"))
        sourceTime = std::min(length - frame, sourceTime + frame);
    ImGui::EndDisabled();
    ImGui::SameLine(0, 8);
    ImGui::PushFont(sx::ui::GetFonts().mono, 0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(sourcePlaying ? style::play : accent), "%s",
                       timecode(sourceTime, s.project.fps).c_str());
    ImGui::PopFont();
    if (sourcePlaying && sourceSpeed > 1) {
        ImGui::SameLine(0, 4);
        ImGui::TextDisabled("×%.0f", sourceSpeed);
    }
    ImGui::SameLine(0, 10);
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
    if (ImGui::Button("In [I]") || (focused && ImGui::IsKeyPressed(ImGuiKey_I, false)))
        sourceIn = std::min(sourceTime, out - frame);
    ImGui::SameLine(0, 2);
    if (ImGui::Button("Out [O]") || (focused && ImGui::IsKeyPressed(ImGuiKey_O, false)))
        sourceOut = std::max(sourceTime + frame, sourceIn + frame);
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("Clear"))
        sourceIn = sourceOut = 0;
    ImGui::SameLine(0, 8);
    ImGui::TextDisabled("%s → %s  (%s)", timecode(sourceIn, s.project.fps).c_str(), timecode(out, s.project.fps).c_str(),
                        timecode(out - sourceIn, s.project.fps).c_str());
    if (focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            sourceTime = std::max(0., sourceTime - (io.KeyShift ? 1 : frame));
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            sourceTime = std::min(length - frame, sourceTime + (io.KeyShift ? 1 : frame));
    }
    if (hasVideo && hasAudio) {
        ImGui::Checkbox("Video", &sourceVideo);
        ImGui::SameLine();
        ImGui::Checkbox("Audio", &sourceAudio);
        ImGui::SameLine(0, 12);
    }
    ImGui::BeginDisabled(!(sourceVideo && hasVideo) && !(sourceAudio && hasAudio));
    if (ImGui::Button("Insert at Playhead [,]") || (focused && ImGui::IsKeyPressed(ImGuiKey_Comma, false)))
        sourceEdit(true);
    tooltip("Split the sequence at the playhead and shift later clips by the marked duration");
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Overwrite at Playhead [.]") || (focused && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Period, false)))
        sourceEdit(false);
    tooltip("Replace target track content over the marked duration");
    ImGui::SameLine(0, 4);
    ImGui::BeginDisabled(!s.project.hasWorkArea());
    if (ImGui::Button("Fit to Fill [Shift+.]") ||
        (focused && s.project.hasWorkArea() && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Period, false)))
        sourceEdit(false, true);
    ImGui::EndDisabled();
    tooltip("Overwrite the work area, changing speed to fit the marked duration");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 4);
    auto *replaced = s.project.findClip(s.selected);
    ImGui::BeginDisabled(!replaced || replaced->source.empty());
    if (ImGui::Button("Replace Clip")) {
        Json a = Json::Object();
        a["clip"] = s.selected;
        a["path"] = sourcePath;
        a["in"] = sourceIn;
        action("replace_source", a);
    }
    tooltip("The selected clip uses this media starting at the in point, keeping its position and duration");
    ImGui::EndDisabled();
    ImGui::End();
}
} // namespace sf

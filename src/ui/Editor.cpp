#include "ui/EnglishPanels.h"
#include "ui/Editor.h"
#include "adapters/CyDawWidgets.h"
#include "core/AtomicFile.h"
#include "core/Version.h"
#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <commdlg.h>
#include <shobjidl.h>
#include <cstdio>
#include <cydaw/graph/GraphDefinition.h>
#include <fstream>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <mfapi.h>
#include <set>

namespace sf {
static constexpr ImU32 Orange = IM_COL32(239, 139, 58, 255), Audio = IM_COL32(66, 179, 184, 255),
                       Muted = IM_COL32(120, 127, 143, 255);
std::string timecode(double time, int fps) {
    int frame = int(std::max(0., time) * fps + 0.0001), sec = frame / fps;
    char b[40];
    snprintf(b, sizeof(b), "%02d:%02d:%02d:%02d", sec / 3600, sec / 60 % 60, sec % 60, frame % fps);
    return b;
}
static Json clipArgs(Id id) {
    Json a = Json::Object();
    a["clip"] = id;
    return a;
}
Json Editor::selectionArgs() const {
    Json args = Json::Object();
    args["clips"] = Json::Array();
    for (auto id : s.selection)
        args["clips"].Push(id);
    return args;
}
std::vector<unsigned char> clipboardDib(const Image &image) {
    BITMAPINFOHEADER header{};
    header.biSize = sizeof(header);
    header.biWidth = LONG(image.width);
    header.biHeight = LONG(image.height);
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = image.width * image.height * 4;
    std::vector<unsigned char> dib(sizeof(header) + header.biSizeImage);
    memcpy(dib.data(), &header, sizeof(header));
    for (unsigned y = 0; y < image.height; ++y) {
        auto *from = image.pixels.data() + size_t(image.height - 1 - y) * image.width * 4;
        auto *to = dib.data() + sizeof(header) + size_t(y) * image.width * 4;
        for (unsigned x = 0; x < image.width; ++x, from += 4, to += 4) {
            to[0] = from[2];
            to[1] = from[1];
            to[2] = from[0];
            to[3] = from[3];
        }
    }
    return dib;
}
void Editor::exportStill() {
    auto path = dialog(true, L"PNG image\0*.png\0Half-float EXR 16-bit\0*.exr\0\0", L"png");
    if (path.empty())
        return;
    Json a = Json::Object();
    a["path"] = path;
    a["time"] = s.time;
    action("render_frame", a);
}
bool Editor::takeSnapshot() {
    auto image = s.renderer.readback();
    if (image.pixels.empty())
        return false;
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
    Com<ID3D11ShaderResourceView> view;
    if (FAILED(s.renderer.gpu()->CreateTexture2D(&d, &pixels, &texture)) ||
        FAILED(s.renderer.gpu()->CreateShaderResourceView(texture.Get(), nullptr, &view)))
        return false;
    snapshotView = view;
    snapshotLabel = "A  ·  " + timecode(s.time, s.project.fps);
    snapshotCompare = true;
    return true;
}
void Editor::guidesMenu() {
    ImGui::CheckboxFlags("Safe Areas (93% Action, 90% Title)", &guideFlags, 1);
    ImGui::CheckboxFlags("Thirds", &guideFlags, 2);
    ImGui::CheckboxFlags("Center", &guideFlags, 4);
    ImGui::SetNextItemWidth(160);
    ImGui::Combo("Target Aspect Ratio", &guideRatio, "None\0" "2.39:1 Cinema\0" "1,85:1\0" "1:1 Square\0" "4:5 portrait\0"
                                              "9:16 vertical\0");
    ImGui::Separator();
    if (ImGui::MenuItem("Take Snapshot (A)"))
        takeSnapshot();
    if (ImGui::MenuItem("Compare A / B", nullptr, snapshotCompare, bool(snapshotView)))
        snapshotCompare = !snapshotCompare;
}
// Over the picture: A/B wipe, target format mask, safe areas, thirds and centre.
void Editor::compositionOverlays(ImVec2 origin, float w, float h, bool hovered) {
    auto *draw = ImGui::GetWindowDrawList();
    ImVec2 end = origin + ImVec2(w, h);
    if (comparing()) {
        float split = origin.x + w * wipe;
        draw->AddImage((ImTextureID)(intptr_t)snapshotView.Get(), origin, {split, end.y}, {0, 0}, {wipe, 1});
        auto mouse = ImGui::GetIO().MousePos;
        bool onWipe = hovered && std::abs(mouse.x - split) < 7 && mouse.y >= origin.y && mouse.y <= end.y;
        if (onWipe && ImGui::IsMouseClicked(0))
            wipeDragging = true;
        if (wipeDragging) {
            wipe = std::clamp((mouse.x - origin.x) / std::max(1.f, w), 0.f, 1.f);
            if (!ImGui::IsMouseDown(0))
                wipeDragging = false;
        }
        if (onWipe || wipeDragging)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        draw->AddLine({split, origin.y}, {split, end.y}, IM_COL32(255, 255, 255, 220), 2);
        draw->AddCircleFilled({split, (origin.y + end.y) * .5f}, 7, IM_COL32(239, 139, 58, 255));
    }
    constexpr float ratios[] = {0, 2.39f, 1.85f, 1, .8f, .5625f};
    if (guideRatio > 0 && guideRatio < 6) {
        // Outside the target format is darkened, as a letterbox or pillarbox.
        float target = ratios[guideRatio], current = w / std::max(1.f, h);
        ImVec2 a = origin, b = end;
        if (target > current) {
            float inner = w / target;
            a.y = origin.y + (h - inner) * .5f;
            b.y = a.y + inner;
            draw->AddRectFilled(origin, {end.x, a.y}, IM_COL32(0, 0, 0, 150));
            draw->AddRectFilled({origin.x, b.y}, end, IM_COL32(0, 0, 0, 150));
        } else {
            float inner = h * target;
            a.x = origin.x + (w - inner) * .5f;
            b.x = a.x + inner;
            draw->AddRectFilled(origin, {a.x, end.y}, IM_COL32(0, 0, 0, 150));
            draw->AddRectFilled({b.x, origin.y}, end, IM_COL32(0, 0, 0, 150));
        }
        draw->AddRect(a, b, IM_COL32(239, 139, 58, 200), 0.f, ImDrawFlags_None, 1.f);
    }
    ImU32 guide = IM_COL32(255, 255, 255, 110);
    if (guideFlags & 1) {
        for (float share : {.93f, .90f}) {
            ImVec2 margin{w * (1 - share) * .5f, h * (1 - share) * .5f};
            draw->AddRect(origin + margin, end - margin, guide, 0.f, ImDrawFlags_None, 1.f);
        }
    }
    if (guideFlags & 2)
        for (float third : {1 / 3.f, 2 / 3.f}) {
            draw->AddLine({origin.x + w * third, origin.y}, {origin.x + w * third, end.y}, guide);
            draw->AddLine({origin.x, origin.y + h * third}, {end.x, origin.y + h * third}, guide);
        }
    if (guideFlags & 4) {
        ImVec2 c = (origin + end) * .5f;
        draw->AddLine(c - ImVec2(12, 0), c + ImVec2(12, 0), guide, 1.5f);
        draw->AddLine(c - ImVec2(0, 12), c + ImVec2(0, 12), guide, 1.5f);
    }
    // The A / B labels stay readable above the format mask and the guides.
    if (comparing()) {
        auto label = [&](ImVec2 at, const char *text) {
            auto extent = ImGui::CalcTextSize(text);
            draw->AddRectFilled(at - ImVec2(4, 2), at + extent + ImVec2(4, 2), IM_COL32(10, 11, 14, 210), 3);
            draw->AddText(at, IM_COL32(222, 224, 232, 255), text);
        };
        label(origin + ImVec2(8, 8), snapshotLabel.c_str());
        label({end.x - ImGui::CalcTextSize("B  ·  current").x - 8, origin.y + 8}, "B  ·  current");
    }
}
bool Editor::copyStill() {
    s.renderer.render(s.project, s.media, s.time, s.project.width, s.project.height);
    auto dib = clipboardDib(s.renderer.readback());
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, dib.size());
    if (!memory)
        return false;
    memcpy(GlobalLock(memory), dib.data(), dib.size());
    GlobalUnlock(memory);
    if (!OpenClipboard(GetActiveWindow())) {
        GlobalFree(memory);
        error = "Presse-papiers indisponible";
        return false;
    }
    EmptyClipboard();
    bool placed = SetClipboardData(CF_DIB, memory) != nullptr;
    CloseClipboard();
    if (!placed)
        GlobalFree(memory);
    return placed;
}
float Editor::previewScale() const {
    return std::min(1.f, std::min(1280.f / s.project.width, 720.f / s.project.height));
}
std::string Editor::renderSignature() const {
    return std::to_string(std::hash<std::string>{}(s.project.json().Dump(0))) + "@" +
           std::to_string(previewScale());
}
void Editor::startRenderCache() {
    bool area = s.project.hasWorkArea();
    float scale = previewScale();
    renderCache.start(s.project, s.renderer.library, renderSignature(), area ? s.project.workIn : 0,
                      area ? s.project.workOut : s.project.duration,
                      std::max(2u, unsigned(s.project.width * scale)), std::max(2u, unsigned(s.project.height * scale)),
                      std::size_t(1536) << 20);
    renderCacheChecked = ImGui::GetTime() + .5;
}
void Editor::startLoudness(double target) {
    if (loudnessJob && !loudnessJob->finished)
        return;
    loudnessJob = std::make_unique<LoudnessJob>();
    loudnessJob->target = target;
    loudnessJob->limit = loudnessLimit;
    auto *job = loudnessJob.get();
    job->worker = std::thread([job, project = s.project] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION);
        try {
            MediaCache offline;
            std::vector<float> block;
            uint64_t total = uint64_t(project.duration * 48000);
            for (uint64_t at = 0; at < total && !job->cancelled; at += 4096) {
                offline.mix(project, at / 48000., unsigned(std::min<uint64_t>(4096, total - at)), block);
                job->progress = float(double(at) / std::max<uint64_t>(1, total));
            }
            job->measured = offline.loudness.integrated();
            job->peak = offline.loudness.truePeak(true);
        } catch (const std::exception &failure) {
            job->error = failure.what();
        }
        MFShutdown();
        CoUninitialize();
        job->finished = true;
    });
}
void Editor::pollLoudness() {
    if (!loudnessJob || !loudnessJob->finished)
        return;
    auto job = std::move(loudnessJob);
    job->worker.join();
    if (job->cancelled)
        return;
    if (!job->error.empty()) {
        error = job->error;
        return;
    }
    Json a = Json::Object();
    a["target"] = job->target;
    a["measured"] = job->measured;
    a["true_peak"] = job->peak;
    a["limit"] = job->limit;
    if (action("normalize_loudness", a)) {
        char text[200];
        double peak = actionResult["true_peak"].Num();
        snprintf(text, sizeof(text), "Measured mix %.1f LUFS; master %+.1f dB - true peak %.1f dBTP%s",
                 actionResult["measured"].Num(), actionResult["gain"].Num(), peak,
                 actionResult.Has("limiter") ? "  ·  limiter -1 dBTP"
                 : peak > -1                 ? " : clipping warning"
                                             : "");
        say(text);
    }
}
void Editor::startAnalysis(std::string label, std::string command, std::function<Json(AnalysisJob &)> work,
                           std::function<std::string(const Json &)> report) {
    if (analysis)
        return;
    analysis = std::make_unique<AnalysisJob>();
    analysis->label = std::move(label);
    analysis->command = std::move(command);
    analysis->report = std::move(report);
    auto *job = analysis.get();
    job->worker = std::thread([job, work = std::move(work)] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION);
        try {
            job->args = work(*job);
        } catch (const std::exception &failure) {
            job->error = failure.what();
        }
        MFShutdown();
        CoUninitialize();
        job->finished = true;
    });
}
void Editor::pollAnalysis() {
    if (!analysis || !analysis->finished)
        return;
    auto job = std::move(analysis);
    job->worker.join();
    if (job->cancelled)
        return;
    if (!job->error.empty()) {
        error = job->error;
        return;
    }
    // A job without a command (an export) only reports, from its own result.
    if (job->command.empty()) {
        if (job->report)
            say(job->report(job->args));
    } else if (action(job->command, job->args) && job->report)
        say(job->report(actionResult));
}
// Rendered on a worker thread with its own renderer; the work area, else the sequence.
void Editor::exportGifInBackground(const std::string &path) {
    auto project = s.project;
    auto library = s.renderer.library;
    ExportRange range;
    if (project.hasWorkArea())
        range = {project.workIn, project.workOut};
    startAnalysis("GIF", "",
                  [project, library, range, path](AnalysisJob &job) {
                      Renderer renderer;
                      renderer.initialize();
                      renderer.library = library;
                      MediaCache media;
                      sf::exportGif(project, media, renderer, path, range, 480, 15, [&](float done) {
                          job.progress = done;
                          return !job.cancelled.load();
                      });
                      Json args = Json::Object();
                      args["path"] = path;
                      return args;
                  },
                  [](const Json &result) { return "GIF exported: " + result["path"].S(); });
}
void Editor::removeSilencesInBackground(Id id, double threshold, double minLength) {
    // The sound of the clip: itself, or the audio member of its linked group.
    auto &p = s.project;
    const Clip *sound = nullptr;
    for (auto member : p.linked(id))
        if (p.owner(member)->kind == TrackKind::Audio && !p.findClip(member)->source.empty())
            sound = p.findClip(member);
    if (!sound) {
        error = "Choose an audio clip or a video clip with linked audio";
        return;
    }
    Clip copy = *sound;
    int fps = p.fps;
    startAnalysis("Silences", "remove_silences",
                  [copy, fps, threshold, minLength](AnalysisJob &) {
                      MediaCache offline;
                      auto spans = silenceSpans(copy, offline.get(copy.source), float(threshold), minLength, .1, fps);
                      Json args = Json::Object();
                      args["clip"] = copy.id;
                      args["spans"] = Json::Array();
                      for (auto [from, to] : spans) {
                          Json span = Json::Array();
                          span.Push(from);
                          span.Push(to);
                          args["spans"].Push(span);
                      }
                      return args;
                  },
                  [](const Json &result) {
                      return std::to_string(result["cuts"].I()) + " silent range(s) removed, " +
                             std::to_string(int(std::round(result["seconds"].Num()))) + " s";
                  });
}
void Editor::detectScenesInBackground(Id id, bool split) {
    auto *c = s.project.findClip(id);
    if (!c || c->source.empty())
        return;
    Clip copy = *c;
    int fps = s.project.fps;
    startAnalysis("Shots", "detect_scenes",
                  [copy, fps, split](AnalysisJob &job) {
                      MediaAsset reader;
                      reader.open(copy.source, {}, false);
                      auto cuts = sf::detectScenes(reader, copy.in, copy.in + copy.duration * copy.speed,
                                               std::max(1. / fps, copy.speed / fps), .15f, [&](float done) {
                                                   job.progress = done;
                                                   return !job.cancelled.load();
                                               });
                      Json args = Json::Object();
                      args["clip"] = copy.id;
                      args["split"] = split;
                      args["times"] = Json::Array();
                      for (double cut : cuts)
                          args["times"].Push(copy.start + (cut - copy.in) / copy.speed);
                      return args;
                  },
                  [](const Json &result) {
                      return std::to_string(result["times"].Size()) + " scene change(s)";
                  });
}
void Editor::rememberProject(const std::string &path) {
    if (path.empty())
        return;
    std::erase(recentProjects, path);
    recentProjects.insert(recentProjects.begin(), path);
    if (recentProjects.size() > 10)
        recentProjects.resize(10);
    // Written like the panel settings: not by previews or tests without an ini file.
    if (!panelSuffix.empty() || !ImGui::GetIO().IniFilename)
        return;
    Json list = Json::Array();
    for (auto &item : recentProjects)
        list.Push(item);
    try {
        AtomicFile output(sx::Narrow((root / "recent-projects.json").wstring()));
        {
            std::ofstream file(output.path(), std::ios::binary);
            file << list.Dump(2);
        }
        output.commit();
    } catch (const std::exception &) {
        // A recent list that cannot be written is only a convenience lost.
    }
}
void Editor::loadRecentProjects() {
    if (recentRead)
        return;
    recentRead = true;
    std::string text;
    Json list;
    if (sx::ReadTextFile(sx::Narrow((root / "recent-projects.json").wstring()), text) && Json::Parse(text, list) &&
        list.IsArray())
        for (auto &item : list.Elements())
            if (!item.S().empty())
                recentProjects.push_back(item.S());
}
void Editor::welcomeCard(ImVec2 origin, ImVec2 size) {
    welcomeShown = false;
    if (!panelSuffix.empty())
        return;
    for (auto &track : s.project.tracks)
        if (!track.clips.empty())
            return;
    loadRecentProjects();
    welcomeShown = true;
    // As tall as its content: the buttons, then up to five recent projects.
    int recent = 0;
    for (auto &path : recentProjects) {
        std::error_code missing;
        recent += recent < 5 && std::filesystem::exists(sx::Utf8Path(path), missing);
    }
    float rows = 6.3f + (recent ? 1.2f + recent : 0.f);
    ImVec2 card{std::min(size.x - 24, 460.f), std::min(size.y - 24, rows * ImGui::GetFrameHeightWithSpacing())};
    if (card.x < 220 || card.y < 160)
        return;
    ImGui::SetCursorScreenPos(origin + (size - card) * .5f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(style::alpha(style::bg1, .94f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6);
    if (ImGui::BeginChild("welcome", card, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
        ImGui::PushFont(style::boldFont(), 0);
        ImGui::TextUnformatted("Welcome to ShaderixFrame");
        ImGui::PopFont();
        ImGui::TextDisabled("The sequence is empty: start with a template, open a project or import media.");
        ImGui::Spacing();
        float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        int n = 0;
        for (auto [label, id] : {std::pair{"YouTube Video", "youtube"}, {"Vertical Video", "short"},
                                 {"Video Podcast", "podcast"}, {"Short Film", "cinema"}}) {
            if (n++ % 2)
                ImGui::SameLine();
            if (ImGui::Button(label, {half, 0})) {
                Json a = Json::Object();
                a["template"] = id;
                action("new_project", a);
            }
        }
        ImGui::Spacing();
        if (ImGui::Button("Open Project...", {half, 0})) {
            auto path = dialog(false, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
            if (!path.empty()) {
                Json a = Json::Object();
                a["path"] = path;
                action("open_project", a);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Import Media...", {half, 0})) {
            auto path = dialog(false,
                               L"Media\0*.mp4;*.mov;*.mkv;*.avi;*.wmv;*.wav;*.mp3;*.flac;*.aac;*.png;*.jpg;*.jpeg;*.bmp\0"
                               L"All\0*.*\0\0",
                               nullptr);
            if (!path.empty())
                drop(path);
        }
        if (!recentProjects.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Recent");
            int shown = 0;
            for (auto &path : recentProjects) {
                std::error_code missing;
                if (shown >= 5 || !std::filesystem::exists(sx::Utf8Path(path), missing))
                    continue;
                ++shown;
                auto label = sx::Narrow(sx::Utf8Path(path).filename().wstring()) + "##recent" + path;
                if (ImGui::Selectable(label.c_str())) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("open_project", a);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", path.c_str());
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Or drop files onto the window.");
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
void Editor::playAround() {
    playAroundReturn = s.time;
    playAroundEnd = std::min(s.project.duration, s.time + 2);
    s.time = std::max(0., s.time - 2);
    shuttleRate = 1;
    s.playing = true;
}
// A missing medium found again: the folder of the file chosen is searched for every missing name.
void Editor::relinkDialog() {
    auto folder = folderDialog(L"Folder to Search for Missing Media");
    if (folder.empty())
        return;
    Json a = Json::Object();
    a["folder"] = folder;
    if (action("relink_media", a)) {
        offlineChecked = 0;
        if (actionResult["missing"].Size())
            error = std::to_string(actionResult["missing"].Size()) + " media file(s) still missing in this folder";
        else
            say(std::to_string(actionResult["relinked"].I()) + " media file(s) relinked");
    }
}
// J reverses, Shift+L goes forward; pressed again in the same direction, the rate doubles up to ×8.
void Editor::shuttleStep(int direction) {
    if (s.playing && (shuttleRate > 0) == (direction > 0))
        shuttleRate = std::clamp(shuttleRate * 2, -8., 8.);
    else
        shuttleRate = direction > 0 ? 1 : -1;
    s.playing = true;
}
void applyFrameTheme() {
    sx::ui::ApplyTheme(1);
    // Shaderixel's fonts, with CyDAW's Segoe MDL2 icons and semibold font for the timeline.
    auto &io = ImGui::GetIO();
    auto font = [&](const char *path, float size, bool merge, bool icons = false) -> ImFont * {
        if (!std::filesystem::exists(path))
            return nullptr;
        ImFontConfig config;
        config.MergeMode = merge;
        config.OversampleH = 2;
        if (icons) {
            config.PixelSnapH = true;
            config.GlyphOffset = {0, 2};
            config.GlyphMinAdvanceX = size;
        }
        return io.Fonts->AddFontFromFileTTF(path, icons ? size * .95f : size, &config);
    };
    auto &fonts = sx::ui::GetFonts();
    fonts.ui = font("C:/Windows/Fonts/segoeui.ttf", 16, false);
    if (fonts.ui) {
        font("C:/Windows/Fonts/segmdl2.ttf", 16, true, true);
        font("C:/Windows/Fonts/seguisym.ttf", 16, true);
    } else
        fonts.ui = io.Fonts->AddFontDefault();
    if (auto *bold = font("C:/Windows/Fonts/seguisb.ttf", 16, false)) {
        font("C:/Windows/Fonts/segmdl2.ttf", 16, true, true);
        style::setBoldFont(bold);
    }
    fonts.mono = font("C:/Windows/Fonts/consola.ttf", 15, false);
    if (!fonts.mono)
        fonts.mono = fonts.ui;
    io.FontDefault = fonts.ui;
    auto &style = ImGui::GetStyle();
    for (auto &c : style.Colors)
        if (c.x > 0.25f && c.z > c.x * 1.2f && c.z > c.y * 1.2f) {
            float a = c.w;
            c = ImVec4(0.94f, 0.55f, 0.23f, a);
        }
    style.WindowRounding = 3;
    style.FrameRounding = 3;
    style.WindowPadding = {10, 8};
}
bool Editor::action(const std::string &name, Json args) {
    try {
        if (name == "save_project" && !applyClipDrafts())
            return false;
        if ((name == "new_project" || name == "open_project" || name == "undo" || name == "redo") &&
            hasClipDrafts() && s.recovery.enabled())
            s.recovery.preserve(projectWithClipDrafts(), s.projectPath, s.time);
        actionResult = protocol.execute(name, args);
        if (name == "open_project" || name == "save_project")
            rememberProject(s.projectPath);
        if (name == "start_export")
            exportsFocus = true;
        if (name == "new_project" || name == "open_project" || name == "undo" || name == "redo") {
            pendingImports.clear();
            thumbnails.clear();
            clipPreviews.clear();
            clipWorkspace.reset();
            clipSession.reset();
            editedContainer = 0;
            timelineDragging = graphDragging = 0;
            timelineDragBefore.reset();
            graphDragBefore.reset();
            marquee = graphPanning = false;
            graphViewIdentity.clear();
            draftIdentity.clear();
        }
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
void Editor::drop(const std::string &path) {
    try {
        auto absolute = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(path)).wstring());
        if (s.media.get(absolute).loading) {
            pendingImports.push_back({absolute, s.time});
            return;
        }
    } catch (const std::exception &failure) {
        error = failure.what();
        return;
    }
    Json a = Json::Object();
    a["path"] = path;
    a["proxy"] = autoProxies;
    if (action("import_media", a) && actionResult.Has("proxy_job"))
        say("Large video: preparing a 720p preview proxy");
}
std::string Editor::dialog(bool save, const wchar_t *f, const wchar_t *ext) {
    wchar_t buffer[32768]{};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = GetActiveWindow();
    o.lpstrFilter = f;
    o.lpstrFile = buffer;
    o.nMaxFile = 32768;
    o.lpstrDefExt = ext;
    o.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    bool ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    return ok ? sx::Narrow(buffer) : "";
}
std::string Editor::folderDialog(const wchar_t *title) {
    Com<IFileOpenDialog> picker;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker))))
        return {};
    DWORD options = 0;
    picker->GetOptions(&options);
    picker->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    picker->SetTitle(title);
    if (FAILED(picker->Show(GetActiveWindow())))
        return {};
    Com<IShellItem> item;
    PWSTR path = nullptr;
    if (FAILED(picker->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return {};
    auto folder = sx::Narrow(path);
    CoTaskMemFree(path);
    return folder;
}
void Editor::editCheckpoint(const Project &before) {
    if (ImGui::IsItemActivated())
        s.history.checkpoint(before);
    if (ImGui::IsItemEdited()) {
        s.dirty = true;
        s.renderer.clearHistory();
    }
}
bool Editor::beginPanel(const char *name) {
    auto &open = panels[name];
    if (!ImGui::Begin(panelTitle(name).c_str(), &open)) {
        ImGui::End();
        return false;
    }
    return true;
}
void Editor::panelSettings() {
    if (!panelSuffix.empty() || !ImGui::GetIO().IniFilename)
        return;
    auto path = panelSettingsFile.empty() ? root / "workspace-panels.json" : panelSettingsFile;
    if (!panelSettingsRead) {
        panelSettingsRead = true;
        std::string text;
        Json j;
        if (sx::ReadTextFile(sx::Narrow(path.wstring()), text) && Json::Parse(text, j) && j.IsObject() &&
            j["modes"].IsArray()) {
            j = englishPanelSettings(j);
            for (int m = 0; m < modeCount && m < int(j["modes"].Size()); ++m)
                for (auto &[name, open] : modePanels[m])
                    if (j["modes"].At(m).Has(name))
                        open = j["modes"].At(m)[name].B(open);
            mode = Mode(std::clamp(j["mode"].I(), 0, modeCount - 1));
            panels = modePanels[int(mode)];
        } else
            layoutForce = true; // the dock layout saved before workspaces is replaced once
    }
    modePanels[int(mode)] = panels;
    Json j = Json::Object();
    j["mode"] = int(mode);
    j["modes"] = Json::Array();
    for (auto &set : modePanels) {
        Json item = Json::Object();
        for (auto &[name, open] : set)
            item[name] = open;
        j["modes"].Push(item);
    }
    auto signature = j.Dump(0);
    if (signature != panelSettingsSignature) {
        try {
            AtomicFile output(sx::Narrow(path.wstring()));
            {
                std::ofstream f(output.path(), std::ios::binary);
                f << signature;
                if (!f)
                    throw std::runtime_error("Write panel settings");
            }
            output.commit();
            panelSettingsSignature = signature;
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
}
void Editor::menu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Sequence", "Ctrl+N"))
                action("new_project");
            if (ImGui::BeginMenu("New from Template")) {
                for (auto [label, id] : {std::pair{"YouTube Video - 1920 x 1080, 30 fps", "youtube"},
                                         {"Vertical Video - 1080 x 1920 (Shorts, Reels, TikTok)", "short"},
                                         {"Video Podcast - Two Cameras, Two Voices", "podcast"},
                                         {"Short Film - 1920 x 1080, 24 fps", "cinema"}})
                    if (ImGui::MenuItem(label)) {
                        Json a = Json::Object();
                        a["template"] = id;
                        action("new_project", a);
                    }
                ImGui::EndMenu();
            }
            loadRecentProjects();
            if (ImGui::BeginMenu("Recent Projects", !recentProjects.empty())) {
                for (auto &path : recentProjects) {
                    std::error_code missing;
                    bool present = std::filesystem::exists(sx::Utf8Path(path), missing);
                    auto label = sx::Narrow(sx::Utf8Path(path).filename().wstring()) + "##" + path;
                    if (ImGui::MenuItem(label.c_str(), nullptr, false, present)) {
                        Json a = Json::Object();
                        a["path"] = path;
                        action("open_project", a);
                    }
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s%s", path.c_str(), present ? "" : "\n(not found)");
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                auto path = dialog(false, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("open_project", a);
                }
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                auto path = s.projectPath.empty() ? dialog(true, L"ShaderixFrame\0*.sframe\0\0", L"sframe")
                                                  : s.projectPath;
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("save_project", a);
                }
            }
            if (ImGui::MenuItem("Save As...")) {
                auto path = dialog(true, L"ShaderixFrame\0*.sframe\0\0", L"sframe");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("save_project", a);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Recover Session...", nullptr, false, s.recovery.enabled())) {
                recoveries = s.recovery.list();
                recoveryVisible = true;
            }
            if (ImGui::MenuItem("Import Media...", "Ctrl+I")) {
                auto path = dialog(false,
                                   L"Media\0*.mp4;*.mov;*.mkv;*.avi;*.wmv;*.wav;*.mp3;*.flac;*.aac;*.png;*."
                                   L"jpg;*.jpeg;*.bmp\0All\0*.*\0\0",
                                   nullptr);
                if (!path.empty())
                    drop(path);
            }
            auto jobs = s.exports.list();
            bool activeExport =
                std::count_if(jobs.begin(), jobs.end(), [](const auto &job) { return job.active(); }) >= 32;
            if (ImGui::MenuItem("Export MP4 · H264 / AAC", nullptr, false, !activeExport)) {
                auto path = dialog(true, L"MP4 Video\0*.mp4\0\0", L"mp4");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    a["kind"] = "video";
                    action("start_export", a);
                }
            }
            if (ImGui::MenuItem("Export current frame…"))
                exportStill();
            ImGui::Separator();
            if (ImGui::MenuItem("Import SRT Subtitles...")) {
                auto path = dialog(false, L"SubRip subtitles\0*.srt\0\0", L"srt");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("import_subtitles", a);
                }
            }
            if (ImGui::MenuItem("Export SRT subtitles…")) {
                auto path = dialog(true, L"SubRip subtitles\0*.srt\0\0", L"srt");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("export_subtitles", a);
                }
            }
            if (ImGui::BeginMenu("Export EDL (CMX 3600)")) {
                // One video track per list.
                for (auto &track : s.project.tracks)
                    if (track.kind == TrackKind::Video && !track.clips.empty() &&
                        ImGui::MenuItem((track.name + "##edl" + std::to_string(track.id)).c_str())) {
                        auto path = dialog(true, L"EDL Edit Decision List\0*.edl\0\0", L"edl");
                        if (!path.empty()) {
                            Json a = Json::Object();
                            a["path"] = path;
                            a["track"] = track.id;
                            if (action("export_edl", a))
                                say(std::to_string(actionResult["events"].I()) + " shot(s) in the EDL");
                        }
                    }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Import EDL...")) {
                auto path = dialog(false, L"EDL Edit Decision List\0*.edl\0\0", L"edl");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    if (action("import_edl", a))
                        say(std::to_string(actionResult["clips"].I()) + " shot(s) imported" +
                            (actionResult["offline"].I() ? ", " + std::to_string(actionResult["offline"].I()) +
                                                               " offline: File > Relink Media"
                                                         : std::string()));
                }
            }
            if (ImGui::MenuItem("Export Shot List (CSV)...")) {
                auto path = dialog(true, L"CSV Shot List\0*.csv\0\0", L"csv");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    action("export_shot_list", a);
                }
            }
            if (ImGui::BeginMenu("Previous Versions", !s.projectPath.empty())) {
                auto versions = projectVersions(s.projectPath);
                if (versions.empty())
                    ImGui::TextDisabled("None: a version is kept each time you save");
                for (auto &[saved, path] : versions) {
                    // AAAA-MM-JJ HHMMSS shown as AAAA-MM-JJ  HH:MM:SS.
                    auto label = saved.size() == 17 ? saved.substr(0, 10) + "  " + saved.substr(11, 2) + ":" +
                                                          saved.substr(13, 2) + ":" + saved.substr(15, 2)
                                                    : saved;
                    if (ImGui::MenuItem(label.c_str())) {
                        Json a = Json::Object();
                        a["path"] = sx::Narrow(path.wstring());
                        if (action("restore_version", a))
                            say("Version from " + label + " restored (Ctrl+Z to undo)");
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Relink Missing Media...", nullptr, false, offlineMedia > 0))
                relinkDialog();
            if (ImGui::MenuItem("Collect Media...")) {
                auto folder = folderDialog(L"Folder for a Copy of Each Media File");
                if (!folder.empty()) {
                    Json a = Json::Object();
                    a["folder"] = folder;
                    if (action("collect_media", a))
                        say(std::to_string(actionResult["copied"].I()) + " media file(s) copied, " +
                            std::to_string(int(actionResult["bytes"].Num() / 1048576)) + " Mo");
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Selection as MP4...", nullptr, false, !activeExport && !s.selection.empty())) {
                auto path = dialog(true, L"MP4 Video\0*.mp4\0\0", L"mp4");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    a["kind"] = "video";
                    a["range"] = "selection";
                    action("start_export", a);
                }
            }
            if (ImGui::MenuItem("Export each chapter to MP4…", nullptr, false, !activeExport && !s.project.markers.empty())) {
                auto folder = folderDialog(L"Chapter Output Folder");
                if (!folder.empty()) {
                    Json a = Json::Object();
                    a["folder"] = folder;
                    if (action("export_chapters", a))
                        say(std::to_string(actionResult["jobs"].Size()) + " chapter(s) exporting");
                }
            }
            bool regions = std::any_of(s.project.markers.begin(), s.project.markers.end(),
                                       [](const Marker &m) { return m.duration > 0; });
            if (ImGui::MenuItem("Export Each Region as MP4...", nullptr, false, !activeExport && regions)) {
                auto folder = folderDialog(L"Region Output Folder");
                if (!folder.empty()) {
                    Json a = Json::Object();
                    a["folder"] = folder;
                    a["regions"] = true;
                    if (action("export_chapters", a))
                        say(std::to_string(actionResult["jobs"].Size()) + " region(s) exporting");
                }
            }
            if (ImGui::MenuItem("Export Animated GIF", nullptr, false, !analysis)) {
                auto path = dialog(true, L"Animated GIF\0*.gif\0\0", L"gif");
                if (!path.empty())
                    exportGifInBackground(path);
            }
            if (ImGui::MenuItem("Export WAV Mix", nullptr, false, !activeExport)) {
                auto path = dialog(true, L"Audio WAV\0*.wav\0\0", L"wav");
                if (!path.empty()) {
                    Json a = Json::Object();
                    a["path"] = path;
                    a["kind"] = "audio";
                    action("start_export", a);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                action("undo");
            if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                action("redo");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Sequence")) {
            if (ImGui::MenuItem("Color Grading")) {
                showGrading();
                ImGui::SetWindowFocus(panelTitle("Color Grading").c_str());
            }
            if (ImGui::MenuItem("Video Scopes"))
                ImGui::SetWindowFocus(panelTitle("Video Scopes").c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(s.project.hasWorkArea() ? "Pre-render Work Area" : "Pre-render Sequence",
                                "Input"))
                startRenderCache();
            if (ImGui::MenuItem("Clear Pre-render Cache", nullptr, false, !renderCache.empty()))
                renderCache.clear();
            if (ImGui::MenuItem("Close All Gaps"))
                action("close_gaps");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Remove ranges where all tracks are empty; shorten the sequence");
            if (ImGui::BeginMenu("Normalize Loudness")) {
                for (auto [label, target] : {std::pair{"-14 LUFS  (YouTube, Spotify)", -14.}, {"-16 LUFS  (podcast, Apple)", -16.},
                                             {"-23 LUFS (EBU R128, Broadcast)", -23.}, {"-24 LUFS  (ATSC A/85)", -24.}})
                    if (ImGui::MenuItem(label, nullptr, false, !loudnessJob))
                        startLoudness(target);
                ImGui::Separator();
                ImGui::MenuItem("-1 dBTP Master Limiter", nullptr, &loudnessLimit);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Format")) {
                // Common delivery sizes; clips keep their place, pictures are fitted to the new frame.
                for (auto [label, width, height] :
                     {std::tuple{"1920 × 1080  ·  HD 16:9", 1920, 1080}, {"3840 × 2160  ·  4K UHD", 3840, 2160},
                      {"1280 × 720  ·  HD 720p", 1280, 720}, {"1080 × 1920  ·  vertical 9:16", 1080, 1920},
                      {"1080 x 1080 - Square", 1080, 1080}, {"1080 × 1350  ·  portrait 4:5", 1080, 1350}})
                    if (ImGui::MenuItem(label, nullptr, s.project.width == width && s.project.height == height)) {
                        Json a = Json::Object();
                        a["width"] = width;
                        a["height"] = height;
                        action("set_sequence", a);
                    }
                ImGui::Separator();
                for (int fps : {24, 25, 30, 50, 60})
                    if (ImGui::MenuItem((std::to_string(fps) + " ips").c_str(), nullptr, s.project.fps == fps)) {
                        Json a = Json::Object();
                        a["fps"] = fps;
                        action("set_sequence", a);
                    }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::TextDisabled("RGBA composition · 16-bit float");
            const char *spaces[] = {"srgb", "linear709", "linear2020"};
            int space = s.project.workingSpace == "srgb" ? 0 : s.project.workingSpace == "linear709" ? 1 : 2;
            if (ImGui::Combo("Working Space", &space,
                             "Legacy sRGB\0Linear Rec.709\0Linear Rec.2020\0")) {
                Json a = Json::Object();
                a["space"] = spaces[space];
                action("set_color_management", a);
            }
            int tone = s.project.toneMap == "clip" ? 0 : s.project.toneMap == "reinhard" ? 1 : 2;
            const char *tones[] = {"clip", "reinhard", "aces"};
            if (ImGui::Combo("SDR display", &tone, "Clip\0Reinhard\0Fitted ACES Curve\0")) {
                Json a = Json::Object();
                a["tone"] = tones[tone];
                action("set_color_management", a);
            }
            auto before = s.project;
            if (ImGui::SliderFloat("Display exposure", &s.project.exposure, -10, 10, "%.1f EV"))
                s.dirty = true;
            editCheckpoint(before);
            bool transparent = s.project.backgroundAlpha == 0;
            if (ImGui::Checkbox("Transparent Background", &transparent)) {
                Json a = Json::Object();
                a["backgroundAlpha"] = transparent ? 0 : 1;
                action("set_color_management", a);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Video Track")) {
                Json a = Json::Object();
                a["kind"] = "video";
                action("add_track", a);
            }
            if (ImGui::MenuItem("Audio Track")) {
                Json a = Json::Object();
                a["kind"] = "audio";
                action("add_track", a);
            }
            if (ImGui::MenuItem("HLSL Generator"))
                action("add_generator");
            if (ImGui::MenuItem("Title / Text"))
                action("add_title");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Command Palette...", keymap.label("view.palette").c_str()))
                openPalette();
            if (panelSuffix.empty() && ImGui::BeginMenu("Mode")) {
                const char *names[] = {"Edit", "Effects", "Motion", "Color Grading", "Audio", "Deliver"};
                for (int m = 0; m < modeCount; ++m) {
                    auto key = "F" + std::to_string(m + 1);
                    if (ImGui::MenuItem(names[m], key.c_str(), int(mode) == m))
                        setMode(Mode(m));
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            for (auto &[name, open] : panels)
                if (ImGui::MenuItem(name.c_str(), nullptr, open)) {
                    openPanel(name);
                }
            if (clipWorkspace && ImGui::BeginMenu("Clip Windows")) {
                for (auto &[name, open] : clipWorkspace->panels)
                    if (ImGui::MenuItem(name.c_str(), nullptr, open)) {
                        clipWorkspace->openPanel(name);
                    }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Drag a tab outside the window to use another screen");
            if (ImGui::MenuItem("Reset This Workspace")) {
                if (panelSuffix.empty()) {
                    resetModeLayout();
                    auto current = mode;
                    auto saved = modePanels;
                    modesReady = false;
                    setMode(current);
                    for (int m = 0; m < modeCount; ++m)
                        if (m != int(current))
                            modePanels[m] = saved[m];
                } else
                    for (auto &[name, open] : panels)
                        open = true;
                layoutReady = false;
                if (auto *workspace = ImGui::FindWindowByName("Workspace"))
                    ImGui::DockBuilderRemoveNode(workspace->GetID("FrameDock"));
                pendingPanelFocus = panelTitle("Composition");
            }
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 310);
        ImGui::TextColored(ImVec4(0.94f, 0.55f, 0.23f, 1), "SHADERIXFRAME");
        ImGui::SameLine();
        ImGui::TextDisabled("/  %s  ·  NATIVE", applicationVersion);
        ImGui::EndMainMenuBar();
    }
}
void Editor::parameter(const char *label, const std::string &key, float lo, float hi, float fallback,
                       int fx) {
    auto *c = s.project.findClip(s.selected);
    if (!c)
        return;
    auto &params = fx < 0 ? c->params : c->effects[fx].params;
    parameterRanges[key] = {lo, hi};
    auto before = s.project;
    auto it = params.find(key);
    float value = it == params.end() ? fallback : it->second.value;
    bool animated = it != params.end() && !it->second.keys.empty();
    ImGui::PushID(key.c_str());
    ImGui::PushID(fx);
    ImGui::TextDisabled("%s%s", label, animated ? "  / ANIMATED" : "");
    ImGui::SetNextItemWidth(std::max(50.f, ImGui::GetContentRegionAvail().x - 42));
    if (ImGui::SliderFloat("##value", &value, lo, hi, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        params.try_emplace(key, Parameter{fallback}).first->second.value = value;
    editCheckpoint(before);
    ImGui::SameLine();
    if (ImGui::Button("K", {30, 0})) {
        s.history.checkpoint(before);
        params.try_emplace(key, Parameter{fallback})
            .first->second.setKey({snapTime(s.time, s.project.fps, true), value, Curve::Smooth, 0});
        s.dirty = true;
        animationKey = key;
        animationEffect = fx;
        automationReveal = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Add a keyframe at the current time; right-click: show curve in timeline");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        animationKey = key;
        animationEffect = fx;
        automationReveal = true;
    }
    ImGui::PopID();
    ImGui::PopID();
}
void Editor::library() {
    if (!panels["Media & Effects"] || !beginPanel("Media & Effects"))
        return;
    if (ImGui::Button("+ Import", {ImGui::GetContentRegionAvail().x, 0})) {
        auto path =
            dialog(false, L"Media\0*.mp4;*.mov;*.avi;*.wav;*.mp3;*.png;*.jpg\0All\0*.*\0\0", nullptr);
        if (!path.empty())
            drop(path);
    }
    ImGui::TextDisabled("Drop a file onto the window");
    ImGui::Checkbox("Preview Proxies", &s.media.proxiesEnabled);
    if (ImGui::Button("Prepare Proxy for Selected Clip") && s.selected)
        action("prepare_proxy", clipArgs(s.selected));
    ImGui::Spacing();
    if (ImGui::BeginTabBar("library")) {
        if (ImGui::BeginTabItem("Media")) {
            mediaBin();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Effects")) {
            ImGui::InputTextWithHint("##search", "Find an effect...", &filter);
            for (auto &d : s.renderer.library.definitions) {
                if (!filter.empty() && d.name.find(filter) == std::string::npos)
                    continue;
                ImGui::PushID(d.id.c_str());
                if (ImGui::Selectable(d.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
                    ImGui::IsMouseDoubleClicked(0) && s.selected) {
                    auto a = clipArgs(s.selected);
                    a["id"] = d.id;
                    auto *track = s.project.owner(s.selected);
                    if (d.id.starts_with("cydaw.") && track && track->kind == TrackKind::Audio) {
                        a = Json::Object();
                        a["scope"] = "track";
                        a["target"] = track->id;
                        a["id"] = d.id;
                        action("add_mix_effect", a);
                    } else
                        action(d.kind == "transition" ? "set_transition" : "add_effect", a);
                }
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(d.kind == "transition" ? "TRANSITION" : "EFFECT", d.id.c_str(),
                                              d.id.size() + 1);
                    ImGui::TextUnformatted(d.name.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::TextDisabled("%s  /  %s", d.category.c_str(), d.origin.c_str());
                ImGui::PopID();
            }
            ImGui::TextWrapped("Double-click: add to selected clip.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sources")) {
            if (ImGui::Button("+ Title / Text", {-1, 0}))
                action("add_title");
            if (ImGui::Button("+ Procedural Light", {-1, 0}))
                action("add_generator");
            ImGui::TextWrapped(
                "Animated HLSL generator. Add effects and animate any parameter.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
void Editor::preview() {
    if (panelSuffix.empty() && !ImGui::FindWindowByName("Composition"))
        ImGui::SetNextWindowFocus();
    if (!panels["Composition"] || !beginPanel("Composition"))
        return;
    auto &viewIo = ImGui::GetIO();
    float ratio = float(s.project.width) / s.project.height;
    // Zoom bar: percentages are screen pixels per image pixel, "Ajuster" fits the panel.
    ImVec2 available = ImGui::GetContentRegionAvail();
    // The viewport child is the panel minus the zoom bar and the transport row.
    float viewportHeight = std::max(80.f, available.y - ImGui::GetFrameHeightWithSpacing() - 60);
    float fitW = std::max(64.f, available.x), fitH = viewportHeight;
    if (fitW / fitH > ratio)
        fitW = fitH * ratio;
    else
        fitH = fitW / ratio;
    auto pixelZoom = [&] { return fitW * compositionView.zoom / float(s.project.width); };
    // A chosen percentage stays exact when the panel is resized; the wheel and "Ajuster" release it.
    auto setPixelZoom = [&](float percent) {
        float previous = compositionView.zoom;
        compositionPixelLock = std::clamp(percent, .01f, 32.f);
        compositionView.zoom = std::clamp(percent * float(s.project.width) / fitW, .125f, 64.f);
        compositionView.pan = compositionView.pan * (compositionView.zoom / previous);
    };
    if (compositionPixelLock > 0) {
        float locked = std::clamp(compositionPixelLock * float(s.project.width) / fitW, .125f, 64.f);
        compositionView.pan = compositionView.pan * (locked / compositionView.zoom);
        compositionView.zoom = locked;
    }
    float barWidth = 3 * (ImGui::GetFrameHeight() + 2 + 2) + 130 + 46;
    char about[96];
    snprintf(about, sizeof(about), "%d × %d  /  %d fps%s", s.project.width, s.project.height, s.project.fps,
             s.dirty ? "  •" : "");
    if (ImGui::CalcTextSize(s.project.name.c_str()).x + ImGui::CalcTextSize(about).x + barWidth + 30 <
        ImGui::GetContentRegionAvail().x) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(s.project.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", about);
        ImGui::SameLine();
    }
    {
        char current[32];
        bool fitted = std::abs(compositionView.zoom - 1) < 1e-4f && compositionView.pan.x == 0 &&
                      compositionView.pan.y == 0;
        snprintf(current, sizeof(current), fitted ? "Fit (%.0f %%)" : "%.0f %%", pixelZoom() * 100);
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - barWidth));
        if (style::iconButton(icon::zoomOut, "Zoom Out [Ctrl + -]"))
            setPixelZoom(pixelZoom() / 1.5f);
        ImGui::SameLine(0, 2);
        ImGui::SetNextItemWidth(130);
        if (ImGui::BeginCombo("##viewZoom", current)) {
            if (ImGui::Selectable("Fit to Window", fitted)) {
                compositionView = {};
                compositionPixelLock = 0;
            }
            for (float percent : {.25f, .5f, 1.f, 2.f, 4.f, 8.f, 16.f}) {
                char label[24];
                snprintf(label, sizeof(label), "%.0f %%%s", percent * 100, percent == 1 ? "  (1:1)" : "");
                if (ImGui::Selectable(label, std::abs(pixelZoom() - percent) < 1e-3f))
                    setPixelZoom(percent);
            }
            ImGui::EndCombo();
        }
        style::tooltip("Final image view zoom; wheel over the image, Space or middle button to pan");
        ImGui::SameLine(0, 2);
        if (style::iconButton(icon::zoomIn, "Zoom In [Ctrl + +]"))
            setPixelZoom(pixelZoom() * 1.5f);
        ImGui::SameLine(0, 2);
        if (style::iconButton(icon::fit, "Fit [Ctrl + 0] - double-click image")) {
            compositionView = {};
            compositionPixelLock = 0;
        }
        ImGui::SameLine(0, 2);
        if (ImGui::Button("1:1", {40, 0}))
            setPixelZoom(1);
        style::tooltip("One image pixel per screen pixel [Ctrl + 1]");
    }
    if (pendingPixelZoom > 0) {
        setPixelZoom(pendingPixelZoom);
        pendingPixelZoom = 0;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && viewIo.KeyCtrl && !viewIo.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0)) {
            compositionView = {};
            compositionPixelLock = 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_1) || ImGui::IsKeyPressed(ImGuiKey_Keypad1))
            setPixelZoom(1);
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
            setPixelZoom(pixelZoom() * 1.5f);
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
            setPixelZoom(pixelZoom() / 1.5f);
    }
    float w = fitW, h = fitH;
    ImGui::BeginChild("CompositionViewport", {0, viewportHeight}, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto areaOrigin = ImGui::GetCursorScreenPos(), areaSize = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##compositionView", areaSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    bool imageHovered = ImGui::IsItemHovered();
    auto centre = areaOrigin + areaSize * .5f;
    if (imageHovered && viewIo.MouseWheel) {
        compositionPixelLock = 0;
        auto fixed =
            (sx::float2{viewIo.MousePos.x - centre.x, viewIo.MousePos.y - centre.y} - compositionView.pan) /
            compositionView.zoom;
        compositionView.zoom =
            std::clamp(compositionView.zoom * std::pow(1.15f, viewIo.MouseWheel), .125f, 64.f);
        compositionView.pan = sx::float2{viewIo.MousePos.x - centre.x, viewIo.MousePos.y - centre.y} -
                              fixed * compositionView.zoom;
    }
    bool spacePan = ImGui::IsKeyDown(ImGuiKey_Space) && imageHovered;
    if (spacePan || compositionPanning)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    compositionHovered = imageHovered;
    if (imageHovered && viewIo.KeyCtrl && viewIo.KeyShift && ImGui::IsKeyPressed(ImGuiKey_C, false))
        copyStill();
    if (imageHovered && (ImGui::IsMouseClicked(2) || (spacePan && ImGui::IsMouseClicked(0)))) {
        compositionPanning = true;
        spacePanned = spacePanned || spacePan;
    }
    if (compositionPanning) {
        compositionView.pan = compositionView.pan + sx::float2{viewIo.MouseDelta.x, viewIo.MouseDelta.y};
        if (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(2))
            compositionPanning = false;
    }
    if (imageHovered && ImGui::IsMouseDoubleClicked(0) && !gradePick && !spacePan) {
        compositionView = {};
        compositionPixelLock = 0;
    }
    if (ImGui::BeginPopupContextItem("CompositionViewMenu")) {
        if (ImGui::MenuItem("Fit to Window", "Ctrl+0")) {
            compositionView = {};
            compositionPixelLock = 0;
        }
        if (ImGui::MenuItem("Pixels 1:1", "Ctrl+1"))
            setPixelZoom(1);
        for (float percent : {2.f, 4.f, 8.f}) {
            char label[16];
            snprintf(label, sizeof(label), "%.0f %%", percent * 100);
            if (ImGui::MenuItem(label))
                setPixelZoom(percent);
        }
        ImGui::Separator();
        guidesMenu();
        ImGui::Separator();
        if (ImGui::MenuItem("Export frame…"))
            exportStill();
        if (ImGui::MenuItem("Copy Image", "Ctrl+Shift+C"))
            copyStill();
        ImGui::EndPopup();
    }
    w *= compositionView.zoom;
    h *= compositionView.zoom;
    // Keep part of the image on screen.
    float limitX = (w + areaSize.x) * .5f - 40, limitY = (h + areaSize.y) * .5f - 40;
    compositionView.pan = {std::clamp(compositionView.pan.x, -limitX, limitX),
                           std::clamp(compositionView.pan.y, -limitY, limitY)};
    ImVec2 imageOrigin = centre + ImVec2(compositionView.pan.x - w * .5f, compositionView.pan.y - h * .5f);
    // The next frames render at the displayed resolution, up to the sequence's own size.
    compositionDetail = std::min(1.f, std::ceil(w / float(s.project.width) * 8) / 8);
    {
        auto *draw = ImGui::GetWindowDrawList();
        auto &platform = ImGui::GetPlatformIO();
        bool pixels = pixelZoom() >= 2 && platform.DrawCallback_SetSamplerNearest;
        if (pixels)
            draw->AddCallback(platform.DrawCallback_SetSamplerNearest, nullptr);
        if (shownView())
            draw->AddImage((ImTextureID)(intptr_t)shownView(), imageOrigin, imageOrigin + ImVec2(w, h));
        if (pixels && platform.DrawCallback_SetSamplerLinear)
            draw->AddCallback(platform.DrawCallback_SetSamplerLinear, nullptr);
        compositionOverlays(imageOrigin, w, h, imageHovered);
        welcomeCard(areaOrigin, areaSize);
        // Navigator: the whole picture and the visible part, once the image overflows the panel.
        if (w > areaSize.x + 1 || h > areaSize.y + 1) {
            float mapW = std::min(180.f, areaSize.x * .3f), mapH = mapW / ratio;
            ImVec2 mapMin = areaOrigin + areaSize - ImVec2(mapW + 10, mapH + 10), mapMax = mapMin + ImVec2(mapW, mapH);
            draw->AddRectFilled(mapMin - ImVec2(3, 3), mapMax + ImVec2(3, 3), IM_COL32(10, 11, 14, 220), 4);
            if (shownView())
                draw->AddImage((ImTextureID)(intptr_t)shownView(), mapMin, mapMax);
            ImVec2 visibleMin{std::max(mapMin.x, mapMin.x + (areaOrigin.x - imageOrigin.x) / w * mapW),
                              std::max(mapMin.y, mapMin.y + (areaOrigin.y - imageOrigin.y) / h * mapH)};
            ImVec2 visibleMax{std::min(mapMax.x, mapMin.x + (areaOrigin.x + areaSize.x - imageOrigin.x) / w * mapW),
                              std::min(mapMax.y, mapMin.y + (areaOrigin.y + areaSize.y - imageOrigin.y) / h * mapH)};
            draw->AddRectFilled(visibleMin, visibleMax, IM_COL32(239, 139, 58, 40));
            draw->AddRect(visibleMin, visibleMax, Orange, 0, 0, 1.5f);
            auto mouse = viewIo.MousePos;
            bool overMap = imageHovered && mouse.x >= mapMin.x && mouse.x <= mapMax.x && mouse.y >= mapMin.y &&
                           mouse.y <= mapMax.y;
            if (overMap && ImGui::IsMouseClicked(0))
                compositionMapDrag = true;
            if (compositionMapDrag) {
                // Centre the view on the image point under the pointer.
                float u = std::clamp((mouse.x - mapMin.x) / mapW, 0.f, 1.f), v = std::clamp((mouse.y - mapMin.y) / mapH, 0.f, 1.f);
                compositionView.pan = {(.5f - u) * w, (.5f - v) * h};
                if (!ImGui::IsMouseDown(0))
                    compositionMapDrag = false;
            }
        } else
            compositionMapDrag = false;
        // Pixel readout under the pointer.
        auto mouse = viewIo.MousePos;
        if (imageHovered && mouse.x >= imageOrigin.x && mouse.y >= imageOrigin.y && mouse.x < imageOrigin.x + w &&
            mouse.y < imageOrigin.y + h) {
            char where[48];
            snprintf(where, sizeof(where), "%d, %d px  ·  %.0f %%", int((mouse.x - imageOrigin.x) / w * s.project.width),
                     int((mouse.y - imageOrigin.y) / h * s.project.height), pixelZoom() * 100);
            auto extent = ImGui::CalcTextSize(where);
            ImVec2 at = areaOrigin + ImVec2(8, areaSize.y - extent.y - 10);
            draw->AddRectFilled(at - ImVec2(5, 3), at + extent + ImVec2(5, 3), IM_COL32(10, 11, 14, 200), 3);
            draw->AddText(at, IM_COL32(222, 224, 232, 255), where);
        }
    }
    bool wasGradePicking = gradePick != 0;
    gradePreview(imageOrigin, w, h);
    auto *selected = s.project.findClip(s.selected);
    if (maskGuides && !wasGradePicking && selected && selected->active(s.time) &&
        s.project.owner(selected->id)->kind == TrackKind::Video && selected->mask.shape != MaskShape::None) {
        auto origin = imageOrigin;
        bool hovered = imageHovered;
        auto *draw = ImGui::GetWindowDrawList();
        auto mouse = ImGui::GetIO().MousePos;
        float angle = selected->param("rotation", s.time, 0) * .01745329252f;
        float scale = std::max(.001f, selected->param("scale", s.time, 1));
        auto rotate = [](ImVec2 v, float a) {
            return ImVec2(std::cos(a) * v.x - std::sin(a) * v.y, std::sin(a) * v.x + std::cos(a) * v.y);
        };
        ImVec2 maskCenter{selected->param("mask_x", s.time, 0) * ratio, selected->param("mask_y", s.time, 0)};
        auto screen = [&](ImVec2 local) {
            auto transformed = rotate(local * scale, angle);
            return ImVec2(origin.x + w * (.5f + transformed.x / ratio + selected->param("x", s.time, 0)),
                          origin.y + h * (.5f + transformed.y + selected->param("y", s.time, 0)));
        };
        auto center = screen(maskCenter);
        if (hovered && std::abs(mouse.x - center.x) < 9 && std::abs(mouse.y - center.y) < 9 &&
            !s.project.owner(selected->id)->locked) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            ImGui::SetTooltip(
                "Move the mask; animated parameters receive a keyframe at the current time");
            if (ImGui::IsMouseClicked(0)) {
                s.history.checkpoint(s.project);
                s.playing = false;
                maskDragging = selected->id;
                maskDragMouse = mouse;
                maskDragX = selected->param("mask_x", s.time, 0);
                maskDragY = selected->param("mask_y", s.time, 0);
                maskDragTime = s.time;
            }
        }
        if (maskDragging == selected->id && ImGui::IsMouseDown(0) && !s.project.owner(selected->id)->locked) {
            ImVec2 delta{(mouse.x - maskDragMouse.x) / h, (mouse.y - maskDragMouse.y) / h};
            delta = rotate(delta, -angle) / scale;
            auto update = [&](const char *key, float value) {
                auto &parameter = selected->params[key];
                value = std::clamp(value, -2.f, 2.f);
                parameter.value = value;
                if (!parameter.keys.empty()) {
                    Keyframe point{maskDragTime, value, Curve::Smooth};
                    for (auto &existing : parameter.keys)
                        if (std::abs(existing.time - maskDragTime) < 1e-7) {
                            point.curve = existing.curve;
                            point.shape = existing.shape;
                        }
                    parameter.setKey(point);
                }
            };
            update("mask_x", maskDragX + delta.x / ratio);
            update("mask_y", maskDragY + delta.y);
            maskCenter = {selected->param("mask_x", s.time, 0) * ratio, selected->param("mask_y", s.time, 0)};
            center = screen(maskCenter);
            s.dirty = true;
        }
        float maskAngle = selected->param("mask_rotation", s.time, 0) * .01745329252f;
        float rx = selected->param("mask_width", s.time, 1) * ratio * .5f;
        float ry = selected->param("mask_height", s.time, 1) * .5f;
        std::vector<ImVec2> outline;
        if (selected->mask.shape == MaskShape::Rectangle) {
            for (ImVec2 corner : {ImVec2(-rx, -ry), ImVec2(rx, -ry), ImVec2(rx, ry), ImVec2(-rx, ry)})
                outline.push_back(screen(maskCenter + rotate(corner, maskAngle)));
        } else if (selected->mask.shape == MaskShape::Bezier) {
            for (size_t i = 0; i < selected->mask.points.size(); ++i) {
                auto &a = selected->mask.points[i];
                auto &b = selected->mask.points[(i + 1) % selected->mask.points.size()];
                for (int step = 0; step < 16; ++step) {
                    float t = step / 16.f, u = 1 - t;
                    ImVec2 point{
                        u * u * u * a.x + 3 * u * u * t * a.outX + 3 * u * t * t * b.inX + t * t * t * b.x,
                        u * u * u * a.y + 3 * u * u * t * a.outY + 3 * u * t * t * b.inY + t * t * t * b.y};
                    outline.push_back(
                        screen(maskCenter + rotate({point.x * rx * 2, point.y * ry * 2}, maskAngle)));
                }
            }
        } else {
            for (int i = 0; i < 64; ++i) {
                float a = i * (6.28318530718f / 64);
                outline.push_back(
                    screen(maskCenter + rotate({rx * std::cos(a), ry * std::sin(a)}, maskAngle)));
            }
        }
        draw->PushClipRect(origin, {origin.x + w, origin.y + h}, true);
        draw->AddPolyline(outline.data(), int(outline.size()), Orange, ImDrawFlags_Closed, 1.5f);
        draw->AddCircle(center, 5, Orange, 16, 1.5f);
        draw->PopClipRect();
    }
    if (!ImGui::IsMouseDown(0) || maskDragging != s.selected || !maskGuides)
        maskDragging = 0;
    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button("|<")) {
        s.time = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("<"))
        s.time = std::max(0., s.time - 1. / s.project.fps);
    ImGui::SameLine();
    if (ImGui::Button(s.playing ? "Pause" : "Playback", {90, 0}))
        s.playing = !s.playing;
    ImGui::SameLine();
    if (ImGui::Button(">"))
        s.time = std::min(s.project.duration, s.time + 1. / s.project.fps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(.94f, .55f, .23f, 1), "%s", timecode(s.time, s.project.fps).c_str());
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &loop);
    ImGui::SameLine();
    ImGui::Checkbox("Mask Guides", &maskGuides);
    ImGui::SameLine();
    if (ImGui::Button("Guides…"))
        ImGui::OpenPopup("compositionGuides");
    if (ImGui::BeginPopup("compositionGuides")) {
        guidesMenu();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Snapshot"))
        takeSnapshot();
    style::tooltip("Store the current image as A and compare it with the live image (B) using a wipe");
    if (snapshotView) {
        ImGui::SameLine();
        ImGui::Checkbox("A / B", &snapshotCompare);
    }
    ImGui::End();
}
void Editor::layers() {
    if (!panels["Layers"] || !beginPanel("Layers"))
        return;
    ImGui::TextDisabled("TOP -> FOREGROUND");
    for (size_t i = 0; i < s.project.tracks.size(); ++i) {
        auto &t = s.project.tracks[i];
        ImGui::PushID(int(t.id));
        auto before = s.project;
        bool visible = !t.muted;
        if (ImGui::Checkbox("##eye", &visible)) {
            s.history.checkpoint(before);
            t.muted = !visible;
            s.dirty = true;
        }
        ImGui::SameLine();
        bool selected = false;
        for (auto &c : t.clips)
            selected |= std::find(s.selection.begin(), s.selection.end(), c.id) != s.selection.end();
        if (ImGui::Selectable(t.name.c_str(), selected))
            if (!t.clips.empty())
                s.selectClip(t.clips.front().id);
        if (ImGui::BeginPopupContextItem("trackMenu")) {
            if (ImGui::MenuItem("Move Up", nullptr, false, i > 0)) {
                s.history.checkpoint(before);
                std::swap(s.project.tracks[i], s.project.tracks[i - 1]);
                s.dirty = true;
            }
            if (ImGui::MenuItem("Move Down", nullptr, false, i + 1 < s.project.tracks.size())) {
                s.history.checkpoint(before);
                std::swap(s.project.tracks[i], s.project.tracks[i + 1]);
                s.dirty = true;
            }
            if (ImGui::MenuItem(t.locked ? "Unlock" : "Lock")) {
                s.history.checkpoint(before);
                t.locked = !t.locked;
                s.dirty = true;
            }
            ImGui::EndPopup();
        }
        ImGui::TextDisabled("    %s  ·  %zu clip(s)%s", t.kind == TrackKind::Video ? "VIDEO" : "AUDIO",
                            t.clips.size(), t.locked ? "  / LOCKED" : "");
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ImGui::Button("+ Video")) {
        Json a = Json::Object();
        a["kind"] = "video";
        action("add_track", a);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Audio")) {
        Json a = Json::Object();
        a["kind"] = "audio";
        action("add_track", a);
    }
    ImGui::End();
}
bool Editor::effectControls(Id clip, int index) {
    auto *c = s.project.findClip(clip);
    if (!c)
        return false;
    int destination = index;
    ImGui::BeginDisabled(index == 0);
    if (ImGui::SmallButton("Move Up"))
        destination = index - 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= int(c->effects.size()));
    if (ImGui::SmallButton("Move Down"))
        destination = index + 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool remove = ImGui::SmallButton("Remove");
    if (!remove && destination == index)
        return false;
    auto args = clipArgs(clip);
    args["effect"] = index;
    if (!remove)
        args["position"] = destination;
    action(remove ? "remove_effect" : "set_effect", args);
    if (error.empty()) {
        auto remap = [&](int old) {
            if (remove)
                return old == index ? -1 : old > index ? old - 1 : old;
            if (old == index)
                return destination;
            if (index < destination && old > index && old <= destination)
                return old - 1;
            if (index > destination && old >= destination && old < index)
                return old + 1;
            return old;
        };
        bool animationRemoved = remove && animationEffect == index;
        selectedEffect = std::max(0, remap(selectedEffect));
        animationEffect = remap(animationEffect);
        if (animationRemoved)
            animationKey = s.project.owner(clip)->kind == TrackKind::Audio ? "gain" : "opacity";
        draftIdentity.clear();
        graphNode = wireNode = 0;
        graphViewIdentity.clear();
    }
    return true;
}
bool Editor::transitionControls(Id id) {
    auto *c = s.project.findClip(id);
    if (!c || s.project.owner(id)->kind != TrackKind::Video ||
        !ImGui::CollapsingHeader("Incoming Transition",
                                 c->transition.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        return false;
    std::string label = c->transition.enabled ? c->transition.id : "None";
    if (auto *definition = s.renderer.library.find(c->transition.id); c->transition.enabled && definition)
        label = definition->name;
    std::string choice;
    bool remove = false;
    if (ImGui::BeginCombo("Transition", label.c_str())) {
        remove = ImGui::Selectable("None", !c->transition.enabled);
        for (auto &definition : s.renderer.library.definitions)
            if (definition.kind == "transition" &&
                ImGui::Selectable(definition.name.c_str(), c->transition.id == definition.id))
                choice = definition.id;
        ImGui::EndCombo();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("TRANSITION"))
            choice = static_cast<const char *>(payload->Data);
        ImGui::EndDragDropTarget();
    }
    if (remove || !choice.empty()) {
        auto args = clipArgs(id);
        if (!remove)
            args["id"] = choice;
        action(remove ? "remove_transition" : "set_transition", args);
        draftIdentity.clear();
        return true;
    }
    if (!c->transition.enabled)
        return false;
    auto before = s.project;
    double duration = c->transition.duration, minimum = 1e-5, maximum = c->duration;
    if (ImGui::DragScalar("Transition Duration", ImGuiDataType_Double, &duration, .01f, &minimum, &maximum,
                          "%.3f s", ImGuiSliderFlags_AlwaysClamp) &&
        std::isfinite(duration))
        c->transition.duration = duration;
    editCheckpoint(before);
    ImGui::TextWrapped(
        "Overlap this clip's start with the layers below. Audio has its own fades.");
    ImGui::Text("Progress at Current Time: %.0f %%", c->transitionProgress(s.time) * 100.f);
    parameter("Progress (Overrides Automatic Time)", "transition_progress", 0, 1,
              c->transitionProgress(s.time));
    if (ImGui::SmallButton("Automatic progress")) {
        auto args = clipArgs(id);
        args["reset_progress"] = true;
        action("set_transition", args);
        return true;
    }
    try {
        for (auto &p : effectParameters(s.renderer.library.transitionCode(c->transition))) {
            int count = p.type == "float2"                        ? 2
                        : p.type == "float3"                      ? 3
                        : p.type == "float4" || p.type == "color" ? 4
                                                                  : 1;
            for (int index = 0; index < count; ++index)
                parameter(p.label.c_str(), p.key + (count > 1 ? "." + std::to_string(index) : ""), p.minimum,
                          p.maximum, p.value[index]);
        }
    } catch (const std::exception &ex) {
        ImGui::TextWrapped("%s", ex.what());
    }
    if (ImGui::SmallButton("Edit Transition HLSL")) {
        selectedEffect = -2;
        draftIdentity.clear();
        ImGui::SetWindowFocus(panelTitle("HLSL").c_str());
    }
    return false;
}
void Editor::inspector() {
    if (!panels["Properties"] || !beginPanel("Properties"))
        return;
    auto *c = s.project.findClip(s.selected);
    auto *track = s.project.owner(s.selected);
    if (!c || !track) {
        ImGui::TextWrapped("Select a clip in the timeline to edit its properties.");
        ImGui::End();
        return;
    }
    ImGui::BeginDisabled(track->locked);
    auto before = s.project;
    ImGui::InputText("Name", &c->name);
    editCheckpoint(before);
    double frame = 1. / s.project.fps;
    auto number = [&](const char *label, double value, double lo, double hi, int field) {
        auto b = s.project;
        bool edited = ImGui::DragScalar(label, ImGuiDataType_Double, &value, 0.03f, &lo, &hi,
                                        field == 3 ? "%.3f ×" : "%.3f s");
        editCheckpoint(b);
        if (edited) {
            try {
                s.project.setTiming(c->id, field == 0 ? value : c->start, field == 1 ? value : c->duration,
                                    field == 2 ? value : c->in, field == 3 ? value : c->speed);
                error.clear();
            } catch (const std::exception &e) {
                error = e.what();
            }
        }
    };
    number("Start", c->start, 0, 86400, 0);
    number("Duration", c->duration, frame, 86400, 1);
    number("Source In", c->in, 0, 86400, 2);
    number("Speed", c->speed, 0.05, 16, 3);
    if (c->link)
        ImGui::TextColored(ImVec4(.25f, .7f, .72f, 1), "Linked Group - %u", c->link);
    ImGui::SeparatorText("Clip Fades");
    auto fade = [&](const char *label, double &value) {
        auto b = s.project;
        double lo = 0, hi = c->duration;
        ImGui::DragScalar(label, ImGuiDataType_Double, &value, .01f, &lo, &hi, "%.3f s",
                          ImGuiSliderFlags_AlwaysClamp);
        editCheckpoint(b);
    };
    fade("Input", c->fadeIn);
    fade("Output", c->fadeOut);
    if (transitionControls(c->id)) {
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }
    if (c->title.enabled && ImGui::CollapsingHeader("Title", ImGuiTreeNodeFlags_DefaultOpen)) {
        char content[16385]{};
        memcpy(content, c->title.text.data(), std::min(c->title.text.size(), sizeof(content) - 1));
        before = s.project;
        if (ImGui::InputTextMultiline("##titleText", content, sizeof(content), {-1, 64}))
            c->title.text = content;
        editCheckpoint(before);
        auto font = c->title.font;
        before = s.project;
        if (ImGui::InputText("Font", &font) && !font.empty() && font.size() <= 256)
            c->title.font = font;
        editCheckpoint(before);
        before = s.project;
        ImGui::Checkbox("Bold", &c->title.bold);
        editCheckpoint(before);
        ImGui::SameLine();
        before = s.project;
        ImGui::Checkbox("Italic", &c->title.italic);
        editCheckpoint(before);
        before = s.project;
        ImGui::Combo("Alignment", &c->title.alignment, "Left\0Center\0Right\0");
        editCheckpoint(before);
        parameter("Text Size (px)", "font_size", 4, 256, 48);
        parameter("Black outline (px)", "text_outline", 0, 16, 0);
        parameter("Shadow Opacity", "text_shadow", 0, 1, 0);
        parameter("Shadow X (px)", "text_shadow_x", -256, 256, 4);
        parameter("Shadow Y (px)", "text_shadow_y", -256, 256, 4);
        parameter("Shadow Blur (px)", "text_shadow_blur", 0, 32, 0);
        float color[4];
        const char *keys[] = {"text_r", "text_g", "text_b", "text_a"};
        for (int i = 0; i < 4; ++i) {
            auto parameter = c->params.find(keys[i]);
            color[i] = parameter == c->params.end() ? 1 : parameter->second.value;
        }
        before = s.project;
        ImGui::SetNextItemWidth(std::max(50.f, ImGui::GetContentRegionAvail().x - 42));
        if (ImGui::ColorEdit4("##textColor", color, ImGuiColorEditFlags_AlphaBar))
            for (int i = 0; i < 4; ++i)
                c->params[keys[i]].value = color[i];
        editCheckpoint(before);
        ImGui::SameLine();
        if (ImGui::Button("K##textColor")) {
            s.history.checkpoint(s.project);
            for (int i = 0; i < 4; ++i)
                c->params[keys[i]].setKey({snapTime(s.time, s.project.fps, true), color[i], Curve::Smooth});
            animationKey = "text_r";
            animationEffect = -1;
            s.dirty = true;
        }
    }
    if (track->kind == TrackKind::Video && ImGui::CollapsingHeader("Source Colors")) {
        const char *spaces[] = {"auto", "srgb", "linear709", "linear2020", "logc3-awg3-ei800"};
        int choice = 0;
        for (int i = 0; i < 5; ++i)
            if (c->sourceColor == spaces[i])
                choice = i;
        before = s.project;
        if (ImGui::Combo(
                "Source Space", &choice,
                "Automatic\0sRGB\0Linear Rec.709\0Linear Rec.2020\0ARRI LogC3 / AWG3 - EI800\0"))
            c->sourceColor = spaces[choice];
        editCheckpoint(before);
        before = s.project;
        ImGui::Checkbox("Premultiplied Source Pixels", &c->sourcePremultiplied);
        editCheckpoint(before);
    }
    if (track->kind == TrackKind::Video && ImGui::CollapsingHeader("Mask")) {
        before = s.project;
        int shape = int(c->mask.shape);
        if (ImGui::Combo("Shape", &shape, "None\0Rectangle\0Ellipse\0Bezier\0")) {
            c->mask.shape = MaskShape(shape);
            if (shape == 3 && c->mask.points.empty())
                c->mask.points = {{-.4f, -.3f, -.4f, -.3f, -.4f, -.3f},
                                  {.4f, -.3f, .4f, -.3f, .4f, -.3f},
                                  {.4f, .3f, .4f, .3f, .4f, .3f},
                                  {-.4f, .3f, -.4f, .3f, -.4f, .3f}};
        }
        editCheckpoint(before);
        if (c->mask.shape != MaskShape::None) {
            before = s.project;
            ImGui::Checkbox("Invert", &c->mask.inverted);
            editCheckpoint(before);
            parameter("Center X", "mask_x", -1, 1, 0);
            parameter("Center Y", "mask_y", -1, 1, 0);
            parameter("Width", "mask_width", .001f, 2, 1);
            parameter("Height", "mask_height", .001f, 2, 1);
            parameter("Mask Rotation", "mask_rotation", -180, 180, 0);
            parameter("Feather (px)", "mask_feather", 0, 256, 0);
            if (c->mask.shape == MaskShape::Bezier) {
                for (size_t i = 0; i < c->mask.points.size(); ++i) {
                    ImGui::PushID(int(i));
                    auto &point = c->mask.points[i];
                    if (ImGui::TreeNode("Point", "Point %u", unsigned(i + 1))) {
                        before = s.project;
                        ImGui::SliderFloat2("Position", &point.x, -1, 1);
                        editCheckpoint(before);
                        before = s.project;
                        ImGui::SliderFloat2("Incoming Handle", &point.inX, -2, 2);
                        editCheckpoint(before);
                        before = s.project;
                        ImGui::SliderFloat2("Outgoing Handle", &point.outX, -2, 2);
                        editCheckpoint(before);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (c->mask.points.size() < 16 && ImGui::Button("Add Bezier Point")) {
                    before = s.project;
                    c->mask.points.push_back({0, .5f, 0, .5f, 0, .5f});
                    s.history.checkpoint(before);
                    s.dirty = true;
                }
                if (c->mask.points.size() > 3 && ImGui::Button("Remove Last Point")) {
                    before = s.project;
                    c->mask.points.pop_back();
                    s.history.checkpoint(before);
                    s.dirty = true;
                }
            }

            if (ImGui::Button("Track Mask...")) {
                trackingBegin = std::max(s.time, c->start);
                trackingEnd = std::min(c->start + c->duration, trackingBegin + 5);
                ImGui::OpenPopup("Mask Tracking");
            }
            bool requested = false;
            if (ImGui::BeginPopup("Mask Tracking")) {
                ImGui::TextWrapped("Tracking calculates translation from the mask's central region. "
                                   "Choose a high-contrast region. Keyframes remain editable.");
                ImGui::InputDouble("Start (s)", &trackingBegin);
                ImGui::InputDouble("End (s)", &trackingEnd);
                ImGui::SliderInt("Search Radius (px)", &trackingRadius, 1, 64);
                auto status = s.tracking.status();
                ImGui::Text("%s · %s", status["state"].S().c_str(), status["error"].S().c_str());
                ImGui::ProgressBar(status["progress"].F());
                if (ImGui::Button("Calculer")) {
                    auto args = clipArgs(c->id);
                    args["begin"] = trackingBegin;
                    args["end"] = trackingEnd;
                    args["radius"] = trackingRadius;
                    action("start_mask_tracking", args);
                    requested = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Undo")) {
                    action("cancel_mask_tracking");
                    requested = true;
                }
                ImGui::BeginDisabled(status["state"].S() != "completed");
                if (ImGui::Button("Apply animations")) {
                    Json args = Json::Object();
                    args["job"] = status["job"];
                    action("apply_mask_tracking", args);
                    requested = true;
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            if (requested) {
                ImGui::EndDisabled();
                ImGui::End();
                return;
            }
        }
    }
    if (track->kind == TrackKind::Video) {
        ImGui::SeparatorText("Transformation");
        parameter("Opacity", "opacity", 0, 1, 1);
        parameter("Position X", "x", -1, 1, 0);
        parameter("Position Y", "y", -1, 1, 0);
        parameter("Scale", "scale", 0.05f, 4, 1);
        parameter("Rotation", "rotation", -180, 180, 0);
        int mode = int(c->param("blend", s.time, 0));
        auto blendBefore = s.project;
        if (ImGui::Combo("Fusion", &mode, "Normal\0Add\0Multiply\0Screen\0")) {
            s.history.checkpoint(blendBefore);
            c->params["blend"].value = float(mode);
            s.dirty = true;
        }
    } else {
        ImGui::SeparatorText("Clip Mix");
        parameter("Gain dB", "gain", -60, 12, 0);
        parameter("Panoramique", "pan", -1, 1, 0);
        animationKey = animationEffect < 0 && animationKey == "opacity" ? "gain" : animationKey;
    }
    if (track->kind == TrackKind::Video) {
        ImGui::SeparatorText("HLSL effect stack");
        if (ImGui::BeginCombo("+ Effect", "Add...")) {
            for (auto &d : s.renderer.library.definitions)
                if (d.kind == "effect" && ImGui::Selectable(d.name.c_str())) {
                    auto a = clipArgs(c->id);
                    a["id"] = d.id;
                    action("add_effect", a);
                    break;
                }
            ImGui::EndCombo();
        }
        if (ImGui::Button("+ Plugin OFX…")) {
            auto path = dialog(false, L"Windows x64 OpenFX plugin\0*.ofx\0All\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto a = clipArgs(c->id);
                a["path"] = path;
                action("add_ofx_effect", a);
            }
        }
        for (int i = 0; i < int(c->effects.size()); ++i) {
            auto &e = c->effects[i];
            ImGui::PushID(i);
            before = s.project;
            ImGui::Checkbox("##enabled", &e.enabled);
            editCheckpoint(before);
            ImGui::SameLine();
            auto *d = s.renderer.library.find(e.id);
            std::string title = e.id == "grading"
                                    ? static_cast<const Effect &>(e).grading["name"].S("Correction")
                                : e.id == "ofx" ? e.graph["name"].S("OFX")
                                : d             ? d->name
                                                : e.id;
            if (ImGui::Selectable(title.c_str(), selectedEffect == i)) {
                selectedEffect = i;
                draftIdentity.clear();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto *p = ImGui::AcceptDragDropPayload("EFFECT")) {
                    auto a = clipArgs(c->id);
                    a["id"] = static_cast<const char *>(p->Data);
                    action("add_effect", a);
                }
                ImGui::EndDragDropTarget();
            }
            if (selectedEffect == i) {
                try {
                    if (e.id == "grading") {
                        if (ImGui::Button("Open Color Grading"))
                            ImGui::SetWindowFocus(panelTitle("Color Grading").c_str());
                    }
                    if (e.id == "ofx")
                        for (auto &p : e.graph["parameters"].Elements())
                            parameter(p["name"].S().c_str(), p["key"].S(), p["minimum"].F(), p["maximum"].F(),
                                      p["default"].F(), i);
                    for (auto &param : effectParameters(e.id == "grading" ? "" : effectCode(e))) {
                        int n = param.type == "float2"                              ? 2
                                : param.type == "float3"                            ? 3
                                : (param.type == "float4" || param.type == "color") ? 4
                                                                                    : 1;
                        for (int k = 0; k < n; ++k) {
                            std::string key = param.key + (n > 1 ? "." + std::to_string(k) : "");
                            std::string label = param.label + (n > 1 ? " " + std::to_string(k) : "");
                            parameter(label.c_str(), key, param.minimum, param.maximum, param.value[k], i);
                        }
                    }
                } catch (const std::exception &ex) {
                    ImGui::TextWrapped("%s", ex.what());
                }
                if (effectControls(c->id, i)) {
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopID();
        }
    }
    if (track->kind == TrackKind::Audio) {
        ImGui::SeparatorText("CyDAW 2 audio effects");
        int stretch = int(std::lround(
            c->param("stretch_mode", s.time, c->param("preserve_pitch", s.time, 0) > .5f ? 1 : 0)));
        if (ImGui::Combo("Time adaptation", &stretch, "Repitch\0Tonal\0Battements\0Texture\0")) {
            auto a = clipArgs(c->id);
            a["key"] = "stretch_mode";
            a["value"] = stretch;
            action("set_parameter", a);
        }
        auto graphTarget = clipArgs(c->id);
        graphTarget["scope"] = "clip";
        if (dspGraphEditor(graphTarget, c->effects)) {
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        if (ImGui::Button("+ Native / VST3 plugin")) {
            auto path = dialog(false, L"Audio plugins\0*.dll;*.vst3\0All\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto args = clipArgs(c->id);
                args["scope"] = "clip";
                args["path"] = path;
                args["kind"] = sx::Utf8Path(path).extension() == L".vst3" ? "vst3" : "native";
                action("add_external_audio", args);
            }
            ImGui::EndDisabled();
            ImGui::End();
            return;
        }
        if (ImGui::BeginCombo("+ DSP", "Add...")) {
            for (auto &d : cydaw::engine::effectCatalogue)
                if (!d.convertsChannels() && ImGui::Selectable(std::string(d.displayName).c_str())) {
                    auto a = clipArgs(c->id);
                    a["id"] = "cydaw." + std::string(d.typeKey);
                    action("add_audio_effect", a);
                    break;
                }
            ImGui::EndCombo();
        }
        for (int i = 0; i < int(c->effects.size()); ++i) {
            auto &e = c->effects[i];
            auto *d = audioEffect(e.id);
            if (!d)
                continue;
            ImGui::PushID(i);
            auto b = s.project;
            ImGui::Checkbox("##bypass", &e.enabled);
            editCheckpoint(b);
            ImGui::SameLine();
            ImGui::TextUnformatted(std::string(d->displayName).c_str());
            auto pluginTarget = clipArgs(c->id);
            pluginTarget["scope"] = "clip";
            pluginTarget["effect"] = i;
            if (pluginControls(pluginTarget, e)) {
                ImGui::PopID();
                break;
            }
            for (auto &param : d->parameters)
                parameter(std::string(param.key).c_str(), std::string(param.key), param.minimum,
                          param.maximum, param.defaultValue, i);
            if (externalAudio(e.id))
                for (auto &[key, value] : e.params)
                    parameter((e.parameterNames.contains(key) ? e.parameterNames.at(key) : key).c_str(), key,
                              0, 1, value.value, i);
            if (effectControls(c->id, i)) {
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}
void Editor::shaderEditor() {
    if (!panels["HLSL"] || !beginPanel("HLSL"))
        return;
    auto *c = s.project.findClip(s.selected);
    bool transition = selectedEffect == -2 && c && c->transition.enabled;
    if (c && !transition && selectedEffect >= 0 && selectedEffect < int(c->effects.size()) &&
        c->effects[selectedEffect].id == "grading") {
        ImGui::TextUnformatted("Shader generated by color grading for the working color space.");
        auto generated = gradeShader(c->effects[selectedEffect], s.project.workingSpace);
        ImGui::InputTextMultiline("##gradeShader", &generated, {-1, -1}, ImGuiInputTextFlags_ReadOnly);
        ImGui::End();
        return;
    }
    if (c && !transition && selectedEffect >= 0 && selectedEffect < int(c->effects.size()) &&
        c->effects[selectedEffect].id == "ofx") {
        ImGui::TextWrapped("OFX - %s. Its parameters and animation are in Properties.",
                           c->effects[selectedEffect].graph["name"].S().c_str());
        ImGui::End();
        return;
    }
    if (!c || (!transition && (selectedEffect < 0 || selectedEffect >= int(c->effects.size())))) {
        ImGui::TextWrapped("Choose an effect in Properties to edit its shader. EffectContext "
                           "provides time, localTime, deltaTime and frame.");
        ImGui::End();
        return;
    }
    std::string identity = std::to_string(c->id) + ":" + std::to_string(selectedEffect);
    if (identity != draftIdentity) {
        try {
            shaderDraft = transition ? s.renderer.library.transitionCode(c->transition)
                                     : effectCode(c->effects[selectedEffect]);
        } catch (...) {
            shaderDraft = transition ? c->transition.code : c->effects[selectedEffect].code;
        }
        draftIdentity = identity;
    }
    if (ImGui::Button("Compile & Apply")) {
        auto a = clipArgs(c->id);
        if (!transition)
            a["effect"] = selectedEffect;
        a["code"] = shaderDraft;
        action(transition ? "set_transition_shader" : "set_shader", a);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
        draftIdentity.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("HLSL 5.0  /  pixel shader");
    ImGui::PushFont(sx::ui::GetFonts().mono);
    ImGui::InputTextMultiline("##code", &shaderDraft, {-1, -1}, ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopFont();
    ImGui::End();
}
void Editor::layerGraphEditor() {
    if (!panels["Layer Graph"] || !beginPanel("Layer Graph"))
        return;
    try {
        const bool implicit = s.project.layerGraph.IsNull();
        if (implicit)
            ImGui::TextWrapped("Timelines -> layers -> Shaderixel composition graph. The graph controls "
                               "node-based compositing.");
        if (ImGui::Button("Rebuild from Tracks")) {
            Json args = Json::Object();
            args["graph"] = defaultLayerGraph(s.project);
            action("set_layer_graph", args);
            ImGui::End();
            return;
        }
        auto draft = implicit ? defaultLayerGraph(s.project) : s.project.layerGraph;
        if (layerCanvas.draw(draft, s.project, s.renderer, s.time)) {
            Json args = Json::Object();
            args["graph"] = draft;
            action("set_layer_graph", args);
        }
        if (auto node = layerCanvas.takeLutRequest()) {
            auto path = dialog(false, L"1D / 2D / 3D LUT\0*.cube;*.png;*.tif;*.exr\0All\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto lut = loadLut(sx::Utf8Path(path), layerCanvas.lutLayoutName());
                auto graph = s.project.layerGraph;
                auto nodes = Json::Array();
                for (auto item : graph["nodes"].Elements()) {
                    if (item["id"].U() == node)
                        item["effect"]["grading"]["lut"] = lut;
                    nodes.Push(item);
                }
                graph["nodes"] = nodes;
                Json args = Json::Object();
                args["graph"] = graph;
                action("set_layer_graph", args);
            }
        }
        if (auto node = layerCanvas.takeShaderRequest()) {
            layerShaderNode = node;
            ImGui::SetWindowFocus(panelTitle("Shader Graph").c_str());
        }
    } catch (const std::exception &e) {
        error = e.what();
        ImGui::TextWrapped("%s", error.c_str());
    }
    ImGui::End();
}
void Editor::graphEditor() {
    if (!panels["Shader Graph"] || !beginPanel("Shader Graph"))
        return;
    Effect layerEffect;
    if (layerShaderNode) {
        try {
            auto graph = decodeLayerGraph(s.project.layerGraph, s.project);
            layerEffect = graph.effects.at(layerShaderNode);
        } catch (...) {
            layerShaderNode = 0;
        }
    }
    if (layerShaderNode) {
        ImGui::Text("Shader Graph for Layer Node %u", layerShaderNode);
        ImGui::SameLine();
        if (ImGui::Button("Back to Clip Effects"))
            layerShaderNode = 0;
    }
    auto *c = s.project.findClip(s.selected);
    if (!layerShaderNode) {
        if (!c || s.project.owner(c->id)->kind != TrackKind::Video) {
            ImGui::TextWrapped(
                "Select a video clip to create or open a Shaderixel effect graph.");
            ImGui::End();
            return;
        }
        if (s.project.owner(c->id)->locked) {
            ImGui::TextDisabled("Track is locked");
            ImGui::End();
            return;
        }
        if (ImGui::Button("+ Shaderixel Graph")) {
            s.history.checkpoint(s.project);
            Effect e;
            e.id = "graph";
            e.graph = sx::EffectGraph::Default().ToJson();
            e.code = effectCode(e);
            c->effects.push_back(e);
            selectedEffect = int(c->effects.size() - 1);
            s.dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Import .json")) {
            auto path = dialog(false, L"Shaderixel graph\0*.json;*.sxgraph\0All\0*.*\0\0", nullptr);
            if (!path.empty())
                try {
                    std::string text;
                    sx::ReadTextFile(path, text);
                    Json j;
                    std::string err;
                    if (!Json::Parse(text, j, &err))
                        throw std::runtime_error(err);
                    Effect e;
                    e.id = "graph";
                    e.graph = j;
                    e.code = effectCode(e);
                    s.renderer.validateShader(e.code);
                    s.history.checkpoint(s.project);
                    c->effects.push_back(e);
                    selectedEffect = int(c->effects.size() - 1);
                    s.dirty = true;
                } catch (const std::exception &ex) {
                    error = ex.what();
                }
        }
        if (selectedEffect < 0 || selectedEffect >= int(c->effects.size()) ||
            c->effects[selectedEffect].graph.IsNull() || c->effects[selectedEffect].id == "ofx") {
            ImGui::TextWrapped(
                "The graph generates the same HLSL as Shaderixel. Text shaders are in "
                "the HLSL panel.");
            ImGui::End();
            return;
        }
    }
    auto &effect = layerShaderNode ? layerEffect : c->effects[selectedEffect];
    auto identity = layerShaderNode ? "layer:" + std::to_string(layerShaderNode)
                                    : std::to_string(c->id) + ":" + std::to_string(selectedEffect);
    bool fitView = graphViewIdentity != identity;
    if (fitView) {
        graphFitPending = true;
        graphViewIdentity = identity;
        graphView = {};
        graphDragging = graphNode = wireNode = 0;
        graphPanning = false;
        graphDragBefore.reset();
    }
    sx::EffectGraph graph;
    if (!sx::EffectGraph::FromJson(effect.graph, graph)) {
        ImGui::End();
        return;
    }
    bool changed = false;
    auto before = s.project;
    // Shaderixel takes the alpha from the RGBA pin of the output: with only RGB wired, the image vanishes.
    if (auto *output = graph.Find(graph.OutputNode())) {
        auto wired = [&](size_t i) { return i < output->inputs.size() && output->inputs[i].node; };
        if (!wired(0) && !wired(2) && wired(1)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, .72f, .35f, 1));
            ImGui::TextWrapped("Output: alpha is disconnected; the image will be transparent. Connect an image to RGBA "
                               "or a value to Alpha.");
            ImGui::PopStyleColor();
            sx::EGNode *input = nullptr;
            for (auto &node : graph.nodes)
                if (node.op == "input" && node.index == 0)
                    input = &node;
            if (input && ImGui::SmallButton("Connect Input Image to RGBA")) {
                output->inputs.resize(std::max<size_t>(output->inputs.size(), 3));
                output->inputs[0] = {input->id, 0};
                changed = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::BeginCombo("##node", "+ Node")) {
        for (auto &op : sx::EffectGraphOps())
            if (sx::OpAllowed(op, sx::EGKind::Effect) && std::string(op.op) != "output" &&
                std::string(op.op) != "opacity") {
                if (ImGui::Selectable(op.label)) {
                    sx::EGNode node;
                    node.id = graph.nextId++;
                    node.op = op.op;
                    auto visible = ImGui::GetContentRegionAvail();
                    node.pos = graphView.world({visible.x * .5f, visible.y * .5f});
                    node.inputs.resize(op.inputs.size());
                    node.value = {1, 1, 1, 1};
                    node.name = "p" + std::to_string(node.id);
                    graph.nodes.push_back(node);
                    changed = true;
                }
            }
        ImGui::EndCombo();
    }
    if (auto *node = graph.Find(graphNode)) {
        ImGui::Text("Node %u - %s", node->id, node->op.c_str());
        if (ImGui::InputText("Parameter Name", &node->name))
            changed = true;
        float v[4] = {node->value.x, node->value.y, node->value.z, node->value.w};
        if (ImGui::DragFloat4("Value", v, .01f)) {
            node->value = {v[0], v[1], v[2], v[3]};
            changed = true;
        }
        if (node->op == "custom" && ImGui::InputTextMultiline("Code", &node->text, {-1, 70}))
            changed = true;
        // Shaderixel's values for unconnected inputs: each scalar pin can differ from the operation default.
        auto definition = sx::EffectiveOp(*node);
        for (size_t i = 0; i < definition.inputs.size(); ++i) {
            auto &pin = definition.inputs[i];
            bool wired = i < node->inputs.size() && node->inputs[i].node;
            if (wired || pin.defValue < 0)
                continue;
            ImGui::PushID(int(i));
            float value = node->PinValue(i, pin.defValue);
            ImGui::SetNextItemWidth(120);
            if (ImGui::DragFloat(pin.name, &value, .01f)) {
                if (node->pinValues.size() <= i)
                    node->pinValues.resize(i + 1, std::numeric_limits<float>::quiet_NaN());
                node->pinValues[i] = value;
                changed = true;
            }
            if (node->PinValueSet(i)) {
                ImGui::SameLine();
                if (ImGui::SmallButton("default")) {
                    node->pinValues[i] = std::numeric_limits<float>::quiet_NaN();
                    changed = true;
                }
            }
            ImGui::PopID();
        }
        ImGui::SameLine();
        if (node->op != "output" && ImGui::SmallButton("Remove Node")) {
            Id id = node->id;
            std::erase_if(graph.nodes, [id](auto &n) { return n.id == id; });
            for (auto &n : graph.nodes)
                for (auto &in : n.inputs)
                    if (in.node == id)
                        in = {};
            graphNode = 0;
            changed = true;
        }
    }
    if (ImGui::Button("Fit All"))
        graphFitPending = true;
    ImGui::SameLine();
    bool actualSize = ImGui::Button("1:1");
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f %% - wheel: zoom - middle button: pan", graphView.zoom * 100);
    ImGui::BeginChild("graphCanvas", {0, 0}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
    avail = {std::max(1.f, avail.x), std::max(1.f, avail.y)};
    ImGui::InvisibleButton("graphInput", avail,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool hovered = ImGui::IsItemHovered();
    // The automatic framing follows panel resizes (mode changes) until the user navigates.
    if (graphAutoFit && (std::abs(avail.x - graphFitSize.x) > 1 || std::abs(avail.y - graphFitSize.y) > 1))
        graphFitPending = true;
    if (graphFitPending && avail.x >= 100 && avail.y >= 80 && !graph.nodes.empty()) {
        graphAutoFit = true;
        graphFitSize = avail;
        sx::float2 low{FLT_MAX, FLT_MAX}, high{-FLT_MAX, -FLT_MAX};
        for (auto &n : graph.nodes) {
            auto def = sx::EffectiveOp(n);
            float height = std::max(68.f, 42.f + 18 * std::max(def.inputs.size(), def.outputs.size()));
            low.x = std::min(low.x, n.pos.x);
            low.y = std::min(low.y, n.pos.y);
            high.x = std::max(high.x, n.pos.x + 165);
            high.y = std::max(high.y, n.pos.y + height);
        }
        graphView.fit(low, high, {avail.x, avail.y});
        graphFitPending = false;
    }
    if (actualSize || (hovered && (ImGui::GetIO().MouseWheel != 0 || ImGui::IsMouseClicked(2))))
        graphAutoFit = false;
    if (actualSize)
        graphView.zoomAt({avail.x * .5f, avail.y * .5f}, 1);
    if (hovered && ImGui::GetIO().MouseWheel != 0 && !graphDragging)
        graphView.zoomAt({mouse.x - origin.x, mouse.y - origin.y},
                         graphView.zoom * std::pow(1.15f, ImGui::GetIO().MouseWheel));
    if (hovered && ImGui::IsMouseClicked(2))
        graphPanning = true;
    if (!ImGui::IsMouseDown(2))
        graphPanning = false;
    if (graphPanning) {
        graphView.pan.x += ImGui::GetIO().MouseDelta.x;
        graphView.pan.y += ImGui::GetIO().MouseDelta.y;
    }
    if (!ImGui::IsMouseDown(0)) {
        graphDragging = 0;
        graphDragBefore.reset();
    }
    float zoom = graphView.zoom;
    auto screen = [&](sx::float2 pos) {
        auto p = graphView.screen(pos);
        return origin + ImVec2(p.x, p.y);
    };
    auto textAt = [&](ImVec2 point, ImU32 color, const char *text) {
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * zoom, point, color, text);
    };
    dl->PushClipRect(origin, origin + avail, true);
    dl->AddRectFilled(origin, origin + avail, IM_COL32(18, 20, 25, 255));
    float grid = 24 * zoom;
    for (float xx = origin.x + std::fmod(graphView.pan.x, grid); xx < origin.x + avail.x; xx += grid)
        for (float yy = origin.y + std::fmod(graphView.pan.y, grid); yy < origin.y + avail.y; yy += grid)
            dl->AddCircleFilled({xx, yy}, .8f, IM_COL32(61, 64, 75, 255));
    auto inputPos = [&](const sx::EGNode &n, int pin) { return screen({n.pos.x, n.pos.y + 38 + pin * 18}); };
    auto outputPos = [&](const sx::EGNode &n, int pin) {
        return screen({n.pos.x + 165, n.pos.y + 38 + pin * 18});
    };
    for (auto &n : graph.nodes)
        for (int pin = 0; pin < int(n.inputs.size()); ++pin) {
            auto &wire = n.inputs[pin];
            if (auto *src = graph.Find(wire.node)) {
                ImVec2 a = outputPos(*src, wire.output), b = inputPos(n, pin);
                dl->AddBezierCubic(a, a + ImVec2(60 * zoom, 0), b - ImVec2(60 * zoom, 0), b, Audio, 2);
            }
        }
    if (wireNode) {
        if (auto *n = graph.Find(wireNode)) {
            auto a = outputPos(*n, wireOutput);
            dl->AddBezierCubic(a, a + ImVec2(50, 0), mouse - ImVec2(50, 0), mouse, Orange, 2);
        }
    }
    for (auto &n : graph.nodes) {
        auto def = sx::EffectiveOp(n);
        float height = std::max(68.f, 42.f + 18 * std::max(def.inputs.size(), def.outputs.size()));
        ImVec2 a = screen(n.pos), b = a + ImVec2(165 * zoom, height * zoom);
        dl->AddRectFilled(a, b, IM_COL32(35, 38, 46, 255), 5);
        dl->AddRect(a, b, n.id == graphNode ? Orange : IM_COL32(75, 78, 90, 255), 5);
        textAt(a + ImVec2(9 * zoom, 8 * zoom), Orange, def.label);
        bool header = hovered && mouse.x > a.x && mouse.x < b.x && mouse.y > a.y && mouse.y < a.y + 27 * zoom;
        if (header && ImGui::IsMouseClicked(0)) {
            graphNode = n.id;
            graphDragging = n.id;
            graphDragBefore = before;
        }
        if (n.id == graphDragging && ImGui::IsMouseDragging(0) &&
            (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0)) {
            if (graphDragBefore) {
                s.history.checkpoint(*graphDragBefore);
                graphDragBefore.reset();
            }
            n.pos.x += ImGui::GetIO().MouseDelta.x / zoom;
            n.pos.y += ImGui::GetIO().MouseDelta.y / zoom;
            changed = true;
        }
        for (int p = 0; p < int(def.inputs.size()); ++p) {
            auto point = inputPos(n, p);
            dl->AddCircleFilled(point, std::max(3.f, 4 * zoom), Audio);
            textAt(point + ImVec2(8 * zoom, -7 * zoom), Muted, def.inputs[p].name);
            float radius = std::max(6.f, 8 * zoom);
            bool hit =
                hovered && std::abs(mouse.x - point.x) < radius && std::abs(mouse.y - point.y) < radius;
            if (hit && ImGui::IsMouseClicked(0) && wireNode && !graph.WouldCycle(wireNode, n.id)) {
                if (n.inputs.size() <= size_t(p))
                    n.inputs.resize(p + 1);
                n.inputs[p] = {wireNode, wireOutput};
                wireNode = 0;
                changed = true;
            }
            if (hit && ImGui::IsMouseClicked(1)) {
                if (n.inputs.size() > size_t(p))
                    n.inputs[p] = {};
                changed = true;
            }
        }
        for (int p = 0; p < int(def.outputs.size()); ++p) {
            auto point = outputPos(n, p);
            dl->AddCircleFilled(point, std::max(3.f, 4 * zoom), Orange);
            float radius = std::max(6.f, 8 * zoom);
            if (hovered && std::abs(mouse.x - point.x) < radius && std::abs(mouse.y - point.y) < radius &&
                ImGui::IsMouseClicked(0)) {
                wireNode = n.id;
                wireOutput = p;
            }
        }
    }
    dl->PopClipRect();
    ImGui::EndChild();
    if (changed) {
        effect.graph = graph.ToJson();
        if (layerShaderNode) {
            auto draft = s.project.layerGraph;
            auto nodes = Json::Array();
            for (auto node : draft["nodes"].Elements()) {
                if (node["id"].U() == layerShaderNode) {
                    node["effect"]["graph"] = effect.graph;
                    node["effect"]["code"] = effectCode(effect);
                }
                nodes.Push(node);
            }
            draft["nodes"] = nodes;
            Json args = Json::Object();
            args["graph"] = draft;
            action("set_layer_graph", args);
        }
        s.dirty = true;
        draftIdentity.clear();
        if (!ImGui::IsMouseDragging(0))
            s.history.checkpoint(before);
    }
    ImGui::End();
}
void Editor::mixDetails() {
    auto *track = mixScope == "track" ? s.project.findTrack(mixTarget) : nullptr;
    auto *bus = mixScope == "bus" ? s.project.findBus(mixTarget) : nullptr;
    auto &mix = track ? track->mix : bus ? bus->mix : s.project.masterMix;
    auto args = [&] {
        Json a = Json::Object();
        a["scope"] = mixScope;
        a["target"] = mixTarget;
        return a;
    };
    if (track || bus) {
        std::string name = track ? track->name : bus->name;
        if (ImGui::InputText("Channel Name", &name, ImGuiInputTextFlags_EnterReturnsTrue)) {
            auto a = args();
            a["name"] = name;
            action("set_mix_channel", a);
            return;
        }
        if (bus && ImGui::SmallButton("Delete This Bus and Reconnect to Master")) {
            Json a = Json::Object();
            a["bus"] = mixTarget;
            action("remove_bus", a);
            return;
        }
    }
    auto parameter = [&](const char *label, const std::string &key, float low, float high, float fallback,
                         int index = -1) {
        auto &params = index < 0 ? mix.params : mix.effects[index].params;
        auto it = params.find(key);
        float value = it == params.end() ? fallback : it->second.at(s.time);
        bool animated = it != params.end() && !it->second.keys.empty();
        ImGui::PushID(index);
        ImGui::PushID(key.c_str());
        ImGui::TextDisabled("%s%s", label, animated ? " / ANIMATED" : "");
        auto before = s.project;
        ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x - 45));
        cydaw::v2::ui::KnobStyle knobStyle;
        knobStyle.diameter = 38;
        knobStyle.label = label;
        knobStyle.flags = low > 0 && high / low > 100 ? cydaw::v2::ui::knobLogarithmic
                          : low == -high              ? cydaw::v2::ui::knobBipolar
                                                      : 0;
        if (cydaw::v2::ui::knob("##value", &value, low, high, fallback, knobStyle)) {
            auto &p = params.try_emplace(key, Parameter{fallback}).first->second;
            if (animated)
                p.setKey({s.time, value, Curve::Smooth, 0});
            else
                p.value = value;
        }
        editCheckpoint(before);
        ImGui::SameLine();
        bool add = ImGui::Button("K", {30, 0});
        bool inspect = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        ImGui::PopID();
        ImGui::PopID();
        if (add || inspect) {
            mixAnimationKey = key;
            mixAnimationEffect = index;
        }
        if (add) {
            auto a = args();
            a["key"] = key;
            a["effect"] = index;
            a["time"] = s.time;
            a["value"] = value;
            a["curve"] = 1;
            action("set_mix_keyframe", a);
        }
        return add;
    };
    ImGui::TextWrapped(track ? "Clip + track pan -> DSP -> pre-fader sends -> gain -> output + post-fader sends"
                       : bus ? "DSP -> pre-fader sends -> gain/pan -> output + post-fader sends"
                             : "Sum -> DSP -> gain/pan -> stereo output");
    if (parameter("Gain dB", "gain", -80, 12, track ? track->gain : bus ? bus->gain : s.project.master))
        return;
    if (parameter("Panoramique", "pan", -1, 1, track ? track->pan : bus ? bus->pan : 0))
        return;
    if (track || bus) {
        auto before = s.project;
        auto &muted = track ? track->muted : bus->muted;
        ImGui::Checkbox("Mute", &muted);
        editCheckpoint(before);
        if (track) {
            ImGui::SameLine();
            before = s.project;
            ImGui::Checkbox("Solo", &track->solo);
            editCheckpoint(before);
        }
        auto destinationName = [&](Id id) {
            auto *b = s.project.findBus(id);
            return b ? b->name : "Master";
        };
        Id destination = mix.output;
        bool changed = false;
        if (ImGui::BeginCombo("Output", destinationName(destination).c_str())) {
            if (ImGui::Selectable("Master", !destination)) {
                destination = 0;
                changed = true;
            }
            for (auto &b : s.project.buses)
                if (b.id != mixTarget && ImGui::Selectable(b.name.c_str(), destination == b.id)) {
                    destination = b.id;
                    changed = true;
                }
            ImGui::EndCombo();
        }
        if (changed) {
            auto a = args();
            a["output"] = destination;
            action("set_mix_channel", a);
            return;
        }
        if (ImGui::CollapsingHeader("Sends", ImGuiTreeNodeFlags_DefaultOpen)) {
            Id add = 0;
            if (ImGui::BeginCombo("+ Send", "Choose a bus...")) {
                for (auto &b : s.project.buses)
                    if (b.id != mixTarget && ImGui::Selectable(b.name.c_str()))
                        add = b.id;
                ImGui::EndCombo();
            }
            if (add) {
                auto a = args();
                a["bus"] = add;
                action("set_send", a);
                return;
            }
            for (auto &send : mix.sends) {
                ImGui::PushID(int(send.bus));
                ImGui::TextColored(ImVec4(.25f, .7f, .72f, 1), "%s", destinationName(send.bus).c_str());
                before = s.project;
                ImGui::Checkbox("Enabled", &send.enabled);
                editCheckpoint(before);
                ImGui::SameLine();
                before = s.project;
                ImGui::Checkbox("Pre-fader", &send.preFader);
                editCheckpoint(before);
                before = s.project;
                ImGui::SetNextItemWidth(220);
                ImGui::SliderFloat("Level dB", &send.gain, -80, 12, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                editCheckpoint(before);
                ImGui::SameLine();
                bool remove = ImGui::SmallButton("Remove");
                if (parameter("Animated Send Level", "send." + std::to_string(send.bus), -80, 12,
                              send.gain)) {
                    ImGui::PopID();
                    return;
                }
                Id id = send.bus;
                ImGui::PopID();
                if (remove) {
                    auto a = args();
                    a["bus"] = id;
                    action("remove_send", a);
                    return;
                }
            }
        }
    }
    if (ImGui::CollapsingHeader("CyDAW 2 DSP stack", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (dspGraphEditor(args(), mix.effects))
            return;
        if (ImGui::Button("+ Native / VST3 plugin")) {
            auto path = dialog(false, L"Audio plugins\0*.dll;*.vst3\0All\0*.*\0\0", nullptr);
            if (!path.empty()) {
                auto a = args();
                a["path"] = path;
                a["kind"] = sx::Utf8Path(path).extension() == L".vst3" ? "vst3" : "native";
                action("add_external_audio", a);
            }
            return;
        }
        std::string id;
        if (ImGui::BeginCombo("+ DSP", "Add...")) {
            for (auto &d : cydaw::engine::effectCatalogue)
                if (!d.convertsChannels() && ImGui::Selectable(std::string(d.displayName).c_str()))
                    id = "cydaw." + std::string(d.typeKey);
            ImGui::EndCombo();
        }
        if (!id.empty()) {
            auto a = args();
            a["id"] = id;
            action("add_mix_effect", a);
            return;
        }
        for (int i = 0; i < int(mix.effects.size()); ++i) {
            auto &effect = mix.effects[i];
            auto *d = audioEffect(effect.id);
            ImGui::PushID(i);
            auto before = s.project;
            ImGui::Checkbox("##active", &effect.enabled);
            editCheckpoint(before);
            ImGui::SameLine();
            bool open = ImGui::TreeNode(std::string(d->displayName).c_str());
            bool edited = false;
            if (open) {
                auto pluginTarget = args();
                pluginTarget["effect"] = i;
                if (pluginControls(pluginTarget, effect)) {
                    ImGui::TreePop();
                    ImGui::PopID();
                    return;
                }
                int to = i;
                ImGui::BeginDisabled(i == 0);
                if (ImGui::SmallButton("Move Up"))
                    to = i - 1;
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(i + 1 == int(mix.effects.size()));
                if (ImGui::SmallButton("Move Down"))
                    to = i + 1;
                ImGui::EndDisabled();
                ImGui::SameLine();
                bool remove = ImGui::SmallButton("Remove DSP");
                if (remove || to != i) {
                    auto a = args();
                    a["effect"] = i;
                    a["position"] = to;
                    ImGui::TreePop();
                    ImGui::PopID();
                    action(remove ? "remove_mix_effect" : "set_mix_effect", a);
                    mixAnimationEffect = -1;
                    mixAnimationKey = "gain";
                    return;
                }
                for (auto &p : d->parameters)
                    if (parameter(std::string(p.key).c_str(), std::string(p.key), p.minimum, p.maximum,
                                  p.defaultValue, i)) {
                        edited = true;
                        break;
                    }
                if (externalAudio(effect.id))
                    for (auto &[key, value] : effect.params)
                        if (parameter(
                                (effect.parameterNames.contains(key) ? effect.parameterNames.at(key) : key)
                                    .c_str(),
                                key, 0, 1, value.value, i)) {
                            edited = true;
                            break;
                        }
                ImGui::TreePop();
            }
            ImGui::PopID();
            if (edited)
                return;
        }
    }
    if (ImGui::CollapsingHeader("Mix Automation", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (mixAnimationEffect >= int(mix.effects.size())) {
            mixAnimationEffect = -1;
            mixAnimationKey = "gain";
        }
        auto &params = mixAnimationEffect < 0 ? mix.params : mix.effects[mixAnimationEffect].params;
        ImGui::Text("%s / %s", mixAnimationEffect < 0 ? "Channel" : mix.effects[mixAnimationEffect].id.c_str(),
                    mixAnimationKey.c_str());
        ImGui::TextDisabled("K: add at current time; right-click K: show curve");
        auto it = params.find(mixAnimationKey);
        if (it != params.end() && !it->second.keys.empty()) {
            float curve[128];
            for (int n = 0; n < 128; ++n)
                curve[n] = it->second.at(s.project.duration * n / 127.);
            ImGui::PlotLines("##curve", curve, 128, 0, nullptr, FLT_MAX, FLT_MAX, {-1, 65});
            for (size_t n = 0; n < it->second.keys.size(); ++n) {
                auto key = it->second.keys[n];
                double oldTime = key.time;
                ImGui::PushID(int(n));
                ImGui::SetNextItemWidth(120);
                bool changed = ImGui::InputDouble("Time (s)", &key.time, 0, 0, "%.4f",
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SetNextItemWidth(120);
                changed |= ImGui::InputFloat("Value", &key.value, 0, 0, "%.4f",
                                             ImGuiInputTextFlags_EnterReturnsTrue);
                int interpolation = int(key.curve);
                ImGui::SetNextItemWidth(180);
                changed |= ImGui::Combo("Curve", &interpolation, "Linear\0Smooth\0Step\0CyDAW Curve\0");
                if (interpolation == int(Curve::Shaped)) {
                    ImGui::SetNextItemWidth(120);
                    changed |= ImGui::InputFloat("Shape (-1 to 1)", &key.shape, 0, 0, "%.3f",
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
                }
                bool remove = ImGui::SmallButton("Delete Keyframe");
                ImGui::PopID();
                if (changed || remove) {
                    auto a = args();
                    a["effect"] = mixAnimationEffect;
                    a["key"] = mixAnimationKey;
                    a["time"] = remove ? oldTime : key.time;
                    a["previous_time"] = oldTime;
                    a["value"] = key.value;
                    a["curve"] = interpolation;
                    a["shape"] = key.shape;
                    action(remove ? "remove_mix_keyframe" : "set_mix_keyframe", a);
                    return;
                }
                ImGui::Separator();
            }
        } else
            ImGui::TextDisabled("No keyframes; the base value is used.");
        if (ImGui::SmallButton("Restore Base Value")) {
            auto a = args();
            a["effect"] = mixAnimationEffect;
            a["key"] = mixAnimationKey;
            action("remove_mix_keyframe", a);
            return;
        }
    }
}
bool Editor::pluginControls(Json target, const Effect &effect) {
    if (effect.id != "vst3")
        return false;
    if (ImGui::Button("Open VST3")) {
        target["visible"] = true;
        action("show_plugin_editor", target);
        return true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Close Window")) {
        action("capture_plugin_state", target);
        target["visible"] = false;
        action("show_plugin_editor", target);
        return true;
    }
    if (ImGui::Button("Capture State")) {
        action("capture_plugin_state", target);
        return true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save preset")) {
        auto path = dialog(true, L"Preset VST3\0*.sfvstpreset\0\0", L"sfvstpreset");
        if (!path.empty()) {
            target["path"] = path;
            action("save_plugin_preset", target);
        }
        return true;
    }
    if (ImGui::Button("Load preset")) {
        auto path = dialog(false, L"Preset VST3\0*.sfvstpreset\0\0", L"sfvstpreset");
        if (!path.empty()) {
            target["path"] = path;
            action("load_plugin_preset", target);
        }
        return true;
    }
    ImGui::TextDisabled("Full state is embedded; automation curves take precedence.");
    return false;
}
bool Editor::dspGraphEditor(Json target, const std::vector<Effect> &chain) {
    bool applied = false;
    auto &draft = dspModalDraft;
    auto &index = dspModalIndex;
    auto &choices = dspModalChoices;
    auto identity = target.Dump(0);
    if (!choices.contains(identity)) {
        int first = -1;
        for (int i = 0; i < int(chain.size()); ++i)
            if (chain[i].id == "cydaw.graph") {
                first = i;
                break;
            }
        choices[identity] = first;
    }
    int &choice = choices[identity];
    if (choice >= int(chain.size()) || (choice >= 0 && chain[choice].id != "cydaw.graph"))
        choice = -1;
    auto label = choice < 0 ? "New Graph"
                            : chain[choice].graph["name"].S("DSP Graph") + " #" + std::to_string(choice + 1);
    if (ImGui::BeginCombo("Graph to Edit", label.c_str())) {
        if (ImGui::Selectable("New Graph", choice < 0))
            choice = -1;
        for (int i = 0; i < int(chain.size()); ++i)
            if (chain[i].id == "cydaw.graph") {
                auto name = chain[i].graph["name"].S("DSP Graph") + " #" + std::to_string(i + 1);
                if (ImGui::Selectable(name.c_str(), choice == i))
                    choice = i;
            }
        ImGui::EndCombo();
    }
    bool requested = openDspNext && target["scope"].S() == mixScope && target["target"].U() == mixTarget;
    if (ImGui::Button("DSP Graph: Edit / Import...") || requested) {
        openDspNext = false;
        index = choice;
        auto graph = index >= 0 ? chain[index].graph : Json::Object();
        if (index < 0) {
            std::string failure;
            Json::Parse("{\"schema\":2,\"nodes\":[],\"connections\":[{\"source\":0,\"destination\":0}]}",
                        graph, &failure);
        }
        dspCanvas.open(graph);
        draft = graph.Dump(2);
        ImGui::OpenPopup("DSP Editor v2");
    }
    ImGui::SetNextWindowSize({1040, 650}, ImGuiCond_Appearing);
    if (ImGui::BeginPopup("DSP Editor v2")) {
        ImGui::TextWrapped("0 = input/output; stable node IDs, cydaw.* processors, sf.sum summation. "
                           "Connections are validated by CyDAW v2.");
        if (ImGui::BeginTabBar("dspTabs")) {
            if (ImGui::BeginTabItem("Graph")) {
                if (dspCanvas.draw(s.time))
                    draft = dspCanvas.document().Dump(2);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("JSON")) {
                if (ImGui::InputTextMultiline("##dsp-json", &draft, {980, 420})) {
                    Json graph;
                    std::string failure;
                    if (Json::Parse(draft, graph, &failure)) {
                        try {
                            decodeDspGraph(graph);
                            dspCanvas.open(graph);
                        } catch (const std::exception &e) {
                            error = e.what();
                        }
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (ImGui::Button("Import JSON")) {
            auto path = dialog(false, L"DSP graph\0*.json\0\0", L"json");
            if (!path.empty()) {
                std::string text;
                if (!sx::ReadTextFile(path, text))
                    error = "Cannot read graph";
                else {
                    Json graph;
                    std::string failure;
                    if (Json::Parse(text, graph, &failure)) {
                        try {
                            decodeDspGraph(graph);
                            dspCanvas.open(graph);
                            draft = text;
                        } catch (const std::exception &e) {
                            error = e.what();
                        }
                    } else
                        error = failure;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Export JSON")) {
            auto path = dialog(true, L"DSP graph\0*.json\0\0", L"json");
            if (!path.empty())
                try {
                    AtomicFile output(path);
                    std::ofstream file(output.path(), std::ios::binary);
                    file << draft;
                    file.close();
                    if (!file)
                        throw std::runtime_error("Graph write failed");
                    output.commit();
                } catch (const std::exception &e) {
                    error = e.what();
                }
        }
        ImGui::SameLine();
        if (ImGui::Button("Compile and Apply")) {
            applied = true;
            Json graph;
            std::string failure;
            if (Json::Parse(draft, graph, &failure)) {
                target["graph"] = graph;
                if (index >= 0)
                    target["effect"] = index;
                action("set_dsp_graph", target);
                if (error.empty())
                    ImGui::CloseCurrentPopup();
            } else
                error = failure;
        }
        ImGui::EndPopup();
    }
    return applied;
}
void Editor::mixer() {
    if (!panels["Mixer & Audio Graph"] || !beginPanel("Mixer & Audio Graph"))
        return;
    if (ImGui::BeginTabBar("audioTabs")) {
        if (ImGui::BeginTabItem("Bandeaux CyDAW")) {
            ImGui::BeginChild("CyDAWMixerStrips", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
            bool first = true;
            auto strip = [&](const char *scope, Id id, const std::string &name, MixChannel &mix, float gain,
                             float pan, bool muted, bool solo, bool locked) {
                if (!first)
                    ImGui::SameLine();
                first = false;
                auto before = s.project;
                cydaw::v2::ui::MixerStrip data;
                data.name = name.c_str();
                auto *dest = s.project.findBus(mix.output);
                data.output = dest ? dest->name.c_str() : "MASTER";
                data.devices = int(mix.effects.size());
                data.selected = mixScope == scope && mixTarget == id;
                data.gainDb = mix.param("gain", s.time, gain);
                data.pan = mix.param("pan", s.time, pan);
                data.muted = muted;
                data.solo = solo;
                data.canMute = std::string(scope) != "master";
                data.canSolo = std::string(scope) == "track";
                auto peak = s.media.mixPeaks.find(id), rms = s.media.mixRms.find(id);
                if (peak != s.media.mixPeaks.end())
                    for (int k = 0; k < 2; ++k)
                        data.peak[k] = peak->second[k];
                if (rms != s.media.mixRms.end())
                    for (int k = 0; k < 2; ++k)
                        data.rms[k] = rms->second[k];
                bool picked = false;
                ImGui::BeginDisabled(locked);
                bool changed =
                    cydaw::v2::ui::mixerStrip((std::string(scope) + std::to_string(id)).c_str(), data, 96,
                                              std::max(160.f, ImGui::GetContentRegionAvail().y - 20), picked);
                ImGui::EndDisabled();
                if (picked) {
                    mixScope = scope;
                    mixTarget = id;
                }
                if (changed) {
                    auto parameter = [&](const char *key, float value, float fallback) {
                        auto &p = mix.params.try_emplace(key, Parameter{fallback}).first->second;
                        if (p.keys.empty())
                            p.value = value;
                        else
                            p.setKey({s.time, value, Curve::Smooth, 0});
                    };
                    parameter("gain", data.gainDb, gain);
                    parameter("pan", data.pan, pan);
                    if (std::string(scope) == "track") {
                        auto *t = s.project.findTrack(id);
                        t->muted = data.muted;
                        t->solo = data.solo;
                    } else if (std::string(scope) == "bus")
                        s.project.findBus(id)->muted = data.muted;
                    editCheckpoint(before);
                }
            };
            for (auto &t : s.project.tracks)
                if (t.kind == TrackKind::Audio)
                    strip("track", t.id, t.name, t.mix, t.gain, t.pan, t.muted, t.solo, t.locked);
            for (auto &b : s.project.buses)
                strip("bus", b.id, b.name, b.mix, b.gain, b.pan, b.muted, false, b.locked);
            strip("master", 0, "MASTER", s.project.masterMix, s.project.master, 0, false, false, false);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mixer", nullptr,
                                openDspNext ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
            if ((mixScope == "track" && (!s.project.findTrack(mixTarget) ||
                                         s.project.findTrack(mixTarget)->kind != TrackKind::Audio)) ||
                (mixScope == "bus" && !s.project.findBus(mixTarget))) {
                mixScope = "master";
                mixTarget = 0;
            }
            bool addBus = ImGui::Button("+ Stereo Bus");
            ImGui::SameLine();
            ImGui::TextDisabled("Select a channel to edit its sends, DSP and animation");
            ImGui::BeginChild("channels", {220, 0}, true);
            auto row = [&](const char *scope, Id id, const std::string &name) {
                ImGui::PushID(scope);
                ImGui::PushID(int(id));
                if (ImGui::Selectable(name.c_str(), mixScope == scope && mixTarget == id)) {
                    mixScope = scope;
                    mixTarget = id;
                    mixAnimationEffect = -1;
                    mixAnimationKey = "gain";
                }
                auto it = s.media.mixPeaks.find(id);
                float peak =
                    s.playing && it != s.media.mixPeaks.end() ? std::max(it->second[0], it->second[1]) : 0;
                char text[32];
                snprintf(text, sizeof(text), "%.1f dB%s", 20 * std::log10(std::max(peak, 1e-4f)),
                         peak > 1 ? " / CLIP" : "");
                ImGui::ProgressBar(std::min(peak, 1.f), {-1, 20}, text);
                // CyDAW v2 Widgets::meter: hold a rising peak for 1.5 seconds.
                auto *storage = ImGui::GetStateStorage();
                auto heldId = ImGui::GetID("peakHold"), timeId = ImGui::GetID("peakTime");
                float held = storage->GetFloat(heldId, 0);
                double stamp = storage->GetFloat(timeId, 0);
                if (peak >= held || ImGui::GetTime() - stamp > 1.5) {
                    held = peak;
                    storage->SetFloat(heldId, held);
                    storage->SetFloat(timeId, float(ImGui::GetTime()));
                }
                auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
                float marker = low.x + (high.x - low.x) * std::clamp(held, 0.f, 1.f);
                ImGui::GetWindowDrawList()->AddLine({marker, low.y}, {marker, high.y},
                                                    IM_COL32(255, 189, 110, 255), 2);
                auto rms = s.media.mixRms.find(id);
                if (rms != s.media.mixRms.end())
                    ImGui::TextDisabled(
                        "RMS %.1f dB",
                        20 * std::log10(std::max(1e-6f, std::max(rms->second[0], rms->second[1]))));
                ImGui::PopID();
                ImGui::PopID();
            };
            ImGui::SeparatorText("TRACKS");
            for (auto &track : s.project.tracks)
                if (track.kind == TrackKind::Audio)
                    row("track", track.id, track.name);
            ImGui::SeparatorText("BUS");
            for (auto &bus : s.project.buses)
                row("bus", bus.id, bus.name);
            ImGui::SeparatorText("OUTPUT");
            row("master", 0, "MASTER");
            ImGui::TextDisabled("M %.1f LUFS", s.media.loudness.momentary());
            ImGui::TextDisabled("S %.1f LUFS · TP %.1f dBTP", s.media.loudness.shortTerm(),
                                s.media.loudness.truePeak());
            ImGui::TextDisabled("I %.1f LUFS", s.media.loudness.integrated());
            ImGui::TextDisabled("Compensated Latency: %u samples", s.media.latencySamples);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("channelDetails", {0, 0}, true);
            auto *track = mixScope == "track" ? s.project.findTrack(mixTarget) : nullptr;
            auto *bus = mixScope == "bus" ? s.project.findBus(mixTarget) : nullptr;
            bool locked = track ? track->locked : bus ? bus->locked : false;
            std::string label = track ? track->name : bus ? bus->name : "MASTER / Stereo 48 kHz";
            ImGui::SeparatorText(label.c_str());
            bool toggleLock = (track || bus) && ImGui::Checkbox("Locked", &locked);
            if (toggleLock) {
                Json a = Json::Object();
                a["scope"] = mixScope;
                a["target"] = mixTarget;
                a["locked"] = locked;
                action("set_mix_channel", a);
            } else {
                ImGui::BeginDisabled(locked);
                mixDetails();
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
            if (addBus) {
                action("add_bus");
                if (error.empty()) {
                    mixScope = "bus";
                    mixTarget = s.project.buses.back().id;
                }
            }
        }
        if (ImGui::BeginTabItem("Signal")) {
            if (s.playing) {
                ImGui::PlotLines("Waveform", audio.analysis.waveform.data(), audio.analysis.waveformCount,
                                 0, nullptr, -1, 1, {-1, 80});
                float bins[128]{};
                cydaw::engine::computeLogSpectrum(audio.analysis, 20, 20000, bins, 128);
                ImGui::PlotHistogram("Spectre logarithmique", bins, 128, 0, nullptr, 0, 1, {-1, 95});
            } else
                ImGui::TextWrapped("Start playback to analyze the mix with CyDAW 2.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Graph")) {
            try {
                auto graph = audioGraph(s.project);
                ImGui::TextDisabled("CYDAW 2 / %zu nodes - %zu validated stereo cables", graph.nodes.size(),
                                    graph.connections.size());
                ImGui::BeginChild("routingGraph", {0, 0}, true, ImGuiWindowFlags_HorizontalScrollbar);
                std::map<uint64_t, int> depth;
                std::map<uint64_t, int> pending;
                std::map<uint64_t, std::vector<uint64_t>> downstream;
                for (auto &wire : graph.connections)
                    if (wire.source.nodeId && wire.destination.nodeId) {
                        ++pending[wire.destination.nodeId];
                        downstream[wire.source.nodeId].push_back(wire.destination.nodeId);
                    }
                std::vector<uint64_t> ready;
                for (auto &node : graph.nodes)
                    if (!pending[node.id])
                        ready.push_back(node.id);
                for (size_t index = 0; index < ready.size(); ++index)
                    for (auto destination : downstream[ready[index]]) {
                        depth[destination] = std::max(depth[destination], depth[ready[index]] + 1);
                        if (!--pending[destination])
                            ready.push_back(destination);
                    }
                std::map<int, int> rows;
                std::map<uint64_t, ImVec2> points;
                auto base = ImGui::GetCursorScreenPos();
                auto *dl = ImGui::GetWindowDrawList();
                float width = 0, height = 0;
                for (auto &node : graph.nodes) {
                    int column = depth[node.id], row = rows[column]++;
                    points[node.id] = base + ImVec2(column * 205.f, row * 70.f);
                    width = std::max(width, column * 205.f + 190);
                    height = std::max(height, row * 70.f + 55);
                }
                for (auto &wire : graph.connections)
                    if (points.contains(wire.source.nodeId) && points.contains(wire.destination.nodeId)) {
                        auto a = points[wire.source.nodeId] + ImVec2(190, 25),
                             b = points[wire.destination.nodeId] + ImVec2(0, 25);
                        dl->AddBezierCubic(a, a + ImVec2(50, 0), b - ImVec2(50, 0), b, Audio, 2);
                    }
                for (auto &node : graph.nodes) {
                    auto a = points[node.id];
                    dl->AddRectFilled(a, a + ImVec2(190, 50), IM_COL32(35, 42, 48, 255), 4);
                    dl->PushClipRect(a + ImVec2(4, 4), a + ImVec2(186, 46), true);
                    dl->AddText(a + ImVec2(8, 6), Orange, node.displayName.c_str());
                    dl->AddText(a + ImVec2(8, 27), Muted, node.processorKey.c_str());
                    dl->PopClipRect();
                }
                ImGui::Dummy({width, height});
                ImGui::EndChild();
            } catch (const std::exception &e) {
                ImGui::TextWrapped("%s", e.what());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
void Editor::integrations() {
    if (!panels["Plugins & MCP"] || !beginPanel("Plugins & MCP"))
        return;
    ImGui::SeparatorText("Plugins HLSL · API 1 / 2");
    ImGui::TextWrapped("Packages in plugins/ declare effects in plugin.json. Code is "
                       "embedded in the project for consistent rendering.");
    if (ImGui::Button("Reload Plugins"))
        s.renderer.library.scan(root);
    for (auto &d : s.renderer.library.definitions)
        if (d.origin.starts_with("Plugin"))
            ImGui::BulletText("%s", d.name.c_str());
    for (auto &e : s.renderer.library.diagnostics)
        ImGui::TextWrapped("%s", e.c_str());
    ImGui::SeparatorText("Native MCP · stdio");
    ImGui::TextWrapped(
        "Start: ShaderixFrame.exe --mcp\nThe MCP client and this process's window share the "
        "sequence. Editing, animation, graphs, effects and exports are available to tools.");
    ImGui::TextDisabled("Last Command: %s", s.lastCommand.empty() ? "—" : s.lastCommand.c_str());
    ImGui::SeparatorText("Sequence");
    auto before = s.project;
    ImGui::DragScalar("Duration (s)", ImGuiDataType_Double, &s.project.duration, .1f);
    s.project.duration = std::clamp(s.project.duration, 0.1, 86400.);
    editCheckpoint(before);
    before = s.project;
    ImGui::SliderInt("Images/s", &s.project.fps, 1, 120);
    editCheckpoint(before);
    ImGui::TextDisabled("%d × %d pixels", s.project.width, s.project.height);
    ImGui::TextWrapped("Shortcuts: Space play, S split, Delete remove, Ctrl+Z/Y undo/redo.");
    ImGui::End();
}
void Editor::recoveryPanel() {
    if (!recoveryScanned) {
        recoveryScanned = true;
        try {
            recoveries = s.recovery.list();
            recoveryVisible = !recoveries.empty();
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
    if (s.recovery.enabled() && s.dirty && ImGui::GetTime() >= nextRecovery) {
        nextRecovery = ImGui::GetTime() + 5;
        try {
            s.syncPluginEditors(true);
            s.recovery.capture(s.project, s.projectPath, s.time);
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
    if (!recoveryVisible)
        return;
    ImGui::SetNextWindowSize({600, 300}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Session Recovery", &recoveryVisible)) {
        ImGui::TextWrapped("Unsaved changes are preserved every 5 seconds and "
                           "on exit. Choose a session to resume.");
        ImGui::Separator();
        if (recoveries.empty())
            ImGui::TextDisabled("No interrupted sessions to recover.");
        for (auto &entry : recoveries) {
            ImGui::PushID(sx::Narrow(entry.file.wstring()).c_str());
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TextDisabled("%s", entry.originalPath.empty() ? "Unsaved Project"
                                                                 : entry.originalPath.c_str());
            bool restore = ImGui::Button("Resume"), discard = false;
            ImGui::SameLine();
            discard = ImGui::Button("Delete This Recovery");
            ImGui::PopID();
            if (restore || discard) {
                try {
                    if (restore) {
                        auto recovered = s.recovery.restore(entry);
                        s.syncPluginEditors(true);
                        s.pluginEditors.clear();
                        if (s.dirty)
                            s.recovery.preserve(s.project, s.projectPath, s.time);
                        s.history.clear();
                        s.project = std::move(recovered);
                        s.projectPath = entry.originalPath;
                        s.time = std::clamp(entry.time, 0., s.project.duration);
                        s.playing = false;
                        s.selectClip(0);
                        s.dirty = true;
                        s.recovery.capture(s.project, s.projectPath, s.time);
                        draftIdentity.clear();
                    }
                    s.recovery.discard(entry);
                    recoveries = s.recovery.list();
                    recoveryVisible = !recoveries.empty();
                } catch (const std::exception &e) {
                    error = e.what();
                }
                break;
            }
            ImGui::Separator();
        }
    }
    ImGui::End();
}
void Editor::exportPanel() {
    if (auto *timeline = ImGui::FindWindowByName(panelTitle("Timeline").c_str());
        timeline && timeline->DockId)
        ImGui::SetNextWindowDockID(timeline->DockId, ImGuiCond_FirstUseEver);
    if (exportsFocus)
        panels["Exports"] = true;
    if (!panels["Exports"])
        return;
    bool visible = ImGui::Begin(panelTitle("Exports").c_str(), &panels["Exports"]);
    if (exportsFocus) {
        ImGui::SetWindowFocus(panelTitle("Exports").c_str());
        exportsFocus = false;
    }
    if (visible) {
        ImGui::TextWrapped("Rendering uses the sequence and effects captured when the export starts. "
                           "You can continue editing or open another project.");
        auto &preset = exportPreset;
        auto &format = exportFormat;
        if (ImGui::BeginCombo("Preset", exportPresets()[preset].name.c_str())) {
            for (int i = 0; i < int(exportPresets().size()); ++i)
                if (ImGui::Selectable(exportPresets()[i].name.c_str(), preset == i))
                    preset = i;
            ImGui::EndCombo();
        }
        ImGui::Combo("Format", &format,
                     "MP4 H264 / AAC\0Stereo 48 kHz WAV\0PNG Sequence\0Loudness Report JSON\0RGB AVI / "
                     "Uncompressed PCM\0");
        if (format == 2) {
            int bits = s.project.imageFormat == "png8" ? 0 : s.project.imageFormat == "png16" ? 1 : 2;
            const char *formats[] = {"png8", "png16", "exr16"};
            if (ImGui::Combo("Image Precision", &bits,
                             "PNG 8-bit\0PNG 16-bit\0EXR half-float 16-bit HDR\0")) {
                Json a = Json::Object();
                a["imageFormat"] = formats[bits];
                action("set_color_management", a);
            }
        }
        // The work area drawn on the ruler can be the exported range.
        ImGui::BeginDisabled(!s.project.hasWorkArea());
        bool workOnly = exportWorkArea && s.project.hasWorkArea();
        if (ImGui::Checkbox("Work Area Only", &workOnly))
            exportWorkArea = workOnly;
        ImGui::EndDisabled();
        if (s.project.hasWorkArea()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s → %s", timecode(s.project.workIn, s.project.fps).c_str(),
                                timecode(s.project.workOut, s.project.fps).c_str());
        } else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Drag the top of the timeline ruler to define a work area");
        if (ImGui::Button("Add to Queue")) {
            auto path = dialog(true,
                               format == 0   ? L"MP4 Video\0*.mp4\0\0"
                               : format == 1 ? L"Audio WAV\0*.wav\0\0"
                               : format == 3 ? L"JSON report\0*.json\0\0"
                               : format == 4 ? L"AVI Video\0*.avi\0\0"
                                             : L"New Sequence Folder\0*.*\0\0",
                               format == 0   ? L"mp4"
                               : format == 1 ? L"wav"
                               : format == 3 ? L"json"
                               : format == 4 ? L"avi"
                                             : nullptr);
            if (!path.empty()) {
                Json args = Json::Object();
                args["path"] = path;
                args["kind"] = format == 0 || format == 4 ? "video"
                               : format == 1              ? "audio"
                               : format == 2              ? "images"
                                                          : "analysis";
                args["preset"] = exportPresets()[preset].id;
                if (workOnly)
                    args["range"] = "work";
                action("start_export", args);
            }
        }
        auto jobs = s.exports.list();
        if (jobs.empty())
            ImGui::TextDisabled("No exports. Choose File > Export MP4 or Export WAV Mix.");
        for (auto &job : jobs) {
            ImGui::PushID(int(job.id));
            ImGui::Separator();
            const char *state = job.state == ExportState::Queued       ? "Queued"
                                : job.state == ExportState::Running    ? "Rendering"
                                : job.state == ExportState::Cancelling ? "Cancelling"
                                : job.state == ExportState::Finalizing ? "Finalizing File"
                                : job.state == ExportState::Completed  ? "Completed"
                                : job.state == ExportState::Cancelled  ? "Canceled"
                                                                       : "Failed";
            ImGui::Text("#%u · %s · %s", job.id,
                        job.kind == ExportKind::Video
                            ? (job.json()["format"].S() == "avi" ? "AVI / RGB · PCM" : "MP4 / H264 · AAC")
                        : job.kind == ExportKind::Audio  ? "WAV / Stereo 48 kHz"
                        : job.kind == ExportKind::Images ? "PNG Sequence"
                                                         : "LUFS report",
                        state);
            ImGui::TextUnformatted(job.path.c_str());
            ImGui::TextDisabled("%s - %.2f s - %d x %d / %d fps - elapsed %.1f s", job.project.c_str(),
                                job.duration, job.width, job.height, job.fps, job.elapsed);
            ImGui::ProgressBar(job.progress, {-1, 18});
            if (job.cancellable() && ImGui::Button("Cancel This Export")) {
                Json args = Json::Object();
                args["job"] = job.id;
                action("cancel_export", args);
            }
            if (!job.error.empty())
                ImGui::TextColored(ImVec4(1, .65f, .3f, 1), "%s", job.error.c_str());
            ImGui::PopID();
        }
    }
    ImGui::End();
}
Editor *Editor::keyboardOwner() {
    auto *nav = GImGui->NavWindow;
    Editor *owner = this;
    if (!nav)
        return owner;
    std::string name = nav->Name;
    while (owner->clipWorkspace && name.find(owner->clipWorkspace->panelSuffix) != std::string::npos)
        owner = owner->clipWorkspace.get();
    return owner;
}
void Editor::handleShortcuts() {
    auto &io = ImGui::GetIO();
    auto *nav = GImGui->NavWindow;
    bool graph = nav && (std::string(nav->Name).find("graphCanvas") != std::string::npos ||
                         std::string(nav->Name).find("ShaderixelComposition") != std::string::npos ||
                         std::string(nav->Name).find("dspCanvas") != std::string::npos);
    bool localDsp = nav && std::string(nav->Name).find("dspCanvas") != std::string::npos;
    // The source monitor has its own playback, frame steps and in/out marks.
    bool source = nav && std::string(nav->Name).starts_with("Source");
    if (!io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        // Over the Composition image, Space also pans: playback then toggles on release, unless it panned.
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !source && io.KeyShift)
            playAround();
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !source && !io.KeyShift) {
            spaceOverView = compositionHovered;
            spacePanned = false;
            if (!spaceOverView)
                s.playing = !s.playing;
        }
        if (ImGui::IsKeyReleased(ImGuiKey_Space) && spaceOverView) {
            if (!spacePanned)
                s.playing = !s.playing;
            spaceOverView = false;
        }
        if (keymap.pressed("edit.split") && !graph && s.selected) {
            auto a = selectionArgs();
            a["time"] = s.time;
            action("split_clips", a);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !graph && s.selected) {
            auto a = selectionArgs();
            a["ripple"] = io.KeyShift;
            action("delete_clips", a);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && !graph && s.selected)
            action("duplicate_clips", selectionArgs());
        // Ctrl + arrows: previous / next animation key of the selected clips (of every clip without selection).
        if (!graph && io.KeyCtrl && !source &&
            (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))) {
            bool forward = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
            double best = forward ? 1e30 : -1e30;
            auto consider = [&](const std::map<std::string, Parameter> &params) {
                for (auto &[key, param] : params)
                    for (auto &k : param.keys)
                        if (forward ? k.time > s.time + 1e-6 && k.time < best : k.time < s.time - 1e-6 && k.time > best)
                            best = k.time;
            };
            for (auto &track : s.project.tracks)
                for (auto &clip : track.clips)
                    if (s.selection.empty() || std::find(s.selection.begin(), s.selection.end(), clip.id) != s.selection.end()) {
                        consider(clip.params);
                        for (auto &effect : clip.effects)
                            consider(effect.params);
                    }
            if (std::abs(best) < 1e29)
                s.time = std::clamp(best, 0., s.project.duration);
        }
        if (!graph && !io.KeyCtrl && !source) {
            double frame = 1. / s.project.fps;
            auto edit = [&](bool forward) {
                std::vector<double> points{0, s.project.duration};
                for (auto &track : s.project.tracks)
                    for (auto &clip : track.clips) {
                        points.push_back(clip.start);
                        points.push_back(clip.start + clip.duration);
                    }
                for (auto &marker : s.project.markers)
                    points.push_back(marker.time);
                double best = forward ? s.project.duration : 0;
                for (double point : points)
                    if (forward ? point > s.time + 1e-6 && point < best : point < s.time - 1e-6 && point > best)
                        best = point;
                return best;
            };
            double step = io.KeyShift ? 1 : frame;
            if (ImGui::IsKeyPressed(ImGuiKey_Home))
                s.time = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_End))
                s.time = s.project.duration;
            // Alt + arrows nudge the selected clips by one frame (Shift: ten); plain arrows move the playhead.
            bool nudge = io.KeyAlt && !s.selection.empty();
            for (auto [key, direction] : {std::pair{ImGuiKey_LeftArrow, -1.}, std::pair{ImGuiKey_RightArrow, 1.}})
                if (ImGui::IsKeyPressed(key)) {
                    if (nudge) {
                        double delta = direction * frame * (io.KeyShift ? 10 : 1), earliest = 1e18;
                        for (auto id : s.project.selection(s.selection, false))
                            if (auto *clip = s.project.findClip(id))
                                earliest = std::min(earliest, clip->start);
                        auto a = selectionArgs();
                        a["delta"] = std::max(delta, -earliest); // never before the sequence start
                        if (std::abs(a["delta"].Num()) > 1e-9)
                            action("move_clips", a);
                    } else
                        s.time = std::clamp(std::round((s.time + direction * step) / frame) * frame, 0.,
                                            s.project.duration);
                }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                s.time = edit(false);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                s.time = edit(true);
            if (keymap.pressed("loop.toggle", false))
                loop = !loop;
            if (keymap.pressed("shuttle.forward", false))
                shuttleStep(1);
            bool curves = nav && std::string(nav->Name).starts_with("Animation");
            if (keymap.pressed("shuttle.back", false))
                shuttleStep(-1);
            if (keymap.pressed("shuttle.stop", false) && !curves) {
                s.playing = false;
                shuttleRate = 1;
            }
            if (keymap.pressed("marker.add", false)) {
                Json a = Json::Object();
                a["time"] = s.time;
                action("add_marker", a);
            }
            if (keymap.pressed("area.in", false) || keymap.pressed("area.out", false))
                workAreaEdge(keymap.pressed("area.in", false));
        }
        if (io.KeyCtrl && !localDsp && ImGui::IsKeyPressed(ImGuiKey_Z) &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            action("undo");
        if (io.KeyCtrl && !localDsp && ImGui::IsKeyPressed(ImGuiKey_Y) &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            action("redo");
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))
            action("new_project");
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
            saveCommand();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
            openCommand();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I))
            importCommand();
        if (keymap.pressed("view.palette", false))
            openPalette();
    }
}
void Editor::draw() {
    if (panelSuffix.empty() && !modesReady)
        setMode(mode);
    panelSettings();
    try {
        s.syncPluginEditors();
    } catch (const std::exception &ex) {
        error = ex.what();
    }
    s.pollJobs();
    s.media.poll();
    for (auto it = pendingImports.begin(); it != pendingImports.end();) {
        try {
            if (s.media.get(it->path).loading) {
                ++it;
                continue;
            }
            Json args = Json::Object();
            args["path"] = it->path;
            args["start"] = it->time;
            args["proxy"] = autoProxies;
            if (action("import_media", args) && actionResult.Has("proxy_job"))
                say("Large video: preparing a 720p preview proxy");
        } catch (const std::exception &failure) {
            error = failure.what();
        }
        it = pendingImports.erase(it);
    }
    s.tidySelection();
    keyboardOwner()->handleShortcuts();
    bool sought = std::abs(s.time - lastTransport) > 1e-7;
    pollLoudness();
    pollAnalysis();
    if (playAroundEnd >= 0 && (!s.playing || s.time >= playAroundEnd - 1e-6)) {
        if (s.playing || s.time >= playAroundEnd - 1e-6)
            s.time = playAroundReturn;
        s.playing = false;
        playAroundEnd = -1;
        sought = true;
    }
    if (ImGui::GetTime() >= offlineChecked) {
        offlineChecked = ImGui::GetTime() + 3;
        offlineMedia = 0;
        for (auto &path : s.project.mediaPaths()) {
            std::error_code missing;
            offlineMedia += !std::filesystem::exists(sx::Utf8Path(path), missing);
        }
    }
    if (!s.playing)
        shuttleRate = 1;
    bool rateChanged = anchoredRate != shuttleRate;
    anchoredRate = shuttleRate;
    if (s.playing && shuttleRate != 1) {
        // Away from ×1 the picture follows the clock alone, backwards too; the sound is silent.
        if (!wasPlaying || sought || rateChanged) {
            audio.stop();
            shuttleOrigin = s.time;
            shuttleWall = ImGui::GetTime();
        }
        double next = shuttleOrigin + (ImGui::GetTime() - shuttleWall) * shuttleRate;
        s.time = std::clamp(next, 0., s.project.duration);
        if (next <= 0 || next >= s.project.duration)
            s.playing = false;
    } else if (s.playing) {
        if (!wasPlaying || sought || rateChanged) {
            s.media.resetAudio();
            audio.start(s.time);
            wallTime = ImGui::GetTime() - s.time;
        }
        try {
            audio.pump(s.project, s.media);
            s.time = audio.playing() ? audio.time() : ImGui::GetTime() - wallTime;
        } catch (const std::exception &e) {
            error = e.what();
            s.playing = false;
        }
        // The loop plays the work area when one is set, as CyDAW's loop range.
        bool area = loop && s.project.hasWorkArea();
        double end = area ? s.project.workOut : s.project.duration, begin = area ? s.project.workIn : 0;
        if (s.time >= end) {
            s.time = loop ? begin : s.project.duration;
            s.playing = loop;
            audio.stop();
            if (loop) {
                s.media.resetAudio();
                audio.start(begin);
                wallTime = ImGui::GetTime() - begin;
            }
            wasPlaying = false;
        }
    } else
        audio.stop();
    wasPlaying = s.playing;
    lastTransport = s.time;
    try {
        // A pre-rendered frame stands for the render during playback while the document matches it;
        // the signature is checked a few times a second and at once after an undoable change.
        cachedView = nullptr;
        if (!renderCache.empty()) {
            std::size_t history = s.history.undoSteps().size() * 4096 + s.history.redoSteps().size();
            if (ImGui::GetTime() >= renderCacheChecked || history != renderCacheHistory) {
                renderCacheChecked = ImGui::GetTime() + .5;
                renderCacheHistory = history;
                if (renderSignature() != renderCache.signature())
                    renderCache.clear();
            }
            if (!renderCache.empty() && s.playing && !gradeComparison && !gradeMatte && !gradePick)
                cachedView = renderCache.frame(s.time, s.renderer.gpu());
        }
        // 720p for the fitted view, more when the Composition view is zoomed in.
        float scale = previewScale();
        scale = std::clamp(std::max(scale, compositionDetail), .05f, 1.f);
        if (gradeComparison == 3) {
            if (!gradeBefore) {
                gradeBefore = std::make_unique<Renderer>();
                gradeBefore->initialize(s.renderer.gpu(), s.renderer.gpuContext());
                gradeBefore->library = s.renderer.library;
            }
            gradeBefore->render(s.project, s.media, s.time, unsigned(s.project.width * scale),
                                unsigned(s.project.height * scale), true);
        }
        s.renderer.captureGradeClip = gradePick ? s.selected : 0;
        s.renderer.captureGradeEffect = selectedEffect;
        if (!cachedView)
            s.renderer.render(s.project, s.media, s.time, unsigned(s.project.width * scale),
                              unsigned(s.project.height * scale), gradeComparison == 1,
                              gradeMatte ? s.selected : 0, selectedEffect);
        if (gradeScopesVisible && ImGui::GetTime() >= gradeScopesNext) {
            gradeScopes = videoScopes(s.renderer.readbackFloat(), s.project.toneMap, s.project.exposure);
            gradeScopesNext = ImGui::GetTime() + .25;
        }
    } catch (const std::exception &e) {
        error = e.what();
    }
    menu();
    modeBar();
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        constexpr ImGuiKey keys[] = {ImGuiKey_F1, ImGuiKey_F2, ImGuiKey_F3, ImGuiKey_F4, ImGuiKey_F5, ImGuiKey_F6};
        for (int m = 0; m < modeCount; ++m)
            if (ImGui::IsKeyPressed(keys[m], false))
                setMode(Mode(m));
    }
    auto *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize({vp->WorkSize.x, vp->WorkSize.y - 28});
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("Workspace", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);
    ImGuiID dock = ImGui::GetID("FrameDock");
    ImGui::DockSpace(dock);
    ImGui::PopStyleVar();
    if (!layoutReady) {
        layoutReady = true;
        bool restored = layoutRestore && !layoutForce && restoreModeLayout(dock, vp->WorkSize);
        if (!restored &&
            (layoutRestore || layoutForce || !ImGui::DockBuilderGetNode(dock) || !ImGui::DockBuilderGetNode(dock)->IsSplitNode()))
            buildLayout(dock, vp->WorkSize);
        layoutForce = layoutRestore = false;
    }
    ImGui::End();
    clipEditor();
    motionPanel();
    shaderixelLibrary();
    dspPanel();
    library();
    preview();
    sourcePanel();
    markersPanel();
    historyPanel();
    subtitlesPanel();
    anglesPanel();
    shortcutsPanel();
    if (paletteOpen)
        commandPalette();
    layers();
    inspector();
    timeline();
    animations();
    shaderEditor();
    graphEditor();
    layerGraphEditor();
    gradingPanel();
    scopesPanel();
    mixer();
    integrations();
    recoveryPanel();
    exportPanel();
    if (!pendingPanelFocus.empty()) {
        ImGui::SetWindowFocus(pendingPanelFocus.c_str());
        pendingPanelFocus.clear();
    }
    if (panelSuffix.empty())
        trackDetachedPanels();
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - 28});
    ImGui::SetNextWindowSize({vp->WorkSize.x, 28});
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::Begin("status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    auto jobs = s.exports.list();
    if (s.media.pending()) {
        ImGui::TextDisabled("Preparing media...");
        ImGui::SameLine();
    }
    auto active = std::find_if(jobs.begin(), jobs.end(), [](const auto &job) { return job.active(); });
    if (active != jobs.end()) {
        auto &job = *active;
        ImGui::ProgressBar(job.progress, {150, 16});
        ImGui::SameLine();
        ImGui::BeginDisabled(!job.cancellable());
        if (ImGui::SmallButton("Cancel Export")) {
            Json args = Json::Object();
            args["job"] = job.id;
            action("cancel_export", args);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (analysis && !analysis->finished) {
        float progress = analysis->progress;
        ImGui::ProgressBar(progress >= 0 ? progress : -float(ImGui::GetTime()), {150, 16}, analysis->label.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel Analysis"))
            analysis->cancelled = true;
        ImGui::SameLine();
    }
    if (loudnessJob && !loudnessJob->finished) {
        ImGui::ProgressBar(loudnessJob->progress, {150, 16}, "Loudness");
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel Measurement"))
            loudnessJob->cancelled = true;
        ImGui::SameLine();
    }
    if (!error.empty()) {
        ImGui::TextColored(ImVec4(1, .65f, .3f, 1), "%s", error.c_str());
    } else if (!notice.empty() && ImGui::GetTime() < noticeUntil) {
        ImGui::TextColored(ImVec4(.36f, .82f, .43f, 1), "%s", notice.c_str());
    } else {
        ImGui::TextDisabled("%s / %zu tracks / HLSL GPU / playback workspace",
                            s.project.name.c_str(), s.project.tracks.size());
    }
    ImGui::End();
}
} // namespace sf

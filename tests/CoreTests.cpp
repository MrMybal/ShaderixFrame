#include "core/CanvasView.h"
#include "mcp/Protocol.h"
#include <cydaw/graph/GraphDefinition.h>
#include <fstream>
#include <iostream>
#include <mfapi.h>

static int checks = 0;
static void require(bool condition, const char *message) {
    ++checks;
    std::cout << message << std::endl;
    if (!condition)
        throw std::runtime_error(message);
}
static void groupEditingTests() {
    sf::Project base;
    base.duration = 12;
    base.addTrack(sf::TrackKind::Video);
    base.addTrack(sf::TrackKind::Audio);
    auto add = [&](int track, double start, double length) {
        sf::Clip c;
        c.id = base.allocate();
        c.start = start;
        c.duration = length;
        c.in = .5;
        c.speed = 1.5;
        c.params["fixture"].setKey({start + .5, .6f});
        sf::Effect effect;
        effect.params["fixture"].setKey({start + 1, .2f});
        c.effects.push_back(effect);
        base.tracks[track].clips.push_back(c);
        return c.id;
    };
    auto a = add(0, 1, 2), sound = add(1, 1, 2), b = add(0, 6, 2);
    auto between = add(0, 4, 1), later = add(1, 9, 2);
    base.linkClips(a, sound);
    base.validate();
    require(base.selection({a, sound, a, b}).size() == 3, "Selection expands and deduplicates linked clips");
    auto work = base;
    work.moveClips({sound, a, b}, 2);
    require(work.findClip(a)->start == 3 && work.findClip(sound)->start == 3 &&
                work.findClip(b)->start == 8 && work.findClip(between)->start == 4 &&
                work.findClip(a)->in == .5 && work.findClip(a)->params["fixture"].keys[0].time == 3.5 &&
                work.findClip(sound)->effects[0].params["fixture"].keys[0].time == 4,
            "Batch move preserves source offsets, gaps and both animation stacks");
    auto unchangedOnError = [&](auto operation, const char *message) {
        auto before = work.json().Dump(0);
        bool rejected = false;
        try {
            operation();
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected && work.json().Dump(0) == before, message);
    };
    unchangedOnError([&] { work.moveClips({a, b}, -4); }, "Negative batch move rolls back all clips");
    work.tracks[1].locked = true;
    require(work.selection({a}, false).size() == 2, "Locked clips remain selectable");
    unchangedOnError([&] { work.moveClips({b, a}, 1); }, "Locked linked member blocks the whole batch");
    work = base;
    auto copies = work.duplicateClips({a, sound, b});
    require(copies.size() == 3 && work.findClip(copies[0])->start == 8 &&
                work.findClip(copies[1])->start == 8 && work.findClip(copies[2])->start == 13 &&
                work.findClip(copies[0])->link == work.findClip(copies[1])->link &&
                work.findClip(copies[0])->link != base.findClip(a)->link &&
                work.findClip(copies[2])->link == 0 && work.duration == 15 &&
                work.findClip(copies[0])->params["fixture"].keys[0].time == 8.5,
            "Batch duplicate preserves arrangement and creates independent link groups");
    work = base;
    work.splitClips({a, sound, b}, 2);
    auto right = work.tracks[0].clips.back().id;
    require(work.findClip(a)->duration == 1 && work.findClip(sound)->duration == 1 &&
                work.findClip(b)->duration == 2 && work.findClip(right)->in == 2 &&
                work.linked(right).size() == 2 && work.findClip(right)->params["fixture"].keys[0].time == 1.5,
            "Batch split cuts linked clips once and ignores clips outside the playhead");
    work = base;
    work.findClip(sound)->start = 3;
    unchangedOnError([&] { work.splitClips({a, b}, 2); }, "Misaligned linked split remains atomic");
    work = base;
    unchangedOnError([&] { work.splitClips({a, b}, 5); }, "Batch split rejects a playhead in a gap");
    work = base;
    work.removeClips({a, sound, b}, true);
    require(!work.findClip(a) && !work.findClip(sound) && !work.findClip(b) &&
                work.findClip(between)->start == 2 && work.findClip(later)->start == 5 &&
                work.duration == 8 && work.findClip(later)->in == .5 &&
                work.findClip(later)->params["fixture"].keys[0].time == 5.5 &&
                work.findClip(later)->effects[0].params["fixture"].keys[0].time == 6,
            "Ripple closes disjoint interval union once and carries later keyframes");
    work = base;
    work.findClip(b)->start = 2;
    work.removeClips({a, b}, true);
    require(work.findClip(later)->start == 6 && work.duration == 9,
            "Overlapping removal intervals close their union, not their total lengths");
    work = base;
    work.findClip(between)->start = 0;
    work.findClip(between)->duration = 5;
    unchangedOnError([&] { work.removeClips({a}, true); }, "Ripple refuses to destroy a crossing survivor");
    work = base;
    work.tracks[1].locked = true;
    unchangedOnError([&] { work.removeClips({b}, true); }, "Ripple refuses to move a locked later track");
    work = base;
    work.linkClips(between, later);
    unchangedOnError([&] { work.removeClips({b}, true); }, "Ripple preserves offsets of linked survivors");
    work = base;
    work.removeClips({a}, false);
    require(work.findClip(later)->start == 9 && work.duration == 12,
            "Normal deletion leaves sequence and survivor timing unchanged");
    work = base;
    work.duration = 7;
    work.tracks[0].clips.clear();
    work.tracks[1].clips.clear();
    sf::Clip beyond;
    beyond.id = work.allocate();
    beyond.start = 6;
    beyond.duration = 3;
    work.tracks[0].clips.push_back(beyond);
    work.removeClips({beyond.id}, true);
    require(work.duration == 6, "Ripple clips removed time to the export sequence");
    work = base;
    std::vector<sf::Id> all;
    for (auto &t : work.tracks)
        for (auto &c : t.clips)
            all.push_back(c.id);
    work.removeClips(all, true);
    require(work.duration == 5, "Deleting all clips preserves unselected empty time");
    sf::CanvasView view;
    view.pan = {-150, 90};
    view.zoom = .8f;
    auto screen = view.screen({120, -30}), round = view.world(screen);
    require(std::abs(round.x - 120) < 1e-4 && std::abs(round.y + 30) < 1e-4,
            "Graph view transform round trip");
    auto anchor = view.world({250, 100});
    view.zoomAt({250, 100}, 2);
    auto anchored = view.screen(anchor);
    require(std::abs(anchored.x - 250) < 1e-4 && std::abs(anchored.y - 100) < 1e-4,
            "Graph zoom keeps the pointer's world position fixed");
    view.fit({-30, -20}, {570, 280}, {800, 400});
    auto low = view.screen({-30, -20}), high = view.screen({570, 280});
    require(low.x >= 24 && low.y >= 24 && high.x <= 776 && high.y <= 376,
            "Graph fit frames nodes with a pixel margin");
}
int main(int argc, char **argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    struct RuntimeGuard {
        ~RuntimeGuard() {
            MFShutdown();
            CoUninitialize();
        }
    } guard;
    try {
        groupEditingTests();
        sf::Parameter param;
        param.setKey({0, 0, sf::Curve::Linear, 0});
        param.setKey({2, 1, sf::Curve::Hold, 0});
        require(std::abs(param.at(1) - .5) < 1e-6, "Linear interpolation");
        param.keys[0].curve = sf::Curve::Hold;
        require(param.at(1) == 0, "Hold interpolation");
        param.setKey({2, 2});
        require(param.keys.size() == 2 && param.at(2) == 2, "Replace duplicate key");
        auto p = sf::Project::demo();
        p.validate();
        auto id = p.tracks[0].clips[0].id;
        p.split(id, 4);
        require(p.tracks[0].clips.size() == 2, "Split count");
        require(p.tracks[0].clips[0].duration == 4 && p.tracks[0].clips[1].in == 4 &&
                    p.tracks[0].clips[1].start == 4,
                "Split preserves source timing");
        auto round = sf::Project::fromJson(p.json());
        require(round.json().Dump(0) == p.json().Dump(0), "Project round trip");
        sf::History history;
        history.checkpoint(p);
        p.remove(id);
        require(history.undo(p) && p.findClip(id), "Undo");
        require(history.redo(p) && !p.findClip(id), "Redo");
        auto bad = round.json();
        bad["schema"] = 99;
        bool rejected = false;
        try {
            sf::Project::fromJson(bad);
        } catch (...) {
            rejected = true;
        }
        require(rejected, "Reject future schema");
        bad = round.json();
        bad["width"] = 123;
        rejected = false;
        try {
            sf::Project::fromJson(bad);
        } catch (...) {
            rejected = true;
        }
        require(rejected, "Reject odd dimensions");
        auto legacy = sf::Project::demo().json();
        legacy["schema"] = 1;
        auto migrated = sf::Project::fromJson(legacy);
        require(migrated.json()["schema"].I() == 10 && !migrated.findClip(id)->link &&
                    !migrated.findClip(id)->title.enabled,
                "Load schema 1 and migrate to schema 10");
        legacy["schema"] = 2;
        require(sf::Project::fromJson(legacy).findClip(id)->mask.shape == sf::MaskShape::None,
                "Load schema 2 with neutral title and mask defaults");
        auto linked = sf::Project::demo();
        auto videoId = linked.tracks[0].clips[0].id;
        auto audioId = linked.allocate();
        sf::Clip linkedSound;
        linkedSound.id = audioId;
        linkedSound.duration = 12;
        linked.tracks[1].clips.push_back(linkedSound);
        linked.findClip(videoId)->params["opacity"].setKey({1, .2f});
        linked.findClip(videoId)->params["opacity"].setKey({5, .8f});
        linked.findClip(videoId)->effects[0].params["exposure"].setKey({2, .3f});
        linked.linkClips(videoId, audioId);
        linked.setTiming(videoId, 3, 12, 0, 1);
        require(linked.findClip(audioId)->start == 3 &&
                    linked.findClip(videoId)->params["opacity"].keys[0].time == 4 &&
                    linked.findClip(videoId)->effects[0].params["exposure"].keys[0].time == 5,
                "Linked move carries clip and effect animations");
        linked.setTiming(videoId, 4, 11, 1, 1, false);
        require(linked.findClip(audioId)->start == 4 && linked.findClip(audioId)->in == 1 &&
                    linked.findClip(videoId)->params["opacity"].keys[0].time == 4,
                "Linked left trim preserves animation timing");
        linked.tracks[1].locked = true;
        auto frozen = linked.json().Dump(0);
        rejected = false;
        try {
            linked.remove(videoId);
        } catch (...) {
            rejected = true;
        }
        require(rejected && linked.json().Dump(0) == frozen, "Locked linked track blocks the entire edit");
        linked.tracks[1].locked = false;
        linked.findClip(videoId)->fadeIn = 2;
        linked.findClip(videoId)->fadeOut = 2;
        linked.split(videoId, 7);
        auto rightId = linked.tracks[0].clips.back().id;
        require(linked.tracks[1].clips.size() == 2 &&
                    linked.findClip(rightId)->link != linked.findClip(videoId)->link &&
                    linked.findClip(rightId)->fadeIn == 0 && linked.findClip(videoId)->fadeOut == 0,
                "Linked split creates independent groups and keeps only exterior fades");
        linked.setTiming(rightId, 0, 8, 4, 1);
        require(linked.findClip(rightId)->params["opacity"].keys[0].time == -3 &&
                    sf::Project::fromJson(linked.json()).findClip(rightId)->params["opacity"].keys[0].time ==
                        -3,
                "Moving a split left preserves keys outside the visible sequence");
        auto copiedId = linked.duplicate(rightId);
        require(linked.linked(copiedId).size() == 2 && linked.findClip(copiedId)->start == 8 &&
                    linked.findClip(copiedId)->params["opacity"].keys[0].time == 5,
                "Duplicate carries linked media and keyframes");
        linked.unlink(copiedId);
        require(linked.linked(copiedId).size() == 1, "Unlink allows independent edits");
        auto fade = *linked.findClip(videoId);
        require(fade.envelope(4) == 0 && fade.envelope(5) == .5f && fade.envelope(6) == 1 &&
                    fade.envelope(7) == 0,
                "Fade envelope and clip boundaries");
        sf::Session s;
        s.renderer.initialize();
        s.renderer.library.scan(sx::Utf8Path(argv[1]));
        require(s.renderer.library.definitions.size() == 325, "Builtin and plugin catalogue");
        for (auto &d : s.renderer.library.definitions)
            s.renderer.validateShader(d.code);
        auto graph = sx::EffectGraph::Default();
        sf::Effect ge;
        ge.graph = graph.ToJson();
        s.renderer.validateShader(sf::effectCode(ge));
        require(graph.WouldCycle(graph.OutputNode(), graph.nodes[0].id), "Shaderixel graph cycle detection");
        s.project = sf::Project::demo();
        s.project.width = 320;
        s.project.height = 180;
        s.project.duration = .2;
        s.renderer.render(s.project, s.media, .1, 320, 180);
        auto image = s.renderer.readback();
        require(image.pixels.size() == 320 * 180 * 4, "GPU frame size");
        bool colored = false;
        for (size_t i = 0; i < image.pixels.size(); i += 4)
            if (image.pixels[i] || image.pixels[i + 1] || image.pixels[i + 2])
                colored = true;
        require(colored, "GPU renders actual pixels");
        sf::Clip title;
        title.id = 900;
        title.title.enabled = true;
        title.title.text = "Été · VFX\n你好 & montage";
        title.params["font_size"] = {30};
        title.params["text_g"] = {.25f};
        title.params["text_b"] = {.1f};
        sf::TitleRasterizer textRenderer;
        auto titleImage = textRenderer.at(title, .1, 320, 180, 180);
        int visible = 0, partial = 0;
        bool correctColor = false;
        for (size_t i = 0; i < titleImage.pixels.size(); i += 4) {
            auto a = titleImage.pixels[i + 3];
            visible += a > 0;
            partial += a > 0 && a < 255;
            if (a > 240)
                correctColor |= titleImage.pixels[i + 2] > 240 && titleImage.pixels[i + 1] < 80 &&
                                titleImage.pixels[i] < 40;
        }
        require(visible > 500 && partial > 100 && titleImage.pixels[3] == 0 && correctColor,
                "Unicode multiline title has transparent, antialiased, straight-alpha pixels");
        auto halfTitle = textRenderer.at(title, .1, 160, 90, 180);
        int halfVisible = 0;
        for (size_t i = 3; i < halfTitle.pixels.size(); i += 4)
            halfVisible += halfTitle.pixels[i] > 0;
        require(halfTitle.width == 160 && halfVisible > visible * .15 && halfVisible < visible * .45,
                "Title size scales consistently between preview and export");
        title.title.text.clear();
        auto emptyTitle = textRenderer.at(title, .1, 320, 180, 180);
        bool empty = true;
        for (size_t i = 3; i < emptyTitle.pixels.size(); i += 4)
            empty &= emptyTitle.pixels[i] == 0;
        require(empty, "Changing title text invalidates the raster cache");
        title.title.text = "VFX";
        title.params["text_r"].setKey({0, 0});
        title.params["text_r"].setKey({1, 1});
        auto earlyTitle = textRenderer.at(title, 0, 320, 180, 180);
        auto lateTitle = textRenderer.at(title, 1, 320, 180, 180);
        bool animatedColor = false;
        for (size_t i = 0; i < lateTitle.pixels.size(); i += 4)
            if (lateTitle.pixels[i + 3] > 240)
                animatedColor |= lateTitle.pixels[i + 2] > 240 && earlyTitle.pixels[i + 2] == 0;
        require(animatedColor, "Animated text color invalidates and updates the raster cache");
        title.params["text_r"].keys.clear();
        title.params["text_r"].value = 1;
        auto titled = s.project;
        title.id = titled.allocate();
        title.title.text = "Été · VFX";
        title.params["font_size"].value = 36;
        titled.addTrack(sf::TrackKind::Video).clips.push_back(title);
        std::rotate(titled.tracks.begin(), titled.tracks.end() - 1, titled.tracks.end());
        titled.validate();
        require(sf::Project::fromJson(titled.json()).findClip(title.id)->title.text == title.title.text,
                "Title style round trip");
        s.renderer.render(titled, s.media, .1, 320, 180);
        auto titleComposite = s.renderer.readback();
        int modified = 0;
        for (size_t i = 0; i < image.pixels.size(); i += 4)
            modified += titleComposite.pixels[i + 2] > image.pixels[i + 2] + 30;
        require(modified > 200 && titleComposite.pixels[0] == image.pixels[0],
                "Title overlays the video while retaining the transparent background");
        auto masked = s.project;
        masked.duration = 1;
        auto &maskClip = masked.tracks[0].clips[0];
        sf::Effect white;
        white.code = "float4 Effect(EffectContext ctx){return float4(1,1,1,1);}";
        maskClip.effects = {white};
        maskClip.mask.shape = sf::MaskShape::Rectangle;
        maskClip.params["mask_width"] = {.5f};
        maskClip.params["mask_height"] = {.5f};
        auto pixel = [](const sf::Image &frame, unsigned x, unsigned y) {
            return frame.pixels[(size_t(y) * frame.width + x) * 4 + 2];
        };
        s.renderer.render(masked, s.media, .1, 320, 180);
        auto rectangle = s.renderer.readback();
        require(pixel(rectangle, 160, 90) == 255 && pixel(rectangle, 10, 90) == 0 &&
                    pixel(rectangle, 90, 50) == 255,
                "Rectangle mask clips rendered pixels");
        maskClip.mask.shape = sf::MaskShape::Ellipse;
        s.renderer.render(masked, s.media, .1, 320, 180);
        auto ellipse = s.renderer.readback();
        require(pixel(ellipse, 160, 90) == 255 && pixel(ellipse, 90, 50) == 0,
                "Ellipse mask uses curved boundaries");
        maskClip.mask.inverted = true;
        s.renderer.render(masked, s.media, .1, 320, 180);
        auto inverted = s.renderer.readback();
        require(pixel(inverted, 160, 90) == 0 && pixel(inverted, 10, 90) == 255,
                "Inverted mask reveals the exterior");
        maskClip.mask.inverted = false;
        maskClip.mask.shape = sf::MaskShape::Rectangle;
        maskClip.params["mask_feather"] = {20};
        s.renderer.render(masked, s.media, .1, 320, 180);
        auto feathered = s.renderer.readback();
        require(pixel(rectangle, 74, 90) == 0 && pixel(feathered, 74, 90) > 0 &&
                    pixel(feathered, 74, 90) < 255,
                "Mask feather produces a gradual alpha edge");
        maskClip.params["mask_feather"].value = 0;
        maskClip.params["mask_width"].value = .8f;
        maskClip.params["mask_height"].value = .2f;
        maskClip.params["mask_rotation"] = {90};
        s.renderer.render(masked, s.media, .1, 320, 180);
        require(pixel(s.renderer.readback(), 160, 40) == 255,
                "Mask rotation respects the frame aspect ratio");
        maskClip.params["mask_rotation"].value = 0;
        maskClip.params["mask_width"].value = .2f;
        maskClip.params["mask_x"].setKey({0, -.3f});
        maskClip.params["mask_x"].setKey({1, .3f});
        s.renderer.render(masked, s.media, .25, 320, 180);
        auto earlyMask = s.renderer.readback();
        s.renderer.render(masked, s.media, .75, 320, 180);
        require(pixel(earlyMask, 112, 90) == 255 && pixel(s.renderer.readback(), 112, 90) == 0,
                "Mask animation changes visible pixels over time");
        require(sf::Project::fromJson(masked.json()).findClip(maskClip.id)->mask.shape ==
                    sf::MaskShape::Rectangle,
                "Mask and animations round trip");
        auto composition = s.project;
        composition.tracks[0].clips[0].params["opacity"].value = 0;
        s.renderer.render(composition, s.media, .1, 320, 180);
        auto transparent = s.renderer.readback();
        bool black = true;
        for (size_t i = 0; i < transparent.pixels.size(); i += 4)
            black &= transparent.pixels[i] == 0 && transparent.pixels[i + 1] == 0 &&
                     transparent.pixels[i + 2] == 0;
        require(black, "Video opacity affects GPU composition");
        composition.tracks[0].clips[0].params["opacity"].value = 1;
        composition.tracks[0].clips[0].fadeIn = 1;
        s.renderer.render(composition, s.media, 0, 320, 180);
        auto faded = s.renderer.readback();
        black = true;
        for (size_t i = 0; i < faded.pixels.size(); i += 4)
            black &= faded.pixels[i] == 0 && faded.pixels[i + 1] == 0 && faded.pixels[i + 2] == 0;
        require(black, "Video fade affects rendered GPU pixels");
        composition.tracks[0].clips[0].fadeIn = 0;
        composition.tracks[0].clips[0].params["opacity"].value = 1;
        composition.tracks[0].clips[0].params["blend"].value = 2;
        s.renderer.render(composition, s.media, .1, 320, 180);
        auto multiply = s.renderer.readback();
        black = true;
        for (size_t i = 0; i < multiply.pixels.size(); i += 4)
            black &= multiply.pixels[i] == 0 && multiply.pixels[i + 1] == 0 && multiply.pixels[i + 2] == 0;
        require(black, "Multiply blend uses the underlying layer");
        auto sandbox = std::filesystem::current_path() / "test-output";
        std::filesystem::create_directories(sandbox);
        sf::Recovery recovery;
        auto recoveryDirectory = sandbox / ("recovery-" + std::to_string(GetTickCount64()));
        recovery.configure(recoveryDirectory);
        recovery.capture(linked, "original.sframe", 2);
        require(recovery.list().empty(), "Recovery ignores sessions owned by a live process");
        std::filesystem::path recoveryFile;
        for (auto &entry : std::filesystem::directory_iterator(recoveryDirectory))
            if (entry.path().extension() == L".sframe")
                recoveryFile = entry.path();
        auto recoveredJson = linked.json();
        recoveredJson["recovery"] = sf::Json::Object();
        recoveredJson["recovery"]["path"] = "original.sframe";
        recoveredJson["recovery"]["time"] = 2;
        recoveredJson["recovery"]["processId"] = 0;
        {
            std::ofstream file(recoveryFile, std::ios::binary);
            file << recoveredJson.Dump(2);
        }
        auto entries = recovery.list();
        require(entries.size() == 1 && entries[0].originalPath == "original.sframe" &&
                    recovery.restore(entries[0]).json().Dump(0) == linked.json().Dump(0),
                "Recover unsaved document, linked clips and source project path");
        recovery.discard(entries[0]);
        require(recovery.list().empty(), "Explicit recovery discard");
        auto png = sx::Narrow((sandbox / "frame.png").wstring());
        auto titleMp4 = sx::Narrow((sandbox / "title.mp4").wstring());
        sf::exportVideo(titled, s.media, s.renderer, titleMp4);
        sf::MediaAsset titleVideo;
        titleVideo.open(titleMp4);
        auto exportedTitle = titleVideo.at(.1);
        int exportedLetters = 0;
        for (size_t i = 0; i < image.pixels.size(); i += 4)
            exportedLetters += exportedTitle.pixels[i + 2] > image.pixels[i + 2] + 40;
        require(exportedLetters > 150, "MP4 export retains the composited title");
        sf::writePng(image, png);
        sf::MediaAsset still;
        still.open(png);
        require(still.frame.width == 320 && still.image, "WIC PNG round trip");
        auto wav = sx::Narrow((sandbox / "silence.wav").wstring());
        sf::exportWav(s.project, s.media, wav);
        sf::MediaAsset audio;
        audio.open(wav);
        require(audio.hasAudio && audio.pcm.size() > 18000, "WAV export and decode");
        auto &tone = s.media.get(wav);
        for (size_t i = 0; i < tone.pcm.size(); ++i)
            tone.pcm[i] = float(.5 * std::sin((i / 2) * .04));
        auto &track = s.project.addTrack(sf::TrackKind::Audio);
        sf::Clip sound;
        sound.id = s.project.allocate();
        sound.source = wav;
        sound.duration = .2;
        track.clips.push_back(sound);
        auto &clip = track.clips.back();
        auto ag = sf::audioGraph(s.project);
        require(ag.nodes.size() == 4 && ag.connections.size() == 6, "CyDAW typed audio graph validation");
        std::vector<float> clean;
        s.media.mix(s.project, 0, 1024, clean);
        clip.fadeIn = .1;
        std::vector<float> fadeMix;
        s.media.resetAudio();
        s.media.mix(s.project, 0, 1024, fadeMix);
        require(std::abs(fadeMix[1000] - clean[1000] * (500.f / 4800)) < 1e-6,
                "Audio fade shapes actual PCM samples");
        clip.fadeIn = 0;
        int dspCount = 0;
        for (auto &descriptor : cydaw::engine::effectCatalogue) {
            if (descriptor.convertsChannels())
                continue;
            clip.effects = {sf::instantiateAudioEffect("cydaw." + std::string(descriptor.typeKey))};
            s.media.resetAudio();
            std::vector<float> processed;
            s.media.mix(s.project, 0, 1024, processed);
            require(processed.size() == clean.size() && std::all_of(processed.begin(), processed.end(),
                                                                    [](float v) { return std::isfinite(v); }),
                    std::string(descriptor.displayName).c_str());
            if (descriptor.typeKey == "bitcrusher")
                require(processed != clean, "Native CyDAW DSP changes audio");
            ++dspCount;
        }
        require(dspCount == 15, "15 native stereo CyDAW audio processors");
        clip.effects = {sf::instantiateAudioEffect("cydaw.eq-three")};
        auto mp4 = sx::Narrow((sandbox / "demo.mp4").wstring());
        auto protectedMp4 = sx::Narrow((sandbox / "protected.mp4").wstring());
        {
            std::ofstream file(sx::Utf8Path(protectedMp4), std::ios::binary);
            file << "Existing destination";
        }
        rejected = false;
        try {
            sf::exportVideo(s.project, s.media, s.renderer, protectedMp4, [](float) { return false; });
        } catch (...) {
            rejected = true;
        }
        std::string protectedContent;
        sx::ReadTextFile(protectedMp4, protectedContent);
        require(rejected && protectedContent == "Existing destination",
                "Cancelled MP4 export preserves the existing destination");
        bool pending = false;
        for (auto &entry : std::filesystem::directory_iterator(sandbox))
            pending |= entry.path().filename().wstring().find(L".pending-") != std::wstring::npos;
        require(!pending, "Cancelled export cleans its temporary file");
        sf::exportVideo(s.project, s.media, s.renderer, mp4);
        sf::MediaAsset video;
        video.open(mp4);
        require(video.video && video.hasAudio, "H264 AAC export decode");
        require(!video.at(.1).pixels.empty(), "Seek exported video");
        require(std::any_of(video.pcm.begin(), video.pcm.end(), [](float v) { return std::abs(v) > .01f; }),
                "AAC contains the mixed audio");
        size_t moon = (size_t(56) * 320 + 246) * 4;
        require(std::abs(int(video.frame.pixels[moon + 2]) - int(image.pixels[moon + 2])) < 30,
                "MP4 keeps the frame orientation and colour");
        sf::Protocol protocol(s);
        sf::Json req = sf::Json::Object();
        req["jsonrpc"] = "2.0";
        req["id"] = 1;
        req["method"] = "initialize";
        req["params"] = sf::Json::Object();
        req["params"]["protocolVersion"] = "2025-06-18";
        require(protocol.respond(req)["result"]["capabilities"].Has("tools"), "MCP capabilities");
        req["method"] = "tools/list";
        require(protocol.respond(req)["result"]["tools"].Size() > 15, "MCP catalogue");
        sf::Json args = sf::Json::Object();
        args["clip"] = s.project.tracks[0].clips[0].id;
        args["start"] = -1;
        auto before = s.project.json().Dump(0);
        rejected = false;
        try {
            protocol.execute("set_clip", args);
        } catch (...) {
            rejected = true;
        }
        require(rejected && s.project.json().Dump(0) == before, "MCP transaction rollback");
        auto savedProject = s.project;
        sf::Effect base, multiplyEffect, addEffect;
        base.code = "float4 Effect(EffectContext ctx){return float4(.2,.2,.2,1);}";
        multiplyEffect.code =
            "float4 Effect(EffectContext ctx){float4 c=InputUV(0,ctx.uv);c.rgb*=2;return c;}";
        addEffect.code = "float4 Effect(EffectContext ctx){float4 c=InputUV(0,ctx.uv);c.rgb+=.1;return c;}";
        addEffect.params["fixture"].setKey({0, .7f});
        s.project.findClip(args["clip"].U())->effects = {base, multiplyEffect, addEffect};
        s.renderer.render(s.project, s.media, .1, 320, 180);
        auto firstOrder = s.renderer.readback();
        args = sf::Json::Object();
        args["clip"] = s.project.tracks[0].clips[0].id;
        args["effect"] = 2;
        args["position"] = 1;
        protocol.execute("set_effect", args);
        s.renderer.render(s.project, s.media, .1, 320, 180);
        auto secondOrder = s.renderer.readback();
        require(pixel(secondOrder, 160, 90) > pixel(firstOrder, 160, 90) + 20 &&
                    s.project.findClip(args["clip"].U())->effects[1].params["fixture"].keys.size() == 1,
                "Reordering changes GPU output and carries effect animations");
        protocol.execute("undo", sf::Json::Object());
        s.renderer.render(s.project, s.media, .1, 320, 180);
        require(pixel(s.renderer.readback(), 160, 90) == pixel(firstOrder, 160, 90),
                "Undo restores the effect order and rendered result");
        protocol.execute("redo", sf::Json::Object());
        args = sf::Json::Object();
        args["clip"] = s.project.tracks[0].clips[0].id;
        args["effect"] = 2;
        args["enabled"] = false;
        protocol.execute("set_effect", args);
        s.renderer.render(s.project, s.media, .1, 320, 180);
        require(pixel(s.renderer.readback(), 160, 90) < pixel(firstOrder, 160, 90),
                "MCP bypass changes the rendered effect stack");
        args["effect"] = 1;
        protocol.execute("remove_effect", args);
        require(s.project.findClip(args["clip"].U())->effects.size() == 2, "MCP removes a video effect");
        s.project = std::move(savedProject);
        std::cout << checks
                  << " checks passed: project, keyframes, history, adapters, HLSL plugins, GPU, PNG, WAV, "
                     "MP4, MCP.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

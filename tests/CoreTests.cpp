#include "core/CanvasView.h"
#include "mcp/Protocol.h"
#include "render/Color.h"
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
        for (auto &d : s.renderer.library.diagnostics)
            std::cerr << d << std::endl;
        require(s.renderer.library.diagnostics.empty() && s.renderer.library.definitions.size() == 414,
                "Builtin and plugin catalogue, with every Shaderixel effect");
        for (auto &d : s.renderer.library.definitions)
            try {
                s.renderer.validateShader(d.code);
            } catch (const std::exception &e) {
                throw std::runtime_error(d.id + " : " + e.what());
            }
        require(true, "Every builtin and Shaderixel shader compiles on the GPU");
        auto graph = sx::EffectGraph::Default();
        sf::Effect ge;
        ge.graph = graph.ToJson();
        s.renderer.validateShader(sf::effectCode(ge));
        require(graph.WouldCycle(graph.OutputNode(), graph.nodes[0].id), "Shaderixel graph cycle detection");
        {
            // Current Shaderixel graphs store values on unconnected inputs ("pv"); they reach the HLSL.
            sf::Effect pinned;
            sf::Json::Parse(R"({"name":"Pins","inputs":1,"nextId":4,"nodes":[
                {"id":1,"op":"input","pos":[0,0],"in":[],"value":[0,0,0,0]},
                {"id":2,"op":"desaturate","pos":[200,0],"in":[[1,1],[0,0]],"pv":[[1,0.375]],"value":[0,0,0,0]},
                {"id":3,"op":"output","pos":[400,0],"in":[[1,0],[2,0],[0,0]],"value":[0,0,0,0]}]})",
                            pinned.graph);
            auto code = sf::effectCode(pinned);
            s.renderer.validateShader(code);
            require(code.find("0.375") != std::string::npos,
                    "A value set on an unconnected Shaderixel graph input reaches the compiled shader");
        }
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
        {
            // Attack detection finds percussive hits over a sustained tone.
            sf::MediaAsset drums;
            drums.hasAudio = true;
            drums.pcm.assign(48000 * 3 * 2, 0);
            std::vector<double> hits{.5, 1., 1.75, 2.4};
            unsigned noise = 12345;
            for (size_t f = 0; f < 48000 * 3; ++f) {
                double t = f / 48000.;
                float value = float(.2 * std::sin(t * 2 * 3.14159265 * 220));
                for (double hit : hits)
                    if (t >= hit && t < hit + .08) {
                        noise = noise * 1664525u + 1013904223u;
                        value += float((noise >> 8) / double(1 << 24) * 2 - 1) * float(std::exp(-(t - hit) * 60));
                    }
                drums.pcm[f * 2] = drums.pcm[f * 2 + 1] = value;
            }
            auto onsets = sf::detectOnsets(drums, 0, 3, .5f, .12);
            bool matched = onsets.size() == hits.size();
            for (size_t i = 0; matched && i < hits.size(); ++i)
                matched = std::abs(onsets[i] - hits[i]) < .025;
            require(matched, "Attack detection finds each percussive hit within 25 ms, not the sustained tone");
        }
        {
            // Replace edit: the clip and its linked sound take another medium, same place and length.
            sf::Project swap;
            swap.duration = 10;
            sf::Clip picture, sound, other;
            picture.id = swap.allocate();
            picture.source = "a.mp4";
            picture.duration = 2;
            picture.in = 1;
            sound = picture;
            sound.id = swap.allocate();
            other = picture;
            other.id = swap.allocate();
            other.start = 4;
            swap.addTrack(sf::TrackKind::Video).clips = {picture, other};
            swap.addTrack(sf::TrackKind::Audio).clips = {sound};
            swap.linkClips(picture.id, sound.id);
            auto swapped = swap;
            swapped.replaceSource(picture.id, "b.mp4", 3, 10, true, true);
            auto *newPicture = swapped.findClip(picture.id);
            require(newPicture->source == "b.mp4" && newPicture->in == 3 && newPicture->start == 0 &&
                        newPicture->duration == 2 && newPicture->name == "b" &&
                        swapped.findClip(sound.id)->source == "b.mp4" && swapped.findClip(other.id)->source == "a.mp4",
                    "A replace edit swaps the medium of a clip and its linked sound only");
            swapped = swap;
            swapped.replaceSource(picture.id, "silent.mp4", 0, 10, true, false);
            bool tooShort = false;
            try {
                auto again = swap;
                again.replaceSource(picture.id, "b.mp4", 3, 4, true, true);
            } catch (const std::exception &) {
                tooShort = true;
            }
            require(swapped.findClip(picture.id)->source == "silent.mp4" &&
                        swapped.findClip(sound.id)->source == "a.mp4" && tooShort,
                    "A replace keeps the sound when the medium has none and refuses a medium too short");
        }
        {
            // Multicam: three takes stacked; switching cuts every angle and keeps one live until the next switch.
            sf::Project shoot;
            shoot.duration = 6;
            for (int take = 0; take < 3; ++take) {
                sf::Clip angleClip;
                angleClip.id = shoot.allocate();
                angleClip.generator = true;
                angleClip.duration = 6;
                shoot.addTrack(sf::TrackKind::Video).clips = {angleClip};
            }
            shoot.switchAngle(2, 2);
            shoot.switchAngle(4, 3);
            auto liveAt = [&](double time) {
                int live = 0, count = 0;
                for (auto id : shoot.anglesAt(time)) {
                    ++count;
                    if (auto *c = shoot.findClip(id); c->live(time))
                        live = live ? -1 : count;
                }
                return live;
            };
            bool missing = false;
            try {
                shoot.switchAngle(5, 4);
            } catch (const std::exception &) {
                missing = true;
            }
            require(liveAt(3) == 2 && liveAt(5) == 3 && shoot.tracks[0].clips.size() == 3 && missing,
                    "Switching angles cuts every take and keeps exactly the chosen one live until the next switch");
            // With the sound following, each take's linked sound is switched with its picture.
            sf::Project sounded;
            sounded.duration = 6;
            std::vector<std::pair<sf::Clip, sf::Clip>> takes(2);
            for (auto &[picture, sound] : takes) {
                picture.id = sounded.allocate();
                picture.generator = true;
                picture.duration = 6;
                sound.id = sounded.allocate();
                sound.source = "take.wav";
                sound.duration = 6;
            }
            for (auto &take : takes)
                sounded.addTrack(sf::TrackKind::Video).clips = {take.first};
            for (auto &take : takes)
                sounded.addTrack(sf::TrackKind::Audio).clips = {take.second};
            for (auto &take : takes)
                sounded.linkClips(take.first.id, take.second.id);
            sounded.switchAngle(2, 2, true);
            // Sounds playing just after the switch, among the takes' linked sounds.
            int soundsLive = 0;
            for (auto id : sounded.anglesAt(3)) {
                auto group = sounded.linked(id);
                for (auto member : group)
                    if (auto *c = sounded.findClip(member); c && c->source == "take.wav" && c->live(3))
                        ++soundsLive;
            }
            require(soundsLive == 1, "With the sound following, only the chosen take's sound plays after the switch");
        }
        {
            // Project templates: format, frame rate and named tracks, top to bottom.
            auto templateBefore = s.project;
            sf::Json youtube = sf::Json::Object();
            youtube["template"] = "youtube";
            sf::Protocol(s).execute("new_project", youtube);
            std::string names;
            for (auto &track : s.project.tracks)
                names += track.name + (track.kind == sf::TrackKind::Video ? "/V " : "/A ");
            bool youtubeOk = s.project.width == 1920 && s.project.height == 1080 && s.project.fps == 30 &&
                             names == "Titles/V Video/V Voice/A Music/A " && s.project.name == "YouTube Video";
            sf::Json podcast = sf::Json::Object();
            podcast["template"] = "podcast";
            sf::Protocol(s).execute("new_project", podcast);
            bool podcastOk = s.project.tracks.size() == 6 && s.project.tracks[3].name == "Host";
            sf::Json shortTemplate = sf::Json::Object();
            shortTemplate["template"] = "short";
            sf::Protocol(s).execute("new_project", shortTemplate);
            bool verticalOk = s.project.width == 1080 && s.project.height == 1920;
            bool unknown = false;
            try {
                sf::Json wrong = sf::Json::Object();
                wrong["template"] = "opera";
                sf::Protocol(s).execute("new_project", wrong);
            } catch (const std::exception &) {
                unknown = true;
            }
            s.project = templateBefore;
            s.history.clear();
            require(youtubeOk && podcastOk && verticalOk && unknown,
                    "Project templates set the format, the frame rate and the named tracks, and refuse an unknown one");
        }
        {
            // Exchange: a CMX 3600 edit decision list and a CSV shot list.
            sf::Project exchanged;
            exchanged.name = "Film";
            exchanged.duration = 8;
            sf::Clip shotA, soundA, black, fast, skipped;
            shotA.id = exchanged.allocate();
            shotA.name = "Plan \"A\"";
            shotA.source = "a.mp4";
            shotA.in = 2;
            shotA.duration = 3;
            soundA = shotA;
            soundA.id = exchanged.allocate();
            black.id = exchanged.allocate();
            black.generator = true;
            black.name = "Lumière";
            black.start = 3;
            black.duration = 2;
            fast.id = exchanged.allocate();
            fast.name = "Rapide";
            fast.source = "c.mp4";
            fast.in = 1;
            fast.start = 5;
            fast.duration = 2;
            fast.speed = 2;
            skipped = fast;
            skipped.id = exchanged.allocate();
            skipped.start = 7;
            skipped.duration = 1;
            skipped.enabled = false;
            exchanged.addTrack(sf::TrackKind::Video).clips = {shotA, black, fast, skipped};
            exchanged.addTrack(sf::TrackKind::Audio).clips = {soundA};
            exchanged.linkClips(shotA.id, soundA.id);
            auto edlText = exchanged.edl();
            auto shots = exchanged.shotListCsv();
            require(edlText.starts_with("TITLE: Film\nFCM: NON-DROP FRAME\n") &&
                        edlText.find("001  AX       B     C        00:00:02:00 00:00:05:00 00:00:00:00 00:00:03:00") !=
                            std::string::npos &&
                        edlText.find("002  BL       V     C        00:00:00:00 00:00:02:00 00:00:03:00 00:00:05:00") !=
                            std::string::npos &&
                        edlText.find("003  AX       V     C        00:00:01:00 00:00:05:00 00:00:05:00 00:00:07:00") !=
                            std::string::npos &&
                        edlText.find("M2   AX       060.0    00:00:01:00") != std::string::npos &&
                        edlText.find("004") == std::string::npos && edlText.find("* SOURCE FILE: c.mp4") != std::string::npos &&
                        std::count(shots.begin(), shots.end(), '\n') == 6 &&
                        shots.find("\"Plan \"\"A\"\"\",00:00:00:00,00:00:03:00") != std::string::npos &&
                        shots.find(",200,0,non") != std::string::npos,
                    "The edit decision list and the shot list describe each clip, its speed and its sound");
            auto exchangedBefore = s.project;
            s.project = exchanged;
            auto edlResult = sf::Protocol(s).execute("export_edl", sf::Json::Object());
            s.project = exchangedBefore;
            require(edlResult["events"].I() == 3 && edlResult["text"].S() == edlText,
                    "MCP export_edl returns the list and counts its events");
            // Round trip: the list read back rebuilds the clips, their speed and the linked sound.
            auto roundTrip = std::filesystem::current_path() / "test-output" / "edl";
            std::filesystem::remove_all(roundTrip);
            std::filesystem::create_directories(roundTrip);
            { std::ofstream(roundTrip / "Rapide") << "x"; } // found by name: the source file of « Rapide » is absent
            {
                std::ofstream file(roundTrip / "film.edl", std::ios::binary);
                file << edlText;
            }
            sf::Project reread;
            reread.duration = 1;
            s.project = reread;
            sf::Json importEdl = sf::Json::Object();
            importEdl["path"] = sx::Narrow((roundTrip / "film.edl").wstring());
            auto imported = sf::Protocol(s).execute("import_edl", importEdl);
            auto &edlVideo = s.project.tracks.front().clips;
            const sf::Clip *edlAudio = nullptr;
            for (auto &track : s.project.tracks)
                if (track.kind == sf::TrackKind::Audio && !track.clips.empty())
                    edlAudio = &track.clips[0];
            bool rebuilt = imported["clips"].I() == 2 && imported["offline"].I() == 1 && edlVideo.size() == 2 &&
                           edlVideo[0].start == 0 && std::abs(edlVideo[0].duration - 3) < 1e-9 && edlVideo[0].in == 2 &&
                           edlVideo[0].source == "a.mp4" && std::abs(edlVideo[1].start - 5) < 1e-9 &&
                           std::abs(edlVideo[1].speed - 2) < 1e-9 && edlVideo[1].in == 1 &&
                           sx::Utf8Path(edlVideo[1].source).filename() == L"Rapide" && edlAudio &&
                           edlAudio->link && edlAudio->link == edlVideo[0].link;
            s.project = exchangedBefore;
            require(rebuilt, "An edit decision list read back rebuilds the clips, their speed and the linked sound");
        }
        {
            // Markers keep their notes and export as YouTube chapters and CSV.
            sf::Project chaptered;
            chaptered.name = "Film";
            chaptered.duration = 4000;
            chaptered.addMarker(75.4, "Scène \"deux\"").note = "à revoir, vite";
            chaptered.addMarker(3725, "End").color = 0x52d1a2;
            chaptered.markers[1].duration = 12.5;
            auto reloaded = sf::Project::fromJson(chaptered.json());
            require(reloaded.markers[0].note == "à revoir, vite" && reloaded.markers[1].note.empty(),
                    "Marker notes are saved and reloaded");
            require(chaptered.markerChapters() == "0:00 Film\n1:15 Scène \"deux\"\n1:02:05 End\n",
                    "Markers export as YouTube chapters starting at 0:00");
            auto csv = chaptered.markerCsv();
            require(csv.find("\"Scène \"\"deux\"\"\",00:01:15:12,75.400,,\"à revoir, vite\"") != std::string::npos &&
                        csv.find("01:02:05:00,3725.000,52D1A2,\"\",12.500\n") != std::string::npos &&
                        csv.find("\"à revoir, vite\",\n") != std::string::npos && csv.starts_with("Name,Timecode,Seconds,Color,Note,Duration\n"),
                    "Markers export as CSV with quoted names, notes and region lengths");
        }
        {
            // Trimming to the playhead: ripple closes the gap and carries the kept picture's keys.
            sf::Project cut;
            cut.duration = 4;
            auto block = [&](double start) {
                sf::Clip c;
                c.id = cut.allocate();
                c.generator = true;
                c.start = start;
                c.duration = 2;
                return c;
            };
            auto first = block(0), second = block(2), partner = block(2);
            second.params["opacity"].setKey({3, .5f});
            cut.addTrack(sf::TrackKind::Video).clips = {first, second};
            cut.addTrack(sf::TrackKind::Video).clips = {partner};
            cut.linkClips(second.id, partner.id);
            auto rippled = cut;
            rippled.trimClip(second.id, 3, true, false);
            auto held = *rippled.findClip(second.id);
            require(held.start == 3 && held.duration == 1 && held.in == 1 && held.params["opacity"].keys[0].time == 3 &&
                        rippled.findClip(partner.id)->start == 3,
                    "Trimming a clip's head without ripple keeps its keys and moves its linked group");
            rippled = cut;
            rippled.trimClip(second.id, 3, true, true);
            held = *rippled.findClip(second.id);
            require(held.start == 2 && held.duration == 1 && held.in == 1 && held.params["opacity"].keys[0].time == 2 &&
                        rippled.findClip(partner.id)->duration == 1 && rippled.duration == 3,
                    "A ripple head trim keeps the clip in place, shifts its keys, and shortens the sequence");
            rippled = cut;
            rippled.trimClip(first.id, 1, false, true);
            require(rippled.findClip(first.id)->duration == 1 && rippled.findClip(second.id)->start == 1 &&
                        rippled.findClip(partner.id)->start == 1 &&
                        rippled.findClip(second.id)->params["opacity"].keys[0].time == 2,
                    "A ripple tail trim pulls the following clips and their keys back");
            bool refused = false;
            try {
                rippled.trimClip(first.id, 5, true, true);
            } catch (const std::exception &) {
                refused = true;
            }
            require(refused, "A trim time outside the clip is refused");
            // Inserting a gap cuts across clips and moves what follows, keys and markers included.
            cut.addMarker(3, "Après");
            auto opened = cut;
            opened.insertGap(1, .5);
            auto &openedTrack = opened.tracks[0].clips;
            auto tailOf = std::find_if(openedTrack.begin(), openedTrack.end(),
                                       [](const sf::Clip &c) { return std::abs(c.start - 1.5) < 1e-9; });
            require(openedTrack.size() == 3 && opened.findClip(first.id)->duration == 1 &&
                        tailOf != openedTrack.end() && tailOf->in == 1 && opened.findClip(second.id)->start == 2.5 &&
                        opened.findClip(partner.id)->start == 2.5 &&
                        opened.findClip(second.id)->params["opacity"].keys[0].time == 3.5 &&
                        opened.markers[0].time == 3.5 && opened.duration == 4.5,
                    "Inserting a gap cuts across clips and moves the following clips, keys and markers");
            // A freeze frame holds the picture under the time, without its keys, and opens the sequence.
            auto frozen = cut;
            auto heldId = frozen.freezeFrame(second.id, 3, 1);
            auto *heldClip = frozen.findClip(heldId);
            auto reopened = sf::Project::fromJson(frozen.json());
            require(heldClip && heldClip->hold && heldClip->start == 3 && heldClip->duration == 1 &&
                        heldClip->sourceTime(3.9) == 1 && heldClip->params["opacity"].keys.empty() &&
                        std::abs(heldClip->params["opacity"].value - .5f) < 1e-6 &&
                        frozen.findClip(second.id)->duration == 1 && frozen.tracks[0].clips.size() == 4 &&
                        frozen.tracks[1].clips.size() == 2 && frozen.tracks[1].clips[1].start == 4 &&
                        frozen.duration == 5 && reopened.findClip(heldId)->hold,
                    "A freeze frame holds the frame under the time, frozen values, and is saved");
            // Closing a span carries the markers: inside it they gather at its start, after it they move back.
            auto closed = cut;
            closed.removeRange(2.5, 3.5);
            auto trimmedBack = cut;
            trimmedBack.trimClip(first.id, 1, false, true);
            auto deleted = cut;
            deleted.removeClips({first.id}, true);
            require(closed.markers[0].time == 2.5 && trimmedBack.markers[0].time == 2 && deleted.markers[0].time == 1,
                    "Extracting, ripple trimming and ripple deleting move the markers with the sequence");
            // Renaming in series follows the timeline order, pads the number and keeps the old name on request.
            auto named = cut;
            auto names = named.renameClips({second.id, first.id, partner.id}, "Plan ## - {nom}", 9);
            require(names.size() == 3 && names[0] == "Plan 09 - Clip" && names[1] == "Plan 10 - Clip" &&
                        names[2] == "Plan 11 - Clip" && named.findClip(partner.id)->name == "Plan 11 - Clip",
                    "Renaming in series numbers clips in timeline order with padding and the old name");
            // Cutting at markers: every crossing clip is cut, the pieces again at the following markers.
            auto chopped = cut;
            chopped.addMarker(1, "Un");
            chopped.addMarker(3.5, "Trois et demi");
            auto onlyFirst = chopped;
            int allCuts = chopped.splitAtMarkers({});
            int firstCuts = onlyFirst.splitAtMarkers({first.id});
            require(allCuts == 3 && chopped.tracks[0].clips.size() == 5 && chopped.tracks[1].clips.size() == 3 &&
                        firstCuts == 1 && onlyFirst.tracks[0].clips.size() == 3 && onlyFirst.tracks[1].clips.size() == 1,
                    "Cutting at markers cuts the chosen clips, or every track, at each marker they cross");
        }
        {
            // Closing gaps: on one track its clips move earlier with their linked members; for the whole
            // sequence only the moments empty on every track go, and the sequence shortens.
            sf::Project gappy;
            gappy.duration = 12;
            auto block = [&](double start, double length) {
                sf::Clip c;
                c.id = gappy.allocate();
                c.generator = true;
                c.start = start;
                c.duration = length;
                return c;
            };
            auto a = block(1, 2), b = block(5, 2), c = block(9, 1), sound = block(5, 2), other = block(0, 1);
            b.params["opacity"].setKey({6, .5f});
            gappy.addTrack(sf::TrackKind::Video).clips = {a, b, c};
            gappy.addTrack(sf::TrackKind::Audio).clips = {other, sound};
            gappy.linkClips(b.id, sound.id);
            auto packed = gappy;
            int closed = packed.closeGaps(packed.tracks[0].id);
            require(closed == 3 && packed.findClip(a.id)->start == 0 && packed.findClip(b.id)->start == 2 &&
                        packed.findClip(c.id)->start == 4 && packed.findClip(sound.id)->start == 2 &&
                        packed.findClip(b.id)->params["opacity"].keys[0].time == 3 && packed.duration == 12 &&
                        packed.findClip(other.id)->start == 0 && packed.closeGaps(packed.tracks[0].id) == 0,
                    "Closing a track's gaps moves its clips, their keys and linked members earlier");
            auto blocked = gappy;
            blocked.tracks[1].clips.push_back(block(3.5, 1));
            bool refused = false;
            try {
                blocked.closeGaps(blocked.tracks[0].id);
            } catch (const std::exception &) {
                refused = true;
            }
            auto whole = gappy;
            whole.addMarker(8, "End");
            int spans = whole.closeGaps(0);
            require(refused && spans == 2 && whole.findClip(a.id)->start == 1 && whole.findClip(b.id)->start == 3 &&
                        whole.findClip(c.id)->start == 5 && whole.findClip(other.id)->start == 0 &&
                        std::abs(whole.duration - 8) < 1e-9 && whole.markers[0].time == 5,
                    "A linked clip that would meet another is refused; sequence gaps close where every track is empty");
            // Beat montage: the chosen clips follow from the first one to each next marker; a source limit stops one early.
            auto beats = gappy;
            for (double t : {2.5, 4.0, 5.0, 7.0})
                beats.addMarker(t, "Time");
            auto limit = [&](const sf::Clip &clip) { return clip.id == c.id ? .75 : 1e9; };
            int placed = beats.fitToMarkers({c.id, a.id, b.id}, limit);
            require(placed == 3 && beats.findClip(a.id)->start == 1 && beats.findClip(a.id)->duration == 1.5 &&
                        beats.findClip(b.id)->start == 2.5 && beats.findClip(b.id)->duration == 1.5 &&
                        beats.findClip(sound.id)->start == 2.5 && beats.findClip(sound.id)->duration == 1.5 &&
                        beats.findClip(b.id)->params["opacity"].keys[0].time == 3.5 &&
                        beats.findClip(c.id)->start == 4 && beats.findClip(c.id)->duration == .75,
                    "Fitting to markers lays the clips from marker to marker, linked sound and keys following");
            bool noMarker = false;
            try {
                gappy.fitToMarkers({a.id}, limit);
            } catch (const std::exception &) {
                noMarker = true;
            }
            require(noMarker, "Fitting to markers needs a marker after the first clip");
        }
        {
            // Regions: a marker with a length, saved, grown by an opened span, shortened by a closed one.
            sf::Project regions;
            regions.duration = 10;
            auto &region = regions.addMarker(2, "Couplet");
            region.duration = 4;
            regions.addMarker(8, "End");
            auto reopened = sf::Project::fromJson(regions.json());
            auto opened = regions;
            opened.insertGap(3, 1);
            auto closed = regions;
            closed.removeRange(5, 7);
            auto swallowed = regions;
            swallowed.removeRange(1, 7);
            auto invalid = regions;
            invalid.markers[0].duration = -1;
            bool refused = false;
            try {
                invalid.validate();
            } catch (const std::exception &) {
                refused = true;
            }
            require(reopened.markers[0].duration == 4 && reopened.markers[1].duration == 0 &&
                        opened.markers[0].time == 2 && opened.markers[0].duration == 5 && opened.markers[1].time == 9 &&
                        closed.markers[0].time == 2 && closed.markers[0].duration == 3 && closed.markers[1].time == 6 &&
                        swallowed.markers[0].time == 1 && swallowed.markers[0].duration == 0 && refused,
                    "A region is saved, grows with an opened span, shrinks with a closed one, and stays positive");
        }
        {
            // An adjustment layer runs the picture of the tracks below through its effects.
            sf::Project adjusted;
            adjusted.width = adjusted.height = 64;
            adjusted.duration = 1;
            sf::Clip base, layer;
            base.id = adjusted.allocate();
            base.generator = true;
            base.duration = 1;
            layer.id = adjusted.allocate();
            layer.adjustment = true;
            layer.duration = 1;
            sf::Effect invert;
            invert.id = "test.invert";
            invert.code = "float4 Effect(EffectContext ctx){float4 c=InputUV(0,ctx.uv);return float4(1-c.rgb,c.a);}";
            layer.effects = {invert};
            adjusted.addTrack(sf::TrackKind::Video).clips = {base};
            adjusted.addTrack(sf::TrackKind::Video).clips = {layer};
            std::rotate(adjusted.tracks.begin(), adjusted.tracks.end() - 1, adjusted.tracks.end()); // layer on top
            auto meanOf = [&](const sf::Project &project) {
                s.renderer.clearHistory();
                s.renderer.render(project, s.media, .5, 64, 64);
                auto picture = s.renderer.readback();
                double sum = 0;
                for (size_t i = 0; i < picture.pixels.size(); i += 4)
                    sum += picture.pixels[i] + picture.pixels[i + 1] + picture.pixels[i + 2];
                return sum / (picture.pixels.size() / 4 * 3);
            };
            double inverted = meanOf(adjusted);
            adjusted.tracks[0].clips[0].enabled = false;
            double plain = meanOf(adjusted);
            bool refused = false;
            try {
                auto wrong = adjusted;
                wrong.tracks[0].clips[0].generator = true;
                wrong.validate();
            } catch (const std::exception &) {
                refused = true;
            }
            require(plain < 100 && inverted > plain + 80 && refused &&
                        sf::Project::fromJson(adjusted.json()).tracks[0].clips[0].adjustment,
                    "An adjustment layer inverts the picture below it, is saved, and cannot also be a generator");
        }
        {
            // Compositing quick actions on a solid green picture over a generator: keyed out, then placed in a corner.
            auto projectBefore = s.project;
            auto greenPath = sx::Narrow((std::filesystem::current_path() / "test-output" / "green.png").wstring());
            std::filesystem::create_directories(std::filesystem::current_path() / "test-output");
            sf::Image green;
            green.width = green.height = 64;
            green.pixels.resize(64 * 64 * 4);
            for (size_t i = 0; i < green.pixels.size(); i += 4) {
                green.pixels[i] = 0;
                green.pixels[i + 1] = 255;
                green.pixels[i + 2] = 0;
                green.pixels[i + 3] = 255;
            }
            sf::writePng(green, greenPath);
            sf::Project keyed;
            keyed.width = keyed.height = 64;
            keyed.duration = 1;
            sf::Clip under, screen;
            under.id = keyed.allocate();
            under.generator = true;
            under.duration = 1;
            screen.id = keyed.allocate();
            screen.source = greenPath;
            screen.duration = 1;
            keyed.addTrack(sf::TrackKind::Video).clips = {screen};
            keyed.addTrack(sf::TrackKind::Video).clips = {under};
            auto greenAt = [&](const sf::Project &project, unsigned x, unsigned y) {
                s.renderer.clearHistory();
                s.renderer.render(project, s.media, .5, 64, 64);
                auto picture = s.renderer.readback();
                return int(picture.pixels[(size_t(y) * 64 + x) * 4 + 1]);
            };
            int plainGreen = greenAt(keyed, 8, 8);
            s.project = keyed;
            sf::Json key = sf::Json::Object();
            key["clips"] = sf::Json::Array();
            key["clips"].Push(screen.id);
            sf::Protocol(s).execute("key_background", key);
            int keyedGreen = greenAt(s.project, 8, 8);
            s.project = keyed;
            sf::Json corner = sf::Json::Object();
            corner["clips"] = sf::Json::Array();
            corner["clips"].Push(screen.id);
            corner["position"] = "bottom_right";
            sf::Protocol(s).execute("place_clip", corner);
            int cornerGreen = greenAt(s.project, 52, 52), awayGreen = greenAt(s.project, 8, 8);
            require(plainGreen > 200 && keyedGreen < 120 && cornerGreen > 200 && awayGreen < 120,
                    "A green screen is keyed out, and a clip placed bottom right covers only that corner");
            // A portrait picture keeps its true share of the frame when placed; a split screen fits two pictures.
            auto portraitPath = sx::Narrow((std::filesystem::current_path() / "test-output" / "portrait.png").wstring());
            sf::Image portrait;
            portrait.width = 32;
            portrait.height = 64;
            portrait.pixels.assign(32 * 64 * 4, 0);
            for (size_t i = 0; i < portrait.pixels.size(); i += 4)
                portrait.pixels[i + 1] = portrait.pixels[i + 3] = 255;
            sf::writePng(portrait, portraitPath);
            auto tall = keyed;
            tall.tracks[0].clips[0].source = portraitPath;
            s.project = tall;
            sf::Protocol(s).execute("place_clip", corner);
            int tallInside = greenAt(s.project, 52, 25), tallOutside = greenAt(s.project, 20, 25);
            auto pair = keyed;
            sf::Clip second = pair.tracks[0].clips[0];
            second.id = pair.allocate();
            pair.addTrack(sf::TrackKind::Video).clips = {second};
            std::rotate(pair.tracks.begin(), pair.tracks.end() - 1, pair.tracks.end());
            pair.tracks.back().clips[0].generator = false;
            pair.tracks.back().clips[0].enabled = false; // nothing under the two pictures
            s.project = pair;
            sf::Json split = sf::Json::Object();
            split["clips"] = sf::Json::Array();
            split["clips"].Push(second.id);
            split["clips"].Push(screen.id);
            split["layout"] = "side_by_side";
            sf::Protocol(s).execute("split_screen", split);
            int leftCell = greenAt(s.project, 16, 32), rightCell = greenAt(s.project, 48, 32),
                band = greenAt(s.project, 32, 6);
            require(tallInside > 200 && tallOutside < 120 && leftCell > 200 && rightCell > 200 && band < 60,
                    "A portrait picture placed in a corner keeps its true size, and a split screen fits two pictures");
            // A box behind a title's text, and a lower third created on a free title track.
            sf::Project lettering;
            lettering.width = 128;
            lettering.height = 64;
            lettering.duration = 2;
            sf::Clip caption;
            caption.id = lettering.allocate();
            caption.title.enabled = true;
            caption.title.text = "Ab";
            caption.duration = 2;
            caption.params["font_size"] = {16};
            lettering.addTrack(sf::TrackKind::Video).clips = {caption};
            auto brightness = [&](const sf::Project &project) {
                s.renderer.clearHistory();
                s.renderer.render(project, s.media, 1, 128, 64);
                auto picture = s.renderer.readback();
                double sum = 0;
                for (size_t i = 0; i < picture.pixels.size(); i += 4)
                    sum += picture.pixels[i] + picture.pixels[i + 1] + picture.pixels[i + 2];
                return sum / (picture.pixels.size() / 4 * 3);
            };
            double bare = brightness(lettering);
            for (auto key : {"text_box", "text_box_r", "text_box_g", "text_box_b"})
                lettering.tracks[0].clips[0].params[key] = sf::Parameter{1};
            double boxed = brightness(lettering);
            s.project = lettering;
            sf::Json third = sf::Json::Object();
            third["name"] = "Camille Martin";
            third["role"] = "Réalisatrice";
            third["start"] = 0;
            third["duration"] = 2;
            auto lower = sf::Protocol(s).execute("add_lower_third", third);
            auto *thirdClip = s.project.findClip(lower["clip"].U());
            require(boxed > bare * 3 && thirdClip && thirdClip->title.text == "Camille Martin\nRéalisatrice" &&
                        thirdClip->title.alignment == 0 && thirdClip->params["text_box"].value > .5f &&
                        thirdClip->params["opacity"].keys.size() == 4 && s.project.tracks.size() == 2,
                    "A box lights up behind a title, and a lower third gets two lines, a box and its animation");
            sf::Json styleArgs = sf::Json::Object();
            styleArgs["clips"] = sf::Json::Array();
            styleArgs["clips"].Push(caption.id);
            styleArgs["style"] = "boxed";
            sf::Protocol(s).execute("apply_title_style", styleArgs);
            bool boxedStyle = s.project.findClip(caption.id)->params["text_box"].value > .5f;
            styleArgs["style"] = "cinema";
            sf::Protocol(s).execute("apply_title_style", styleArgs);
            auto *styledCaption = s.project.findClip(caption.id);
            require(boxedStyle && styledCaption->params["text_box"].value == 0 && styledCaption->title.font == "Georgia" &&
                        styledCaption->params["text_shadow"].value > .5f,
                    "A title style replaces the previous one entirely");
            // Typewriter and neon on a title alone.
            sf::Project typed;
            typed.width = 128;
            typed.height = 64;
            typed.duration = 2;
            sf::Clip words = caption;
            words.id = typed.allocate();
            words.params.clear();
            words.params["font_size"] = {16};
            typed.addTrack(sf::TrackKind::Video).clips = {words};
            auto channelMean = [&](const sf::Project &project, double time, int channel) {
                s.renderer.clearHistory();
                s.renderer.render(project, s.media, time, 128, 64);
                auto picture = s.renderer.readback();
                double sum = 0;
                for (size_t i = 0; i < picture.pixels.size(); i += 4)
                    sum += picture.pixels[i + size_t(channel)];
                return sum / (picture.pixels.size() / 4);
            };
            s.project = typed;
            sf::Json typing = sf::Json::Object();
            typing["clips"] = sf::Json::Array();
            typing["clips"].Push(words.id);
            typing["preset"] = "typewriter";
            sf::Protocol(s).execute("apply_animation", typing);
            double unwritten = channelMean(s.project, .01, 1), written = channelMean(s.project, 1, 1);
            styleArgs["clips"] = sf::Json::Array();
            styleArgs["clips"].Push(words.id);
            styleArgs["style"] = "classic";
            sf::Protocol(s).execute("apply_title_style", styleArgs);
            double classicBlue = channelMean(s.project, 1, 0);
            styleArgs["style"] = "neon";
            sf::Protocol(s).execute("apply_title_style", styleArgs);
            double neonBlue = channelMean(s.project, 1, 0), neonRed = channelMean(s.project, 1, 2);
            require(unwritten < written * .2 && written > 1 && neonBlue > classicBlue * 1.2 && neonBlue > neonRed,
                    "A typewritten title starts empty and ends whole, and the neon style glows cyan");
            // Automatic grade: a solid blue-cast picture comes out neutral and near 18 % grey.
            auto castPath = sx::Narrow((std::filesystem::current_path() / "test-output" / "cast.png").wstring());
            sf::Image cast;
            cast.width = cast.height = 32;
            cast.pixels.resize(32 * 32 * 4);
            for (size_t i = 0; i < cast.pixels.size(); i += 4) {
                cast.pixels[i] = 200;     // blue
                cast.pixels[i + 1] = 120; // green
                cast.pixels[i + 2] = 60;  // red
                cast.pixels[i + 3] = 255;
            }
            sf::writePng(cast, castPath);
            sf::Project graded;
            graded.width = graded.height = 64;
            graded.duration = 1;
            sf::Clip castClip;
            castClip.id = graded.allocate();
            castClip.source = castPath;
            castClip.duration = 1;
            graded.addTrack(sf::TrackKind::Video).clips = {castClip};
            s.project = graded;
            sf::Json gradeArgs = sf::Json::Object();
            gradeArgs["clip"] = castClip.id;
            gradeArgs["time"] = .5;
            sf::Protocol(s).execute("auto_grade", gradeArgs);
            s.renderer.clearHistory();
            s.renderer.render(s.project, s.media, .5, 64, 64);
            auto gradedPixels = s.renderer.readbackFloat();
            auto centre = (size_t(32) * 64 + 32) * 4;
            float r = gradedPixels.rgba[centre], g = gradedPixels.rgba[centre + 1], b = gradedPixels.rgba[centre + 2];
            // Measured in the project's working space (sRGB here): mid grey is 0.18 once linear.
            auto linear = sf::convertColor({r, g, b}, s.project.workingSpace, "linear709");
            float gradedLuma = linear.x * .2126f + linear.y * .7152f + linear.z * .0722f;
            require(std::abs(r - g) < .02f && std::abs(b - g) < .02f && std::abs(gradedLuma - .18f) < .03f,
                    "Automatic grading neutralises a colour cast and brings the picture to mid grey");
            // Matching colours: the blue-cast picture takes a warm reference's mean colour and level.
            auto warmPath = sx::Narrow((std::filesystem::current_path() / "test-output" / "warm.png").wstring());
            sf::Image warm = cast;
            for (size_t i = 0; i < warm.pixels.size(); i += 4) {
                warm.pixels[i] = 100;     // blue
                warm.pixels[i + 1] = 150; // green
                warm.pixels[i + 2] = 210; // red
            }
            sf::writePng(warm, warmPath);
            // A realistic cast this time (an extreme one needs gains beyond the balance's range).
            auto coolPath = sx::Narrow((std::filesystem::current_path() / "test-output" / "cool.png").wstring());
            sf::Image cool = cast;
            for (size_t i = 0; i < cool.pixels.size(); i += 4) {
                cool.pixels[i] = 170;     // blue
                cool.pixels[i + 1] = 130; // green
                cool.pixels[i + 2] = 100; // red
            }
            sf::writePng(cool, coolPath);
            sf::Project matched = graded;
            matched.tracks[0].clips[0].source = coolPath;
            matched.tracks[0].clips[0].effects.clear();
            matched.duration = 2;
            sf::Clip warmClip;
            warmClip.id = matched.allocate();
            warmClip.source = warmPath;
            warmClip.start = 1;
            warmClip.duration = 1;
            matched.tracks[0].clips.push_back(warmClip);
            s.project = matched;
            sf::Json matchArgs = sf::Json::Object();
            matchArgs["clip"] = castClip.id;
            matchArgs["reference"] = warmClip.id;
            matchArgs["time"] = .5;
            auto matchResult = sf::Protocol(s).execute("match_grade", matchArgs);
            auto linearAt = [&](double time) {
                s.renderer.clearHistory();
                s.renderer.render(s.project, s.media, time, 64, 64);
                auto picture = s.renderer.readbackFloat();
                auto at = (size_t(32) * 64 + 32) * 4;
                return sf::convertColor({picture.rgba[at], picture.rgba[at + 1], picture.rgba[at + 2]},
                                        s.project.workingSpace, "linear709");
            };
            auto matchedColour = linearAt(.5), referenceColour = linearAt(1.5);
            auto close = [](float a, float b) { return std::abs(a - b) <= .05f * std::max(b, .01f) + .005f; };
            require(close(matchedColour.x, referenceColour.x) && close(matchedColour.y, referenceColour.y) &&
                        close(matchedColour.z, referenceColour.z) && !matchResult["clamped"].B(),
                    "Matching colours moves a blue-cast picture onto a warm reference's colour and level");
            // An extreme gap: a second correction carries what the balance cannot; a selection matches in one step.
            auto extremePath = sx::Narrow((std::filesystem::current_path() / "test-output" / "extreme.png").wstring());
            sf::Image extreme = cast;
            for (size_t i = 0; i < extreme.pixels.size(); i += 4) {
                extreme.pixels[i] = 60;      // blue
                extreme.pixels[i + 1] = 140; // green
                extreme.pixels[i + 2] = 220; // red
            }
            sf::writePng(extreme, extremePath);
            auto opposite = matched;
            opposite.tracks[0].clips[0].source = castPath; // the strong blue cast
            opposite.tracks[0].clips[0].effects.clear();
            opposite.tracks[0].clips[1].source = extremePath;
            sf::Clip secondCast = opposite.tracks[0].clips[0];
            secondCast.id = opposite.allocate();
            secondCast.start = 2;
            opposite.tracks[0].clips.push_back(secondCast);
            opposite.duration = 3;
            s.project = opposite;
            sf::Json batch = sf::Json::Object();
            batch["clips"] = sf::Json::Array();
            batch["clips"].Push(castClip.id);
            batch["clips"].Push(secondCast.id);
            batch["clips"].Push(warmClip.id); // the reference itself is skipped
            batch["reference"] = warmClip.id;
            s.history.clear();
            auto batched = sf::Protocol(s).execute("match_grade", batch);
            auto firstMatched = linearAt(.5), secondMatched = linearAt(2.5), extremeReference = linearAt(1.5);
            require(batched["clips"].I() == 2 && !batched["clamped"].B() && s.history.undoSteps().size() == 1 &&
                        s.project.tracks[0].clips[0].effects.size() == 2 && close(firstMatched.x, extremeReference.x) &&
                        close(firstMatched.y, extremeReference.y) && close(firstMatched.z, extremeReference.z) &&
                        close(secondMatched.z, extremeReference.z),
                    "An extreme colour gap is matched with a second correction, for a whole selection in one step");
            // Micro-fades on the cuts of an audio track.
            sf::Project cutsTrack;
            cutsTrack.duration = 4;
            sf::Clip left, right;
            left.id = cutsTrack.allocate();
            left.source = greenPath;
            left.duration = 2;
            right = left;
            right.id = cutsTrack.allocate();
            right.start = 2;
            auto audioTrack = cutsTrack.addTrack(sf::TrackKind::Audio).id;
            cutsTrack.tracks.back().clips = {left, right};
            s.project = cutsTrack;
            sf::Json fades = sf::Json::Object();
            fades["track"] = audioTrack;
            auto softened = sf::Protocol(s).execute("audio_cut_fades", fades);
            require(softened["cuts"].I() == 1 && s.project.findClip(left.id)->fadeOut == .05 &&
                        s.project.findClip(right.id)->fadeIn == .05 && s.project.findClip(left.id)->fadeIn == 0,
                    "Micro-fades soften each cut of an audio track on both sides only");
            s.project = projectBefore;
            s.history.clear();
        }
        {
            // A disabled clip stays in the document but is no longer rendered.
            sf::Project lone;
            lone.width = lone.height = 64;
            lone.duration = 1;
            sf::Clip light;
            light.id = lone.allocate();
            light.generator = true;
            light.duration = 1;
            lone.addTrack(sf::TrackKind::Video).clips = {light};
            auto brightness = [&] {
                s.renderer.clearHistory();
                s.renderer.render(lone, s.media, .5, 64, 64);
                auto frame = s.renderer.readback();
                double sum = 0;
                for (size_t i = 0; i < frame.pixels.size(); i += 4)
                    sum += frame.pixels[i] + frame.pixels[i + 1] + frame.pixels[i + 2];
                return sum;
            };
            double on = brightness();
            lone.tracks[0].clips[0].enabled = false;
            double off = brightness();
            require(on > 0 && off == 0 && !sf::Project::fromJson(lone.json()).tracks[0].clips[0].enabled,
                    "A disabled clip is kept, saved, and no longer rendered");
        }
        {
            // Three-point editing: overwrite clears the span on the target tracks, insert pushes the rest.
            sf::Project edit;
            edit.duration = 10;
            auto &picture = edit.addTrack(sf::TrackKind::Video);
            sf::Clip light;
            light.id = edit.allocate();
            light.generator = true;
            light.duration = 4;
            picture.clips = {light};
            auto &sound = edit.addTrack(sf::TrackKind::Audio);
            sf::Clip pad;
            pad.id = edit.allocate();
            pad.source = "pad.wav";
            pad.duration = 4;
            sound.clips = {pad};
            sf::Project::SourceEdit put;
            put.path = "shot.mp4";
            put.in = 5;
            put.out = 7;
            put.at = 1;
            auto placed = edit.placeSource(put);
            auto starts = [](const sf::Track &track) {
                std::vector<double> values;
                for (auto &clip : track.clips)
                    values.push_back(clip.start);
                std::sort(values.begin(), values.end());
                return values;
            };
            auto *shot = edit.findClip(placed[0]);
            require(placed.size() == 2 && shot->in == 5 && shot->duration == 2 && shot->link &&
                        starts(edit.tracks[0]) == std::vector<double>{0, 1, 3} &&
                        starts(edit.tracks[1]) == std::vector<double>{0, 1, 3} && edit.bin.size() == 1,
                    "Overwrite places the source span and keeps both sides of the covered clips");
            put.insert = true;
            put.at = 2;
            put.in = 0;
            put.out = 1;
            edit.placeSource(put);
            require(starts(edit.tracks[0]) == std::vector<double>{0, 1, 2, 3, 4} &&
                        std::abs(edit.duration - 10) < 1e-9 && edit.findClip(placed[0])->duration == 1 &&
                        sf::Project::fromJson(edit.json()).bin.size() == 1,
                    "Insert cuts at the edit point and pushes what follows on every track");
            // Fit to fill: the marked span lasts the given length, its speed set to fill it.
            auto filled = edit;
            sf::Project::SourceEdit fit;
            fit.path = "shot.mp4";
            fit.in = 2;
            fit.out = 5;
            fit.at = 6;
            fit.fill = 1.5;
            auto fitted = filled.placeSource(fit);
            auto *fast = filled.findClip(fitted[0]);
            fit.fill = 600;
            bool tooSlow = false;
            try {
                filled.placeSource(fit);
            } catch (const std::exception &) {
                tooSlow = true;
            }
            require(fast->duration == 1.5 && fast->speed == 2 && std::abs(fast->sourceTime(7.4) - 4.8) < 1e-9 &&
                        filled.findClip(fitted[1])->speed == 2 && tooSlow,
                    "Fit to fill sets the speed so the marked span lasts the chosen length, within limits");
            // Lift empties a span on every track without moving the rest; the gap is then detected.
            auto lifted = edit;
            lifted.liftRange(.5, 1.5);
            double gapBegin = 0, gapEnd = 0;
            require(starts(lifted.tracks[0]) == std::vector<double>{0, 1.5, 2, 3, 4} &&
                        lifted.gapAt(lifted.tracks[0].id, 1, gapBegin, gapEnd) && gapBegin == .5 && gapEnd == 1.5 &&
                        lifted.spanEmpty(gapBegin, gapEnd) && !lifted.gapAt(lifted.tracks[0].id, 2.5, gapBegin, gapEnd) &&
                        std::abs(lifted.duration - edit.duration) < 1e-9,
                    "Lift leaves an empty span on every track, found again as a gap");
            auto copied = edit;
            auto &copy = copied.duplicateTrack(copied.tracks[0].id);
            require(copied.tracks.size() == edit.tracks.size() + 1 && copied.tracks[1].id == copy.id &&
                        copy.clips.size() == edit.tracks[0].clips.size() && copy.clips[0].id != edit.tracks[0].clips[0].id &&
                        std::all_of(copy.clips.begin(), copy.clips.end(), [](auto &clip) { return clip.link == 0; }),
                    "A duplicated track sits under the original with new, unlinked clips");
            auto layered = sf::Project::load(sx::Narrow((sx::Utf8Path(argv[1]) / "examples/LayerGraphStudy.sframe").wstring()));
            for (auto &track : layered.tracks)
                if (track.kind == sf::TrackKind::Video) {
                    layered.duplicateTrack(track.id);
                    break;
                }
            layered.validate();
            require(layered.layerGraph.IsNull() || layered.layerGraph.Dump(0).find("surface") != std::string::npos,
                    "Duplicating a video track keeps a valid Shaderixel layer graph");
        }
        // Every example shows a picture: a Shaderixel graph output without alpha once rendered black.
        for (auto &entry : std::filesystem::directory_iterator(sx::Utf8Path(argv[1]) / "examples")) {
            if (entry.path().extension() != L".sframe")
                continue;
            auto example = sf::Project::load(sx::Narrow(entry.path().wstring()));
            bool video = false;
            for (auto &track : example.tracks)
                video = video || (track.kind == sf::TrackKind::Video && !track.clips.empty());
            if (!video)
                continue;
            s.renderer.clearHistory();
            s.renderer.render(example, s.media, example.duration * .5, 160, 90);
            auto frame = s.renderer.readback();
            double light = 0;
            for (size_t i = 0; i < frame.pixels.size(); i += 4)
                light += frame.pixels[i] + frame.pixels[i + 1] + frame.pixels[i + 2];
            light /= 3. * (frame.pixels.size() / 4);
            if (light < 2)
                throw std::runtime_error("Black example render: " + sx::Narrow(entry.path().filename().wstring()));
        }
        require(true, "Every example sequence renders a visible picture halfway through");
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
        {
            // Subtitles: SubRip read (BOM, CRLF, tags, overlap), imported as title clips, written back.
            auto cues = sf::parseSrt("\xEF\xBB\xBF" "1\r\n00:00:01,000 --> 00:00:02,500\r\n<i>Bonjour</i>\r\nà tous\r\n\r\n"
                                     "2\r\n00:00:02.000 --> 00:00:04,000\r\nSuite\r\n\r\nbruit\r\n");
            sf::Project subtitled;
            subtitled.duration = 3;
            subtitled.addTrack(sf::TrackKind::Video);
            sf::Id subtitleTrack = subtitled.importSubtitles(cues, "Dialogues");
            auto &subs = subtitled.tracks[0];
            auto written = subtitled.subtitlesSrt(subtitleTrack);
            auto again = sf::parseSrt(written);
            require(cues.size() == 2 && cues[0].text == "Bonjour\nà tous" && subs.id == subtitleTrack &&
                        subs.name == "Dialogues" && subs.clips.size() == 2 && subs.clips[0].title.enabled &&
                        subs.clips[0].duration == 1 && subs.clips[1].start == 2 && subtitled.duration == 4 &&
                        written.starts_with("1\n00:00:01,000 --> 00:00:02,000\nBonjour\nà tous\n\n2\n") &&
                        again.size() == 2 && again[1].text == "Suite",
                    "SubRip cues import as title clips without overlap and export back");
            // A cue added in a gap takes the look of the one before and stops at the next; the style covers the track.
            subtitled.setSubtitleStyle(subtitleTrack, {{"font_size", 60}});
            sf::Id added = subtitled.addSubtitle(subtitleTrack, .2, 2, "Avant");
            bool occupied = false;
            try {
                subtitled.addSubtitle(subtitleTrack, 1.5, 1, "Dessus");
            } catch (const std::exception &) {
                occupied = true;
            }
            auto *addedClip = subtitled.findClip(added);
            require(addedClip && std::abs(addedClip->duration - .8) < 1e-9 && addedClip->title.text == "Avant" &&
                        addedClip->params["font_size"].value == 40 && occupied &&
                        subtitled.setSubtitleStyle(subtitleTrack, {{"y", .3f}}) == 3 &&
                        subtitled.findClip(added)->params["y"].value == .3f &&
                        subtitled.tracks[0].clips[0].params["font_size"].value == 60,
                    "A cue fills a gap up to the next one, and a style applies to every cue of the track");
            // Offline media are found again by name in a folder; collecting copies them beside the project.
            auto saved = s.project;
            auto relinkRoot = sandbox / "relink";
            std::filesystem::create_directories(relinkRoot / "deep");
            std::filesystem::copy_file(sandbox / "silence.wav", relinkRoot / "deep" / "found.wav",
                                       std::filesystem::copy_options::overwrite_existing);
            sf::Project offline;
            offline.duration = 1;
            sf::Clip lost;
            lost.id = offline.allocate();
            lost.source = sx::Narrow((sandbox / "gone" / "found.wav").wstring());
            lost.duration = .1;
            offline.addTrack(sf::TrackKind::Audio).clips = {lost};
            offline.bin = {lost.source, sx::Narrow((sandbox / "gone" / "absent.wav").wstring())};
            s.project = offline;
            sf::Json folder = sf::Json::Object();
            folder["folder"] = sx::Narrow(relinkRoot.wstring());
            auto relinked = sf::Protocol(s).execute("relink_media", folder);
            auto movedTo = s.project.tracks[0].clips[0].source;
            folder["folder"] = sx::Narrow((sandbox / "collected").wstring());
            auto collected = sf::Protocol(s).execute("collect_media", folder);
            auto status = sf::Protocol(s).execute("media_status", sf::Json::Object());
            require(relinked["relinked"].I() == 1 && relinked["missing"].Size() == 1 &&
                        std::filesystem::equivalent(sx::Utf8Path(movedTo), relinkRoot / "deep" / "found.wav") &&
                        collected["copied"].I() == 1 &&
                        std::filesystem::equivalent(sx::Utf8Path(s.project.tracks[0].clips[0].source),
                                                    sandbox / "collected" / "found.wav") &&
                        status["media"].Size() == 2 && status["media"].At(0)["online"].B() &&
                        !status["media"].At(1)["online"].B(),
                    "Offline media relink by name in a folder, collect into one, and report their state");
            {
                // Loudness: a 2 s, 1 kHz tone written to disk, measured offline, then the master reaches -14 LUFS.
                auto tonePath = sandbox / "tone.wav";
                {
                    std::ofstream wav(tonePath, std::ios::binary);
                    auto u32 = [&](uint32_t v) { wav.write(reinterpret_cast<const char *>(&v), 4); };
                    auto u16 = [&](uint16_t v) { wav.write(reinterpret_cast<const char *>(&v), 2); };
                    uint32_t frames = 48000 * 2;
                    wav.write("RIFF", 4);
                    u32(36 + frames * 4);
                    wav.write("WAVEfmt ", 8);
                    u32(16), u16(1), u16(2), u32(48000), u32(48000 * 4), u16(4), u16(16);
                    wav.write("data", 4);
                    u32(frames * 4);
                    for (uint32_t f = 0; f < frames; ++f) {
                        auto value = int16_t(std::lround(.25 * 32767 * std::sin(f * 2 * 3.14159265358979 * 1000 / 48000)));
                        u16(uint16_t(value)), u16(uint16_t(value));
                    }
                }
                sf::Project loud;
                loud.duration = 2;
                sf::Clip toneClip;
                toneClip.id = loud.allocate();
                toneClip.source = sx::Narrow(tonePath.wstring());
                toneClip.duration = 2;
                loud.addTrack(sf::TrackKind::Audio).clips = {toneClip};
                s.project = loud;
                sf::Json target = sf::Json::Object();
                target["target"] = -14;
                auto first = sf::Protocol(s).execute("normalize_loudness", target);
                auto second = sf::Protocol(s).execute("normalize_loudness", target);
                require(first["measured"].Num() > -20 && first["measured"].Num() < -5 &&
                            std::abs(second["measured"].Num() + 14) < .2 &&
                            std::abs(s.project.master - first["gain"].Num() - second["gain"].Num()) < 1e-3,
                        "MCP normalize_loudness measures the mix offline and brings it to -14 LUFS");
                // A measure given by the caller is used as is; the limiter is added once on the master.
                float masterBefore = s.project.master;
                target["measured"] = -20;
                target["true_peak"] = -8;
                target["limit"] = true;
                auto limited = sf::Protocol(s).execute("normalize_loudness", target);
                sf::Protocol(s).execute("normalize_loudness", target);
                int limiters = 0;
                for (auto &effect : s.project.masterMix.effects)
                    if (effect.id == "cydaw.limiter" && effect.params["ceiling"].value == -1)
                        ++limiters;
                require(std::abs(limited["gain"].Num() - 6) < 1e-6 && std::abs(s.project.master - masterBefore - 12) < 1e-4 &&
                            limiters == 1 && limited["limiter"].B(),
                        "A given loudness measure moves the master, and the -1 dBTP limiter is added once");
            }
            {
                // Sequence format, and pictures of another aspect filled or fitted to it.
                sf::Project framed;
                framed.duration = 2;
                sf::Clip stillClip;
                stillClip.id = framed.allocate();
                stillClip.source = png;
                stillClip.duration = 2;
                framed.addTrack(sf::TrackKind::Video).clips = {stillClip};
                s.project = framed;
                sf::Json vertical = sf::Json::Object();
                vertical["width"] = 1080;
                vertical["height"] = 1920;
                vertical["fps"] = 25;
                sf::Protocol(s).execute("set_sequence", vertical);
                bool odd = false;
                try {
                    sf::Json wrong = sf::Json::Object();
                    wrong["width"] = 1081;
                    sf::Protocol(s).execute("set_sequence", wrong);
                } catch (const std::exception &) {
                    odd = true;
                }
                sf::Json fill = sf::Json::Object();
                fill["clips"] = sf::Json::Array();
                fill["clips"].Push(stillClip.id);
                fill["mode"] = "fill";
                auto filled = sf::Protocol(s).execute("fit_clips", fill)["scales"];
                double expected = (320. / 180.) / (1080. / 1920.);
                fill["mode"] = "fit";
                sf::Protocol(s).execute("fit_clips", fill);
                require(s.project.width == 1080 && s.project.height == 1920 && s.project.fps == 25 && odd &&
                            std::abs(filled.At(0).Num() - expected) < 1e-3 &&
                            s.project.tracks[0].clips[0].params["scale"].value == 1,
                        "The sequence format changes, odd sizes are refused, and pictures fill or fit the frame");
            }
            {
                // Speech-like bursts with silences written to disk: jump cuts and ducking on a real file.
                auto writeWav = [&](const std::filesystem::path &path, double seconds, auto value) {
                    std::ofstream wav(path, std::ios::binary);
                    auto u32 = [&](uint32_t v) { wav.write(reinterpret_cast<const char *>(&v), 4); };
                    auto u16 = [&](uint16_t v) { wav.write(reinterpret_cast<const char *>(&v), 2); };
                    uint32_t frames = uint32_t(48000 * seconds);
                    wav.write("RIFF", 4);
                    u32(36 + frames * 4);
                    wav.write("WAVEfmt ", 8);
                    u32(16), u16(1), u16(2), u32(48000), u32(48000 * 4), u16(4), u16(16);
                    wav.write("data", 4);
                    u32(frames * 4);
                    for (uint32_t f = 0; f < frames; ++f) {
                        auto sample = int16_t(std::lround(std::clamp(value(f / 48000.), -1., 1.) * 32767));
                        u16(uint16_t(sample)), u16(uint16_t(sample));
                    }
                };
                auto speechPath = sandbox / "speech.wav";
                writeWav(speechPath, 6, [](double t) {
                    bool talking = t < 1 || (t >= 2 && t < 3) || (t >= 4.5 && t < 5.5);
                    return talking ? .3 * std::sin(t * 2 * 3.14159265358979 * 220) : 0.;
                });
                auto speechSource = sx::Narrow(speechPath.wstring());
                sf::Project talk;
                talk.duration = 6;
                sf::Clip words;
                words.id = talk.allocate();
                words.source = speechSource;
                words.duration = 6;
                talk.addTrack(sf::TrackKind::Audio).clips = {words};
                s.project = talk;
                sf::Json quiet = sf::Json::Object();
                quiet["clip"] = words.id;
                quiet["min_length"] = .4;
                auto jumped = sf::Protocol(s).execute("remove_silences", quiet);
                require(jumped["cuts"].I() == 3 && std::abs(jumped["seconds"].Num() - 2.4) < .07 &&
                            std::abs(s.project.duration - 3.6) < .07 && s.project.tracks[0].clips.size() == 4,
                        "Removing silences keeps the speech with its padding and closes the sequence");
                sf::Project mixdown;
                mixdown.duration = 6;
                sf::Clip song, voiceOver;
                song.id = mixdown.allocate();
                song.source = sx::Narrow((sandbox / "tone.wav").wstring());
                song.duration = 6;
                voiceOver.id = mixdown.allocate();
                voiceOver.source = speechSource;
                voiceOver.duration = 6;
                auto musicTrack = mixdown.addTrack(sf::TrackKind::Audio).id;
                mixdown.tracks.back().clips = {song};
                auto voiceTrack = mixdown.addTrack(sf::TrackKind::Audio).id;
                mixdown.tracks.back().clips = {voiceOver};
                s.project = mixdown;
                sf::Json duck = sf::Json::Object();
                duck["track"] = musicTrack;
                duck["voice"] = voiceTrack;
                auto ducked = sf::Protocol(s).execute("auto_duck", duck);
                auto &gainCurve = s.project.findTrack(musicTrack)->clips[0].params["gain"];
                require(ducked["spans"].I() == 3 && std::abs(gainCurve.at(2.5) + 12) < .01 && std::abs(gainCurve.at(1.6)) < .01 &&
                            std::abs(gainCurve.at(5) + 12) < .01,
                        "Ducking lowers the music by 12 dB while the voice speaks and restores it between");
                // Titles share a free title track; an overlapping one, or new_track, gets its own.
                sf::Project titled;
                titled.duration = 20;
                s.project = titled;
                auto titleAt = [&](double start, bool fresh) {
                    sf::Json t = sf::Json::Object();
                    t["start"] = start;
                    t["duration"] = 3;
                    t["new_track"] = fresh;
                    return sf::Protocol(s).execute("add_title", t)["track"].U();
                };
                auto firstTitle = titleAt(0, false), secondTitle = titleAt(5, false), overlapping = titleAt(6, false),
                     forced = titleAt(12, true);
                require(firstTitle == secondTitle && overlapping != firstTitle && forced != firstTitle &&
                            forced != overlapping && s.project.tracks.size() == 3,
                        "New titles go on a free title track, an overlapping or forced one on a new track");
                // Saving keeps the replaced file as a version, which can be restored in one undo step.
                auto versionDir = sandbox / "versions";
                std::filesystem::remove_all(versionDir);
                std::filesystem::create_directories(versionDir);
                sf::Json save = sf::Json::Object();
                save["path"] = sx::Narrow((versionDir / "Film.sframe").wstring());
                sf::Protocol(s).execute("save_project", save);
                auto firstSaved = s.project.json().Dump(0);
                titleAt(16, false);
                sf::Protocol(s).execute("save_project", save);
                auto versions = sf::Protocol(s).execute("list_versions", sf::Json::Object())["versions"];
                s.history.clear();
                sf::Json restore = sf::Json::Object();
                if (versions.Size())
                    restore["path"] = versions.At(0)["path"];
                if (versions.Size())
                    sf::Protocol(s).execute("restore_version", restore);
                bool restored = s.project.json().Dump(0) == firstSaved;
                require(versions.Size() == 1 && restored && s.history.undo(s.project) && s.project.tracks.size() == 3 &&
                            std::filesystem::exists(versionDir / "Film versions"),
                        "Saving keeps the replaced file as a version, restored as one undoable change");
                // Scene detection on an MP4 whose picture changes at one second.
                sf::Project cutVideo;
                cutVideo.width = 320;
                cutVideo.height = 180;
                cutVideo.duration = 2;
                sf::Clip glow, card;
                glow.id = cutVideo.allocate();
                glow.generator = true;
                glow.duration = 1;
                card.id = cutVideo.allocate();
                card.source = png;
                card.start = 1;
                card.duration = 1;
                cutVideo.addTrack(sf::TrackKind::Video).clips = {glow, card};
                auto cutPath = sx::Narrow((sandbox / "scenes.mp4").wstring());
                sf::exportVideo(cutVideo, s.media, s.renderer, cutPath);
                sf::Project scenes;
                scenes.duration = 2;
                sf::Clip footage;
                footage.id = scenes.allocate();
                footage.source = cutPath;
                footage.duration = 2;
                scenes.addTrack(sf::TrackKind::Video).clips = {footage};
                s.project = scenes;
                sf::Json detect = sf::Json::Object();
                detect["clip"] = footage.id;
                auto found = sf::Protocol(s).execute("detect_scenes", detect)["times"];
                require(found.Size() == 1 && std::abs(found.At(0).Num() - 1) <= 1. / 30 + 1e-6 &&
                            s.project.tracks[0].clips.size() == 2,
                        "Scene detection finds the picture change of a real MP4 and cuts the clip there");
                // Importing a heavy video prepares its 720p proxy; unused media leave the bin.
                {
                    sf::Project heavy;
                    heavy.width = 2560;
                    heavy.height = 1600;
                    heavy.duration = .2;
                    sf::Clip glowClip;
                    glowClip.id = heavy.allocate();
                    glowClip.generator = true;
                    glowClip.duration = .2;
                    heavy.addTrack(sf::TrackKind::Video).clips = {glowClip};
                    auto heavyPath = sx::Narrow((sandbox / "heavy.mp4").wstring());
                    sf::exportVideo(heavy, s.media, s.renderer, heavyPath);
                    // Proxies live with the interactive preview's cache: a session of its own, loading in the background.
                    sf::Session preview;
                    preview.renderer.initialize();
                    preview.renderer.library = s.renderer.library;
                    preview.media.enableBackground(sandbox / "cache");
                    std::error_code stale;
                    std::filesystem::remove(preview.media.proxyPath(heavyPath), stale);
                    for (int wait = 0; wait < 1000 && preview.media.get(heavyPath).loading; ++wait) {
                        preview.media.poll();
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    preview.project.duration = 1;
                    sf::Json importArgs = sf::Json::Object();
                    importArgs["path"] = heavyPath;
                    importArgs["proxy"] = true;
                    auto imported = sf::Protocol(preview).execute("import_media", importArgs);
                    sf::Json proxyJobs = sf::Json::Array();
                    if (imported.Has("proxy_job"))
                        proxyJobs.Push(imported["proxy_job"]);
                    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(120);
                    while (proxyJobs.Size() && preview.exports.status(proxyJobs.At(0).U()).active() &&
                           std::chrono::steady_clock::now() < until)
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    auto proxyFile = preview.media.proxyPath(heavyPath);
                    sf::MediaAsset proxyVideo;
                    bool proxyReady = std::filesystem::exists(proxyFile);
                    if (proxyReady)
                        proxyVideo.open(sx::Narrow(proxyFile.wstring()), {}, false);
                    s.project = preview.project;
                    s.project.bin.push_back(sx::Narrow((sandbox / "unused.wav").wstring()));
                    auto cleaned = sf::Protocol(s).execute("remove_unused_media", sf::Json::Object());
                    require(proxyJobs.Size() == 1 && proxyReady && proxyVideo.frame.height == 720 &&
                                cleaned["removed"].Size() == 1 && s.project.bin.size() == 1 && s.project.bin[0] == heavyPath,
                            "A heavy video imported with proxies gets a 720p proxy, and unused media leave the bin");
                }
                // Delivery: an animated GIF, one file per chapter, and the span of the selection.
                s.project = cutVideo;
                sf::Json gif = sf::Json::Object();
                gif["path"] = sx::Narrow((sandbox / "clip.gif").wstring());
                gif["width"] = 160;
                gif["fps"] = 10;
                int gifFrames = sf::Protocol(s).execute("export_gif", gif)["frames"].I();
                UINT decodedFrames = 0, gifWidth = 0, gifHeight = 0;
                {
                    sf::Com<IWICImagingFactory> factory;
                    sf::Com<IWICBitmapDecoder> decoder;
                    sf::Com<IWICBitmapFrameDecode> first;
                    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(&factory))) &&
                        SUCCEEDED(factory->CreateDecoderFromFilename((sandbox / "clip.gif").c_str(), nullptr, GENERIC_READ,
                                                                     WICDecodeMetadataCacheOnDemand, &decoder)) &&
                        SUCCEEDED(decoder->GetFrameCount(&decodedFrames)) && SUCCEEDED(decoder->GetFrame(0, &first)))
                        first->GetSize(&gifWidth, &gifHeight);
                }
                require(gifFrames == 20 && decodedFrames == 20 && gifWidth == 160 && gifHeight == 90,
                        "An animated GIF of two seconds at 10 frames per second decodes as 20 frames of 160 x 90");
                auto waitJobs = [&](const sf::Json &jobs) {
                    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(120);
                    for (bool busy = true; busy && std::chrono::steady_clock::now() < until;) {
                        busy = false;
                        for (auto &job : jobs.Elements())
                            busy = busy || s.exports.status(job.U()).active();
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                };
                s.project.addMarker(.5, "Intro");
                s.project.addMarker(1.2, "Suite");
                auto chapterDir = sandbox / "chapitres";
                std::filesystem::remove_all(chapterDir);
                sf::Json chapters = sf::Json::Object();
                chapters["folder"] = sx::Narrow(chapterDir.wstring());
                auto chapterJobs = sf::Protocol(s).execute("export_chapters", chapters)["jobs"];
                waitJobs(chapterJobs);
                auto chapterLength = [&](const wchar_t *file) {
                    sf::MediaAsset part;
                    part.open(sx::Narrow((chapterDir / file).wstring()), {}, false);
                    return part.duration;
                };
                double opening = chapterLength(L"01 Untitled.mp4"), middle = chapterLength(L"02 Intro.mp4"),
                       last = chapterLength(L"03 Suite.mp4");
                require(chapterJobs.Size() == 3 && std::abs(opening - .5) < .07 && std::abs(middle - .7) < .07 &&
                            std::abs(last - .8) < .07,
                        "Each chapter, from the start or a marker to the next, exports to its own MP4");
                // Regions export on their own span only.
                s.project.findMarker(s.project.markers[1].id)->duration = .4;
                auto regionDir = sandbox / "regions";
                std::filesystem::remove_all(regionDir);
                sf::Json regionArgs = sf::Json::Object();
                regionArgs["folder"] = sx::Narrow(regionDir.wstring());
                regionArgs["regions"] = true;
                auto regionJobs = sf::Protocol(s).execute("export_chapters", regionArgs)["jobs"];
                waitJobs(regionJobs);
                sf::MediaAsset regionPart;
                regionPart.open(sx::Narrow((regionDir / L"01 Suite.mp4").wstring()), {}, false);
                require(regionJobs.Size() == 1 && std::abs(regionPart.duration - .4) < .07,
                        "Exporting regions writes one MP4 per marker with a length, over that length");
                s.selectClip(card.id);
                sf::Json selectionExport = sf::Json::Object();
                selectionExport["path"] = sx::Narrow((sandbox / "selection.wav").wstring());
                selectionExport["kind"] = "audio";
                selectionExport["range"] = "selection";
                sf::Json selectionJobs = sf::Json::Array();
                selectionJobs.Push(sf::Protocol(s).execute("start_export", selectionExport)["job"]);
                waitJobs(selectionJobs);
                sf::MediaAsset selectionAudio;
                selectionAudio.open(sx::Narrow((sandbox / "selection.wav").wstring()));
                require(std::abs(selectionAudio.duration - 1) < .05,
                        "Exporting the selection covers the span of the selected clips only");
                // Reverse playback: an export of the clip played backwards shows its frames in the opposite order.
                sf::Project backwards;
                backwards.width = 320;
                backwards.height = 180;
                backwards.duration = 2;
                sf::Clip reversed = footage;
                reversed.id = backwards.allocate();
                reversed.reverse = true;
                backwards.addTrack(sf::TrackKind::Video).clips = {reversed};
                auto reversedPath = sx::Narrow((sandbox / "reversed.mp4").wstring());
                sf::exportVideo(backwards, s.media, s.renderer, reversedPath);
                auto meanLuma = [](const std::string &path, double time) {
                    sf::MediaAsset video;
                    video.open(path, {}, false);
                    auto &image = video.at(time);
                    double sum = 0;
                    for (size_t i = 0; i < image.pixels.size(); i += 4)
                        sum += .0722 * image.pixels[i] + .7152 * image.pixels[i + 1] + .2126 * image.pixels[i + 2];
                    return sum / std::max<size_t>(1, image.pixels.size() / 4);
                };
                sf::MediaAsset reversedVideo;
                reversedVideo.open(reversedPath, {}, false);
                auto reversedCuts = sf::detectScenes(reversedVideo, 0, 2, 1. / 30, .15f);
                double forwardLate = meanLuma(cutPath, 1.5), reversedEarly = meanLuma(reversedPath, .5),
                       forwardEarly = meanLuma(cutPath, .5);
                require(std::abs(reversed.sourceTime(0) - 2) < 1e-3 && std::abs(reversed.sourceTime(1.5) - .5) < 1e-3 &&
                            sf::Project::fromJson(backwards.json()).tracks[0].clips[0].reverse &&
                            reversedCuts.size() == 1 && std::abs(reversedCuts[0] - 1) <= 1. / 30 + 1e-6 &&
                            std::abs(reversedEarly - forwardLate) < std::abs(reversedEarly - forwardEarly),
                        "A reversed clip renders its frames backwards, is saved, and keeps its picture change");
                // Decoder repositionings: slow motion reads on, stepping back seeks every frame.
                sf::MediaAsset slow, back;
                slow.open(cutPath, {}, false);
                for (int k = 0; k < 120; ++k)
                    slow.at(k / 120.);
                back.open(cutPath, {}, false);
                for (int k = 0; k < 20; ++k)
                    back.at(1.9 - k / 30.);
                require(slow.seeks <= 2 && back.seeks >= 19,
                        "Slow motion reads frames on without seeking, while stepping back seeks each frame");
                // Reversed sound: a rising ramp is heard falling.
                auto rampPath = sandbox / "ramp.wav";
                writeWav(rampPath, 2, [](double t) { return .25 * t; });
                sf::Project rampProject;
                rampProject.duration = 2;
                sf::Clip ramp;
                ramp.id = rampProject.allocate();
                ramp.source = sx::Narrow(rampPath.wstring());
                ramp.duration = 2;
                rampProject.addTrack(sf::TrackKind::Audio).clips = {ramp};
                auto sampleAt = [&](const sf::Project &project, double time) {
                    sf::MediaCache offline;
                    std::vector<float> block;
                    offline.mix(project, time, 64, block);
                    return block.empty() ? 0.f : block[0];
                };
                float forwardEarlyLevel = sampleAt(rampProject, .1);
                rampProject.tracks[0].clips[0].reverse = true;
                float reversedEarlyLevel = sampleAt(rampProject, .1), reversedLateLevel = sampleAt(rampProject, 1.5);
                require(reversedEarlyLevel > forwardEarlyLevel * 5 && reversedLateLevel < reversedEarlyLevel * .5 &&
                            reversedLateLevel > 0,
                        "A reversed clip plays its sound backwards: a rising ramp is heard falling");
                // Speed ramps: the source time is the integral of the animated speed factor.
                sf::Clip rampedClip;
                rampedClip.duration = 4;
                rampedClip.params["speed_ramp"].setKey({0, 1, sf::Curve::Linear, 0});
                rampedClip.params["speed_ramp"].setKey({4, 3, sf::Curve::Linear, 0});
                double rampMiddle = rampedClip.sourceTime(2), rampEnd = rampedClip.sourceTime(4);
                rampedClip.reverse = true;
                double rampReversedStart = rampedClip.sourceTime(0);
                require(std::abs(rampMiddle - 3) < 1e-3 && std::abs(rampEnd - 8) < 1e-3 &&
                            std::abs(rampReversedStart - 8) < 1e-3 && rampedClip.ramped(),
                        "A speed ramp integrates its factor: 1 to 3 over 4 s reads 8 s of source, reversed too");
                // A ramp at twice the speed reads the sound twice as far.
                auto rampTwice = rampProject;
                rampTwice.tracks[0].clips[0].reverse = false;
                float plainLevel = sampleAt(rampTwice, .4);
                rampTwice.tracks[0].clips[0].params["speed_ramp"].setKey({0, 2, sf::Curve::Linear, 0});
                rampTwice.tracks[0].clips[0].params["speed_ramp"].setKey({2, 2, sf::Curve::Linear, 0});
                float rampedLevel = sampleAt(rampTwice, .4);
                require(std::abs(rampedLevel / plainLevel - 2) < .1,
                        "A clip's sound follows its speed ramp: at twice the speed, the ramp is read twice as far");
                // Animation presets through MCP: fades, Ken Burns, and a ramp on a sound but no zoom on it.
                sf::Project animated;
                animated.duration = 6;
                sf::Clip picture, voice;
                picture.id = animated.allocate();
                picture.generator = true;
                picture.duration = 4;
                voice.id = animated.allocate();
                voice.source = ramp.source;
                voice.duration = 2;
                animated.addTrack(sf::TrackKind::Video).clips = {picture};
                animated.addTrack(sf::TrackKind::Audio).clips = {voice};
                s.project = animated;
                auto animate = [&](const char *preset, std::initializer_list<sf::Id> ids) {
                    sf::Json args = sf::Json::Object();
                    args["clips"] = sf::Json::Array();
                    for (auto id : ids)
                        args["clips"].Push(id);
                    args["preset"] = preset;
                    return sf::Protocol(s).execute("apply_animation", args)["clips"].I();
                };
                int faded = animate("fade_in_out", {picture.id, voice.id});
                animate("ken_burns", {picture.id});
                int ramped = animate("ramp_down", {voice.id});
                auto *pictureNow = s.project.findClip(picture.id);
                auto *voiceNow = s.project.findClip(voice.id);
                require(faded == 1 && ramped == 1 && pictureNow->param("opacity", 0, 1) < .01f &&
                            pictureNow->param("opacity", 2, 0) > .99f && pictureNow->param("opacity", 4, 1) < .01f &&
                            std::abs(pictureNow->param("scale", 4, 0) - 1.18f) < 1e-4 && voiceNow->ramped() &&
                            !voiceNow->params.contains("opacity"),
                        "Animation presets fade and drift pictures, and ramp the speed of a sound");
                // A speed ramp preset reaches the whole linked group.
                animated.tracks[0].clips[0].params.clear();
                s.project = animated;
                s.project.linkClips(picture.id, voice.id);
                animate("ramp_up", {picture.id});
                require(s.project.findClip(picture.id)->ramped() && s.project.findClip(voice.id)->ramped(),
                        "A speed ramp preset on a picture also ramps its linked sound");
                // Sound sync: a recorder that started 1.3 s into the scene is moved to line up with the camera.
                auto scene = [](double t) {
                    double value = 0;
                    unsigned seed = 7;
                    for (double hit : {.4, 1.1, 1.5, 2.6, 3.0, 4.2, 5.1, 6.3, 7.0})
                        if (t >= hit && t < hit + .15) {
                            seed = seed * 1103515245u + unsigned((t - hit) * 48000) * 2654435761u;
                            value += ((seed >> 9) / double(1 << 23) - 1) * std::exp(-(t - hit) * 30);
                        }
                    return .5 * value;
                };
                writeWav(sandbox / "camera.wav", 8, scene);
                writeWav(sandbox / "recorder.wav", 6, [&](double t) { return scene(t + 1.3); });
                sf::Project shoot;
                shoot.duration = 10;
                sf::Clip camera, recorder;
                camera.id = shoot.allocate();
                camera.source = sx::Narrow((sandbox / "camera.wav").wstring());
                camera.start = 2;
                camera.duration = 8;
                recorder.id = shoot.allocate();
                recorder.source = sx::Narrow((sandbox / "recorder.wav").wstring());
                recorder.duration = 6;
                shoot.addTrack(sf::TrackKind::Audio).clips = {camera};
                shoot.addTrack(sf::TrackKind::Audio).clips = {recorder};
                s.project = shoot;
                sf::Json sync = sf::Json::Object();
                sync["clip"] = recorder.id;
                sync["reference"] = camera.id;
                auto synced = sf::Protocol(s).execute("sync_audio", sync);
                require(std::abs(s.project.findClip(recorder.id)->start - 3.3) <= 1. / 30 + 1e-6 &&
                            synced["confidence"].Num() > .8,
                        "Sound sync moves a recorder 1.3 s into the scene to line up with the camera");
                // Voice enhancement: the gate lowers the room noise between phrases, the speech stays.
                writeWav(sandbox / "room.wav", 3, [](double t) {
                    if (t >= .5 && t < 1.5)
                        return .3 * std::sin(t * 2 * 3.14159265358979 * 220);
                    return .003 * std::sin(t * 2 * 3.14159265358979 * 1733) * std::sin(t * 37);
                });
                sf::Project room;
                room.duration = 3;
                sf::Clip speaker;
                speaker.id = room.allocate();
                speaker.source = sx::Narrow((sandbox / "room.wav").wstring());
                speaker.duration = 3;
                room.addTrack(sf::TrackKind::Audio).clips = {speaker};
                auto levels = [&](const sf::Project &project) {
                    sf::MediaCache offline;
                    std::vector<float> mixed;
                    offline.mix(project, 0, 48000 * 3, mixed);
                    auto rms = [&](double from, double to) {
                        double sum = 0;
                        size_t a0 = size_t(from * 48000) * 2, b0 = size_t(to * 48000) * 2;
                        for (size_t i = a0; i < b0 && i < mixed.size(); ++i)
                            sum += double(mixed[i]) * mixed[i];
                        return std::sqrt(sum / double(std::max<size_t>(1, b0 - a0)));
                    };
                    return std::pair{rms(1, 1.4), rms(2.2, 2.8)};
                };
                auto [speechBefore, noiseBefore] = levels(room);
                s.project = room;
                sf::Json voiceArgs = sf::Json::Object();
                voiceArgs["clips"] = sf::Json::Array();
                voiceArgs["clips"].Push(speaker.id);
                sf::Protocol(s).execute("enhance_voice", voiceArgs);
                auto [speechAfter, noiseAfter] = levels(s.project);
                require(s.project.tracks[0].clips[0].effects.size() == 3 && noiseAfter < noiseBefore * .5 &&
                            speechAfter > speechBefore * .5,
                        "Enhancing a voice gates the room noise between phrases and keeps the speech");
                // Paste attributes: effects and transform move to another clip with their keys.
                sf::Project styled;
                styled.duration = 20;
                sf::Clip model, target, sound;
                model.id = styled.allocate();
                model.generator = true;
                model.duration = 4;
                model.params["scale"].setKey({1, .5f});
                model.params["scale"].setKey({3, 1.5f});
                sf::Effect look;
                look.id = "color";
                look.params["amount"].setKey({2, .8f});
                model.effects = {look};
                target = model;
                target.id = styled.allocate();
                target.start = 10;
                target.effects.clear();
                target.params.clear();
                target.params["opacity"] = sf::Parameter{.3f};
                sound.id = styled.allocate();
                sound.source = ramp.source;
                sound.duration = 2;
                styled.addTrack(sf::TrackKind::Video).clips = {model, target};
                styled.addTrack(sf::TrackKind::Audio).clips = {sound};
                sf::AttributeChoice everything;
                int pastedClips = styled.pasteAttributes(model, sf::TrackKind::Video, {target.id, sound.id}, everything);
                auto *styledTarget = styled.findClip(target.id);
                require(pastedClips == 2 && styledTarget->effects.size() == 1 &&
                            styledTarget->effects[0].params["amount"].keys[0].time == 12 &&
                            styledTarget->params["scale"].keys[1].time == 13 && !styledTarget->params.contains("opacity") &&
                            styled.findClip(sound.id)->effects.empty(),
                        "Pasted attributes carry effects and transform with their keys, not onto another kind of track");
            }
            s.project = saved;
            s.history.clear();
        }
        {
            // One still per marker, named after the base file, its number and the marker.
            auto unmarked = s.project;
            s.project.addMarker(.05, "Début: plan");
            s.project.addMarker(.15, "End");
            sf::Json stills = sf::Json::Object();
            stills["path"] = sx::Narrow((sandbox / "chapitre.png").wstring());
            auto files = sf::Protocol(s).execute("export_marker_stills", stills)["files"];
            sf::MediaAsset firstStill;
            firstStill.open(sx::Narrow((sandbox / sx::Utf8Path("chapitre_01_Début_ plan.png")).wstring()));
            require(files.Size() == 2 && firstStill.image && firstStill.frame.width == unsigned(s.project.width) &&
                        std::filesystem::exists(sandbox / "chapitre_02_End.png"),
                    "MCP export_marker_stills renders one named still per marker");
            s.project = unmarked;
        }
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
        {
            // Normalising reads the clip's source peak (0.5 → -6 dBFS) and lifts it to -1 dBFS.
            sf::Json normalise = sf::Json::Object();
            normalise["clips"] = sf::Json::Array();
            normalise["clips"].Push(clip.id);
            auto gains = sf::Protocol(s).execute("normalize_audio", normalise)["gains"];
            auto *normalised = s.project.findClip(clip.id);
            float expected = -1 - 20 * std::log10(.5f);
            require(gains.Size() == 1 && std::abs(normalised->params["gain"].value - expected) < .05f,
                    "MCP normalize_audio sets the clip gain so its peak reaches -1 dBFS");
            normalised->params.erase("gain");
            s.history.clear();
        }
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

#include "mcp/Protocol.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mfapi.h>

static int checks = 0;
static void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
    ++checks;
    std::cout << message << std::endl;
}
int main(int argc, char **argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    int result = 0;
    try {
        auto root = sx::Utf8Path(argv[1]);
        auto directory = std::filesystem::current_path() / "test-output/mixer";
        std::filesystem::create_directories(directory);
        sf::Session s;
        s.renderer.library.scan(root);
        s.project = {};
        s.project.duration = .1;
        s.project.width = 320;
        s.project.height = 180;
        auto path = sx::Narrow((directory / "constant.wav").wstring());
        {
            std::ofstream wav(sx::Utf8Path(path), std::ios::binary);
            auto write = [&](auto value) {
                wav.write(reinterpret_cast<const char *>(&value), sizeof(value));
            };
            wav.write("RIFF", 4);
            write(uint32_t(36 + 4800 * 4));
            wav.write("WAVEfmt ", 8);
            write(uint32_t(16));
            write(uint16_t(1));
            write(uint16_t(2));
            write(uint32_t(48000));
            write(uint32_t(192000));
            write(uint16_t(4));
            write(uint16_t(16));
            wav.write("data", 4);
            write(uint32_t(4800 * 4));
            for (int n = 0; n < 4800; ++n) {
                write(int16_t(8192));
                write(int16_t(4096));
            }
        }
        auto trackId = s.project.addTrack(sf::TrackKind::Audio).id;
        sf::Clip clip;
        clip.id = s.project.allocate();
        clip.duration = .1;
        clip.source = path;
        auto clipId = clip.id;
        s.project.findTrack(trackId)->clips.push_back(clip);
        sf::Protocol protocol(s);
        auto a = [&](const char *scope, sf::Id target) {
            auto j = sf::Json::Object();
            j["scope"] = scope;
            j["target"] = target;
            return j;
        };
        auto render = [&] {
            std::vector<float> out;
            s.media.resetAudio();
            s.media.mix(s.project, 0, 4096, out);
            return out;
        };
        auto closeTo = [](float x, float y) { return std::abs(x - y) < 1e-5; };
        auto original = s.project;
        require(closeTo(render()[0], .25f), "Legacy neutral mix keeps the source amplitude");
        s.project.findTrack(trackId)->pan = .6f;
        s.project.findClip(clipId)->params["pan"] = {-.6f};
        require(closeTo(render()[0], .25f) && closeTo(render()[1], .125f),
                "Legacy clip and track balance still combine before the channel DSP");
        s.project = original;
        auto busId = protocol.execute("add_bus", {})["bus"].U();
        auto args = a("track", trackId);
        args["output"] = busId;
        protocol.execute("set_mix_channel", args);
        args = a("bus", busId);
        args["gain"] = -6;
        args["pan"] = .5;
        protocol.execute("set_mix_channel", args);
        auto mixed = render();
        float six = std::pow(10.f, -6.f / 20);
        require(closeTo(mixed[0], .25f * six * .5f) && closeTo(mixed[1], .125f * six),
                "A routed bus applies its own stereo gain and balance");
        require(closeTo(s.media.mixPeaks[trackId][0], .25f) &&
                    closeTo(s.media.mixPeaks[busId][0], mixed[0]) &&
                    closeTo(s.media.mixPeaks[0][1], mixed[1]),
                "Track, bus and master meters measure their own samples");
        auto second = s.project.addBus().id;
        s.project.findBus(busId)->mix.output = second;
        s.project.findBus(second)->gain = -6;
        std::reverse(s.project.buses.begin(), s.project.buses.end());
        require(closeTo(render()[0], .25f * six * six * .5f),
                "Nested buses process in dependency order, independent of UI ordering");
        auto frozen = s.project.json().Dump(0);
        auto reject = [&](const char *tool, sf::Json request, const char *message) {
            bool rejected = false;
            try {
                protocol.execute(tool, request);
            } catch (...) {
                rejected = true;
            }
            require(rejected && s.project.json().Dump(0) == frozen, message);
        };
        args = a("bus", second);
        args["output"] = busId;
        reject("set_mix_channel", args, "A main-output feedback loop is rejected atomically");
        args = a("bus", second);
        args["bus"] = busId;
        args["enabled"] = false;
        reject("set_send", args, "A disabled feedback send cannot hide a routing cycle");
        args = a("track", trackId);
        args["output"] = 99999;
        reject("set_mix_channel", args, "Unknown routing destinations preserve the project");
        args = a("bus", busId);
        args["key"] = "gain";
        args["value"] = 30;
        reject("set_mix_parameter", args, "Out-of-range mixer automation is rejected");
        args["key"] = "unknown";
        args["value"] = 0;
        reject("set_mix_parameter", args, "Unknown mixer parameters cannot be silently ignored");
        s.project = original;
        busId = s.project.addBus().id;
        s.project.findTrack(trackId)->gain = -12;
        args = a("track", trackId);
        args["bus"] = busId;
        args["gain"] = -6;
        args["pre_fader"] = false;
        protocol.execute("set_send", args);
        float twelve = std::pow(10.f, -12.f / 20);
        require(closeTo(render()[0], .25f * twelve * (1 + six)),
                "Post-fader sends follow the source channel gain");
        args["pre_fader"] = true;
        protocol.execute("set_send", args);
        require(closeTo(render()[0], .25f * (twelve + six)),
                "Pre-fader sends remain independent of the source fader");
        s.project.findTrack(trackId)->muted = true;
        require(closeTo(render()[0], 0), "Source mute silences both direct and pre-fader paths");
        s.project.findTrack(trackId)->muted = false;
        s.project.findBus(busId)->muted = true;
        require(closeTo(render()[0], .25f * twelve), "Bus mute leaves the source's dry path intact");
        s.project.findBus(busId)->muted = false;
        args["enabled"] = false;
        protocol.execute("set_send", args);
        require(closeTo(render()[0], .25f * twelve), "Send bypass preserves the dry mix");
        s.project = original;
        busId = s.project.addBus().id;
        s.project.findTrack(trackId)->mix.output = busId;
        s.project.findBus(busId)->mix.effects = {sf::instantiateAudioEffect("cydaw.transient-shaper")};
        s.project.findBus(busId)->mix.effects[0].params["output"] = {-6};
        s.project.findTrack(trackId)->mix.effects = {sf::instantiateAudioEffect("cydaw.transient-shaper")};
        s.project.findTrack(trackId)->mix.effects[0].params["output"] = {-6};
        s.project.masterMix.effects = {sf::instantiateAudioEffect("cydaw.transient-shaper")};
        s.project.masterMix.effects[0].params["output"] = {-6};
        require(std::abs(render()[4094 * 2] - .25f * six * six * six) < 1e-5,
                "Track, bus and master DSP stacks process in sequence");
        args = a("bus", busId);
        args["effect"] = 0;
        args["enabled"] = false;
        protocol.execute("set_mix_effect", args);
        require(std::abs(render()[4094 * 2] - .25f * six * six) < 1e-5,
                "Bus DSP bypass removes only its own processing");
        s.project = original;
        busId = s.project.addBus().id;
        s.project.findTrack(trackId)->mix.output = busId;
        s.project.findTrack(trackId)->gain = 12;
        s.project.findBus(busId)->gain = -12;
        auto copy = *s.project.findClip(clipId);
        copy.id = s.project.allocate();
        s.project.findTrack(trackId)->clips.push_back(copy);
        require(closeTo(render()[0], .5f),
                "Intermediate bus sums retain headroom before final output clipping");
        s.project.findBus(busId)->gain = 0;
        auto clipped = render();
        require(clipped[0] == 1 && s.media.mixPeaks[0][0] > 1.9f,
                "Final output clamps samples while meters retain overload evidence");
        s.project = original;
        args = a("track", trackId);
        args["key"] = "gain";
        args["time"] = 0;
        args["value"] = -12;
        protocol.execute("set_mix_keyframe", args);
        args["time"] = .1;
        args["value"] = 0;
        protocol.execute("set_mix_keyframe", args);
        require(closeTo(s.project.findTrack(trackId)->mix.param("gain", .05, 0), -6),
                "Channel automation uses sequence time");
        require(closeTo(render()[0], .25f * twelve), "Channel automation changes the mixed PCM");
        auto before = s.project.json().Dump(0);
        s.history.clear();
        args["previous_time"] = .1;
        args["time"] = .08;
        protocol.execute("set_mix_keyframe", args);
        protocol.execute("undo", {});
        require(s.project.json().Dump(0) == before && !s.history.undo(s.project),
                "Moving an automation point is one undo step");
        auto saved = s.project.json();
        require(sf::Project::fromJson(saved).json().Dump(0) == saved.Dump(0),
                "Schema 5 preserves channel automation");
        for (int schema = 1; schema <= 4; ++schema) {
            saved["schema"] = schema;
            auto migrated = sf::Project::fromJson(saved);
            require(migrated.buses.empty() && migrated.masterMix.effects.empty() &&
                        migrated.findTrack(trackId)->mix.params.empty(),
                    "Older schemas migrate with neutral mix channels");
        }
        args = a("track", trackId);
        args["key"] = "gain";
        protocol.execute("remove_mix_keyframe", args);
        require(closeTo(render()[0], .25f), "Clearing channel automation restores its base fader");
        s.project = original;
        busId = s.project.addBus().id;
        s.project.findTrack(trackId)->mix.output = busId;
        s.project.findBus(busId)->locked = true;
        frozen = s.project.json().Dump(0);
        args = a("bus", busId);
        args["id"] = "cydaw.limiter";
        reject("add_mix_effect", args, "Locked buses refuse DSP changes");
        args = a("bus", busId);
        args["locked"] = false;
        protocol.execute("set_mix_channel", args);
        s.project.findTrack(trackId)->locked = true;
        frozen = s.project.json().Dump(0);
        args = sf::Json::Object();
        args["bus"] = busId;
        reject("remove_bus", args, "Removing a bus cannot rewrite a locked source track");
        s.project.findTrack(trackId)->locked = false;
        protocol.execute("remove_bus", args);
        require(!s.project.findBus(busId) && !s.project.findTrack(trackId)->mix.output,
                "Bus removal reconnects surviving sources to master");
        protocol.execute("undo", {});
        require(s.project.findBus(busId) != nullptr, "Undo restores removed routing and bus state");
        s.project = original;
        busId = s.project.addBus().id;
        s.project.findClip(clipId)->duration = .005;
        auto delay = sf::instantiateAudioEffect("cydaw.ping-pong");
        delay.params["time"] = {40};
        delay.params["mix"] = {1};
        delay.params["feedback"] = {.5};
        s.project.findBus(busId)->mix.effects = {delay};
        s.project.findTrack(trackId)->mix.sends = {{busId, 0, true, true}};
        std::vector<float> block;
        s.media.resetAudio();
        s.media.mix(s.project, 0, 1024, block);
        s.media.mix(s.project, 1024. / 48000, 1024, block);
        require(*std::max_element(block.begin(), block.end()) > .1f,
                "Bus delay tails continue after every source clip has ended");
        s.media.mix(s.project, 1, 1024, block);
        require(*std::max_element(block.begin(), block.end()) == 0,
                "Seeking resets bus delay state and prevents stale echoes");
        auto graph = sf::audioGraph(s.project);
        require(std::any_of(graph.nodes.begin(), graph.nodes.end(),
                            [](const auto &node) { return node.processorKey == "cydaw.ping-pong"; }) &&
                    graph.nodes.size() > 4,
                "CyDAW validates the actual bus, send and DSP topology");
        require(protocol.execute("get_audio_graph", {})["graph"]["version"].S() == "2.0.0",
                "MCP exposes the versioned stereo routing graph");
        auto exportedPath = sx::Narrow((directory / "tail.wav").wstring());
        sf::exportWav(s.project, s.media, exportedPath);
        sf::MediaAsset exported;
        exported.open(exportedPath);
        require(exported.pcm[2000 * 2] > .1f, "Exported WAV preserves the delayed bus tail");
        auto routedSnapshot = s.project;
        auto job = s.exports.start(s.project, s.renderer.library, sf::ExportKind::Audio,
                                   sx::Narrow((directory / "background.wav").wstring()));
        s.project.buses.clear();
        s.project.findTrack(trackId)->mix = {};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (s.exports.status(job).active() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(s.exports.status(job).state == sf::ExportState::Completed,
                "A background audio snapshot retains its original bus topology");
        sf::MediaAsset background;
        background.open(sx::Narrow((directory / "background.wav").wstring()));
        require(std::any_of(background.pcm.begin() + 1920 * 2, background.pcm.begin() + 2150 * 2,
                            [](float x) { return std::abs(x) > .0001f; }),
                "Background WAV contains its bus tail after live routing changes");
        auto moviePath = sx::Narrow((directory / "bus-tail.mp4").wstring());
        auto movieJob = s.exports.start(routedSnapshot, s.renderer.library, sf::ExportKind::Video, moviePath);
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (s.exports.status(movieJob).active() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(s.exports.status(movieJob).state == sf::ExportState::Completed,
                "Background MP4 export supports the same routed bus snapshot");
        sf::MediaAsset movie;
        movie.open(moviePath);
        require(movie.hasAudio && movie.pcm.size() > 2150 * 2 &&
                    std::any_of(movie.pcm.begin() + 1920 * 2, movie.pcm.begin() + 2150 * 2,
                                [](float x) { return std::abs(x) > .03f; }),
                "Decoded AAC retains bus audio after the source clips end");
        {
            sf::Project child;
            child.width = child.height = 64;
            child.duration = .1;
            sf::Clip sound;
            sound.id = child.allocate();
            sound.source = path;
            sound.duration = .1;
            child.addTrack(sf::TrackKind::Audio).clips = {sound};
            sf::Project parent;
            parent.width = parent.height = 64;
            parent.duration = .1;
            sf::Clip container;
            container.id = parent.allocate();
            container.start = .02;
            container.duration = .08;
            container.speed = 2;
            container.composition = child.json();
            parent.addTrack(sf::TrackKind::Video).clips = {container};
            sf::MediaCache nested, direct;
            std::vector<float> inner, outer;
            direct.mix(child, .02, 64, inner);
            nested.mix(parent, .03, 64, outer);
            require(std::abs(inner[0] - outer[0]) < 1e-5 && std::abs(inner[1] - outer[1]) < 1e-5,
                    "Audio inside a video container follows its parent source time and speed");
            nested.mix(parent, 0, 64, outer);
            require(std::all_of(outer.begin(), outer.end(), [](float x) { return x == 0; }),
                    "Container audio stays silent before the parent clip starts");
            parent.tracks[0].muted = true;
            nested.mix(parent, .03, 64, outer);
            require(std::all_of(outer.begin(), outer.end(), [](float x) { return x == 0; }),
                    "Muting a video container also mutes its inner audio timeline");
            auto echo = sf::instantiateAudioEffect("cydaw.ping-pong");
            echo.params["time"] = {40};
            echo.params["mix"] = {1};
            echo.params["feedback"] = {.5};
            child.tracks[0].mix.effects = {echo};
            parent.tracks[0].muted = false;
            auto &item = parent.tracks[0].clips[0];
            item.start = 0;
            item.speed = 1;
            item.composition = child.json();
            nested.resetAudio();
            direct.resetAudio();
            direct.mix(child, 0, 1024, inner);
            nested.mix(parent, 0, 1024, outer);
            direct.mix(child, 1024. / 48000, 1024, inner);
            nested.mix(parent, 1024. / 48000, 1024, outer);
            float deviation = 0;
            for (size_t n = 0; n < outer.size(); ++n)
                deviation = std::max(deviation, std::abs(inner[n] - outer[n]));
            require(deviation < 1e-5 && *std::max_element(outer.begin(), outer.end()) > .1f,
                    "Container interpolation keeps CyDAW delay state continuous between audio blocks");
            item.params["pan"] = {-1};
            nested.mix(parent, 2048. / 48000, 1024, outer);
            bool rightSilent = true;
            for (size_t n = 1; n < outer.size(); n += 2)
                rightSilent &= outer[n] == 0;
            require(rightSilent && *std::max_element(outer.begin(), outer.end()) > .05f,
                    "Container pan balances the embedded mix without discarding its DSP tail");
        }
        std::cout << checks << " mixer checks passed.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        result = 1;
    }
    MFShutdown();
    CoUninitialize();
    return result;
}

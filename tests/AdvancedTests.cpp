#include "adapters/DspGraph.h"
#include "mcp/Protocol.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mfapi.h>
#include <thread>
static int checks = 0;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << message << std::endl;
}
template <class F> static bool rejects(F call) {
    try {
        call();
        return false;
    } catch (const std::exception &) {
        return true;
    }
}
int main(int argc, char **argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    int result = 0;
    try {
        auto p = sf::Project::demo();
        p.duration = 12;
        auto id = p.tracks[0].clips[0].id;
        p.findClip(id)->duration = 4;
        sf::Clip next = *p.findClip(id);
        next.id = p.allocate();
        next.start = 4;
        next.duration = 4;
        p.tracks[0].clips.push_back(next);
        auto following = next.id;
        p.rippleTrim(id, -1);
        require(p.findClip(id)->duration == 3 && p.findClip(following)->start == 3 && p.duration == 11,
                "Ripple trim moves downstream clips and sequence duration");
        p.rollTrim(id, following, .5);
        require(p.findClip(id)->duration == 3.5 && p.findClip(following)->start == 3.5 &&
                    p.findClip(following)->duration == 3.5 && p.duration == 11,
                "Roll trim keeps the sequence and incoming end fixed");
        auto before = p.json().Dump(0);
        p.owner(following)->locked = true;
        auto locked = p.json().Dump(0);
        require(rejects([&] { p.rollTrim(id, following, 1); }) && p.json().Dump(0) == locked,
                "Locked roll edits fail atomically");
        p.owner(following)->locked = false;
        p.overlapTransition(id, following, .5);
        require(p.findClip(following)->start == 3 && p.findClip(following)->transition.enabled &&
                    p.findClip(following)->duration == 4,
                "Automatic transition extends an incoming handle and preserves its end");
        auto range = sf::Project::demo();
        range.duration = 12;
        range.findClip(range.tracks[0].clips[0].id)->duration = 12;
        range.removeRange(2, 4);
        require(range.duration == 10 && range.tracks[0].clips.size() == 2 &&
                    range.tracks[0].clips[0].duration == 2 && range.tracks[0].clips[1].start == 2,
                "Removing a range splits crossing clips and closes the interval");
        sf::Session s;
        s.renderer.initialize();
        s.project = sf::Project::demo();
        s.project.width = 320;
        s.project.height = 180;
        auto &clip = s.project.tracks[0].clips[0];
        clip.effects.clear();
        sf::Effect effect;
        effect.code = "//@passes 3\nfloat4 Effect(EffectContext c){return float4(Prev(c.px).r+.1,0,0,1);}";
        clip.effects = {effect};
        s.renderer.render(s.project, s.media, 0, 320, 180);
        auto frame = s.renderer.readback();
        require(frame.pixels[(90 * 320 + 10) * 4 + 2] > 70,
                "Multiple passes consume the previous pass instead of overwriting their input");
        clip.effects[0].code =
            "float4 Effect(EffectContext c){return float4(HasHistory()?History(c.px).r+.2:.1,0,0,1);}";
        s.renderer.render(s.project, s.media, 0, 320, 180);
        s.renderer.render(s.project, s.media, 1. / 30, 320, 180);
        frame = s.renderer.readback();
        require(frame.pixels[(90 * 320 + 10) * 4 + 2] > 70 && frame.pixels[(90 * 320 + 10) * 4 + 2] < 85,
                "Temporal history retains the preceding processed clip frame");
        s.renderer.render(s.project, s.media, 1. / 30, 320, 180);
        require(s.renderer.readback().pixels == frame.pixels,
                "Repeating a paused frame does not accumulate temporal effects");
        s.renderer.render(s.project, s.media, 2, 320, 180);
        frame = s.renderer.readback();
        require(frame.pixels[(90 * 320 + 10) * 4 + 2] < 30,
                "Seeking resets temporal history for reproducible rendering");
        auto lower = s.project.addTrack(sf::TrackKind::Video).id;
        sf::Clip under;
        under.id = s.project.allocate();
        under.generator = true;
        under.duration = 6;
        under.effects = {effect};
        under.effects[0].code = "float4 Effect(EffectContext c){return float4(1,0,0,1);}";
        s.project.findTrack(lower)->clips.push_back(under);
        s.project.tracks[0].clips[0].effects[0].code =
            "float4 Effect(EffectContext c){return float4(Input(1,c.px).r*.5,0,0,1);}";
        s.renderer.render(s.project, s.media, 0, 320, 180);
        frame = s.renderer.readback();
        require(frame.pixels[(90 * 320 + 10) * 4 + 2] > 120 && frame.pixels[(90 * 320 + 10) * 4 + 2] < 135,
                "Input one reads the lower composition in VFX shaders");
        auto root = sx::Utf8Path(argv[1]);
        auto source = sx::Narrow((root / "examples/firstlight.wav").wstring());
        for (int schema = 1; schema <= 8; ++schema) {
            auto migrated = sf::Project::load(sx::Narrow(
                (root / ("tests/fixtures/compat/project-v" + std::to_string(schema) + ".json")).wstring()));
            require(migrated.json()["schema"].I() == 10 && migrated.findClip(2)->generator &&
                        migrated.findClip(2)->duration == 1,
                    "Versioned project fixtures migrate into schema eight without altering their timing");
        }
        std::string graphFixture;
        sf::Json shaderGraph;
        sx::ReadTextFile(sx::Narrow((root / "tests/fixtures/compat/shaderixel-graph.json").wstring()),
                         graphFixture);
        sf::Json::Parse(graphFixture, shaderGraph);
        sf::Effect migratedShader;
        migratedShader.graph = shaderGraph;
        s.renderer.validateShader(sf::effectCode(migratedShader));
        require(!sf::effectCode(migratedShader).empty(),
                "The vendored Shaderixel compiler still accepts its saved graph fixture");
        auto directory = std::filesystem::current_path() / "test-output/async-media";
        std::filesystem::create_directories(directory);
        sf::MediaAsset original;
        original.open(source);
        sf::MediaAsset disk;
        disk.open(source, directory);
        require(disk.diskPcm && disk.pcm.empty() && disk.samples() == original.pcm.size() &&
                    disk.sample(200) == original.pcm[200],
                "Disk PCM cache maps exact decoded stereo samples without retaining a RAM copy");
        sf::MediaAsset reused;
        reused.open(source, directory);
        require(reused.diskPcm && reused.sample(300) == disk.sample(300),
                "The on-disk audio cache can be reused by another decoder");
        sf::MediaCache async;
        async.enableBackground(directory);
        auto began = std::chrono::steady_clock::now();
        require(async.get(source).loading &&
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() < .1,
                "Background media loading returns immediately");
        auto deadline = began + std::chrono::seconds(5);
        while (async.get(source).loading && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(!async.get(source).loading && async.get(source).samples() == original.samples(),
                "Background audio metadata and PCM mapping arrive on the UI thread");
        async.clear();
        require(async.get(source).loading,
                "Clearing the cache invalidates results from older project generations");
        auto absent = sx::Narrow((directory / "nonexistent-video.mp4").wstring());
        async.get(absent);
        bool missingReported = false;
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!missingReported && std::chrono::steady_clock::now() < deadline) {
            missingReported = rejects([&] { async.get(absent); });
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(missingReported,
                "Missing asynchronous media reports an error instead of remaining stuck loading");
        sf::Protocol protocol(s);
        auto presets = protocol.execute("list_export_presets", sf::Json::Object());
        require(presets["presets"].Size() == 5, "MCP exposes export presets independently of the document");
        auto nativePath = sx::Narrow((std::filesystem::current_path() / "native_gain.dll").wstring());
        {
            sf::IsolatedAudio plugin("native", nativePath);
            auto params = plugin.parameters();
            require(params.size() == 1 && params.at("p0") == .5f && plugin.latency() == 0,
                    "Native ABI v1 exposes parameters from an isolated process");
            std::vector<float> data(256 * 2, .4f);
            std::map<std::string, sf::Parameter> controls{{"p0", {.25f}}};
            plugin.process(0, 256, data, controls);
            require(std::abs(data[0] - .2f) < 1e-6,
                    "The native example processes stereo audio with normalized automation");
            plugin.terminateForTest();
            require(rejects([&] { plugin.process(.1, 256, data, controls); }),
                    "A crashed native plugin is reported without crashing the editor process");
        }
        auto vstPath = sx::Narrow(
            (root /
             "vendor/cydaw2/build/desktop-asio/CyDAWTestGain_artefacts/Release/VST3/CyDAW Test Gain.vst3")
                .wstring());
        {
            sf::IsolatedAudio plugin("vst3", vstPath);
            auto params = plugin.parameters();
            std::vector<float> data(256 * 2, .25f);
            plugin.process(0, 256, data, {});
            require(!params.empty() && std::isfinite(data[0]) && data[0] != 0,
                    "CyDAW v2's isolated VST3 worker loads and processes a real VST3 fixture");
        }
        sf::Json add = sf::Json::Object();
        add["scope"] = "master";
        add["kind"] = "native";
        add["path"] = nativePath;
        auto added = protocol.execute("add_external_audio", add);
        sf::Json control = sf::Json::Object();
        control["scope"] = "master";
        control["effect"] = added["effect"];
        control["key"] = "p0";
        control["value"] = .25;
        protocol.execute("set_mix_parameter", control);
        auto restored = sf::Project::fromJson(s.project.json());
        require(restored.masterMix.effects.back().code == nativePath &&
                    restored.masterMix.effects.back().params.at("p0").value == .25f,
                "Projects preserve native plugin paths and controls through JSON round trips");
        sf::Loudness loudness;
        std::vector<float> sine(48000 * 2);
        for (size_t n = 0; n < 48000; ++n)
            sine[n * 2] = sine[n * 2 + 1] = float(.1 * std::sin(2 * 3.141592653589793 * 997 * n / 48000));
        loudness.push(sine);
        require(std::abs(loudness.integrated() + 20) < .1 && std::abs(loudness.samplePeak() + 20) < .1,
                "LUFS K-weighting is calibrated on a 997 Hz stereo reference signal");
        loudness.push(std::vector<float>(48000 * 2, 0));
        require(std::abs(loudness.integrated() + 20) < 1,
                "Loudness gating excludes silent blocks instead of averaging them into the programme");
        sf::Project sound;
        sound.duration = .05;
        auto tid = sound.addTrack(sf::TrackKind::Audio).id;
        sf::Clip soundClip;
        soundClip.id = sound.allocate();
        soundClip.source = source;
        soundClip.duration = .05;
        sound.findTrack(tid)->clips.push_back(soundClip);
        sf::MediaCache reference;
        std::vector<float> baseline;
        reference.mix(sound, 0, 2048, baseline);
        auto delayed = sound;
        sf::Effect delay;
        delay.id = "native";
        delay.code = sx::Narrow((std::filesystem::current_path() / "native_latency.dll").wstring());
        delayed.findClip(soundClip.id)->effects = {delay};
        sf::MediaCache compensated;
        std::vector<float> alignment;
        compensated.mix(delayed, 0, 2048, alignment);
        double difference = 0;
        for (size_t n = 0; n < alignment.size(); ++n)
            difference = std::max(difference, double(std::abs(alignment[n] - baseline[n])));
        require(compensated.latencySamples == 32 && difference < 1e-6,
                "Reported native plugin latency is removed from the sequence output");
        auto duplicate = soundClip;
        duplicate.id = delayed.allocate();
        delayed.addTrack(sf::TrackKind::Audio).clips.push_back(duplicate);
        compensated.resetAudio();
        compensated.mix(delayed, 0, 2048, alignment);
        difference = 0;
        for (size_t n = 0; n < alignment.size(); ++n)
            difference =
                std::max(difference, double(std::abs(alignment[n] - std::clamp(baseline[n] * 2, -1.f, 1.f))));
        require(difference < 1e-6, "Parallel paths with different plugin latencies remain sample aligned");
        sf::Json graph;
        std::string parseError;
        sf::Json::Parse(
            R"({"schema":2,"nodes":[{"id":1,"processor":"cydaw.eq-three"},{"id":2,"processor":"cydaw.eq-three"}],"connections":[{"source":0,"destination":1},{"source":0,"destination":2},{"source":1,"destination":0},{"source":2,"destination":0}]})",
            graph, &parseError);
        auto definition = sf::decodeDspGraph(graph);
        require(definition.order.size() == 2,
                "Imported DSP graphs validate their typed CyDAW v2 ports and processing order");
        sf::Json dspFixture;
        sx::ReadTextFile(sx::Narrow((root / "tests/fixtures/compat/cydaw-dsp-v2.json").wstring()),
                         graphFixture);
        sf::Json::Parse(graphFixture, dspFixture);
        require(sf::decodeDspGraph(dspFixture).order.size() == 2,
                "The saved CyDAW v2 DSP fixture retains its branches after adapter compilation");
        sf::Effect graphEffect;
        graphEffect.id = "cydaw.graph";
        graphEffect.graph = graph;
        sound.findTrack(tid)->mix.effects = {graphEffect};
        sf::MediaCache graphMedia;
        std::vector<float> graphResult;
        graphMedia.mix(sound, 0, 2048, graphResult);
        difference = 0;
        for (size_t n = 0; n < graphResult.size(); ++n)
            difference = std::max(difference,
                                  double(std::abs(graphResult[n] - std::clamp(baseline[n] * 2, -1.f, 1.f))));
        require(difference < 1e-5,
                "A parallel DSP graph executes both branches and sums their stereo outputs");
        sf::Json cycle;
        sf::Json::Parse(
            R"({"schema":2,"nodes":[{"id":1,"processor":"sf.sum"}],"connections":[{"source":1,"destination":1},{"source":1,"destination":0}]})",
            cycle);
        require(rejects([&] { sf::decodeDspGraph(cycle); }),
                "DSP graph cycles are rejected before they reach the audio engine");
        auto invalidClip = sound;
        graphEffect.graph = cycle;
        invalidClip.findClip(soundClip.id)->effects = {graphEffect};
        require(rejects([&] { sf::Project::fromJson(invalidClip.json()); }),
                "Project loading rejects invalid embedded DSP graphs on audio clips");
        auto fx = sf::Project::demo();
        fx.width = 320;
        fx.height = 180;
        fx.duration = .4;
        fx.fps = 30;
        auto &moving = fx.tracks[0].clips[0];
        moving.effects.clear();
        sf::Effect marker;
        marker.code = "float4 Effect(EffectContext c){float2 q=abs(c.uv-float2(.5+c.time*.12,.5));return "
                      "float4(any(q>float2(.025,.04))?float3(.05,.05,.05):float3(1,1,1),1);}";
        moving.effects = {marker};
        sf::MediaCache movieMedia;
        sf::Renderer movieRenderer;
        movieRenderer.initialize();
        auto moviePath = sx::Narrow((directory / "marker.mp4").wstring());
        sf::exportVideo(fx, movieMedia, movieRenderer, moviePath);
        sf::Project track;
        track.duration = .4;
        track.width = 320;
        track.height = 180;
        sf::Clip tracked;
        tracked.id = track.allocate();
        tracked.source = moviePath;
        tracked.duration = .4;
        tracked.mask.shape = sf::MaskShape::Rectangle;
        tracked.params["mask_width"] = {.1};
        tracked.params["mask_height"] = {.1};
        track.addTrack(sf::TrackKind::Video).clips.push_back(tracked);
        sf::TrackingJobs tracker;
        auto trackingId = tracker.start(track, tracked.id, 0, .35, 12);
        auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (tracker.status()["state"].S() == "running" && std::chrono::steady_clock::now() < limit)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(tracker.status()["state"].S() == "completed",
                "Background mask tracking follows a rendered moving marker");
        tracker.apply(track, trackingId);
        auto &trackedX = track.findClip(tracked.id)->params["mask_x"];
        require(trackedX.keys.size() > 8 && std::abs(trackedX.keys.back().value - .04) < .012,
                "Tracking applies editable position keyframes close to the measured trajectory");
        sf::Json points = sf::Json::Array();
        for (auto point : std::vector<sf::BezierPoint>{{-.3f, -.3f, -.3f, -.3f, -.3f, -.3f},
                                                       {.3f, -.3f, .3f, -.3f, .3f, -.3f},
                                                       {0, .3f, 0, .3f, 0, .3f}}) {
            sf::Json item = sf::Json::Array();
            for (float value : {point.x, point.y, point.inX, point.inY, point.outX, point.outY})
                item.Push(value);
            points.Push(item);
        }
        s.project = sf::Project::demo();
        s.project.width = 320;
        s.project.height = 180;
        s.project.tracks[0].clips[0].effects = {marker};
        s.project.tracks[0].clips[0].effects[0].code =
            "float4 Effect(EffectContext c){return float4(1,0,0,1);}";
        sf::Json maskArgs = sf::Json::Object();
        maskArgs["clip"] = s.project.tracks[0].clips[0].id;
        maskArgs["points"] = points;
        protocol.execute("set_bezier_mask", maskArgs);
        s.renderer.render(s.project, s.media, 0, 320, 180);
        auto maskImage = s.renderer.readback();
        require(maskImage.pixels[(90 * 320 + 160) * 4 + 2] > 240 &&
                    maskImage.pixels[(10 * 320 + 10) * 4 + 2] < 10,
                "Bézier masks clip the GPU composition inside the closed path");
        require(sf::Project::fromJson(s.project.json()).tracks[0].clips[0].mask.points.size() == 3,
                "Bézier control points survive project round trips");
        auto tonePath = sx::Narrow((directory / "tone.wav").wstring());
        {
            std::ofstream file(sx::Utf8Path(tonePath), std::ios::binary);
            auto write = [&](auto value) {
                file.write(reinterpret_cast<const char *>(&value), sizeof(value));
            };
            file.write("RIFF", 4);
            write(uint32_t(36 + 48000 * 4));
            file.write("WAVEfmt ", 8);
            write(uint32_t(16));
            write(uint16_t(1));
            write(uint16_t(2));
            write(uint32_t(48000));
            write(uint32_t(192000));
            write(uint16_t(4));
            write(uint16_t(16));
            file.write("data", 4);
            write(uint32_t(48000 * 4));
            for (unsigned n = 0; n < 48000; ++n) {
                auto value = int16_t(8192 * std::sin(2 * 3.141592653589793 * 440 * n / 48000));
                write(value);
                write(value);
            }
        }
        sf::Project stretchProject;
        stretchProject.duration = .5;
        sf::Clip fast;
        fast.id = stretchProject.allocate();
        fast.source = tonePath;
        fast.speed = 2;
        fast.duration = .5;
        stretchProject.addTrack(sf::TrackKind::Audio).clips.push_back(fast);
        auto dominant = [](const std::vector<float> &data) {
            double strongest = 0;
            int frequency = 0;
            for (int hz = 400; hz <= 1000; hz += 5) {
                double re = 0, im = 0;
                for (unsigned n = 0; n < 4096; ++n) {
                    double angle = 2 * 3.141592653589793 * hz * n / 48000;
                    double value =
                        data[(n + 4096) * 2] * (.5 - .5 * std::cos(2 * 3.141592653589793 * n / 4096));
                    re += value * std::cos(angle);
                    im += value * std::sin(angle);
                }
                double strength = re * re + im * im;
                if (strength > strongest) {
                    strongest = strength;
                    frequency = hz;
                }
            }
            return frequency;
        };
        sf::MediaCache stretchMedia;
        std::vector<float> fastData;
        stretchMedia.mix(stretchProject, 0, 24000, fastData);
        require(std::abs(dominant(fastData) - 880) <= 10,
                "Ordinary speed changes also change the audio pitch");
        stretchProject.findClip(fast.id)->params["preserve_pitch"] = {1};
        stretchMedia.resetAudio();
        stretchMedia.mix(stretchProject, 0, 24000, fastData);
        require(std::abs(dominant(fastData) - 440) <= 10,
                "Waveform-aligned time stretching preserves a 440 Hz tone at double speed");
        s.project = track;
        s.selection.clear();
        s.selected = 0;
        sf::Json proxy = sf::Json::Object();
        proxy["clip"] = tracked.id;
        proxy["path"] = sx::Narrow((directory / "proxy.mp4").wstring());
        auto proxyJob = protocol.execute("prepare_proxy", proxy)["job"].U();
        limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (s.exports.status(proxyJob).active() && std::chrono::steady_clock::now() < limit)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(s.exports.status(proxyJob).state == sf::ExportState::Completed &&
                    s.project.findClip(tracked.id)->source == moviePath,
                "Proxy generation keeps the project source and clip timing intact");
        sf::MediaAsset proxyAsset;
        proxyAsset.open(proxy["path"].S());
        require(proxyAsset.video && proxyAsset.at(.1).width == 320,
                "The generated H264 proxy can be independently decoded");
        auto reportPath = sx::Narrow((directory / "loudness.json").wstring());
        auto report =
            s.exports.start(stretchProject, s.renderer.library, sf::ExportKind::Analysis, reportPath);
        limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (s.exports.status(report).active() && std::chrono::steady_clock::now() < limit)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        std::string reportText;
        sf::Json reportJson;
        sx::ReadTextFile(reportPath, reportText);
        sf::Json::Parse(reportText, reportJson);
        require(s.exports.status(report).state == sf::ExportState::Completed &&
                    reportJson["gating_blocks"].I() > 0 && reportJson["true_peak_measured"].B(),
                "Background loudness reports distinguish integrated LUFS from sample peaks");
        std::cout << checks << " advanced checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        result = 1;
    }
    MFShutdown();
    CoUninitialize();
    return result;
}

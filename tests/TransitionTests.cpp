#include "adapters/LayerGraph.h"
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
    int exit = 0;
    try {
        sf::Session s;
        s.renderer.initialize();
        s.renderer.library.scan(sx::Utf8Path(argv[1]));
        s.project = {};
        s.project.width = 320;
        s.project.height = 180;
        s.project.duration = 4;
        auto add = [&](double start, double length, const char *code) {
            auto &track = s.project.addTrack(sf::TrackKind::Video);
            sf::Clip c;
            c.id = s.project.allocate();
            c.generator = true;
            c.start = start;
            c.duration = length;
            sf::Effect effect;
            effect.code = code;
            c.effects.push_back(effect);
            track.clips.push_back(c);
            return c.id;
        };
        auto incoming = add(1, 3, "float4 Effect(EffectContext ctx){return float4(.8,.1,.05,1);}");
        auto outgoing = add(0, 2, "float4 Effect(EffectContext ctx){return float4(.05,.1,.8,1);}");
        sf::Protocol protocol(s);
        sf::Json args = sf::Json::Object();
        args["clip"] = incoming;
        args["id"] = "transition.crossfade";
        args["duration"] = 1;
        protocol.execute("set_transition", args);
        auto render = [&](double time) {
            s.renderer.render(s.project, s.media, time, 320, 180);
            return s.renderer.readback();
        };
        auto channel = [](const sf::Image &image, int x, int y, int component) {
            return image.pixels[(size_t(y) * image.width + x) * 4 + component];
        };
        auto beginning = render(1), middle = render(1.5), end = render(2);
        require(channel(beginning, 160, 90, 0) > 190 && channel(beginning, 160, 90, 2) < 20,
                "Transition begins on the outgoing composition");
        require(std::abs(int(channel(middle, 160, 90, 0)) - 108) < 3 &&
                    std::abs(int(channel(middle, 160, 90, 2)) - 108) < 3,
                "Crossfade mixes both actual GPU textures at half progress");
        require(channel(end, 160, 90, 2) > 190 && channel(end, 160, 90, 0) < 20,
                "Transition finishes on the incoming clip after the outgoing clip ends");
        require(s.project.findClip(incoming)->transitionProgress(1.5) == .5f &&
                    !s.project.findClip(incoming)->transition.code.empty(),
                "Automatic progress follows sequence time and embeds the shader");
        args["id"] = "transition.wipe";
        protocol.execute("set_transition", args);
        auto wipe = render(1.5);
        require(channel(wipe, 40, 90, 2) > 190 && channel(wipe, 280, 90, 0) > 190,
                "Wipe reveals the incoming image from left to right");
        for (int direction = 1; direction < 4; ++direction) {
            s.project.findClip(incoming)->params["transition_direction"] = {float(direction)};
            auto image = render(1.5);
            bool correct =
                direction == 1   ? channel(image, 280, 90, 2) > 190 && channel(image, 40, 90, 0) > 190
                : direction == 2 ? channel(image, 160, 20, 2) > 190 && channel(image, 160, 160, 0) > 190
                                 : channel(image, 160, 160, 2) > 190 && channel(image, 160, 20, 0) > 190;
            require(correct, "Wipe direction changes the spatial reveal");
        }
        auto *clip = s.project.findClip(incoming);
        clip->params["transition_direction"] = {0};
        clip->params["transition_softness"] = {.5f};
        auto soft = render(1.5);
        require(channel(soft, 150, 90, 2) > 70 && channel(soft, 150, 90, 0) > 50,
                "Wipe feather blends at its moving edge");
        clip->params["transition_progress"].setKey({1, 0});
        clip->params["transition_progress"].setKey({3, 1});
        require(clip->transitionProgress(2) == .5f, "Progress keyframes can override the automatic duration");
        auto animated = render(1.5);
        require(channel(animated, 280, 90, 0) > 190, "Progress animation affects the rendered wipe");
        auto serialized = s.project.json();
        require(sf::Project::fromJson(serialized).json().Dump(0) == serialized.Dump(0),
                "Schema 5 round trip preserves transition source, parameters and curves");
        for (int schema = 1; schema <= 3; ++schema) {
            serialized["schema"] = schema;
            require(!sf::Project::fromJson(serialized).findClip(incoming)->transition.enabled,
                    "Older project schemas load with a neutral transition");
        }
        auto frozen = s.project.json().Dump(0);
        auto rejectedWithoutChange = [&](const char *tool, sf::Json request, const char *message) {
            bool rejected = false;
            try {
                protocol.execute(tool, request);
            } catch (...) {
                rejected = true;
            }
            require(rejected && s.project.json().Dump(0) == frozen, message);
        };
        args["duration"] = 10;
        rejectedWithoutChange("set_transition", args, "Oversized transition is rejected atomically");
        args["duration"] = -1;
        rejectedWithoutChange("set_transition", args, "Negative transition duration is rejected atomically");
        args["duration"] = 1;
        args["id"] = "color";
        rejectedWithoutChange("set_transition", args, "An ordinary effect cannot be used as a transition");
        sf::Json shader = sf::Json::Object();
        shader["clip"] = incoming;
        shader["code"] = "Invalid HLSL";
        rejectedWithoutChange("set_transition_shader", shader,
                              "Invalid transition HLSL preserves the document");
        shader["code"] =
            "//@param opacity float 1 0 1\nfloat4 Effect(EffectContext ctx){return InputUV(0,ctx.uv);}";
        rejectedWithoutChange("set_transition_shader", shader,
                              "Transition parameters cannot overwrite clip transform parameters");
        s.project.owner(incoming)->locked = true;
        frozen = s.project.json().Dump(0);
        rejectedWithoutChange("remove_transition", shader, "Locked tracks refuse transition edits");
        s.project.owner(incoming)->locked = false;
        s.project.findClip(incoming)->params["transition_softness"].setKey({1, .1f});
        s.project.findClip(incoming)->params["transition_softness"].setKey({2, .2f});
        shader["code"] =
            "float4 Effect(EffectContext ctx){return "
            "HasInput(1)?lerp(InputUV(1,ctx.uv),InputUV(0,ctx.uv),ctx.transitionProgress):float4(0,1,0,1);}";
        protocol.execute("set_transition_shader", shader);
        require(s.project.findClip(incoming)->params["transition_softness"].keys.size() == 2,
                "Recompiling a transition keeps its parameter animations");
        s.project.findClip(incoming)->params.erase("transition_progress");
        auto custom = render(1.5);
        require(std::abs(int(channel(custom, 160, 90, 2)) - 108) < 3,
                "Custom HLSL receives input 1 and transition progress");
        auto original = s.project;
        auto &c = *s.project.findClip(incoming);
        c.mask.shape = sf::MaskShape::Rectangle;
        c.params["mask_width"] = {.5f};
        c.params["mask_height"] = {.5f};
        auto masked = render(1.5);
        require(channel(masked, 20, 20, 0) > 190 && std::abs(int(channel(masked, 160, 90, 2)) - 108) < 3,
                "Transition preserves the incoming mask and the composition outside it");
        s.project = original;
        s.project.findClip(incoming)->params["opacity"] = {.5f};
        auto opacity = render(1.5);
        require(channel(opacity, 160, 90, 0) > 150 && channel(opacity, 160, 90, 2) < 75,
                "Incoming opacity is applied before the transition");
        s.project = original;
        auto green = add(1, 3, "float4 Effect(EffectContext ctx){return float4(.05,.8,.05,1);}");
        std::rotate(s.project.tracks.begin(), s.project.tracks.end() - 1, s.project.tracks.end());
        sf::Json greenArgs = sf::Json::Object();
        greenArgs["clip"] = green;
        greenArgs["id"] = "transition.crossfade";
        protocol.execute("set_transition", greenArgs);
        auto stacked = render(1.5);
        require(std::abs(int(channel(stacked, 160, 90, 1)) - 115) < 3 &&
                    std::abs(int(channel(stacked, 160, 90, 2)) - 61) < 3,
                "Multiple active transitions compose without reading their own output target");
        s.project = original;
        s.project.findClip(incoming)->params["transition_progress"].setKey({1, 0});
        s.project.findClip(incoming)->params["transition_progress"].setKey({2, 1});
        auto copied = s.project.duplicate(incoming);
        require(s.project.findClip(copied)->transition.enabled &&
                    s.project.findClip(copied)->params["transition_progress"].keys[0].time == 4,
                "Duplicate carries embedded transitions and their animations");
        s.project.split(incoming, 1.5);
        auto right = s.project.owner(incoming)->clips.back().id;
        require(s.project.findClip(incoming)->transition.duration == .5 &&
                    !s.project.findClip(right)->transition.enabled,
                "Split keeps the entry transition on the left segment only");
        s.project = original;
        s.project.setTiming(incoming, 1, .25, 0, 1);
        require(s.project.findClip(incoming)->transition.duration == .25,
                "Trim clamps the transition duration to the shortened clip");
        s.project = original;
        sf::Json toggle = sf::Json::Object();
        toggle["clip"] = incoming;
        toggle["enabled"] = false;
        protocol.execute("set_transition", toggle);
        s.project.setTiming(incoming, 1, .25, 0, 1);
        toggle["enabled"] = true;
        protocol.execute("set_transition", toggle);
        require(s.project.findClip(incoming)->transition.enabled &&
                    s.project.findClip(incoming)->transition.duration == .25,
                "Reactivating a disabled transition adapts it to a shortened clip");
        s.project = original;
        auto directory = std::filesystem::current_path() / "test-output/transitions";
        std::filesystem::create_directories(directory);
        auto pluginRoot = directory / "catalogue";
        auto pluginDir = pluginRoot / "plugins/test-transition";
        std::filesystem::create_directories(pluginDir);
        {
            std::ofstream code(pluginDir / "reveal.hlsl");
            code << "float4 Effect(EffectContext ctx){return "
                    "lerp(InputUV(1,ctx.uv),InputUV(0,ctx.uv),ctx.transitionProgress);}";
        }
        sf::Json manifest = sf::Json::Object(), definition = sf::Json::Object();
        manifest["apiVersion"] = 1;
        manifest["name"] = "Transition fixture";
        definition["id"] = "fixture.reveal";
        definition["kind"] = "transition";
        definition["shader"] = "reveal.hlsl";
        manifest["effects"] = sf::Json::Array();
        manifest["effects"].Push(definition);
        auto saveManifest = [&] {
            std::ofstream file(pluginDir / "plugin.json");
            file << manifest.Dump(2);
        };
        saveManifest();
        sf::EffectLibrary pluginLibrary;
        pluginLibrary.scan(pluginRoot);
        require(pluginLibrary.definitions.empty() && !pluginLibrary.diagnostics.empty(),
                "API 1 plugins cannot silently opt into two-input transitions");
        manifest["apiVersion"] = 2;
        saveManifest();
        pluginLibrary.scan(pluginRoot);
        require(pluginLibrary.definitions.size() == 1 && pluginLibrary.definitions[0].kind == "transition" &&
                    pluginLibrary.diagnostics.empty(),
                "API 2 loads versioned transition plugins");
        bool ordinaryRejected = false;
        try {
            pluginLibrary.instantiate("fixture.reveal");
        } catch (...) {
            ordinaryRejected = true;
        }
        require(ordinaryRejected, "Transition plugins cannot enter the ordinary single-input effect stack");
        auto file = sx::Narrow((directory / "transition.mp4").wstring());
        auto job = s.exports.start(s.project, s.renderer.library, sf::ExportKind::Video, file);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (s.exports.status(job).active() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(s.exports.status(job).state == sf::ExportState::Completed,
                "Background video export supports two-input transition shaders");
        sf::MediaAsset exported;
        exported.open(file);
        auto frame = exported.at(1.5);
        require(std::abs(int(channel(frame, 160, 90, 2)) - 108) < 20 &&
                    std::abs(int(channel(frame, 160, 90, 0)) - 108) < 20,
                "Decoded MP4 matches the half-progress transition preview");
        s.history.clear();
        args = sf::Json::Object();
        args["clip"] = incoming;
        auto before = s.project.json().Dump(0);
        protocol.execute("remove_transition", args);
        require(!s.project.findClip(incoming)->transition.enabled, "MCP removes transitions");
        protocol.execute("undo", sf::Json::Object());
        require(s.project.json().Dump(0) == before,
                "Undo restores the embedded transition and rendered contract");
        {
            auto saved = s.project;
            auto a = *s.project.findClip(incoming), b = *s.project.findClip(outgoing);
            a.params.clear();
            b.params.clear();
            a.transition = {true, "transition.crossfade", {}, 1};
            b.transition.enabled = false;
            s.project.layerGraph = sf::Json();
            s.project.tracks.clear();
            s.project.addTrack(sf::TrackKind::Video).clips = {a, b};
            auto mixed = render(1.5);
            require(std::abs(int(channel(mixed, 160, 90, 0)) - 108) < 3 &&
                        std::abs(int(channel(mixed, 160, 90, 2)) - 108) < 3,
                    "Single-track transitions sort overlapping clips by start time, independent of insertion "
                    "order");
            s.project = saved;
            s.project.findClip(incoming)->transition.enabled = false;
            s.project.findClip(outgoing)->transition.enabled = false;
            s.project.findClip(incoming)->params.clear();
            s.project.findClip(outgoing)->params.clear();
            auto g = sf::Json::Object();
            g["engine"] = "shaderixel.composition";
            g["schema"] = 1;
            g["output"] = 4;
            g["nodes"] = sf::Json::Array();
            for (unsigned id = 1; id <= 4; ++id) {
                sf::Json n = sf::Json::Object();
                n["id"] = id;
                n["kind"] = id < 3 ? "surface" : id == 3 ? "effect" : "output";
                n["name"] = "Transition test";
                n["pos"] = sf::Json::Array();
                n["pos"].Push(id * 200);
                n["pos"].Push(100);
                n["inputs"] = sf::Json::Array();
                if (id < 3)
                    n["surface"] = s.project.owner(id == 1 ? incoming : outgoing)->id;
                if (id == 3) {
                    n["inputs"].Push(1);
                    n["inputs"].Push(2);
                    n["effect"]["id"] = "transition";
                    n["effect"]["code"] =
                        "//@input Arrivée\n//@input Départ\n//@param progress float 0 0 1 \"Progression\"\n" +
                        s.renderer.library.find("transition.crossfade")->code + "\n//@premultiplied\n";
                    sf::Parameter progress;
                    progress.keys = {{1, 0, sf::Curve::Linear, 0}, {2, 1, sf::Curve::Linear, 0}};
                    n["params"]["progress"] = sf::parameterJson(progress);
                }
                if (id == 4)
                    n["inputs"].Push(3);
                g["nodes"].Push(n);
            }
            s.project.layerGraph = g;
            s.project.validate();
            mixed = render(1.5);
            require(std::abs(int(channel(mixed, 160, 90, 0)) - 108) < 3 &&
                        std::abs(int(channel(mixed, 160, 90, 2)) - 108) < 3,
                    "Layer graph transitions mix two timeline tracks with an animated progress parameter");
            s.project = saved;
        }
        std::cout << checks << " transition checks passed.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        exit = 1;
    }
    MFShutdown();
    CoUninitialize();
    return exit;
}

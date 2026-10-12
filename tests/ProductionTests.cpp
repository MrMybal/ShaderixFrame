#include "adapters/LayerGraph.h"
#include "core/Binary.h"
#include "core/Motion.h"
#include "mcp/Protocol.h"
#include "render/Color.h"
#include "render/Grading.h"
#include "ui/ClipPreviews.h"
#include "ui/DspCanvas.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mfapi.h>
#include <set>
#include <thread>
static int checks = 0;
static void require(bool v, const char *text) {
    if (!v)
        throw std::runtime_error(text);
    ++checks;
    std::cout << text << std::endl;
}
template <class F> static bool rejects(F f) {
    try {
        f();
        return false;
    } catch (...) {
        return true;
    }
}
static sf::Json parse(const std::string &text) {
    sf::Json value;
    if (!sf::Json::Parse(text, value))
        throw std::runtime_error("json");
    return value;
}
static void sharedAudioChecks() {
    auto direct = sf::instantiateAudioEffect("cydaw.tremolo-pan");
    direct.params["depth"].keys = {{0, 0, sf::Curve::Linear, 0}, {.5, 1, sf::Curve::Linear, 0}};
    sf::Effect graph;
    graph.id = "cydaw.graph";
    graph.graph = parse(
        R"({"schema":2,"nodes":[{"id":1,"processor":"cydaw.tremolo-pan"}],"connections":[{"source":0,"destination":1},{"source":1,"destination":0}]})");
    auto node = graph.graph["nodes"].At(0);
    node["params"] = sf::Json::Object();
    node["params"]["depth"] = sf::parameterJson(direct.params.at("depth"));
    graph.graph["nodes"] = sf::Json::Array();
    graph.graph["nodes"].Push(node);
    sf::AudioEffects chain, plan;
    std::vector<float> source(2048);
    for (size_t k = 0; k < source.size(); ++k)
        source[k] = float(std::sin(k * .04) * .3);
    auto expected = source, actual = source;
    chain.begin(.25);
    plan.begin(.25);
    chain.process(1, {direct}, .25, 1024, expected);
    plan.process(1, {graph}, .25, 1024, actual);
    float error = 0, change = 0;
    for (size_t k = 0; k < source.size(); ++k) {
        error = std::max(error, std::abs(actual[k] - expected[k]));
        change = std::max(change, std::abs(actual[k] - source[k]));
    }
    require(error < 1e-6 && change > .001,
            "CyDAW AudioRenderPlan and its native processor chain agree with automated DSP parameters");
}
static void layerGraphChecks(const std::filesystem::path &root) {
    sf::Session session;
    session.renderer.initialize();
    session.renderer.library.scan(root);
    auto &p = session.project;
    p = {};
    p.width = p.height = 64;
    p.duration = 2;
    p.backgroundAlpha = 0;
    p.workingSpace = "linear709";
    auto source = [&](const char *shader) {
        auto id = p.addTrack(sf::TrackKind::Video).id;
        sf::Clip c;
        c.id = p.allocate();
        c.generator = true;
        c.duration = 2;
        sf::Effect e;
        e.id = "custom";
        e.code = shader;
        c.effects = {e};
        p.findTrack(id)->clips = {c};
        return id;
    };
    auto red = source("float4 Effect(EffectContext c){return float4(2,0,0,1);}");
    auto blue = source("float4 Effect(EffectContext c){return float4(0,0,1,1);}");
    auto graph = parse(R"({"engine":"shaderixel.composition","schema":1,"output":4,"nodes":[
      {"id":1,"kind":1,"surface":1,"name":"Timeline rouge","pos":[10,10],"inputs":[]},
      {"id":2,"kind":1,"surface":3,"name":"Timeline bleue","pos":[10,210],"inputs":[]},
      {"id":3,"kind":3,"name":"Fusion","pos":[230,100],"blend":0,"opacity":0.5,"inputs":[1,2,0]},
      {"id":4,"kind":0,"name":"Sortie","pos":[460,100],"inputs":[3]}]})");
    // Timeline IDs stay independent of node IDs.
    auto nodes = sf::Json::Array();
    for (auto n : graph["nodes"].Elements()) {
        if (n["id"].U() == 1)
            n["surface"] = red;
        if (n["id"].U() == 2)
            n["surface"] = blue;
        nodes.Push(n);
    }
    graph["nodes"] = nodes;
    sf::Protocol protocol(session);
    auto args = sf::Json::Object();
    args["graph"] = graph;
    protocol.execute("set_layer_graph", args);
    auto pixel = [&](double time) {
        session.renderer.render(p, session.media, time, 64, 64);
        return session.renderer.readbackFloat();
    };
    auto image = pixel(0);
    require(std::abs(image.rgba[0] - 1) < .002 && std::abs(image.rgba[2] - .5) < .002,
            "Shaderixel composition mixes two timeline sources in half-float HDR");
    auto conditional = graph;
    nodes = sf::Json::Array();
    for (auto n : conditional["nodes"].Elements()) {
        if (n["id"].U() == 3)
            n["blendIf"] = parse("[255,255,255,255,0,0,255,255]");
        nodes.Push(n);
    }
    conditional["nodes"] = nodes;
    p.layerGraph = conditional;
    image = pixel(0);
    require(image.rgba[0] == 2 && image.rgba[2] == 0,
            "Original Shaderixel Blend If excludes a blue source below its luminance threshold");
    nodes = sf::Json::Array();
    for (auto n : graph["nodes"].Elements()) {
        if (n["id"].U() == 3)
            n["channelOff"] = 1;
        nodes.Push(n);
    }
    conditional["nodes"] = nodes;
    p.layerGraph = conditional;
    image = pixel(0);
    require(image.rgba[0] == 2 && std::abs(image.rgba[2] - .5) < .002,
            "Shaderixel channel masks keep the base red channel while blending blue");
    p.layerGraph = graph;
    args = parse(R"({"node":3,"key":"opacity","time":0,"value":0,"curve":0})");
    protocol.execute("set_layer_keyframe", args);
    args["time"] = 1;
    args["value"] = 1;
    protocol.execute("set_layer_keyframe", args);
    image = pixel(.5);
    require(std::abs(image.rgba[0] - 1) < .002 && std::abs(image.rgba[2] - .5) < .002,
            "Layer graph automation evaluates between keys in sequence time");
    image = pixel(1);
    require(image.rgba[0] == 0 && image.rgba[2] == 1, "Animated graph fusion reaches its authored endpoint");
    auto round = sf::Project::fromJson(p.json());
    require(round.layerGraph.Dump(0) == p.layerGraph.Dump(0),
            "Layer graph connections and automation roundtrip with the project");
    auto invalid = p.layerGraph;
    nodes = sf::Json::Array();
    for (auto n : invalid["nodes"].Elements()) {
        if (n["id"].U() == 3) {
            n["inputs"] = sf::Json::Array();
            n["inputs"].Push(3);
            n["inputs"].Push(2);
            n["inputs"].Push(0);
        }
        nodes.Push(n);
    }
    invalid["nodes"] = nodes;
    args = sf::Json::Object();
    args["graph"] = invalid;
    auto original = p.json().Dump(0);
    require(rejects([&] { protocol.execute("set_layer_graph", args); }) && p.json().Dump(0) == original,
            "Cycles use Shaderixel detection and failed edits roll back atomically");
    // An effect consumes both timelines directly: no dependence on stack order.
    auto branch = graph;
    nodes = sf::Json::Array();
    for (auto n : branch["nodes"].Elements()) {
        if (n["id"].U() == 3) {
            n["kind"] = 2;
            n["inputs"] = sf::Json::Array();
            n["inputs"].Push(1);
            n["inputs"].Push(2);
            n["inputs"].Push(1);
            n["inputs"].Push(2);
            n["effect"] = parse(
                R"({"id":"custom","code":"float4 Effect(EffectContext c){return float4((Input(0,c.px).rgb+Input(1,c.px).rgb+Input(2,c.px).rgb+Input(3,c.px).rgb)*.25,1);}"})");
        }
        nodes.Push(n);
    }
    branch["nodes"] = nodes;
    p.layerGraph = branch;
    image = pixel(0);
    require(std::abs(image.rgba[0] - 1) < .002 && std::abs(image.rgba[2] - .5) < .002,
            "Shader graph effects accept all four layer graph image inputs");
    auto grade = sf::makeGrade("Après mélange");
    grade.params["exposure"] = {1};
    sf::Json correction =
        parse(R"({"id":5,"kind":2,"name":"Correction après fusion","pos":[450,100],"inputs":[3]})");
    correction["effect"] = sf::Json::Object();
    correction["effect"]["id"] = "grading";
    correction["effect"]["grading"] = grade.grading;
    correction["params"] = sf::Json::Object();
    for (auto &[key, param] : grade.params)
        correction["params"][key] = sf::parameterJson(param);
    nodes = sf::Json::Array();
    for (auto n : branch["nodes"].Elements()) {
        if (n["id"].U() == 4) {
            n["inputs"] = sf::Json::Array();
            n["inputs"].Push(5);
        }
        nodes.Push(n);
    }
    nodes.Push(correction);
    branch["nodes"] = nodes;
    p.layerGraph = branch;
    p.validate();
    image = pixel(0);
    require(std::abs(image.rgba[0] - 2) < .003 && std::abs(image.rgba[2] - 1) < .003,
            "Color grading after a graph merge preserves and raises HDR values");
    args = parse(R"({"node":5,"key":"exposure","time":0,"value":0,"curve":0})");
    protocol.execute("set_layer_keyframe", args);
    args["time"] = 1;
    args["value"] = 1;
    protocol.execute("set_layer_keyframe", args);
    image = pixel(1);
    require(std::abs(image.rgba[0] - 2) < .003,
            "Shader parameters inside layer effect nodes can be automated");
}
static void gradingChecks(const std::filesystem::path &root) {
    sf::Session s;
    s.renderer.initialize();
    s.renderer.library.scan(root);
    s.project = sf::Project{};
    s.project.width = s.project.height = 64;
    s.project.duration = 1;
    s.project.workingSpace = "linear709";
    s.project.backgroundAlpha = 0;
    sf::Clip c;
    c.id = s.project.allocate();
    c.generator = true;
    c.duration = 1;
    sf::Effect input;
    input.id = "custom";
    input.code = "float4 Effect(EffectContext ctx){return float4(2+ctx.uv.x*.02,-.25,ctx.uv.y,.25);}";
    c.effects = {input, sf::makeGrade()};
    s.project.addTrack(sf::TrackKind::Video).clips = {c};
    s.project.validate();
    auto &grade = s.project.findClip(c.id)->effects[1];
    s.renderer.render(s.project, s.media, 0, 64, 64);
    auto neutral = s.renderer.readbackFloat();
    std::cout << "Neutral grade first pixel: " << neutral.rgba[0] << ", " << neutral.rgba[1] << ", "
              << neutral.rgba[2] << ", alpha " << neutral.rgba[3] << std::endl;
    require(neutral.rgba[0] >= 2 && neutral.rgba[1] == -.25f && neutral.rgba[3] == .25f,
            "Neutral grading preserves negative HDR RGB and straight alpha on GPU");
    grade.grading["curves"]["hue_hue"] = parse("[[0,0],[1,0]]");
    s.renderer.render(s.project, s.media, 0, 64, 64);
    require(s.renderer.readbackFloat().rgba[1] == -.25f,
            "Identity hue curves preserve the negative RGB residue on GPU");
    grade = sf::makeGrade();
    grade.params["exposure"].value = 1;
    s.renderer.render(s.project, s.media, 0, 64, 64);
    auto exposed = s.renderer.readbackFloat();
    require(std::abs(exposed.rgba[0] - neutral.rgba[0] * 2) < .005f && exposed.rgba[1] == -.5f &&
                exposed.rgba[3] == .25f,
            "Grade exposure gains HDR instead of clipping and retains alpha");
    grade.params["intensity"].value = 0;
    s.renderer.render(s.project, s.media, 0, 64, 64);
    require(s.renderer.readbackFloat().rgba == neutral.rgba,
            "Zero grading intensity leaves the original half-float image intact");
    grade.params["intensity"].value = 1;
    grade.params["mask_mode"].value = 1;
    grade.params["mask_width"].value = .25f;
    grade.params["mask_height"].value = .25f;
    grade.params["mask_feather"].value = 0;
    s.renderer.render(s.project, s.media, 0, 64, 64);
    auto local = s.renderer.readbackFloat();
    size_t center = (32 * 64 + 32) * 4;
    require(local.rgba[0] == neutral.rgba[0] && local.rgba[3] == neutral.rgba[3] &&
                std::abs(local.rgba[center] - neutral.rgba[center] * 2) < .005f,
            "Local correction only changes selected pixels, preserving the image outside and all alpha");
    s.renderer.render(s.project, s.media, 0, 64, 64, true);
    require(s.renderer.readbackFloat().rgba == neutral.rgba,
            "Preview grading bypass does not change the stored project");
    grade = sf::makeGrade();
    grade.params["exposure"] = {.3f};
    grade.params["temperature"] = {1200};
    grade.params["tint"] = {.13f};
    grade.params["contrast"] = {1.2f};
    grade.params["saturation"] = {.7f};
    grade.params["vibrance"] = {.15f};
    grade.params["shadow_b"] = {.03f};
    grade.params["mid_r"] = {-.01f};
    grade.params["high_g"] = {.02f};
    grade.params["offset_r"] = {.01f};
    grade.grading["curves"]["master"] = parse("[[0,0],[0.5,0.6],[1,1]]");
    grade.grading["curves"]["r"] = parse("[[0,0],[0.3,0.4],[1,1]]");
    grade.grading["curves"]["g"] = parse("[[0,0],[0.5,0.45],[1,1]]");
    grade.grading["curves"]["b"] = parse("[[0,0],[0.5,0.55],[1,1]]");
    grade.grading["curves"]["hue_hue"] = parse("[[0,0.1],[0.5,-0.1],[1,0.1]]");
    grade.grading["curves"]["hue_sat"] = parse("[[0,1],[0.4,0.8],[1,1]]");
    grade.grading["curves"]["hue_luma"] = parse("[[0,1],[0.5,0.9],[1,1]]");
    grade.grading["curves"]["luma_sat"] = parse("[[0,1],[0.5,0.8],[1,1]]");
    sf::validateGrade(grade);
    s.renderer.render(s.project, s.media, 0, 64, 64);
    auto gpu = s.renderer.readbackFloat();
    float worst = 0;
    for (unsigned y = 2; y < 64; y += 9)
        for (unsigned x = 2; x < 64; x += 9) {
            size_t i = (y * 64 + x) * 4;
            auto cpu = sf::gradeColor(grade, {neutral.rgba[i], neutral.rgba[i + 1], neutral.rgba[i + 2]},
                                      "linear709", 0, (x + .5f) / 64, (y + .5f) / 64);
            for (int k = 0; k < 3; ++k)
                worst = std::max(worst, std::abs(cpu[k] - gpu.rgba[i + k]));
        }
    require(worst < .015f,
            "GPU white balance, primaries, wheels and all eight curves match the CPU reference");
    sf::Effect selected = sf::makeGrade();
    selected.params["qualifier"] = {1};
    selected.params["hue"] = {0};
    selected.params["hue_width"] = {.03f};
    require(sf::gradeSelection(selected, {.5f, 0, 0}, .5f, .5f, 0) > .99f &&
                sf::gradeSelection(selected, {0, .5f, 0}, .5f, .5f, 0) < .001f,
            "HSL qualifier selects red while rejecting green");
    selected.params["qual_invert"] = {1};
    require(sf::gradeSelection(selected, {.5f, 0, 0}, .5f, .5f, 0) < .001f,
            "Qualifier inversion reverses only correction coverage");
    selected.params["hue"] = {.99f};
    selected.params["qual_invert"] = {0};
    require(sf::gradeSelection(selected, {.5f, 0, 0}, .5f, .5f, 0) > .99f,
            "Hue selection wraps across red at the 0/1 seam");
    grade = selected;
    grade.params["exposure"] = {1};
    s.renderer.render(s.project, s.media, 0, 64, 64);
    gpu = s.renderer.readbackFloat();
    worst = 0;
    for (unsigned y = 4; y < 64; y += 11)
        for (unsigned x = 4; x < 64; x += 11) {
            size_t i = (y * 64 + x) * 4;
            auto cpu = sf::gradeColor(grade, {neutral.rgba[i], neutral.rgba[i + 1], neutral.rgba[i + 2]},
                                      "linear709", 0, (x + .5f) / 64, (y + .5f) / 64);
            for (int k = 0; k < 3; ++k)
                worst = std::max(worst, std::abs(cpu[k] - gpu.rgba[i + k]));
        }
    require(worst < .006f, "Soft HSL qualifier agrees on CPU and GPU");
    grade = sf::makeGrade();
    grade.params["mask_mode"] = {3};
    grade.params["exposure"] = {1};
    grade.grading["maskPoints"] = parse("[[-0.5,-0.5],[0.5,-0.5],[0,0.5]]");
    s.project.validate();
    s.renderer.render(s.project, s.media, 0, 64, 64);
    gpu = s.renderer.readbackFloat();
    require(gpu.rgba[0] == neutral.rgba[0] && gpu.rgba[center] > neutral.rgba[center] * 1.9f,
            "Feathered polygon masks execute in the native grading shader");
    grade = sf::makeGrade();
    sf::neutralGrade(grade, {.4f, .2f, .1f});
    auto balanced = sf::gradeColor(grade, {.4f, .2f, .1f}, "linear709", 0);
    require(std::abs(balanced.x - balanced.y) < 1e-6f && std::abs(balanced.y - balanced.z) < 1e-6f,
            "Neutral pipette balances a tinted gray without changing its target luminance");
    require(rejects([&] { sf::neutralGrade(grade, {0, .1f, .2f}); }),
            "Pipette rejects black or invalid samples");
    auto qa = root / "build/qa/production";
    for (int dim : {1, 3}) {
        auto path = qa / (dim == 1 ? "grade-1d.cube" : "grade-3d.cube");
        std::ofstream file(path);
        file << "TITLE \"Test\"\nLUT_" << dim << "D_SIZE 2\nDOMAIN_MIN -1 -1 -1\nDOMAIN_MAX 3 3 3\n";
        if (dim == 1)
            file << "0 0 0\n1 2 3\n";
        else
            for (int z = 0; z < 2; ++z)
                for (int y = 0; y < 2; ++y)
                    for (int x = 0; x < 2; ++x)
                        file << x << ' ' << y * 2 << ' ' << z * 3 << '\n';
        file.close();
        grade = sf::makeGrade();
        grade.grading["lut"] = sf::loadCube(path);
        s.renderer.render(s.project, s.media, 0, 64, 64);
        gpu = s.renderer.readbackFloat();
        auto cpu =
            sf::gradeColor(grade, {neutral.rgba[center], neutral.rgba[center + 1], neutral.rgba[center + 2]},
                           "linear709", 0);
        std::cout << "LUT " << dim << "D CPU " << cpu.x << ", " << cpu.y << ", " << cpu.z << " GPU "
                  << gpu.rgba[center] << ", " << gpu.rgba[center + 1] << ", " << gpu.rgba[center + 2]
                  << std::endl;
        require(std::abs(cpu.x - gpu.rgba[center]) < .003f &&
                    std::abs(cpu.y - gpu.rgba[center + 1]) < .003f &&
                    std::abs(cpu.z - gpu.rgba[center + 2]) < .003f,
                "Embedded .cube LUT respects 1D/3D axes, declared domains and interpolation on GPU");
        grade.grading["lutInput"] = "srgb";
        grade.grading["lutOutput"] = "linear2020";
        s.renderer.render(s.project, s.media, 0, 64, 64);
        gpu = s.renderer.readbackFloat();
        cpu =
            sf::gradeColor(grade, {neutral.rgba[center], neutral.rgba[center + 1], neutral.rgba[center + 2]},
                           "linear709", 0);
        require(std::abs(cpu.x - gpu.rgba[center]) < .005f && std::abs(cpu.y - gpu.rgba[center + 1]) < .005f,
                "LUT input/output transfer functions and gamut conversion agree on CPU/GPU");
    }
    grade.params["lut_intensity"] = {0};
    s.renderer.render(s.project, s.media, 0, 64, 64);
    require(s.renderer.readbackFloat().rgba == neutral.rgba,
            "LUT intensity zero bypasses the look without clipping the source");
    auto bad = qa / "grade-bad.cube";
    {
        std::ofstream f(bad);
        f << "LUT_3D_SIZE 2\n0 0 0\n";
    }
    require(rejects([&] { sf::loadCube(bad); }), "Truncated .cube files are rejected before GPU allocation");
    grade = sf::makeGrade();
    grade.params["exposure"].setKey({0, 0});
    grade.params["exposure"].setKey({1, 1});
    s.renderer.render(s.project, s.media, .5, 64, 64);
    gpu = s.renderer.readbackFloat();
    require(std::abs(gpu.rgba[0] - neutral.rgba[0] * std::sqrt(2.f)) < .005f,
            "Grade parameters animate in sequence time through native keyframes");
    auto round = sf::Project::fromJson(s.project.json());
    require(round.json().Dump(0) == s.project.json().Dump(0) && round.json()["schema"].I() == 10,
            "Project schema10 roundtrips grading configuration and animation");
    sf::Protocol protocol(s);
    auto a = parse("{\"effect\":1}");
    a["clip"] = c.id;
    a["path"] = sx::Narrow((qa / "grade.sgrade").wstring());
    protocol.execute("save_grade_preset", a);
    grade.params["exposure"] = {2};
    protocol.execute("load_grade_preset", a);
    require(s.project.findClip(c.id)->effects[1].params.at("exposure").keys.size() == 2,
            "Portable grading presets restore animated parameters and embedded configuration");
    auto previous = s.project.json().Dump(0);
    a["settings"] = parse("{\"curves\":{\"master\":[[0,0],[0,1]]}}");
    require(rejects([&] { protocol.execute("set_grade", a); }) && s.project.json().Dump(0) == previous,
            "Invalid grading mutations roll back atomically through MCP");
    auto scopes = sf::videoScopes(neutral, "reinhard");
    require(scopes.samples == 4096 && scopes.below == 4096 && scopes.above == 4096 && scopes.maximum > 2 &&
                scopes.minimum == -.25f,
            "Video scopes retain scene HDR/negative statistics while plotting the display transform");
    require(scopes.waveform.size() == 256 * 128 && scopes.parade.size() == 3 * 128 * 128 &&
                scopes.histogram.size() == 3 * 256 && scopes.vectorscope.size() == 256 * 256,
            "All four video scope analyses produce their declared layouts");
    auto log = sf::convertColor({.18f, .18f, .18f}, "linear709", "logc3-awg3-ei800");
    auto lin = sf::convertColor(log, "logc3-awg3-ei800", "linear709");
    require(std::abs(log.x - .391f) < .001f && std::abs(lin.x - .18f) < .00001f,
            "Official ARRI EI800 LogC3 transform maps 18 percent gray and roundtrips");
    for (auto profile : {"srgb", "linear2020", "logc3-awg3-ei800"}) {
        auto encoded = sf::convertColor({.6f, .3f, -.01f}, "linear709", profile);
        auto decoded = sf::convertColor(encoded, profile, "linear709");
        require(std::abs(decoded.x - .6f) < .00002f && std::abs(decoded.y - .3f) < .00002f &&
                    std::abs(decoded.z + .01f) < .00002f,
                "Color transforms preserve chromatic samples and negative values on roundtrip");
    }
    auto &lastGrade = s.project.findClip(c.id)->effects[1];
    lastGrade = sf::makeGrade();
    lastGrade.grading["lut"] = sf::loadCube(qa / "grade-1d.cube");
    lastGrade.grading["lutInput"] = "logc3-awg3-ei800";
    lastGrade.grading["lutOutput"] = "logc3-awg3-ei800";
    s.renderer.render(s.project, s.media, 0, 64, 64);
    gpu = s.renderer.readbackFloat();
    auto logCpu =
        sf::gradeColor(lastGrade, {neutral.rgba[center], neutral.rgba[center + 1], neutral.rgba[center + 2]},
                       "linear709", 0);
    require(std::abs(gpu.rgba[center] - logCpu.x) < std::abs(logCpu.x) * .002f + .001f,
            "LogC3 LUT input/output executes with matching float CPU/GPU transfer functions");
    lastGrade = sf::makeGrade();
    lastGrade.params["exposure"] = {.5f};
    for (auto working : {"srgb", "linear2020"}) {
        s.project.workingSpace = working;
        s.renderer.render(s.project, s.media, 0, 64, 64, true);
        auto original = s.renderer.readbackFloat();
        s.renderer.render(s.project, s.media, 0, 64, 64);
        gpu = s.renderer.readbackFloat();
        auto cpu = sf::gradeColor(
            lastGrade, {original.rgba[center], original.rgba[center + 1], original.rgba[center + 2]}, working,
            0);
        require(std::abs(cpu.x - gpu.rgba[center]) < .01f && std::abs(cpu.y - gpu.rgba[center + 1]) < .01f,
                "Grading converts legacy sRGB and Rec.2020 working spaces to scene linear and back");
    }
}
static void workspaceChecks(const std::filesystem::path &root) {
    sf::Renderer r;
    r.initialize();
    r.library.scan(root);
    unsigned count = 0;
    std::string failures;
    for (auto &d : r.library.definitions)
        if (d.id.starts_with("shaderixel.")) {
            ++count;
            try {
                auto e = r.library.instantiate(d.id);
                r.validateShader(e.id == "grading" ? sf::gradeShader(e, "linear709") : sf::effectCode(e));
            } catch (const std::exception &e) {
                failures += d.id + ": " + e.what() + "\n";
            }
        }
    if (!failures.empty())
        throw std::runtime_error(failures);
    require(count == 317, "All 317 Shaderixel library shaders compile with their native defaults");
    sf::Project p;
    p.width = p.height = 64;
    p.backgroundAlpha = 0;
    p.workingSpace = "linear709";
    p.duration = 2;
    auto &t = p.addTrack(sf::TrackKind::Video);
    sf::Clip c;
    c.id = p.allocate();
    c.duration = 2;
    c.generator = true;
    sf::Effect e;
    e.id = "custom";
    e.code = "float4 Effect(EffectContext ctx){return float4(2,.25,0,.5);}";
    c.effects = {e};
    t.clips = {c};
    auto id = c.id;
    sf::MediaCache m;
    r.render(p, m, .4, 64, 64);
    auto original = r.readbackFloat().rgba;
    p.makeContainer(id);
    p.validate();
    r.render(p, m, .4, 64, 64);
    require(r.readbackFloat().rgba == original,
            "Converting a clip to an inner composition preserves HDR colors and alpha");
    auto child = sf::Project::fromJson(p.findClip(id)->composition);
    child.findClip(1)->effects[0].code = "float4 Effect(EffectContext ctx){return float4(0,0,3,1);}";
    p.findClip(id)->composition = child.json();
    r.render(p, m, .4, 64, 64);
    require(r.readbackFloat().rgba[2] == 3,
            "Edits to the container timeline reach the parent floating-point composition");
    p.findClip(id)->widget = sf::Json::Object();
    auto &w = p.findClip(id)->widget;
    w["type"] = "container";
    w["anchorX"] = .5;
    w["anchorY"] = .5;
    w["pivotX"] = .5;
    w["pivotY"] = .5;
    p.findClip(id)->params["widget_width"] = {32};
    p.findClip(id)->params["widget_height"] = {32};
    p.validate();
    r.render(p, m, .4, 64, 64);
    auto image = r.readbackFloat();
    require(image.rgba[3] == 0 && image.rgba[(32 * 64 + 32) * 4 + 2] == 3,
            "Motion anchors place nested content inside its widget rectangle");
    p.findClip(id)->params["widget_x"].keys = {{0, 0, sf::Curve::Linear, 0}, {1, 16, sf::Curve::Linear, 0}};
    require(sf::widgetRect(p, *p.findClip(id), .5).x == 24,
            "Widget layout evaluates animated offsets in project pixels");
    auto bad = p;
    bad.findClip(id)->widget["parent"] = id;
    require(rejects([&] { bad.validate(); }), "Motion parent cycles are rejected before rendering");
    auto copied = p;
    auto duplicates = copied.duplicateClips({id});
    const auto &duplicateLayout = copied.findClip(duplicates.front())->widget;
    require(!duplicateLayout.Has("parent"),
            "Duplicating a root widget preserves its optional parent field without inserting null");
    sf::FloatImage atlas;
    atlas.width = 4;
    atlas.height = 2;
    atlas.rgba.resize(32);
    atlas.colorSpace = "srgb";
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x) {
                auto at = (y * 4 + z * 2 + x) * 4;
                atlas.rgba[at] = float(x);
                atlas.rgba[at + 1] = float(y);
                atlas.rgba[at + 2] = float(z);
                atlas.rgba[at + 3] = 1;
            }
    auto path = root / "build/qa/identity-atlas.png";
    sf::writePng16(atlas, sx::Narrow(path.wstring()));
    auto lut = sf::loadLut(path);
    require(lut["size"].I() == 2 && lut["layout"].S() == "atlas" && lut["data"].At(23).F() == 1,
            "2D LUT atlas imports as a red-fast 3D color volume from a 16-bit image");
    require(rejects([&] { sf::loadLut(path, "invalid"); }), "Unknown LUT layouts are rejected");
    {
        sf::ClipPreviews previews;
        auto source = c;
        sf::Clip title = source;
        title.id = 101;
        title.generator = false;
        title.effects.clear();
        title.title.enabled = true;
        title.title.text = "Preview";
        sf::Clip picture = title;
        picture.id = 102;
        picture.title.enabled = false;
        picture.source = sx::Narrow(path.wstring());
        bool shaderReady = false, titleReady = false, imageReady = false;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline && !(shaderReady && titleReady && imageReady)) {
            previews.beginFrame();
            shaderReady |= previews.get(p, source, .4, r) != nullptr;
            titleReady |= previews.get(p, title, .4, r) != nullptr;
            imageReady |= previews.get(p, picture, .4, r) != nullptr;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        require(shaderReady && titleReady && imageReady,
                "Background timeline thumbnails render HLSL clips, titles and image media");
    }
    {
        sf::Project timed;
        timed.width = timed.height = 64;
        timed.duration = 3;
        timed.backgroundAlpha = 0;
        auto item = c;
        item.start = 1;
        item.duration = 2;
        item.effects[0].code = "float4 Effect(EffectContext ctx){return float4(ctx.time,ctx.localTime,0,1);}";
        timed.addTrack(sf::TrackKind::Video).clips = {item};
        timed.nextId = std::max(timed.nextId, item.id + 1);
        r.render(timed, m, 1.4, 64, 64);
        auto before = r.readbackFloat().rgba;
        timed.makeContainer(item.id);
        timed.validate();
        r.render(timed, m, 1.4, 64, 64);
        require(
            r.readbackFloat().rgba == before,
            "Container conversion preserves global shader time and source-local time at nonzero clip starts");
    }
    auto round = sf::Project::fromJson(p.json());
    require(round.json().Dump(0) == p.json().Dump(0),
            "Nested compositions and motion layout roundtrip in schema10");
}
int main(int argc, char **argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    int status = 0;
    try {
        auto root = sx::Utf8Path(argv[1]);
        auto qa = root / "build/qa/production";
        std::filesystem::create_directories(qa);
        auto path = [&](const char *name) { return sx::Narrow((qa / name).wstring()); };
        sf::DspCanvas canvas;
        canvas.open(parse(R"({"schema":2,"nodes":[],"connections":[{"source":0,"destination":0}]})"));
        auto gain = canvas.add("cydaw.eq-three", {300, 0});
        canvas.connect(0, gain);
        canvas.disconnect(0, sf::DspCanvas::outputBoundary);
        canvas.connect(gain, sf::DspCanvas::outputBoundary);
        require(sf::decodeDspGraph(canvas.document()).order.size() == 1,
                "Visual DSP draft compiles through CyDAW v2");
        auto second = canvas.add("cydaw.eq-three", {560, 0});
        canvas.connect(gain, second);
        require(rejects([&] { canvas.connect(second, gain); }),
                "Visual DSP rejects cycles without mutating the draft");
        canvas.arrange();
        auto arranged = canvas.document().Dump(0);
        require(canvas.undo() && canvas.redo() && canvas.document().Dump(0) == arranged,
                "DSP layout and cable changes have local undo/redo");
        canvas.remove({second});
        require(sf::decodeDspGraph(canvas.document()).order.size() == 1,
                "Removing a DSP node removes its cables");
        sf::Renderer renderer;
        renderer.initialize();
        renderer.library.scan(root);
        sf::MediaCache media;
        sf::Project p;
        p.width = p.height = 64;
        p.duration = 1;
        p.workingSpace = "linear709";
        p.backgroundAlpha = 0;
        auto &track = p.addTrack(sf::TrackKind::Video);
        sf::Clip c;
        c.id = p.allocate();
        auto id = c.id;
        c.generator = true;
        c.duration = 1;
        sf::Effect gradient;
        gradient.id = "custom";
        gradient.code = "float4 Effect(EffectContext ctx){return float4(2+ctx.uv.x*.02,-.25,ctx.uv.y,.25);}";
        c.effects = {gradient};
        track.clips = {c};
        p.validate();
        renderer.render(p, media, 0, 64, 64);
        auto fp = renderer.readbackFloat();
        require(fp.rgba[0] >= 2 && fp.rgba[1] == -.25f && fp.rgba[3] == .25f,
                "RGBA16F preserves HDR, negative RGB and straight alpha");
        std::set<float> unique;
        for (unsigned x = 0; x < 64; ++x)
            unique.insert(fp.rgba[x * 4]);
        std::cout << "Gradient levels: " << unique.size() << ", first: " << fp.rgba[0]
                  << ", last: " << fp.rgba[252] << std::endl;
        require(unique.size() > 8, "Shader gradients retain sub-8-bit precision");
        sf::writeExr(fp, path("hdr.exr"));
        auto exr = sf::readExr(path("hdr.exr"));
        require(exr.rgba == fp.rgba && exr.colorSpace == "linear709",
                "EXR half roundtrip preserves HDR and premultiplied alpha");
        sf::MediaAsset imported;
        imported.open(path("hdr.exr"));
        require(imported.frame.rgba == fp.rgba, "EXR media import keeps original float pixels");
        sf::FloatImage ramp{1024, 1, std::vector<float>(4096), "srgb"};
        for (unsigned x = 0; x < 1024; ++x) {
            for (int k = 0; k < 3; ++k)
                ramp.rgba[x * 4 + k] = x / 1023.f;
            ramp.rgba[x * 4 + 3] = .5f;
        }
        sf::writePng16(ramp, path("ramp16.png"));
        sf::MediaAsset png;
        png.open(path("ramp16.png"));
        std::set<float> levels;
        for (unsigned x = 0; x < 1024; ++x)
            levels.insert(png.frame.rgba[x * 4]);
        require(levels.size() == 1024 && std::abs(png.frame.rgba[3] - .5f) < .00002,
                "PNG16 export and WIC import keep 1024 unique levels and alpha");
        p.toneMap = "reinhard";
        renderer.render(p, media, 0, 64, 64);
        auto display = renderer.readback();
        auto preciseDisplay = renderer.readbackFloat(true);
        require(std::abs(display.pixels[2] / 255.f - preciseDisplay.rgba[0]) < .005,
                "GPU preview and precise PNG tone mapping agree");
        auto roundtrip = sf::Project::fromJson(p.json());
        require(roundtrip.workingSpace == "linear709" && roundtrip.backgroundAlpha == 0 &&
                    roundtrip.toneMap == "reinhard",
                "Project schema7 preserves color management");
        require(std::abs(sf::srgbEncode(sf::srgbDecode(.5f)) - .5f) < 1e-6,
                "sRGB transfer functions roundtrip correctly");
        auto ofxPath = sx::Narrow((std::filesystem::path(argv[0]).parent_path() / "ofx_gain.ofx").wstring());
        sf::IsolatedOfx worker(ofxPath, "", 64, 64);
        auto metadata = worker.metadata();
        sf::Effect ofx;
        ofx.id = "ofx";
        ofx.code = ofxPath;
        ofx.graph = metadata;
        ofx.params["p0"] = {2};
        sf::validateOfx(ofx);
        auto output = worker.render(fp, ofx, 0, 30, 1);
        require(std::abs(output.rgba[0] - fp.rgba[0] * 2) < 1e-6 && output.rgba[3] == .25f &&
                    output.rgba[2] == fp.rgba[2] * 2,
                "Real OpenFX ABI processes float HDR, orientation and alpha in an isolated worker");
        p.findClip(id)->effects.push_back(ofx);
        renderer.render(p, media, 0, 64, 64);
        require(renderer.readbackFloat().rgba[0] >= 4,
                "OFX composes with HLSL on the RGBA16F render pipeline");
        worker.terminateForTest();
        require(rejects([&] { worker.render(fp, ofx, 0, 30, 1); }),
                "OFX worker crash is reported without crashing its host");
        require(sf::Project::fromJson(p.json()).findClip(id)->effects.back().graph["identifier"].S() ==
                    metadata["identifier"].S(),
                "OFX identity and controls survive project save/load");
        {
            auto basicPath =
                sx::Narrow((std::filesystem::path(argv[0]).parent_path() / "ofx_basic.ofx").wstring());
            sf::IsolatedOfx basic(basicPath, "uk.co.thefoundry.BasicGainPlugin", 64, 64);
            sf::Effect effect;
            effect.id = "ofx";
            effect.code = basicPath;
            effect.graph = basic.metadata();
            for (auto &p : effect.graph["parameters"].Elements())
                effect.params[p["key"].S()] = {p["default"].F()};
            auto rendered = basic.render(fp, effect, 0, 30, 1);
            require(rendered.rgba == fp.rgba,
                    "Unmodified ASWF Basic example processes RGBA float through the independent OFX host");
            for (auto &p : effect.graph["parameters"].Elements())
                if (p["name"].S() == "scale")
                    effect.params[p["key"].S()] = {2};
            rendered = basic.render(fp, effect, .1, 30, 1);
            require(
                rendered.rgba[3] == .5f && rendered.rgba[0] == fp.rgba[0],
                "Foreign OFX parameters and instance-change notifications preserve premultiplied semantics");
        }
        {
            auto &clip = *p.findClip(id);
            clip.effects.resize(1);
            clip.effects[0].code =
                "//@premultiplied\nfloat4 Effect(EffectContext ctx){return float4(.25,0,0,.25);}";
            renderer.render(p, media, 0, 64, 64);
            auto result = renderer.readbackFloat();
            require(result.rgba[0] == 1 && result.rgba[3] == .25f,
                    "Premultiplied HLSL effects convert correctly into the float composition");
            clip.effects[0].code = "float4 Effect(EffectContext ctx){return float4(1,1,1,.5);}";
            p.backgroundAlpha = 1;
            p.toneMap = "clip";
            renderer.render(p, media, 0, 64, 64);
            require(renderer.readbackFloat().rgba[0] == .5f &&
                        std::abs(renderer.readback().pixels[2] - 188) < 2,
                    "Linear-light alpha composition produces the expected sRGB display value");
        }
        auto vstFixture =
            root /
            "vendor/cydaw2/build/desktop-asio/CyDAWTestGain_artefacts/Release/VST3/CyDAW Test Gain.vst3";
        if (std::filesystem::is_regular_file(vstFixture / "Contents/x86_64-win/CyDAW Test Gain.vst3")) {
            sf::IsolatedAudio vst(
                "vst3", sx::Narrow((root / "vendor/cydaw2/build/desktop-asio/CyDAWTestGain_artefacts/Release/"
                                           "VST3/CyDAW Test Gain.vst3")
                                       .wstring()));
            auto original = vst.parameters();
            vst.setParameter(0, .2f);
            auto state = vst.state();
            vst.setParameter(0, .8f);
            vst.restoreState(state);
            require(std::abs(vst.parameters().at("p0") - .2f) < .001 &&
                        !vst.parameterNames().at("p0").empty(),
                    "Real CyDAW VST3 worker saves/restores opaque state and exposes named parameters");
            vst.showEditor(true);
            vst.showEditor(false);
            require(!sf::decodeBase64(state).empty(), "VST3 native editor opens/closes through CyDAW IPC v4");
            require(rejects([] { sf::decodeBase64("bad!!!"); }), "Malformed plugin states are rejected");
            sf::Session session;
            sf::Protocol protocol(session);
            sf::Json add = parse("{\"scope\":\"master\",\"kind\":\"vst3\"}");
            add["path"] = sx::Narrow(
                (root /
                 "vendor/cydaw2/build/desktop-asio/CyDAWTestGain_artefacts/Release/VST3/CyDAW Test Gain.vst3")
                    .wstring());
            auto added = protocol.execute("add_external_audio", add);
            auto target = parse("{\"scope\":\"master\",\"effect\":0}");
            target["effect"] = added["effect"];
            target["state"] = state;
            protocol.execute("set_plugin_state", target);
            target["path"] = path("gain.sfvstpreset");
            protocol.execute("save_plugin_preset", target);
            protocol.execute("load_plugin_preset", target);
            require(std::abs(session.project.masterMix.effects.back().params.at("p0").value - .2f) < .001,
                    "Full VST3 presets restore state and parameter values through MCP");
        } else
            std::cout
                << "SKIP: optional CyDAW VST3 fixture not installed; use prepare_dependencies.py --tests.\n";
        std::vector<float> signal(48000 * 8);
        for (unsigned n = 0; n < signal.size() / 2; ++n)
            signal[n * 2] = signal[n * 2 + 1] =
                float(.9 * std::sin(2 * 3.141592653589793 * 12000 * n / 48000 + 3.141592653589793 / 4));
        sf::Loudness loudness;
        loudness.push(signal);
        require(loudness.truePeak(true) > loudness.samplePeak() + 2 &&
                    std::abs(loudness.truePeak(true) - 20 * std::log10(.9)) < .3,
                "4x FIR true peak detects intersample peaks hidden between 12kHz samples");
        sf::Loudness chunks;
        for (size_t n = 0; n < signal.size(); n += 514)
            chunks.push(
                std::vector<float>(signal.begin() + n, signal.begin() + std::min(n + 514, signal.size())));
        require(std::abs(chunks.truePeak(true) - loudness.truePeak(true)) < 1e-9 &&
                    std::abs(chunks.integrated() - loudness.integrated()) < 1e-9 &&
                    loudness.shortTerm() > -120,
                "True peak and LUFS are independent of audio block boundaries");
        sf::Loudness silent;
        silent.push(std::vector<float>(48000 * 6));
        require(silent.shortTerm() == -120 && silent.truePeak(true) == -120,
                "Short-term LUFS and true peak handle silence");
        workspaceChecks(root);
        layerGraphChecks(root);
        sharedAudioChecks();
        gradingChecks(root);
        std::cout << checks << " production checks passed" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << e.what() << std::endl;
        status = 1;
    }
    MFShutdown();
    CoUninitialize();
    return status;
}

#include "adapters/CyDawAudio.h"
#include "adapters/DspGraph.h"
namespace sf {
// CyDAW's render plan owns summing, ordering, buffers and processor lifetime.
// This boundary translates the timeline's interleaved stereo source only.
class TimelineGraphInput final : public cydaw::engine::AudioProcessor {
  public:
    void prepare(const cydaw::engine::ProcessSpec &) override {}
    void reset() noexcept override {}
    void process(cydaw::engine::AudioBlockView in, cydaw::engine::AudioBlockView out,
                 const cydaw::engine::ProcessContext &) noexcept override {
        cydaw::engine::copyAudio(out, in);
    }
};
using namespace cydaw::engine;
const EffectDescriptor *audioEffect(const std::string &id) {
    static const EffectDescriptor native{"native",       "native", NativeDspNodeKind::passthrough, "", "",
                                         "Native Plugin", "",       EffectGroup::modulation};
    static const EffectDescriptor vst{"vst3",        "vst3", NativeDspNodeKind::passthrough, "", "",
                                      "Plugin VST3", "",     EffectGroup::modulation};
    if (id == "native")
        return &native;
    if (id == "vst3")
        return &vst;
    static const EffectDescriptor graph{
        "graph", "cydaw.graph",          NativeDspNodeKind::passthrough, "", "", "DSP Graph",
        "",      EffectGroup::modulation};
    if (id == "cydaw.graph")
        return &graph;
    if (!id.starts_with("cydaw."))
        return nullptr;
    auto *e = effectForType(id.substr(6));
    return e && !e->convertsChannels() ? e : nullptr;
}
Effect instantiateAudioEffect(const std::string &id) {
    auto *d = audioEffect(id);
    if (!d)
        throw std::runtime_error("Audio effect not found");
    Effect e;
    e.id = id;
    for (auto &p : d->parameters)
        e.params[std::string(p.key)] = {p.defaultValue};
    return e;
}
void AudioEffects::process(const Clip &c, double time, unsigned frames, std::vector<float> &data) {
    process(c.id, c.effects, time, frames, data);
}
unsigned AudioEffects::latency(Id owner, const std::vector<Effect> &chain) {
    unsigned total = 0;
    for (int index = 0; index < int(chain.size()); ++index) {
        auto &effect = chain[index];
        if (!effect.enabled || !externalAudio(effect.id))
            continue;
        auto &entry = instances[{owner, index}];
        auto identity = effect.id + ":" + effect.code;
        used.insert({owner, index});
        if (entry.id != identity || entry.state != effect.pluginState) {
            entry.external = std::make_unique<IsolatedAudio>(effect.id, effect.code);
            entry.external->restoreState(effect.pluginState);
            entry.state = effect.pluginState;
            entry.id = identity;
            entry.processor.reset();
        }
        total += entry.external->latency();
        if (total > 480000)
            throw std::runtime_error("Total channel latency exceeds 10 seconds");
    }
    return total;
}
void AudioEffects::process(Id owner, const std::vector<Effect> &chain, double time, unsigned frames,
                           std::vector<float> &data) {
    for (int index = 0; index < int(chain.size()); ++index) {
        auto &effect = chain[index];
        if (!effect.enabled)
            continue;
        auto *d = audioEffect(effect.id);
        if (!d)
            throw std::runtime_error("Unknown audio effect: " + effect.id);
        used.insert({owner, index});
        auto &entry = instances[{owner, index}];
        if (effect.id == "cydaw.graph") {
            auto decoded = decodeDspGraph(effect.graph);
            auto identity = effect.graph.Dump(0);
            if (entry.id != identity) {
                AudioRenderPlanBuilder builder;
                constexpr uint64_t inputId = 0x100000000ull, outputId = inputId + 1;
                entry.graphProcessors.clear();
                auto add = [&](uint64_t id, std::unique_ptr<AudioProcessor> processor, bool input = false) {
                    if (builder.addNode(id, std::move(processor), input) != RenderPlanError::none)
                        throw std::runtime_error("CyDAW: cannot add processor");
                };
                add(inputId, std::make_unique<TimelineGraphInput>(), true);
                add(outputId, std::make_unique<TimelineGraphInput>());
                for (auto &item : decoded.nodes) {
                    std::unique_ptr<AudioProcessor> processor;
                    if (item.effect.id == "sf.sum")
                        processor = std::make_unique<TimelineGraphInput>();
                    else {
                        auto *d = audioEffect(item.effect.id);
                        NativeDspNode node;
                        node.kind = d->kind;
                        for (auto &parameter : d->parameters)
                            setEffectValue(node.parameters, parameter,
                                           item.effect.params.at(std::string(parameter.key)).value);
                        processor = makeEffectProcessor(node);
                        entry.graphProcessors[item.id] = processor.get();
                    }
                    add(item.id, std::move(processor));
                }
                for (auto &wire : decoded.definition.connections)
                    if (builder.connect(wire.source.nodeId ? wire.source.nodeId : inputId,
                                        wire.destination.nodeId ? wire.destination.nodeId : outputId,
                                        wire.source.portId, wire.destination.portId) != RenderPlanError::none)
                        throw std::runtime_error("CyDAW: invalid audio connection");
                auto compiled = builder.compile({48000, 1024, 2}, outputId);
                if (!compiled)
                    throw std::runtime_error(compiled.message);
                entry.graph = std::move(compiled.plan);
                entry.id = identity;
                entry.processor.reset();
                entry.external.reset();
            }
            for (unsigned offset = 0; offset < frames; offset += 64) {
                unsigned count = std::min(64u, frames - offset);
                double now = time + offset / 48000.;
                for (auto &item : decoded.nodes)
                    if (item.effect.id != "sf.sum") {
                        auto *d = audioEffect(item.effect.id);
                        NativeDspNode node;
                        node.kind = d->kind;
                        for (auto &parameter : d->parameters)
                            setEffectValue(node.parameters, parameter,
                                           item.effect.params.at(std::string(parameter.key)).at(now));
                        updateEffectProcessor(*entry.graphProcessors.at(item.id), node);
                    }
                auto in = input.view(count), out = output.view(count);
                for (unsigned n = 0; n < count; ++n)
                    for (unsigned ch = 0; ch < 2; ++ch)
                        in.channel(ch)[n] = data[(offset + n) * 2 + ch];
                ProcessContext context;
                context.transport.state = PlaybackState::playing;
                context.transport.samplePosition = int64_t(now * 48000);
                context.transport.quarterNotePosition = now * 2;
                entry.graph->process(in, out, context);
                for (unsigned n = 0; n < count; ++n)
                    for (unsigned ch = 0; ch < 2; ++ch)
                        data[(offset + n) * 2 + ch] = out.channel(ch)[n];
            }
            continue;
        }
        if (externalAudio(effect.id)) {
            auto identity = effect.id + ":" + effect.code;
            if (entry.id != identity || entry.state != effect.pluginState) {
                entry.external = std::make_unique<IsolatedAudio>(effect.id, effect.code);
                entry.external->restoreState(effect.pluginState);
                entry.state = effect.pluginState;
                entry.id = identity;
                entry.processor.reset();
            }
            entry.external->process(time, frames, data, effect.params);
            continue;
        }
        if (entry.id != effect.id) {
            NativeDspNode node;
            node.kind = d->kind;
            for (auto &p : d->parameters)
                setEffectValue(node.parameters, p, p.defaultValue);
            entry.processor = makeEffectProcessor(node);
            entry.id = effect.id;
            entry.processor->prepare({48000, 1024, 2});
        }
        for (unsigned offset = 0; offset < frames; offset += 64) {
            unsigned count = std::min(64u, frames - offset);
            double now = time + offset / 48000.;
            NativeDspNode node;
            node.kind = d->kind;
            for (auto &param : d->parameters) {
                auto it = effect.params.find(std::string(param.key));
                float value = it == effect.params.end() ? param.defaultValue : it->second.at(now);
                setEffectValue(node.parameters, param, std::clamp(value, param.minimum, param.maximum));
            }
            updateEffectProcessor(*entry.processor, node);
            auto in = input.view(count), out = output.view(count);
            for (unsigned n = 0; n < count; ++n) {
                in.channel(0)[n] = data[(offset + n) * 2];
                in.channel(1)[n] = data[(offset + n) * 2 + 1];
            }
            ProcessContext context;
            context.transport.state = PlaybackState::playing;
            context.transport.samplePosition = int64_t(now * 48000);
            context.transport.quarterNotePosition = now * 2;
            entry.processor->process(in, out, context);
            for (unsigned n = 0; n < count; ++n) {
                data[(offset + n) * 2] = out.channel(0)[n];
                data[(offset + n) * 2 + 1] = out.channel(1)[n];
            }
        }
    }
}
cydaw::graph::GraphDefinition audioGraph(const Project &p) {
    using namespace cydaw::graph;
    p.validate();
    GraphDefinition g;
    g.reference = {"shaderixframe.audio-mix", {2, 0, 0}};
    g.schemaVersion = 2;
    g.displayName = "Video Mix";
    g.role = GraphRole::audioEffect;
    auto port = [](PortId id, std::string key, PortDirection direction, bool many = false) {
        GraphPort p;
        p.id = id;
        p.key = key;
        p.displayName = key;
        p.type = PortType::audio;
        p.audioType = AudioPortType::stereo;
        p.direction = direction;
        p.rate = PortRate::audio;
        p.channels = 2;
        p.acceptsMultipleConnections = many;
        return p;
    };
    NodeId next = 1;
    auto node = [&](std::string name, std::string processor, bool sum = false) {
        GraphNode n;
        n.id = next++;
        n.displayName = name;
        n.processorKey = processor;
        n.ports = {port(1, "in", PortDirection::input, sum), port(2, "out", PortDirection::output)};
        g.nodes.push_back(n);
        return n.id;
    };
    auto wire = [&](GraphEndpoint from, NodeId to) { g.connections.push_back({from, {to, 1}}); };
    std::map<Id, NodeId> sums;
    std::map<Id, std::pair<GraphEndpoint, GraphEndpoint>> taps;
    sums[0] = node("Master Sum", "sf.stereo-mixer", true);
    for (auto &bus : p.buses)
        sums[bus.id] = node(bus.name + " / Sum", "sf.stereo-mixer", true);
    auto channel = [&](Id id, std::string name, const MixChannel &mix, GraphEndpoint source,
                       bool trackChannel = false) {
        for (auto &effect : mix.effects)
            if (effect.enabled) {
                auto *d = audioEffect(effect.id);
                auto n = node(name + " / " + std::string(d->displayName), effect.id);
                wire(source, n);
                source = {n, 2};
            }
        auto gain = node(name, trackChannel ? "sf.gain" : "sf.gain-pan");
        wire(source, gain);
        taps[id] = {source, {gain, 2}};
    };
    PortId input = 2;
    g.ports.push_back(port(1, "out", PortDirection::output));
    for (auto &track : p.tracks)
        if (track.kind == TrackKind::Audio) {
            g.ports.push_back(port(input, "track-" + std::to_string(track.id), PortDirection::input));
            channel(track.id, track.name, track.mix, {0, input++}, true);
        }
    for (auto &bus : p.buses)
        channel(bus.id, bus.name + " / fader", bus.mix, {sums.at(bus.id), 2});
    channel(0, "Master / fader", p.masterMix, {sums.at(0), 2});
    auto route = [&](Id id, const MixChannel &mix) {
        wire(taps.at(id).second, sums.at(mix.output));
        for (auto &send : mix.sends)
            if (send.enabled) {
                auto gain =
                    node("Send " + std::to_string(id) + " → " + std::to_string(send.bus), "sf.send-gain");
                wire(send.preFader ? taps.at(id).first : taps.at(id).second, gain);
                g.nodes.back().parameters.push_back({"gain", -80, 12, send.gain});
                wire({gain, 2}, sums.at(send.bus));
            }
    };
    for (auto &track : p.tracks)
        if (track.kind == TrackKind::Audio)
            route(track.id, track.mix);
    for (auto &bus : p.buses)
        route(bus.id, bus.mix);
    g.connections.push_back({taps.at(0).second, {0, 1}});
    GraphLibrary library;
    auto validation = library.install(g);
    if (!validation.ok())
        throw std::runtime_error(validation.issues.front().message);
    if (!library.compile())
        throw std::runtime_error("CyDAW audio graph compilation failed");
    return g;
}
} // namespace sf

#pragma once
#include "core/Project.h"
#include "plugins/IsolatedAudio.h"
#include <cydaw/engine/AudioRenderPlan.h>
#include <cydaw/engine/EffectProcessors.h>
#include <cydaw/graph/GraphDefinition.h>
#include <set>

namespace sf {
// A stereo insert ShaderixFrame offers: not a channel converter, MIDI effect, modulator or container.
bool audioInsert(const cydaw::engine::EffectDescriptor &descriptor);
const cydaw::engine::EffectDescriptor *audioEffect(const std::string &id);
Effect instantiateAudioEffect(const std::string &id);
cydaw::graph::GraphDefinition audioGraph(const Project &p);
class AudioEffects {
    struct Instance {
        std::string id;
        std::string state;
        std::unique_ptr<cydaw::engine::AudioProcessor> processor;
        std::unique_ptr<cydaw::engine::NativeDspNode> node; // reused for parameter updates (several KB)
        std::unique_ptr<IsolatedAudio> external;
        std::unique_ptr<cydaw::engine::AudioRenderPlan> graph;
        std::map<Id, cydaw::engine::AudioProcessor *> graphProcessors;
    };
    std::map<std::pair<Id, int>, Instance> instances;
    // Creates a native CyDAW device with its values at time (a convolution builds its response from them).
    void prepareNative(Instance &entry, const Effect &effect, const cydaw::engine::EffectDescriptor &d, double time);
    std::set<std::pair<Id, int>> used;
    cydaw::engine::AudioBuffer input{2, 1024}, output{2, 1024};
    double lastEnd = -1;

  public:
    void begin(double time) {
        if (lastEnd < 0 || std::abs(time - lastEnd) > 1. / 48000)
            instances.clear();
        used.clear();
    }
    void end(double time) {
        std::erase_if(instances, [&](const auto &entry) { return !used.contains(entry.first); });
        lastEnd = time;
    }
    void reset() {
        instances.clear();
        lastEnd = -1;
    }
    void process(const Clip &clip, double time, unsigned frames, std::vector<float> &interleaved);
    void process(Id owner, const std::vector<Effect> &chain, double time, unsigned frames,
                 std::vector<float> &interleaved);
    unsigned latency(Id owner, const std::vector<Effect> &chain);
};
} // namespace sf

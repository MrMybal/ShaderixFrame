// CyDAW's effect containers (chains, layers, selectors, splits, the replacer) build on its instrument
// stack, which ShaderixFrame does not ship. Their factory stands in here with a device that passes the
// sound through; CyDawAudio never offers containers, so this only keeps the effect library linkable.
#include <cydaw/engine/ContainerProcessors.h>

namespace cydaw::engine {
namespace {
class PassThrough final : public EffectProcessor {
  public:
    using EffectProcessor::EffectProcessor;
    void prepare(const ProcessSpec &) override {}
    void reset() noexcept override {}
    void process(AudioBlockView input, AudioBlockView output, const ProcessContext &) noexcept override {
        copyAudio(output, input);
    }
};
} // namespace

std::unique_ptr<EffectProcessor> makeContainerHead(const EffectDescriptor &descriptor) {
    return std::make_unique<PassThrough>(descriptor);
}
} // namespace cydaw::engine

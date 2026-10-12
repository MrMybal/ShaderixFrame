#pragma once

#include <cydaw/engine/AudioGraph.h>
#include <cydaw/engine/AudioProcessor.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cydaw::engine {

enum class RenderPlanError : std::uint8_t {
    none,
    invalidSpec,
    invalidNode,
    duplicateNode,
    missingNode,
    duplicateConnection,
    cycleDetected,
    missingOutput
};

class AudioRenderPlan;

struct RenderPlanResult {
    RenderPlanError error { RenderPlanError::none };
    std::string message;
    std::unique_ptr<AudioRenderPlan> plan;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == RenderPlanError::none && plan != nullptr;
    }
};

class AudioRenderPlan final {
public:
    struct RuntimeConnection {
        NodeId sourceNodeId {};
        std::size_t sourceNodeIndex {};
        std::uint64_t sourcePort {};
        std::uint64_t destinationPort {};
    };

    struct RuntimeNode {
        struct InputBus {
            std::uint64_t portId {};
            AudioBuffer buffer;
        };

        NodeId id {};
        std::unique_ptr<AudioProcessor> processor;
        MultiBusAudioProcessor* multiBusProcessor {};
        std::vector<RuntimeConnection> predecessors;
        std::vector<InputBus> inputBuses;
        std::vector<AudioInputBusView> inputBusViews;
        AudioBuffer inputBuffer;
        AudioBuffer outputBuffer;
        bool receivesExternalInput {};
        // Which external input: 0 the main one, then the extra ones.
        std::uint32_t externalBus {};
    };

    void process(AudioBlockView input, AudioBlockView output, const ProcessContext& context) noexcept;
    // With inputs beside the main one: `extraInputs[k]` feeds the input
    // nodes of bus k + 1 (empty views give silence).
    void process(AudioBlockView input, const AudioBlockView* extraInputs, std::size_t extraCount, AudioBlockView output,
                 const ProcessContext& context) noexcept;
    // Right after process(), on the same thread: what a node received
    // (its inputs summed) and what it produced in that block. False when the
    // plan has no such node.
    [[nodiscard]] bool lastSignals(NodeId node, std::uint32_t frameCount, AudioBlockView& input,
                                   AudioBlockView& output) noexcept;
    void reset() noexcept;
    [[nodiscard]] const ProcessSpec& spec() const noexcept;
    [[nodiscard]] std::size_t nodeCount() const noexcept;
    [[nodiscard]] std::size_t connectionCount() const noexcept;
    [[nodiscard]] std::size_t inputBusCount(NodeId node) const noexcept;
    [[nodiscard]] bool hasConnection(NodeId source, std::uint64_t sourcePort,
                                     NodeId destination,
                                     std::uint64_t destinationPort) const noexcept;

private:
    friend class AudioRenderPlanBuilder;
    ProcessSpec spec_;
    std::vector<RuntimeNode> nodes_;
    std::size_t outputNodeIndex_ {};
};

class AudioRenderPlanBuilder final {
public:
    [[nodiscard]] RenderPlanError addNode(NodeId id,
                                          std::unique_ptr<AudioProcessor> processor,
                                          bool receivesExternalInput = false,
                                          std::uint32_t externalBus = 0);
    [[nodiscard]] RenderPlanError connect(NodeId source, NodeId destination,
                                          std::uint64_t sourcePort = 0,
                                          std::uint64_t destinationPort = 0);
    [[nodiscard]] RenderPlanResult compile(ProcessSpec spec, NodeId outputNodeId);

private:
    struct BuildNode {
        std::unique_ptr<AudioProcessor> processor;
        bool receivesExternalInput {};
        std::uint32_t externalBus {};
    };
    struct BuildConnection {
        NodeId source {};
        NodeId destination {};
        std::uint64_t sourcePort {};
        std::uint64_t destinationPort {};
        [[nodiscard]] bool operator==(const BuildConnection&) const = default;
    };

    std::unordered_map<NodeId, BuildNode> nodes_;
    std::vector<BuildConnection> connections_;
};

} // namespace cydaw::engine

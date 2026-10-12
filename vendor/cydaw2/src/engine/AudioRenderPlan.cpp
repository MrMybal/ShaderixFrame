#include <cydaw/engine/AudioRenderPlan.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_set>

namespace cydaw::engine {

void AudioRenderPlan::process(const AudioBlockView input,
                              const AudioBlockView output,
                              const ProcessContext& context) noexcept
{
    process(input, nullptr, 0, output, context);
}

void AudioRenderPlan::process(const AudioBlockView input, const AudioBlockView* const extraInputs, const std::size_t extraCount,
                              const AudioBlockView output, const ProcessContext& context) noexcept
{
    const auto frames = std::min({ input.frameCount, output.frameCount, spec_.maximumBlockSize });
    clearAudio({ output.data, output.channelCount, frames, output.channelStride });

    for (auto& node : nodes_) {
        const auto nodeInput = node.inputBuffer.view(frames);
        const auto nodeOutput = node.outputBuffer.view(frames);
        clearAudio(nodeInput);
        for (auto& bus : node.inputBuses)
            bus.buffer.clear(frames);
        if (node.receivesExternalInput) {
            if (node.externalBus == 0) {
                copyAudio(nodeInput, { input.data, input.channelCount, frames, input.channelStride });
            } else if (node.externalBus - 1 < extraCount && extraInputs != nullptr) {
                const auto& extra = extraInputs[node.externalBus - 1];
                if (extra.data != nullptr && extra.frameCount >= frames)
                    copyAudio(nodeInput, { extra.data, extra.channelCount, frames, extra.channelStride });
            }
        }
        for (const auto& predecessor : node.predecessors) {
            const auto predecessorOutput =
                nodes_[predecessor.sourceNodeIndex].outputBuffer.view(frames);
            addAudio(nodeInput, predecessorOutput);
            const auto bus = std::find_if(
                node.inputBuses.begin(), node.inputBuses.end(),
                [&predecessor](const RuntimeNode::InputBus& value) {
                    return value.portId == predecessor.destinationPort;
                });
            if (bus != node.inputBuses.end())
                addAudio(bus->buffer.view(frames), predecessorOutput);
        }

        clearAudio(nodeOutput);
        if (node.multiBusProcessor != nullptr) {
            for (auto index = std::size_t {}; index < node.inputBuses.size();
                 ++index) {
                node.inputBusViews[index] = {
                    node.inputBuses[index].portId,
                    node.inputBuses[index].buffer.view(frames)
                };
            }
            node.multiBusProcessor->processAudioBuses(node.inputBusViews,
                                                       nodeOutput, context);
        } else {
            node.processor->process(nodeInput, nodeOutput, context);
        }
    }

    copyAudio({ output.data, output.channelCount, frames, output.channelStride },
              nodes_[outputNodeIndex_].outputBuffer.view(frames));
}

bool AudioRenderPlan::lastSignals(const NodeId node, const std::uint32_t frameCount, AudioBlockView& input,
                                  AudioBlockView& output) noexcept
{
    const auto frames = std::min(frameCount, spec_.maximumBlockSize);
    for (auto& candidate : nodes_)
        if (candidate.id == node) {
            input = candidate.inputBuffer.view(frames);
            output = candidate.outputBuffer.view(frames);
            return true;
        }
    return false;
}

void AudioRenderPlan::reset() noexcept
{
    for (auto& node : nodes_) {
        node.processor->reset();
        node.inputBuffer.clear(spec_.maximumBlockSize);
        node.outputBuffer.clear(spec_.maximumBlockSize);
    }
}

const ProcessSpec& AudioRenderPlan::spec() const noexcept { return spec_; }
std::size_t AudioRenderPlan::nodeCount() const noexcept { return nodes_.size(); }

std::size_t AudioRenderPlan::connectionCount() const noexcept
{
    auto count = std::size_t {};
    for (const auto& node : nodes_)
        count += node.predecessors.size();
    return count;
}

std::size_t AudioRenderPlan::inputBusCount(const NodeId node) const noexcept
{
    const auto found = std::find_if(
        nodes_.begin(), nodes_.end(), [node](const RuntimeNode& value) {
            return value.id == node;
        });
    return found == nodes_.end() ? 0 : found->inputBuses.size();
}

bool AudioRenderPlan::hasConnection(const NodeId source,
                                    const std::uint64_t sourcePort,
                                    const NodeId destination,
                                    const std::uint64_t destinationPort) const noexcept
{
    const auto node = std::find_if(
        nodes_.begin(), nodes_.end(), [destination](const RuntimeNode& value) {
            return value.id == destination;
        });
    if (node == nodes_.end())
        return false;
    return std::any_of(
        node->predecessors.begin(), node->predecessors.end(),
        [source, sourcePort, destinationPort](const RuntimeConnection& value) {
            return value.sourceNodeId == source
                   && value.sourcePort == sourcePort
                   && value.destinationPort == destinationPort;
        });
}

RenderPlanError AudioRenderPlanBuilder::addNode(const NodeId id,
                                                 std::unique_ptr<AudioProcessor> processor,
                                                 const bool receivesExternalInput,
                                                 const std::uint32_t externalBus)
{
    if (id == 0 || processor == nullptr)
        return RenderPlanError::invalidNode;
    if (nodes_.contains(id))
        return RenderPlanError::duplicateNode;
    nodes_.emplace(id, BuildNode { std::move(processor), receivesExternalInput, externalBus });
    return RenderPlanError::none;
}

RenderPlanError AudioRenderPlanBuilder::connect(
    const NodeId source, const NodeId destination,
    const std::uint64_t sourcePort, const std::uint64_t destinationPort)
{
    if (!nodes_.contains(source) || !nodes_.contains(destination))
        return RenderPlanError::missingNode;
    const BuildConnection connection { source, destination, sourcePort,
                                       destinationPort };
    if (std::find(connections_.begin(), connections_.end(), connection) != connections_.end())
        return RenderPlanError::duplicateConnection;
    connections_.push_back(connection);
    return RenderPlanError::none;
}

RenderPlanResult AudioRenderPlanBuilder::compile(const ProcessSpec spec, const NodeId outputNodeId)
{
    if (!std::isfinite(spec.sampleRate) || spec.sampleRate <= 0.0
        || spec.maximumBlockSize == 0 || spec.channelCount == 0)
        return { RenderPlanError::invalidSpec, "The audio process specification is invalid", nullptr };
    if (!nodes_.contains(outputNodeId))
        return { RenderPlanError::missingOutput, "The output node does not exist", nullptr };

    std::unordered_map<NodeId, std::size_t> incoming;
    std::unordered_map<NodeId, std::vector<NodeId>> outgoing;
    std::unordered_map<NodeId, std::vector<BuildConnection>> predecessors;
    for (const auto& [id, unused] : nodes_) {
        (void) unused;
        incoming.emplace(id, 0);
    }
    for (const auto& connection : connections_) {
        ++incoming[connection.destination];
        outgoing[connection.source].push_back(connection.destination);
        predecessors[connection.destination].push_back(connection);
    }

    std::priority_queue<NodeId, std::vector<NodeId>, std::greater<>> ready;
    for (const auto& [id, count] : incoming)
        if (count == 0) ready.push(id);

    std::vector<NodeId> processingOrder;
    while (!ready.empty()) {
        const auto id = ready.top();
        ready.pop();
        processingOrder.push_back(id);
        for (const auto destination : outgoing[id])
            if (--incoming[destination] == 0) ready.push(destination);
    }
    if (processingOrder.size() != nodes_.size())
        return { RenderPlanError::cycleDetected, "The render graph contains a cycle", nullptr };

    auto plan = std::make_unique<AudioRenderPlan>();
    plan->spec_ = spec;
    plan->nodes_.reserve(nodes_.size());
    std::unordered_map<NodeId, std::size_t> runtimeIndices;
    for (const auto id : processingOrder) {
        const auto index = plan->nodes_.size();
        runtimeIndices.emplace(id, index);
        auto& buildNode = nodes_.at(id);
        buildNode.processor->prepare(spec);
        AudioRenderPlan::RuntimeNode runtimeNode;
        runtimeNode.id = id;
        runtimeNode.processor = std::move(buildNode.processor);
        runtimeNode.multiBusProcessor =
            dynamic_cast<MultiBusAudioProcessor*>(runtimeNode.processor.get());
        runtimeNode.inputBuffer = AudioBuffer(spec.channelCount,
                                              spec.maximumBlockSize);
        runtimeNode.outputBuffer = AudioBuffer(spec.channelCount,
                                               spec.maximumBlockSize);
        runtimeNode.receivesExternalInput = buildNode.receivesExternalInput;
        runtimeNode.externalBus = buildNode.externalBus;
        plan->nodes_.push_back(std::move(runtimeNode));
    }

    for (auto& runtimeNode : plan->nodes_) {
        for (const auto& predecessor : predecessors[runtimeNode.id]) {
            runtimeNode.predecessors.push_back(
                { predecessor.source,
                  runtimeIndices.at(predecessor.source),
                  predecessor.sourcePort,
                  predecessor.destinationPort });
            const auto existingBus = std::find_if(
                runtimeNode.inputBuses.begin(), runtimeNode.inputBuses.end(),
                [&predecessor](
                    const AudioRenderPlan::RuntimeNode::InputBus& bus) {
                    return bus.portId == predecessor.destinationPort;
                });
            if (existingBus == runtimeNode.inputBuses.end())
                runtimeNode.inputBuses.push_back(
                    { predecessor.destinationPort,
                      AudioBuffer(spec.channelCount, spec.maximumBlockSize) });
        }
        runtimeNode.inputBusViews.resize(runtimeNode.inputBuses.size());
    }
    plan->outputNodeIndex_ = runtimeIndices.at(outputNodeId);
    return { RenderPlanError::none, {}, std::move(plan) };
}

} // namespace cydaw::engine

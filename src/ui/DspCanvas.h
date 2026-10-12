#pragma once
#include "adapters/DspGraph.h"
#include "core/CanvasView.h"
#include <imgui.h>
#include <optional>
#include <set>
namespace sf {
// Adaptation of CyDAW v2 GraphEditor/GraphLayout interactions; engine and project stay local.
class DspCanvas {
    Json graph;
    std::vector<Json> past, future;
    CanvasView view;
    std::set<Id> selection;
    Id dragging = 0, wire = 0;
    bool hasWire = false, wireOutput = true, box = false, fitPending = true;
    ImVec2 origin{}, mouseOrigin{}, boxOrigin{};
    std::map<Id, sx::float2> moveOrigins;
    std::optional<Json> beforeDrag;
    std::string search, error, automationKey;
    Id automationNode = 0, disconnectFrom = 0, disconnectTo = 0;
    sx::float2 newPosition;
    struct Card {
        Id id;
        sx::float2 world;
        ImVec2 min, max, input, output;
    };
    std::vector<Card> cards;
    void checkpoint();
    void nodeSearch();
    void select(Id node, bool extend);
    sx::float2 position(Id node) const;
    void setPosition(Id node, sx::float2 point);

  public:
    static constexpr Id outputBoundary = 0xffffffffu;
    void open(const Json &json);
    const Json &document() const { return graph; }
    Id add(const std::string &processor, sx::float2 position);
    void connect(Id from, Id to);
    void disconnect(Id from, Id to);
    void remove(const std::set<Id> &nodes);
    void arrange();
    bool undo();
    bool redo();
    bool draw(double time = 0); // draft changed; explicit Apply commits one project history entry
    const CanvasView &navigation() const { return view; }
    ImVec2 screenPoint(sx::float2 world) const {
        auto p = view.screen(world);
        return {origin.x + p.x, origin.y + p.y};
    }
};
} // namespace sf

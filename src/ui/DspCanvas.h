#pragma once
#include "adapters/DspGraph.h"
#include "core/CanvasView.h"
#include <imgui.h>
#include <cyui/modules/Nodes.h>
#include <cyui/modules/Curves.h>
#include <optional>
#include <set>
namespace sf {
// CyUI node interactions over the application-owned CyDAW-compatible DSP draft.
class DspCanvas {
    cyui::NodeState nodesState;
    cyui::CurveState curveState;
    std::uint64_t revision = 0;
    std::string stamp, curveIdentity;
    Id contextNode = 0;
    Json graph;
    std::vector<Json> past, future;
    CanvasView view;
    std::set<Id> selection;
    Id wire = 0;
    bool hasWire = false, wireOutput = true, fitPending = true;
    ImVec2 origin{};
    std::string search, error, automationKey;
    Id automationNode = 0;
    sx::float2 newPosition;
    void checkpoint();
    void nodeSearch();
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

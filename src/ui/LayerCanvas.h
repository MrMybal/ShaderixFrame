#pragma once
#include "adapters/LayerGraph.h"
#include "render/Renderer.h"
#include <imgui.h>
#include <cyui/modules/Nodes.h>
#include "ui/CodeFields.h"
#include <set>
namespace sf {
class LayerCanvas {
    cyui::NodeState nodesState;
    CodeFields codeFields;
    std::uint64_t revision = 0;
    std::string stamp;
    struct State {
        sx::float2 pan{12, 12};
        float zoom = .75f;
        std::set<Id> selection;
    } ed;
    Id selected = 0, shaderRequest = 0, lutRequest = 0;
    bool needsFit = true;
    Id contextNode = 0;
    Json clipboard;
    int lutLayout = 0;
    std::string automationKey = "opacity", shaderDraft, draftIdentity;

  public:
    // 0: the project's layer graph; otherwise the effect graph of that video track.
    Id track = 0;
    bool ownsKeyboard() const {return codeFields.ownsKeyboard();}
    float zoomLevel() const { return ed.zoom; }
    const char *lutLayoutName() const { return lutLayout == 1 ? "atlas" : lutLayout == 2 ? "rg" : "auto"; }
    Id takeLutRequest() {
        Id id = lutRequest;
        lutRequest = 0;
        return id;
    }
    Id takeShaderRequest() {
        Id id = shaderRequest;
        shaderRequest = 0;
        return id;
    }
    bool draw(Json &graph, const Project &project, Renderer &renderer, double time);
};
} // namespace sf

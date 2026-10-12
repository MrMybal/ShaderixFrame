#pragma once
#include "adapters/LayerGraph.h"
#include "render/Renderer.h"
#include <imgui.h>
#include <set>
namespace sf {
class LayerCanvas {
    struct State {
        sx::float2 pan{12, 12};
        float zoom = .75f;
        std::set<Id> selection;
        bool draggingLink = false;
        Id linkFrom = 0, linkTo = 0;
        int linkChannel = 0, linkInput = 0;
    } ed;
    Id selected = 0, dragging = 0, shaderRequest = 0, lutRequest = 0;
    bool needsFit = true;
    Id contextNode = 0;
    Json clipboard;
    int lutLayout = 0;
    std::string automationKey = "opacity", shaderDraft, draftIdentity;

  public:
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

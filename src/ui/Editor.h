#pragma once
#include "core/CanvasView.h"
#include "mcp/Protocol.h"
#include "render/Grading.h"
#include "ui/ClipPreviews.h"
#include "ui/DspCanvas.h"
#include "ui/LayerCanvas.h"
#include <imgui.h>
#include <optional>
namespace sf {
class Editor {
    struct PendingImport {
        std::string path;
        double time;
    };
    std::vector<PendingImport> pendingImports;
    std::map<std::string, Com<ID3D11ShaderResourceView>> thumbnails;
    ClipPreviews clipPreviews;
    DspCanvas dspCanvas;
    LayerCanvas layerCanvas;
    Id layerShaderNode = 0;
    void layerGraphEditor();
    std::string panelSuffix;
    std::string pendingPanelFocus;
    std::string panelTitle(const char *name) const { return std::string(name) + panelSuffix; }
    Id editedContainer = 0;
    bool clipDetached = false, clipDetachRequested = false;
    std::unique_ptr<Session> clipSession;
    std::unique_ptr<Editor> clipWorkspace;
    void clipEditor();
    void drawEmbedded(ImGuiID dock, ImVec2 size);
    void openClipEditor(Id clip, bool discard = false);
    void handleShortcuts();
    Editor *keyboardOwner();
    void dspPanel();
    void motionPanel();
    void shaderixelLibrary();
    void addLibraryLayer(const std::string &id);
    std::string shaderixelFilter, shaderixelCategory;
    int shaderixelMode = 0;
    Id motionDragging = 0;
    ImVec2 motionDragOrigin;
    float motionStartX = 0, motionStartY = 0, motionStartWidth = 0, motionStartHeight = 0;
    bool motionResizing = false;
    std::optional<Project> motionBefore;
    CanvasView motionView;
    DspCanvas channelDsp;
    std::string channelDspIdentity;
    bool channelDspDirty = false;
    int channelDspIndex = -1;

    bool openDspNext = false;
    int gradeComparison = 0, gradePick = 0, gradeCurveIndex = 0, gradeScopeIndex = 0;
    bool gradeMatte = false, gradeScopesVisible = false;
    float gradeSplit = .5f;
    double gradeScopesNext = 0;
    VideoScopes gradeScopes;
    std::unique_ptr<Renderer> gradeBefore;
    Com<ID3D11ShaderResourceView> gradeReference;
    std::string pendingGradeReference;
    int gradeCurvePoint = -1;
    Id gradeGestureClip = 0;
    int gradeGestureEffect = -1;
    void gradingPanel();
    void scopesPanel();
    void gradePreview(ImVec2 origin, float width, float height);
    void gradeComparisonControls();
    void captureGradeReference(const Image &image);
    bool dspGraphEditor(Json target, const std::vector<Effect> &chain);
    bool pluginControls(Json target, const Effect &effect);
    Session &s;
    Protocol protocol;
    AudioOutput audio;
    std::filesystem::path root;
    float zoom = 55;
    double scroll = 0;
    bool snap = true, loop = false, layoutReady = false, exportsFocus = false;
    bool recoveryScanned = false, recoveryVisible = false;
    double nextRecovery = 0;
    std::vector<RecoveryEntry> recoveries;
    std::string error, shaderDraft, draftIdentity, filter, animationKey = "opacity";
    int selectedEffect = 0, animationEffect = -1;
    int timelineKeyDrag = -1, trackingRadius = 24, exportPreset = 0, exportFormat = 0, dspModalIndex = -1,
        gradeLutLayout = 0;
    double trackingBegin = 0, trackingEnd = 1, rangeBegin = 0, rangeEnd = 1;
    float trimDelta = -.5f, transitionOverlap = .5f;
    bool leftTrim = false;
    std::string dspModalDraft;
    std::map<std::string, int> dspModalChoices;
    std::string mixScope = "master", mixAnimationKey = "gain";
    Id mixTarget = 0;
    int mixAnimationEffect = -1;
    Id graphNode = 0, wireNode = 0;
    int wireOutput = 0;
    CanvasView graphView, compositionView;
    double navigateAnchor = 0;
    std::map<std::string, bool> panels{{"Médias & effets", true},
                                       {"Composition", true},
                                       {"Calques", true},
                                       {"Propriétés", true},
                                       {"Timeline", true},
                                       {"Animations", true},
                                       {"HLSL", true},
                                       {"Graphe VFX", true},
                                       {"Graphe des calques", true},
                                       {"Étalonnage", true},
                                       {"Scopes vidéo", true},
                                       {"Mixeur & graphe audio", true},
                                       {"Plugins & MCP", true},
                                       {"Exports", true},
                                       {"Graphe DSP", false},
                                       {"Éditeur de clip", false},
                                       {"Motion design", false},
                                       {"Bibliothèque Shaderixel", true}};
    bool beginPanel(const char *name);
    bool panelSettingsRead = false;
    std::string panelSettingsSignature;
    void panelSettings();
    std::string graphViewIdentity;
    Id graphDragging = 0;
    bool graphPanning = false, graphFitPending = false;
    std::optional<Project> graphDragBefore;
    Id timelineDragging = 0;
    int timelineTrim = 0;
    float timelineDragX = 0;
    double timelineStart = 0, timelineLength = 0, timelineIn = 0;
    double timelineMoved = 0, timelineMinimum = 0;
    double timelineSequenceLength = 0;
    std::vector<Id> timelineDragClips;
    std::optional<Project> timelineDragBefore;
    bool marquee = false, marqueeAdd = false;
    ImVec2 marqueeAnchor;
    std::vector<Id> marqueeBase;
    double wallTime = 0, lastTransport = 0;
    bool wasPlaying = false;
    bool maskGuides = true;
    Id maskDragging = 0;
    ImVec2 maskDragMouse;
    float maskDragX = 0, maskDragY = 0;
    double maskDragTime = 0;
    void menu();
    void library();
    void preview();
    void layers();
    void inspector();
    void timeline();
    void animations();
    void shaderEditor();
    void graphEditor();
    void mixer();
    void mixDetails();
    void integrations();
    void recoveryPanel();
    void exportPanel();
    bool action(const std::string &name, Json args = Json::Object());
    std::string dialog(bool save, const wchar_t *filter, const wchar_t *extension);
    void parameter(const char *label, const std::string &key, float minimum, float maximum, float fallback,
                   int effect = -1);
    bool effectControls(Id clip, int index);
    bool transitionControls(Id clip);
    Json selectionArgs() const;
    void editCheckpoint(const Project &before);

  public:
    Editor(Session &session, std::filesystem::path base) : s(session), protocol(s), root(std::move(base)) {}
    void draw();
    void showDspGraph() {
        openPanel("Graphe DSP");
        panels["Mixeur & graphe audio"] = true;
        mixScope = "master";
        mixTarget = 0;
        for (auto &track : s.project.tracks)
            if (track.kind == TrackKind::Audio &&
                std::any_of(track.mix.effects.begin(), track.mix.effects.end(),
                            [](const Effect &e) { return e.id == "cydaw.graph"; })) {
                mixScope = "track";
                mixTarget = track.id;
                break;
            }
        ImGui::SetWindowFocus(panelTitle("Graphe DSP").c_str());
    }
    void showClipEditor() {
        if (s.selected)
            openClipEditor(s.selected);
    }
    void showGrading() {
        selectedEffect = -1;
        if (auto *clip = s.project.findClip(s.selected))
            for (int i = 0; i < int(clip->effects.size()); ++i)
                if (clip->effects[i].id == "grading") {
                    selectedEffect = i;
                    break;
                }
    }
    void drop(const std::string &path);
    void openPanel(const std::string &name) {
        panels[name] = true;
        pendingPanelFocus = panelTitle(name.c_str());
    }
    bool hasClipDrafts() const {
        return clipWorkspace && (clipSession->dirty || clipWorkspace->hasClipDrafts());
    }
    Project projectWithClipDrafts() const;
    bool applyClipDrafts();
    const CanvasView &previewNavigation() const { return compositionView; }
    float timelinePixelsPerSecond() const { return zoom; }
    double timelineFirstVisibleTime() const { return scroll; }
    float layerGraphZoom() const { return layerCanvas.zoomLevel(); }
    bool hasError() const { return !error.empty(); }
};
void applyFrameTheme();
} // namespace sf

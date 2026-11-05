#pragma once
#include "core/CanvasView.h"
#include "mcp/Protocol.h"
#include "render/Grading.h"
#include "ui/ClipPreviews.h"
#include "ui/DspCanvas.h"
#include "ui/LayerCanvas.h"
#include "ui/RenderCache.h"
#include "ui/Keymap.h"
#include "ui/Help.h"
#include "ui/CodeFields.h"
#include <cyui/modules/Commands.h>
#include <cyui/modules/Nodes.h>
#include <cyui/modules/AudioViews.h>
#include <cyui/modules/Inspector.h>
#include <cyui/modules/Timeline.h>
#include <cyui/modules/Images.h>
#include <imgui.h>
#include <array>
#include <set>
#include <optional>
namespace sf {
class Editor {
  public:
    // Workspaces, as CyDAW's application modes.
    enum class Mode { Edit, Effects, Motion, Color, Audio, Deliver };
    static constexpr int modeCount = 6;

  private:
    HelpDocumentation help;
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
    Id layerShaderTrack = 0; // with layerShaderNode: a node of that track's effect graph (0: the layer graph)
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
    cyui::CanvasState motionCanvas;
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
    cyui::CurveState gradeCurveState;
    std::string gradeCurveIdentity, gradeCurveStamp;
    std::uint64_t gradeCurveRevision=0;
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
    CodeFields codeFields;
    cyui::CommandState paletteState;
    cyui::NodeState shaderNodes, routingNodes;
    std::string routingStamp;
    cyui::PlotState signalWave{{{0,1},false},{{-1,1},false}}, signalSpectrum{{{20,20000},true},{{0,1},false}};
    cyui::InspectorState sequenceInspector;
    cyui::TimelineState sourceTimeline;
    cyui::ImageState sourceImage;
    std::string sourceViewIdentity, sequenceStamp;
    std::uint64_t sequenceRevision=0;
    std::uint64_t shaderRevision=0;
    std::string shaderGraphStamp;
    Session &s;
    Protocol protocol;
    AudioOutput audio;
    std::filesystem::path root;
    float zoom = 55;
    double scroll = 0;
    bool snap = true, loop = false, layoutReady = false, exportsFocus = false, exportWorkArea = true;
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
    float compositionDetail = 0, pendingPixelZoom = 0, compositionPixelLock = 0;
    std::string audioWarning; // last sound device problem shown, so it is reported once
    bool audioDeviceRead = false;
    void soundOutputMenu(); // the playback device, kept in audio-output.json
    // Track Effects panel (ui/AudioChain.cpp): the insert chain of mixScope/mixTarget as devices.
    void trackEffectsPanel();
    // Track Graph panel (ui/TrackGraph.cpp): the effect graph of graphTrack.
    void trackGraphEditor();
    LayerCanvas trackCanvas;
    Id graphTrack = 0, graphFollowed = 0;
    Id trackOpacityFor = 0;        // the track whose opacity the context menu slider shows
    // Effects Browser (ui/EffectsBrowser.cpp): cards with previews rendered by their own worker.
    void effectsBrowser();
    std::shared_ptr<const Project> previewDocument(const EffectDefinition &d, int kind, const Clip *reference);
    ClipPreviews effectPreviews;
    std::map<std::string, std::shared_ptr<const Project>> browserDocuments;
    std::string browserSearch, browserCategory, browserReferenceKey;
    int browserKind = 0;
    float browserCard = 120;
    bool browserOnClip = false;
    float trackOpacityDraft = 100;
    std::string trackEffectSearch;
    Id chainFollowed = 0;      // the selection last followed by the chain
    Parameter chainEditStart;  // a knob's parameter when its gesture began, for one undo step
    std::string chainSearch;
    void loadSoundOutput();
    bool compositionHand = false; // Hand tool of the final image view: a left drag moves the view
    bool compositionPanning = false, compositionMapDrag = false, compositionHovered = false, spaceOverView = false,
         spacePanned = false;
    double navigateAnchor = 0;
    std::map<std::string, bool> panels{{"Media & Effects", true},
                                       {"Composition", true},
                                       {"Layers", true},
                                       {"Properties", true},
                                       {"Timeline", true},
                                       {"Animation", true},
                                       {"HLSL", true},
                                       {"Shader Graph", true},
                                       {"Layer Graph", true},
                                       {"Color Grading", true},
                                       {"Video Scopes", true},
                                       {"Mixer & Audio Graph", true},
                                       {"Plugins & MCP", true},
                                       {"Exports", true},
                                       {"DSP Graph", false},
                                       {"Clip Editor", false},
                                       {"Motion Design", false},
                                       {"Shaderixel Library", true},
                                       {"Source", false},
                                       {"Markers", false},
                                       {"History", false},
                                       {"Subtitles", false},
                                       {"Camera Angles", false},
                                       {"Keyboard Shortcuts", false},
                                       {"Track Effects", false},
                                       {"Track Graph", false},
                                       {"Effects Browser", true}};
    // Sequence panels (ui/SequencePanels.cpp).
    void markersPanel();
    void historyPanel();
    void subtitlesPanel();
    void anglesPanel();
    Id subtitleTrack = 0;
    std::string subtitleFilter;
    // Skimming over a bin card: a private reader decodes the frame under the pointer.
    std::unique_ptr<MediaAsset> skimAsset;
    std::string skimPath;
    Com<ID3D11ShaderResourceView> skimView;
    double skimTime = -1, skimDecoded = 0;
    ID3D11ShaderResourceView *skimFrame(const std::string &path, double fraction);
    float subtitleStyle[4]{40, .38f, 2, 0};
    bool subtitleStyleEditing = false;
    // Windows' folder picker; empty when cancelled.
    std::string folderDialog(const wchar_t *title);
    // Confirmation shown in the status bar for a few seconds.
    std::string notice;
    double noticeUntil = 0;
    // Loudness measured on a worker thread; the result is applied as one undoable change.
    struct LoudnessJob {
        std::thread worker;
        std::atomic<float> progress = 0;
        std::atomic<bool> finished = false, cancelled = false;
        double measured = 0, peak = 0, target = -14;
        bool limit = true;
        std::string error;
        ~LoudnessJob() {
            cancelled = true;
            if (worker.joinable())
                worker.join();
        }
    };
    std::unique_ptr<LoudnessJob> loudnessJob;
    // An analysis of media (silences, shots) on a worker thread; its result becomes the arguments of one
    // undoable command applied when it finishes.
    struct AnalysisJob {
        std::thread worker;
        std::atomic<float> progress = -1; // negative: no measure of progress
        std::atomic<bool> finished = false, cancelled = false;
        std::string label, command, error;
        Json args;
        std::function<std::string(const Json &)> report;
        ~AnalysisJob() {
            cancelled = true;
            if (worker.joinable())
                worker.join();
        }
    };
    std::unique_ptr<AnalysisJob> analysis;
    void startAnalysis(std::string label, std::string command, std::function<Json(AnalysisJob &)> work,
                       std::function<std::string(const Json &)> report);
    void pollAnalysis();
    void removeSilencesInBackground(Id clip, double threshold, double minLength);
    void exportGifInBackground(const std::string &path);
    void detectScenesInBackground(Id clip, bool split);
    bool loudnessLimit = true;
    void startLoudness(double target);
    void pollLoudness();
    void say(std::string text) {
        notice = std::move(text);
        noticeUntil = ImGui::GetTime() + 6;
    }
    int historySeen = -1;
    std::string markerFilter;
    bool markerPictures = true;
    std::optional<Project> markerEditBefore;
    Mode mode = Mode::Edit;
    bool modesReady = false, layoutForce = false;
    std::array<std::map<std::string, bool>, modeCount> modePanels;
    void modeBar();
    void buildLayout(ImGuiID dock, ImVec2 size);
    bool layoutRestore = false;
    void saveModeLayout();
    bool restoreModeLayout(ImGuiID dock, ImVec2 size);
    void resetModeLayout();
    std::set<std::string> detachedPanels; // open in their own native window
    void trackDetachedPanels();
    bool beginPanel(const char *name);
    bool panelSettingsRead = false;
    std::filesystem::path panelSettingsFile;
    std::string panelSettingsSignature;
    void panelSettings();
    std::string graphViewIdentity;
    Id graphDragging = 0;
    bool graphPanning = false, graphFitPending = false, graphAutoFit = false;
    ImVec2 graphFitSize;
    std::optional<Project> graphDragBefore;
    // Timeline: CyDAW ArrangementView/Cinema model adapted to seconds and frames (ui/Timeline.cpp).
    enum class Tool { Select, Blade, Slip, Hand, Roll, Stretch, Slide };
    enum class Gesture {
        None, Seek, WorkDraw, WorkIn, WorkOut, WorkMove, Marker, Navigator, TrackHeight, TrackMove,
        HeaderWidth, Fade, Key, Pan, HScroll, VScroll, Slip, Ripple, Roll, Gain, Stretch, Slide, Transition,
        RegionEnd
    };
    struct AutomationLane {
        bool visible = false;
        std::string key = "opacity", effect; // effect id ; empty for clip parameters
    };
    struct TimelineGeometry {
        ImVec2 navigator, lanes;   // top-left of the navigator band and of the first lane
        float ruler = 0;           // top of the timecode ruler
        std::vector<float> tops, rows; // lane tops (screen space) and clip row heights
    };
    Tool tool = Tool::Select;
    int gridIndex = 0;
    bool snapEdges = true, follow = true, followPaused = false, rippleEdits = false, timelinePlaying = false,
         showFilmstrip = true, showClipThumbnails = true, showWaveforms = true, showKeyframes = true;
    float verticalScroll = 0, laneZoom = 1, headerWidth = 200;
    std::map<Id, AutomationLane> lanes;
    std::map<std::string, std::pair<float, float>> parameterRanges;
    bool automationReveal = false;
    Gesture gesture = Gesture::None;
    Id gestureId = 0, gestureOther = 0;
    int gestureIndex = -1;
    double gestureTime = 0, gestureValue = 0;
    float gestureMouse = 0;
    std::optional<Project> gestureBefore;
    Id renamingTrack = 0, renamingMarker = 0;
    std::string renameDraft;
    bool renameFocus = false;
    Id contextTrack = 0, contextMarker = 0, contextGroup = 0, renamingGroup = 0;
    double contextTime = 0;
    TimelineGeometry geometry;
    // Animations panel curve editor (ui/CurveEditor.cpp).
    struct CurveKey {
        int effect;
        std::string key;
        int index;
    };
    struct CurvePoint {
        int effect;
        std::string key;
        int index;
        ImVec2 at;
    };
    std::vector<CurveKey> curveSelection;
    std::vector<Keyframe> curveOriginal;
    std::vector<CurvePoint> curvePoints;
    std::set<std::string> curveVisible;
    double curveStart = 0, curveEnd = 1;
    Id curveClip = 0;
    bool curveTable = false, curveDelete = false, curveCopy = false, curvePaste = false;
    // Copied keys, their times relative to the first one: pasted at the playhead, on any clip.
    struct CopiedKey {
        int effect;
        std::string key;
        Keyframe frame;
    };
    std::vector<CopiedKey> curveClipboard;
    int curveInterpolation = -1, curveGesture = 0, curveBend = -1;
    float curveBendShape = 0;
    ImVec2 curveAnchor;
    std::optional<Project> curveBefore;
    std::map<std::string, Parameter> *curveParams(Clip &clip, int effect);
    std::pair<float, float> curveRange(const std::string &key, const Parameter &param) const;
    void timelineToolbar();
    double gridStep() const;
    double snapToGrid(double seconds, bool force = false) const;
    std::vector<double> snapPoints(const std::vector<Id> &ignore) const;
    float trackHeight(const Track &track) const;
    void blade(Id clip, double time);
    std::vector<std::pair<Id, Clip>> clipboard; // owning track, clip
    std::string effectSearch;
    // Media bin and source monitor (ui/SourceMonitor.cpp).
    std::string sourcePath, binFilter;
    double sourceTime = 0, sourceIn = 0, sourceOut = 0, sourceFrameTime = -1;
    bool sourceVideo = true, sourceAudio = true, sourcePlaying = false;
    float sourceSpeed = 1;
    int sourceMarkDrag = 0; // 1: in point, 2: out point, while dragged on the scrub bar
    ImVec2 sourceBarMin, sourceBarSize; // scrub bar of the last frame
    AudioOutput sourceSound;   // the source monitor's own sound, at normal speed
    Project sourceSoundProject; // one audio clip of the opened medium
    std::string sourceSoundPath;
    void matchFrame();
    Id targetVideo = 0, targetAudio = 0; // destination tracks of source edits, 0: first free track
    std::unique_ptr<MediaAsset> sourceAsset;
    Com<ID3D11Texture2D> sourceTexture;
    Com<ID3D11ShaderResourceView> sourceFrame;
    unsigned sourceFrameWidth = 0, sourceFrameHeight = 0;
    void sourcePanel();
    void mediaBin();
    void sourceEdit(bool insert, bool fit = false);
    ID3D11ShaderResourceView *mediaThumbnail(const std::string &path);
    static void centredLabel(ImDrawList *list, ImVec2 min, ImVec2 max, const char *label);
    Id effectClip = 0;
    void effectMenu(Id clip);
    void copyClips(bool cut);
    void pasteClips(double at);
    void toggleClipsEnabled();
    void trimToPlayhead(bool head, bool ripple);
    void insertGapMenu(double at);
    void crossfadeAtPlayhead();
    void selectFrom(double time, bool forward, Id onlyTrack = 0);
    void freezeAtPlayhead(double length);
    void exportStill();
    Id speedClip = 0;
    float speedDraft = 100;
    double durationDraft = 1;
    bool openSpeed = false, openRename = false, openAttributes = false, openLowerThird = false;
    bool autoProxies = true;    // heavy videos get a preview proxy when imported
    // Editable shortcuts, kept beside the panel settings (keymap.json).
    Keymap keymap;
    bool keymapRead = false;
    std::string keymapCapture; // action waiting for its new key in the Raccourcis panel
    void saveKeymap();
    void shortcutsPanel();
    // Command palette (ui/CommandPalette.cpp): every editor command by name, filtered as one types.
    struct Command {
        std::string label, shortcut;
        std::function<void()> run;
        bool enabled = true;
    };
    bool paletteOpen = false, paletteFocus = false;
    std::string paletteQuery;
    int paletteIndex = 0;
    std::vector<Command> commands();
    void commandPalette();
    void saveCommand();
    void openCommand();
    void importCommand();
    void workAreaEdge(bool in);
    // Projects opened or saved lately, newest first, kept beside the panel settings.
    std::vector<std::string> recentProjects;
    bool recentRead = false;
    void rememberProject(const std::string &path);
    void loadRecentProjects();
    // A welcome card over an empty sequence: templates, open, recent projects, import.
    void welcomeCard(ImVec2 origin, ImVec2 size);
    bool welcomeShown = false;
    bool angleSound = true;     // multicam: each angle's linked sound follows its picture
    bool clipMenuRequest = false; // opens the clip menu on the selection (captures)
    std::string lowerName, lowerRole;
    double lowerStart = 0;
    AttributeChoice attributeChoice;
    // Pastes the attributes of the first copied clip onto the selection, one undo step.
    void pasteAttributes();
    std::string renamePattern = "Shot ##";
    int renameFirst = 1;
    // Ctrl+F: clips found by name, track or media file; the chosen one is selected and revealed.
    bool openFind = false, findFocus = false;
    std::string findText;
    double pendingReveal = -1;
    ImVec2 speedButtons[2]{}; // centres of the 200 % preset and of « Appliquer », for the tests
    void revealTime(double time);
    void fitTimeline(float width);
    void fitSelection(float width);
    bool headerWheelHovered = false; // a header widget used the wheel last frame
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
    // Pre-render of the work area (Enter in the timeline): shown during playback while the document matches.
    RenderCache renderCache;
    double renderCacheChecked = 0;
    std::size_t renderCacheHistory = 0;
    ID3D11ShaderResourceView *cachedView = nullptr;
    std::string renderSignature() const;
    float previewScale() const;
    ID3D11ShaderResourceView *shownView() const { return cachedView ? cachedView : s.renderer.view(); }
    // Shuttle (J / K / Shift+L): playback rate, picture only away from ×1; anchored at a time and a clock.
    double shuttleRate = 1, anchoredRate = 1, shuttleOrigin = 0, shuttleWall = 0;
    void shuttleStep(int direction);
    bool wasPlaying = false;
    bool maskGuides = true;
    // Composition overlays: framing guides (1 safe areas, 2 thirds, 4 centre) and a target aspect ratio.
    int guideFlags = 0, guideRatio = 0;
    // A/B comparison: a snapshot of the view on the left of a draggable wipe, the live picture on the right.
    Com<ID3D11ShaderResourceView> snapshotView;
    std::string snapshotLabel;
    bool snapshotCompare = false, wipeDragging = false;
    float wipe = .5f;
    void compositionOverlays(ImVec2 origin, float w, float h, bool hovered);
    void guidesMenu();
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
    Json actionResult; // result of the last successful action
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
    void showManual(const std::string &topic = {}, const std::string &language = {},
                    const std::string &anchor = {}, HelpDocumentation::State state = {}) {
        if (!help.open(root, topic, language.empty() ? cyui::locale() : language, anchor, std::move(state)))
            error = help.error();
    }
    HelpDocumentation &documentation() { return help; }
    void setHelpOpener(const std::function<bool(const std::string &)> &launch) {
        help.setOpener(launch);
        if (clipWorkspace) clipWorkspace->setHelpOpener(launch);
    }
    std::string helpUrl(const std::string &topic = "welcome", const std::string &language = "en",
                        const std::string &anchor = {}, HelpDocumentation::State state = {}) {
        return help.url(root, topic, language, anchor, std::move(state));
    }
    void showDspGraph() {
        openPanel("DSP Graph");
        panels["Mixer & Audio Graph"] = true;
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
        ImGui::SetWindowFocus(panelTitle("DSP Graph").c_str());
    }
    void openSource(const std::string &path);
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
    void openTrackGraph(Id track);
    Id trackGraphShown() const { return graphTrack; }
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
    void setTimelineView(float pixelsPerSecond, double firstVisible) {
        zoom = std::clamp(pixelsPerSecond, 2.f, 20000.f);
        scroll = std::max(0., firstVisible);
    }
    void setCompositionPixelZoom(float percent) { pendingPixelZoom = percent; }
    // Screen point inside the clip row of a track at a sequence time, from the last drawn frame.
    ImVec2 timelinePoint(double seconds, int track) const {
        bool known = track >= 0 && track < int(geometry.tops.size());
        float top = known ? geometry.tops[track] : geometry.lanes.y, row = known ? geometry.rows[track] : 60;
        return {geometry.lanes.x + float((seconds - scroll) * zoom), top + row * .6f};
    }
    float timelineHeaderLeft() const { return geometry.lanes.x - headerWidth; }
    // Screen point on the ruler at a sequence time, dy pixels below its top.
    ImVec2 rulerPoint(double seconds, float dy) const {
        return {geometry.lanes.x + float((seconds - scroll) * zoom), geometry.ruler + dy};
    }
    const std::string &sourceMedia() const { return sourcePath; }
    std::pair<double, double> sourceMarks() const { return {sourceIn, sourceOut}; }
    std::pair<ImVec2, ImVec2> sourceScrubBar() const { return {sourceBarMin, sourceBarSize}; }
    ImVec2 speedButton(int which) const { return speedButtons[which]; }
    float timelineTrackTop(int track) const {
        return track >= 0 && track < int(geometry.tops.size()) ? geometry.tops[track] : geometry.lanes.y;
    }
    // Screen position of a key drawn by the Animations curve editor in the last frame.
    ImVec2 curvePoint(int effect, const std::string &key, int index) const {
        for (auto &point : curvePoints)
            if (point.effect == effect && point.key == key && point.index == index)
                return point.at;
        return {-1, -1};
    }
    ImVec2 timelineNavigatorPoint(double seconds) const {
        return {geometry.lanes.x + float((seconds - scroll) * zoom), geometry.navigator.y + 6};
    }
    void setMode(Mode next);
    int storedModeLayouts() const;
    void setPanelSettingsFile(std::filesystem::path path) { panelSettingsFile = std::move(path); }
    Mode currentMode() const { return mode; }
    double timelineFirstVisibleTime() const { return scroll; }
    float layerGraphZoom() const { return layerCanvas.zoomLevel(); }
    bool hasError() const { return !error.empty(); }
    const std::string &errorMessage() const { return error; }
    // The frame under the playhead, at the sequence's size, on the Windows clipboard.
    bool copyStill();
    // Keeps the current view as the A side of the comparison, which turns on.
    bool takeSnapshot();
    bool comparing() const { return snapshotCompare && snapshotView; }
    void setWipe(float position) { wipe = std::clamp(position, 0.f, 1.f); }
    // Selects the n-th clip, tracks from the top then by start (captures).
    void selectClipAt(int index) {
        int n = 0;
        for (auto &track : s.project.tracks)
            for (auto &clip : track.clips)
                if (n++ == index)
                    s.selectClip(clip.id);
    }
    void setGuides(int flags, int ratio) {
        guideFlags = flags;
        guideRatio = ratio;
    }
    // History panel row: 0 is the oldest state kept, undoSteps().size() the current one.
    void goToHistoryStep(int target);
    double playbackRate() const { return s.playing ? shuttleRate : 0; }
    void measureLoudness(double target) { startLoudness(target); }
    bool measuringLoudness() const { return loudnessJob != nullptr; }
    void removeSilences(Id clip, double threshold, double minLength) { removeSilencesInBackground(clip, threshold, minLength); }
    void detectScenes(Id clip, bool split) { detectScenesInBackground(clip, split); }
    bool analysing() const { return analysis != nullptr; }
    const std::vector<std::string> &recentList() const { return recentProjects; }
    void remember(const std::string &path) {
        recentRead = true;
        rememberProject(path);
    }
    bool welcomeVisible() const { return welcomeShown; }
    const CanvasView& shaderNavigation() const {return graphView;}
    Keymap &shortcuts() { return keymap; }
    void captureShortcut(const std::string &id) { keymapCapture = id; }
    void openPalette(const std::string &query = {}) {
        paletteOpen = paletteFocus = true;
        paletteQuery = query;
        paletteIndex = 0;
    }
    bool paletteVisible() const { return paletteOpen; }
    std::vector<std::string> paletteMatches(const std::string &query);
    bool looping() const { return loop; }
    std::pair<std::string, Id> chainChannel() const { return {mixScope, mixTarget}; }
    bool runAction(const std::string &name, Json args = Json::Object()) { return action(name, std::move(args)); }
    void setBrowserKind(int kind) { browserKind = std::clamp(kind, 0, 4); }
    void openClipMenuAt(int index) {
        selectClipAt(index);
        clipMenuRequest = s.selected != 0;
    }
    void openLowerThirdDialog(double at) {
        lowerStart = at;
        openLowerThird = true;
    }
    void exportGif(const std::string &path) { exportGifInBackground(path); }
    const std::string &lastNotice() const { return notice; }
    void pasteCopiedAttributes() { pasteAttributes(); }
    void setAttributeChoice(const AttributeChoice &choice) { attributeChoice = choice; }
    // The frame skimmed at a fraction of a medium, and its time (test hooks).
    bool skim(const std::string &path, double fraction) { return skimFrame(path, fraction) != nullptr; }
    double skimmedTime() const { return skimTime; }
    // Shift+Space: plays two seconds around the playhead, then returns to it.
    void playAround();
    double playAroundReturn = 0, playAroundEnd = -1;
    // Media files missing from disk, checked every few seconds.
    int offlineMedia = 0;
    double offlineChecked = 0;
    void relinkDialog();
    int offlineCount() const { return offlineMedia; }
    // Pre-renders the work area (the whole sequence without one) into memory.
    void startRenderCache();
    const RenderCache &renderCacheState() const { return renderCache; }
    bool showingCachedFrame() const { return cachedView != nullptr; }
};
void applyFrameTheme();
// CF_DIB bytes of an RGBA image: header, then bottom-up BGRA rows.
std::vector<unsigned char> clipboardDib(const Image &image);
} // namespace sf

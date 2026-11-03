#pragma once
#include "core/Json.h"
#include <map>
#include <functional>
#include <stdexcept>

namespace sf {
using Json = sx::Json;
using Id = sx::u32;
enum class TrackKind { Video, Audio };
enum class Curve { Linear, Smooth, Hold, Shaped };
struct Keyframe {
    double time = 0;
    float value = 0;
    Curve curve = Curve::Linear;
    float shape = 0;
};
struct Parameter {
    float value = 0;
    std::vector<Keyframe> keys;
    float at(double time) const;
    void setKey(Keyframe key);
};
struct Effect {
    std::string id = "color";
    bool enabled = true;
    std::string code; // embedded source keeps the project independent of plugin updates
    Json graph;
    Json grading; // native grading schema, independent of Shaderixel's graph format
    std::map<std::string, Parameter> params;
    std::string pluginState; // opaque VST3 state, base64, 4 MiB decoded maximum
    std::map<std::string, std::string> parameterNames;
};
struct Title {
    bool enabled = false;
    std::string text = "Your title", font = "Segoe UI";
    int alignment = 1; // left, center, right
    bool bold = true, italic = false;
};
enum class MaskShape { None, Rectangle, Ellipse, Bezier };
struct BezierPoint {
    float x = 0, y = 0, inX = 0, inY = 0, outX = 0, outY = 0;
};
struct Mask {
    MaskShape shape = MaskShape::None;
    bool inverted = false;
    std::vector<BezierPoint> points;
};
struct Transition {
    bool enabled = false;
    std::string id = "transition.crossfade", code;
    double duration = 1;
};
struct AudioSend {
    Id bus = 0;
    float gain = -12;
    bool preFader = false, enabled = true;
};
struct MixChannel {
    Id output = 0; // 0 = master; other IDs must refer to audio buses
    std::vector<AudioSend> sends;
    std::vector<Effect> effects;
    std::map<std::string, Parameter> params;
    float param(const std::string &key, double time, float fallback) const;
};
struct AudioBus {
    Id id = 0;
    std::string name = "Bus";
    bool muted = false, locked = false;
    float gain = 0, pan = 0;
    MixChannel mix;
};
struct Clip {
    Id id = 0;
    std::string name = "Clip", source;
    double start = 0, duration = 6, in = 0, speed = 1;
    Id link = 0;
    double fadeIn = 0, fadeOut = 0;
    bool generator = false;
    sx::u32 color = 0; // 0xRRGGBB, 0 : couleur de la piste ou du type de clip
    std::string sourceColor = "auto";
    bool sourcePremultiplied = false;
    Json composition; // embedded Project; its timeline, audio and layer graph use local IDs/time.
    Json widget; // motion layout: parent, anchor, pivot, type; dimensions/offsets are animated parameters.
    Title title;
    Mask mask;
    Transition transition;
    std::map<std::string, Parameter> params;
    std::vector<Effect> effects;
    bool enabled = true; // un clip désactivé reste en place mais n'est ni rendu ni mixé
    bool hold = false;   // arrêt sur image : la source reste figée à « in », sans son
    bool reverse = false; // lecture à l'envers de la portion de source, son compris
    bool adjustment = false; // calque d'effets : ses effets s'appliquent à l'image des pistes du dessous
    bool active(double time) const { return time >= start && time < start + duration; }
    bool live(double time) const { return enabled && active(time); }
    // Reversed, the clip starts on the last frame of its span (just inside its end) and ends on « in ».
    // An animated « speed_ramp » (a factor of the speed) is integrated from the start (reversed: to the end).
    double sourceTime(double time) const;
    bool ramped() const;
    float param(const std::string &key, double time, float fallback) const;
    float envelope(double time) const;
    float transitionProgress(double time) const;
};
struct Track {
    Id id = 0;
    std::string name;
    TrackKind kind = TrackKind::Video;
    bool muted = false, solo = false, locked = false;
    float gain = 0, pan = 0;
    sx::u32 color = 0;  // 0xRRGGBB, 0 : couleur du type de piste
    float height = 0;   // hauteur de la timeline en pixels, 0 : hauteur par défaut
    Id group = 0;       // groupe de pistes de la timeline, 0 : aucun
    std::vector<Clip> clips;
    MixChannel mix;
    // Video only: the track's own effect graph (Shaderixel composition schema), applied to the picture of
    // its clips before it is blended with the tracks below. Null: no track effects.
    Json graph;
    // Video only: how the track is blended over the tracks below in the automatic layer graph: one of
    // Shaderixel's 27 modes and an opacity, or a library blend effect (inputs: below, track, mask).
    int blend = 0;
    float opacity = 1;
    Effect blendEffect{.id = {}}; // id empty: the blend mode
};
// Groupe de pistes repliable, comme les dossiers de l'ArrangementView CyDAW. Organisation seule :
// la composition et le mixage ne changent pas.
struct TrackGroup {
    Id id = 0;
    std::string name = "Group";
    sx::u32 color = 0; // 0xRRGGBB, 0 : couleur d'accent
    bool collapsed = false;
};
// Repère de la règle, comme les locators de l'ArrangementView CyDAW.
struct Marker {
    Id id = 0;
    double time = 0;
    std::string name;
    sx::u32 color = 0; // 0xRRGGBB, 0 : couleur d'accent
    std::string note;
    double duration = 0; // région : longueur après time, 0 : repère ponctuel
    double end() const { return time + duration; }
};
// One cue of a SubRip (.srt) file, in seconds.
struct Subtitle {
    double start = 0, end = 0;
    std::string text;
};
std::vector<Subtitle> parseSrt(const std::string &text);
// One event of a CMX 3600 edit decision list, in seconds at the frame rate given to the parser.
struct EdlEvent {
    std::string reel, channels, clipName, sourceFile;
    double sourceIn = 0, sourceOut = 0, recordIn = 0, recordOut = 0, speed = 1;
};
std::vector<EdlEvent> parseEdl(const std::string &text, int fps);
// What « Coller les attributs » carries from one clip to others.
struct AttributeChoice {
    bool effects = true, transform = true, opacity = true, audio = true, mask = false, replaceEffects = false;
};
struct Project {
    std::string name = "Untitled";
    int width = 1280, height = 720, fps = 30;
    double duration = 12, timeOrigin = 0;
    float master = 0;
    std::string workingSpace = "srgb", toneMap = "clip", imageFormat = "png8";
    float exposure = 0, backgroundAlpha = 1;
    MixChannel masterMix;
    Json layerGraph; // Shaderixel composition nodes; surface IDs bind timeline tracks.
    Id nextId = 1;
    std::vector<Track> tracks;
    std::vector<AudioBus> buses;
    std::vector<Marker> markers;    // triés par temps
    std::vector<TrackGroup> groups;
    std::vector<std::string> bin; // médias importés dans le chutier, utilisés ou non
    double workIn = 0, workOut = 0; // zone de travail / boucle ; workOut <= workIn : désactivée
    bool hasWorkArea() const { return workOut > workIn; }
    Id allocate() { return nextId++; }
    Track &addTrack(TrackKind kind);
    void removeTrack(Id id);
    void moveTrack(Id id, int index);
    // Copy of a track under it: clips, effects, animations and mix; links to other tracks are not copied.
    Track &duplicateTrack(Id id);
    Marker &addMarker(double time, const std::string &name = {});
    void removeMarker(Id id);
    Marker *findMarker(Id id);
    // Markers as YouTube chapters ("0:00 Intro", a first chapter at 0:00 added when missing) or as CSV.
    std::string markerChapters() const;
    // Edit decision list (CMX 3600) of a video track (the first one holding clips for 0): one event per
    // enabled clip, its linked sound marked, speed changes as M2 lines, generators and titles as black.
    std::string edl(Id track = 0) const;
    // Every clip as a CSV row: track, name, record and source timecodes, speed, effects, state.
    std::string shotListCsv() const;
    std::string markerCsv() const;
    TrackGroup *findGroup(Id id);
    // Regroupe des pistes : les suivantes se placent sous la première, qui garde sa position.
    TrackGroup &groupTracks(const std::vector<Id> &tracks, const std::string &name = {});
    void ungroup(Id group);
    // Montage trois points : la portion [in, out) d'un média va à `at` sur une piste vidéo et/ou audio.
    // insert décale tout ce qui suit sur toutes les pistes ; sinon la portion écrase les pistes cibles.
    // Une cible 0 prend la première piste libre du type, créée au besoin.
    struct SourceEdit {
        std::string path, name;
        double in = 0, out = 0, at = 0;
        bool video = true, audio = true, insert = false;
        Id videoTrack = 0, audioTrack = 0;
        double fill = 0; // fit to fill: timeline length, the speed set so [in, out) lasts it (0 : out - in)
    };
    std::vector<Id> placeSource(const SourceEdit &edit);
    // Empties [begin, end) on the concerned tracks: clips crossing a boundary are cut, those inside
    // removed, without moving anything (lift). removeRange closes the span instead (extract).
    void clearRange(double begin, double end, const std::function<bool(const Track &)> &concerned);
    void liftRange(double begin, double end);
    // The empty span around `time` on a track, if `time` is between clips (or before the first one).
    bool gapAt(Id track, double time, double &begin, double &end) const;
    // True when no clip of any track overlaps [begin, end).
    bool spanEmpty(double begin, double end) const;
    AudioBus &addBus();
    Track *findTrack(Id id);
    AudioBus *findBus(Id id);
    const AudioBus *findBus(Id id) const;
    std::vector<Id> audioBusOrder() const;
    void removeBus(Id id);
    Clip *findClip(Id id);
    const Clip *findClip(Id id) const;
    Track *owner(Id clip);
    const Track *owner(Id clip) const { return const_cast<Project *>(this)->owner(clip); }
    bool audible(const Track &t) const;
    std::vector<Id> linked(Id clip, bool editable = true) const;
    std::vector<Id> selection(const std::vector<Id> &clips, bool editable = true) const;
    void moveClips(const std::vector<Id> &clips, double delta);
    void removeClips(const std::vector<Id> &clips, bool ripple = false);
    std::vector<Id> duplicateClips(const std::vector<Id> &clips);
    void splitClips(const std::vector<Id> &clips, double time);
    void linkClips(Id clip, Id other);
    void unlink(Id clip);
    void setTiming(Id clip, double start, double duration, double in, double speed, bool moveKeys = true);
    void rippleTrim(Id clip, double delta, bool left = false);
    // Trims a clip and its linked group to a time inside it: head removes what is before, otherwise
    // what is after. Ripple closes the gap; the kept picture keeps its animation keys.
    void trimClip(Id clip, double time, bool head, bool ripple);
    // Opens an empty span at time on every track: clips across it are cut, the following clips,
    // their keys and the markers after it move later.
    void insertGap(double time, double length);
    // Cuts the given clips (every clip of the unlocked tracks when empty) at each marker they cross,
    // the pieces cut again at the next markers. Returns the number of cuts.
    int splitAtMarkers(const std::vector<Id> &clips);
    // Closes the empty spans: on one track its clips move earlier (their linked members with them),
    // with track 0 the spans empty on every track are removed from the sequence. Returns the spans closed.
    int closeGaps(Id track);
    // Beat montage: the given clips of the earliest one's track, in order, start at the first one and
    // then at each following marker, each lasting until the next (the last keeps its length). `available`
    // gives the longest timeline length a clip's source allows. Returns the clips placed.
    int fitToMarkers(const std::vector<Id> &clips, const std::function<double(const Clip &)> &available);
    // Multicam: the angles at a time are the unlocked video tracks with a picture there (top first).
    std::vector<Id> anglesAt(double time) const;
    // Cuts every angle at the time and keeps only the chosen one (1-based) enabled from there; with
    // followSound, each angle's linked sound is switched with its picture.
    void switchAngle(double time, int angle, bool followSound = false);
    // Paste attributes: effects, transform, opacity, gain/pan and mask of a clip onto others, their
    // animation keys moved with the clip start; effects only between clips of the same kind of track.
    int pasteAttributes(const Clip &source, TrackKind sourceKind, const std::vector<Id> &targets,
                        const AttributeChoice &choice);
    // Subtitles: cues become title clips on a new top video track (low in the frame, outlined);
    // export writes the title clips of a track (all title clips for 0) as SubRip.
    Id importSubtitles(const std::vector<Subtitle> &cues, const std::string &trackName = "Subtitles");
    std::string subtitlesSrt(Id track = 0) const;
    // A cue added on a subtitle track, in the style of the cue before it; it stops at the next clip.
    Id addSubtitle(Id track, double start, double duration, const std::string &text);
    // Font size, vertical position and outline set on every title clip of a track (their keys removed).
    int setSubtitleStyle(Id track, const std::map<std::string, float> &values);
    // Media files the sequence uses (bin first, then clip sources), and a path replacement everywhere.
    std::vector<std::string> mediaPaths() const;
    int relinkMedia(const std::map<std::string, std::string> &moves);
    // Names clips in timeline order from a pattern: a run of # is the number (zero padded to its length),
    // {nom} the clip's current name. Returns the new names in that order.
    std::vector<std::string> renameClips(const std::vector<Id> &clips, const std::string &pattern, int first);
    // Inserts a held frame of a video clip at time (the clip is cut there and the sequence opened);
    // its animated values are frozen at that time. Returns the held clip.
    Id freezeFrame(Id clip, double time, double length);
    // Replace edit: the clip and its linked members showing the same medium take another one from in,
    // keeping their place and length. mediaLength 0 is a still image; members of a kind the medium lacks stay.
    void replaceSource(Id clip, const std::string &path, double in, double mediaLength, bool hasVideo,
                       bool hasAudio);
    void rollTrim(Id clip, Id next, double delta);
    void overlapTransition(Id clip, Id next, double seconds);
    void removeRange(double begin, double end);
    Id duplicate(Id clip);
    void split(Id clip, double time);
    void remove(Id clip);
    void makeContainer(Id clip);
    void validate() const;
    Json json() const;
    static Project fromJson(const Json &j);
    static Project demo();
    void save(const std::string &path) const;
    static Project load(const std::string &path);
};
class History {
  public:
    // A state and the name of the change made from it (or, in future, of the change leading back to it).
    struct Step {
        Project project;
        std::string label;
    };

  private:
    std::vector<Step> past, future;

  public:
    void checkpoint(const Project &p, std::string label = {});
    bool undo(Project &p);
    bool redo(Project &p);
    const std::vector<Step> &undoSteps() const { return past; }
    const std::vector<Step> &redoSteps() const { return future; } // the next redo is the last one
    void clear() {
        past.clear();
        future.clear();
    }
};
// CyDAW 2 ArrangementView's shaped curve and coordinate model, adapted to seconds/frames.
float shapedAmount(float raw, float shape);
double snapTime(double seconds, int fps, bool snap);
} // namespace sf

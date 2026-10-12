#pragma once
#include "core/Json.h"
#include <map>
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
    std::string text = "Votre titre", font = "Segoe UI";
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
    std::string sourceColor = "auto";
    bool sourcePremultiplied = false;
    Json composition; // embedded Project; its timeline, audio and layer graph use local IDs/time.
    Json widget; // motion layout: parent, anchor, pivot, type; dimensions/offsets are animated parameters.
    Title title;
    Mask mask;
    Transition transition;
    std::map<std::string, Parameter> params;
    std::vector<Effect> effects;
    bool active(double time) const { return time >= start && time < start + duration; }
    double sourceTime(double time) const { return in + (time - start) * speed; }
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
    std::vector<Clip> clips;
    MixChannel mix;
};
struct Project {
    std::string name = "Sans titre";
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
    Id allocate() { return nextId++; }
    Track &addTrack(TrackKind kind);
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
    std::vector<Project> past, future;

  public:
    void checkpoint(const Project &p);
    bool undo(Project &p);
    bool redo(Project &p);
    void clear() {
        past.clear();
        future.clear();
    }
};
// CyDAW 2 ArrangementView's shaped curve and coordinate model, adapted to seconds/frames.
float shapedAmount(float raw, float shape);
double snapTime(double seconds, int fps, bool snap);
} // namespace sf

#include "mcp/Protocol.h"
#include "adapters/DspGraph.h"
#include "adapters/LayerGraph.h"
#include "core/AtomicFile.h"
#include "core/Motion.h"
#include "core/Version.h"
#include "render/Grading.h"
#include <fstream>
namespace sf {
// Versions: "<project> versions/<project> AAAA-MM-JJ HHMMSS.sframe", the newest ten kept.
static std::filesystem::path versionFolder(const std::filesystem::path &project) {
    return project.parent_path() / (project.stem().wstring() + L" versions");
}
std::vector<std::pair<std::string, std::filesystem::path>> projectVersions(const std::string &projectPath) {
    std::vector<std::pair<std::string, std::filesystem::path>> versions;
    if (projectPath.empty())
        return versions;
    std::error_code missing;
    auto folder = versionFolder(sx::Utf8Path(projectPath));
    for (std::filesystem::directory_iterator it(folder, missing), end; !missing && it != end; it.increment(missing))
        if (it->path().extension() == L".sframe") {
            // The date is the end of the name: AAAA-MM-JJ HHMMSS.
            auto stem = sx::Narrow(it->path().stem().wstring());
            versions.push_back({stem.size() >= 17 ? stem.substr(stem.size() - 17) : stem, it->path()});
        }
    std::sort(versions.begin(), versions.end(), [](auto &a, auto &b) { return a.first > b.first; });
    return versions;
}
static void keepVersion(const std::string &path) {
    auto project = sx::Utf8Path(path);
    std::error_code missing;
    if (!std::filesystem::is_regular_file(project, missing))
        return;
    auto folder = versionFolder(project);
    std::filesystem::create_directories(folder, missing);
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t stamp[32];
    swprintf(stamp, 32, L" %04d-%02d-%02d %02d%02d%02d", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
             now.wSecond);
    std::filesystem::copy_file(project, folder / (project.stem().wstring() + stamp + L".sframe"),
                               std::filesystem::copy_options::overwrite_existing, missing);
    auto versions = projectVersions(path);
    for (size_t i = 10; i < versions.size(); ++i)
        std::filesystem::remove(versions[i].second, missing);
}
// A title clip goes on a video track holding only titles and free over it; otherwise on a new top track.
static Id placeTitle(Project &p, Clip c, bool newTrack) {
    Track *free = nullptr;
    for (auto &t : p.tracks) {
        if (free || newTrack || t.kind != TrackKind::Video || t.locked || t.clips.empty())
            continue;
        bool titlesOnly = true, room = true;
        for (auto &other : t.clips) {
            titlesOnly = titlesOnly && other.title.enabled;
            room = room && (other.start >= c.start + c.duration - 1e-7 || other.start + other.duration <= c.start + 1e-7);
        }
        if (titlesOnly && room)
            free = &t;
    }
    p.duration = std::max(p.duration, c.start + c.duration);
    if (free) {
        free->clips.push_back(std::move(c));
        return free->id;
    }
    auto &track = p.addTrack(TrackKind::Video);
    track.name = "Title";
    track.clips.push_back(std::move(c));
    appendLayerSurface(p, track.id, track.name);
    std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
    return p.tracks.front().id;
}
// Names of the undoable commands in the History panel.
static std::string actionLabel(const std::string &name) {
    static const std::map<std::string, std::string> labels{
        {"import_media", "Import Media"},
        {"add_track", "New Track"},
        {"set_track", "Track Settings"},
        {"remove_track", "Delete Track"},
        {"move_track", "Move Track"},
        {"duplicate_track", "Duplicate Track"},
        {"group_tracks", "Track Group"},
        {"set_group", "Group Settings"},
        {"ungroup", "Ungroup Tracks"},
        {"edit_source", "Edit from Source"},
        {"lift_range", "Lift Range"},
        {"remove_range", "Extract / Close Gap"},
        {"insert_gap", "Insert Gap"},
        {"split_at_markers", "Split at Markers"},
        {"close_gaps", "Close Gaps"},
        {"fit_to_markers", "Align to Markers"},
        {"switch_angle", "Angle change"},
        {"auto_grade", "Auto Color"},
        {"match_grade", "Matched colors"},
        {"paste_attributes", "Paste Attributes"},
        {"apply_animation", "Animation Preset"},
        {"sync_audio", "Audio Synchronization"},
        {"add_adjustment_layer", "New Adjustment Layer"},
        {"place_clip", "Keying"},
        {"split_screen", "Split Screen"},
        {"key_background", "Green Screen"},
        {"audio_cut_fades", "Audio microfades"},
        {"rename_clips", "Batch Rename"},
        {"add_subtitle", "New Subtitle Cue"},
        {"set_subtitle_style", "Subtitle Style"},
        {"normalize_loudness", "Loudness Normalization"},
        {"set_sequence", "Sequence Format"},
        {"fit_clips", "Clip Framing"},
        {"import_subtitles", "Import Subtitles"},
        {"relink_media", "Relink Media"},
        {"collect_media", "Collect Media"},
        {"remove_unused_media", "Clean Media Bin"},
        {"import_edl", "Import d'EDL"},
        {"detect_beats", "Transient Markers"},
        {"restore_version", "Restore Version"},
        {"remove_silences", "Remove Silence"},
        {"auto_duck", "Automatic ducking"},
        {"detect_scenes", "Scene Detection"},
        {"freeze_frame", "Freeze Frame"},
        {"replace_source", "Replace Media"},
        {"add_marker", "New Marker"},
        {"set_marker", "Marker Settings"},
        {"remove_marker", "Delete Marker"},
        {"set_work_area", "Work Area"},
        {"add_generator", "New Generator"},
        {"add_title", "New title"},
        {"add_lower_third", "New lower third"},
        {"apply_title_style", "Title Style"},
        {"enhance_voice", "Voice Enhancement"},
        {"set_title", "Title Text"},
        {"set_clip", "Clip Settings"},
        {"ripple_trim", "Ripple Trim"},
        {"roll_trim", "Trim roll"},
        {"trim_clip", "Split at Playhead"},
        {"normalize_audio", "Audio normalization"},
        {"overlap_transition", "Crossfade"},
        {"set_transition", "Transition"},
        {"remove_transition", "Remove Transition"},
        {"split_clip", "Split"},
        {"split_clips", "Split"},
        {"delete_clip", "Delete"},
        {"delete_clips", "Delete"},
        {"move_clips", "Move"},
        {"duplicate_clip", "Duplicate"},
        {"duplicate_clips", "Duplicate"},
        {"link_clips", "Liaison"},
        {"unlink_clip", "Unlink"},
        {"set_parameter", "Parameter"},
        {"set_keyframe", "Point d'animation"},
        {"add_effect", "Add effect"},
        {"set_track_graph", "Track effect graph"},
        {"add_track_effect", "Add track effect"},
        {"add_audio_effect", "Add audio effect"},
        {"set_effect", "Effect Settings"},
        {"remove_effect", "Remove effect"},
        {"set_mask", "Mask"},
        {"set_bezier_mask", "Bezier Mask"},
        {"add_grade", "Color Grading"},
        {"set_grade", "Color Grading"},
        {"set_mix_channel", "Mix"},
        {"add_bus", "New bus"},
        {"remove_bus", "Delete Bus"},
    };
    if (auto it = labels.find(name); it != labels.end())
        return it->second;
    auto text = name;
    std::replace(text.begin(), text.end(), '_', ' ');
    return text;
}
static Json schema(std::initializer_list<std::pair<const char *, const char *>> properties,
                   std::initializer_list<const char *> required = {}) {
    Json s = Json::Object();
    s["type"] = "object";
    s["properties"] = Json::Object();
    s["required"] = Json::Array();
    for (auto &[n, t] : properties) {
        s["properties"][n] = Json::Object();
        s["properties"][n]["type"] = t;
        if (std::string(t) == "array" && (std::string(n) == "clips" || std::string(n) == "tracks")) {
            s["properties"][n]["items"] = Json::Object();
            s["properties"][n]["items"]["type"] = "integer";
            s["properties"][n]["items"]["minimum"] = 1;
            s["properties"][n]["items"]["maximum"] = 2147483647;
            s["properties"][n]["maxItems"] = 4096;
        }
        if (std::string(n) == "points") {
            auto &points = s["properties"][n];
            points["minItems"] = 3;
            points["maxItems"] = 16;
            points["items"] = Json::Object();
            auto &point = points["items"];
            point["type"] = "array";
            point["minItems"] = 6;
            point["maxItems"] = 6;
            point["items"] = Json::Object();
            point["items"]["type"] = "number";
            point["items"]["minimum"] = -2;
            point["items"]["maximum"] = 2;
        }
    }
    for (auto n : required)
        s["required"].Push(n);
    return s;
}
Json Protocol::tools() const {
    Json list = Json::Array();
    auto add = [&](const char *n, const char *d, Json s) {
        Json t = Json::Object();
        t["name"] = n;
        t["description"] = d;
        t["inputSchema"] = s;
        list.Push(t);
    };
    add("set_motion_widget",
        "Configure video widget hierarchy/anchors; widget_x/y/width/height support animation",
        schema({{"clip", "integer"}, {"layout", "object"}, {"enabled", "boolean"}}, {"clip"}));
    add("get_motion_widgets", "Motion hierarchy and evaluated rectangles at the requested time",
        schema({{"time", "number"}}));
    add("make_clip_container", "Convert a video clip to a nested composition while preserving its content",
        schema({{"clip", "integer"}}, {"clip"}));
    add("get_clip_composition", "Container's local timeline and graph",
        schema({{"clip", "integer"}}, {"clip"}));
    add("set_clip_composition", "Replace the container's local composition after validation",
        schema({{"clip", "integer"}, {"project", "object"}}, {"clip", "project"}));
    add("get_layer_graph", "Shaderixel composition graph: timelines, blends, effects and output",
        schema({}));
    add("set_layer_graph", "Replace the composition graph; Shaderixel validation, undo and HDR rendering",
        schema({{"graph", "object"}}, {"graph"}));
    add("get_track_graph",
        "Effect graph of a video track (its clips composited, then its effect nodes, before blending with the "
        "tracks below); returns graph and explicit (false: no track effects yet)",
        schema({{"track", "integer"}}, {"track"}));
    add("set_track_graph",
        "Replace a video track's effect graph (same schema as the layer graph; surfaces: the track or tracks below "
        "it); graph null removes the track effects",
        schema({{"track", "integer"}, {"graph", "object"}}, {"track"}));
    add("add_track_effect", "Append a library effect to the end of a video track's effect chain; returns node",
        schema({{"track", "integer"}, {"id", "string"}}, {"track", "id"}));
    add("set_layer_parameter", "Base value of a composition node parameter",
        schema({{"node", "integer"}, {"key", "string"}, {"value", "number"}}, {"node", "key", "value"}));
    add("set_layer_keyframe", "Animate the blending graph in sequence time",
        schema({{"node", "integer"},
                {"key", "string"},
                {"time", "number"},
                {"value", "number"},
                {"curve", "integer"},
                {"shape", "number"}},
               {"node", "key", "time", "value"}));
    add("status", "Sequence status, transport and project content", schema({}));
    add("add_grade",
        "Add an HDR correction to a video clip; stack order and animation via "
        "set_effect/set_parameter/set_keyframe",
        schema({{"clip", "integer"}, {"name", "string"}}, {"clip"}));
    add("set_grade",
        "Set schema 1 correction data (name, curves, maskPoints, lutInput/lutOutput); "
        "animatable parameters via set_parameter",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"settings", "object"}},
               {"clip", "effect", "settings"}));
    add("set_grade_curve",
        "Master/r/g/b/hue_hue/hue_sat/hue_luma/luma_sat curve; 2..32 [x,y] points, endpoints 0 and 1",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"curve", "string"}, {"values", "array"}},
               {"clip", "effect", "curve", "values"}));
    add("import_grade_lut",
        "Import and embed a 2D image LUT or 1D/3D .cube, with explicit domain and spaces; no "
        "reference "
        "external",
        schema({{"clip", "integer"},
                {"effect", "integer"},
                {"path", "string"},
                {"layout", "string"},
                {"input", "string"},
                {"output", "string"}},
               {"clip", "effect", "path"}));
    add("neutral_grade",
        "Neutral balance from a pre-correction linear Rec.709 RGB sample; replace WB gains "
        "and their keys",
        schema(
            {{"clip", "integer"}, {"effect", "integer"}, {"r", "number"}, {"g", "number"}, {"b", "number"}},
            {"clip", "effect", "r", "g", "b"}));
    add("list_grade_presets", "Built-in color grading presets", schema({}));
    add("apply_grade_preset", "Replace a correction with a built-in preset",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"name", "string"}}, {"clip", "effect", "name"}));
    add("save_grade_preset", "Save a portable correction, including LUT and animation",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"path", "string"}}, {"clip", "effect", "path"}));
    add("load_grade_preset", "Replace a correction from a validated .sgrade file",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"path", "string"}}, {"clip", "effect", "path"}));
    add("video_scopes",
        "Waveform 256x128, parade 3x128x128, vectorscope 256x256, histogram 3x256; analysis up to 640x360",
        schema({{"time", "number"}, {"bins", "boolean"}}));
    add("set_color_management",
        "RGBA16F composition, working color space, display and high-precision sequences",
        schema({{"space", "string"},
                {"tone", "string"},
                {"exposure", "number"},
                {"backgroundAlpha", "number"},
                {"imageFormat", "string"}}));
    add("set_source_color", "Interpret a video source's color space and alpha",
        schema({{"clip", "integer"}, {"space", "string"}, {"premultiplied", "boolean"}}, {"clip"}));
    add("new_project",
        "Create an empty sequence or use a template (youtube, short, podcast, cinema: format, frame rate and "
        "named tracks)",
        schema({{"name", "string"},
                {"width", "integer"},
                {"height", "integer"},
                {"fps", "integer"},
                {"duration", "number"},
                {"template", "string"}}));
    add("open_project", "Open a .sframe project", schema({{"path", "string"}}, {"path"}));
    add("save_project", "Save a .sframe project; keep the replaced file among the ten most recent versions (versions)",
        schema({{"path", "string"}, {"versions", "boolean"}}));
    add("list_versions", "Saved project versions, newest first: path, saved",
        schema({}));
    add("restore_version", "Replace the sequence with a previous version as one undoable operation",
        schema({{"path", "string"}}, {"path"}));
    add("import_media",
        "Import video, image or audio and create clips (separate audio); proxy prepares a 720p proxy for videos over "
        "1440 lines; return clips, proxy_job",
        schema({{"path", "string"}, {"start", "number"}, {"proxy", "boolean"}}, {"path"}));
    add("remove_unused_media", "Remove media unused by any clip from the bin; return removed",
        schema({}));
    add("add_track", "Add a video or audio track",
        schema({{"kind", "string"}, {"name", "string"}}, {"kind"}));
    add("set_track",
        "Rename a track; set its 0xRRGGBB color, height, mute, solo and lock; for a video track its blending "
        "over the tracks below: blend (0-26 or a mode name such as Multiply, Screen, Overlay), opacity 0-1, "
        "blend_effect (a library blend effect id, empty to go back to the mode)",
        schema({{"track", "integer"},
                {"blend", "string"},
                {"opacity", "number"},
                {"blend_effect", "string"},
                {"name", "string"},
                {"color", "integer"},
                {"enabled", "boolean"},
                {"height", "number"},
                {"muted", "boolean"},
                {"solo", "boolean"},
                {"locked", "boolean"},
                {"group", "integer"}},
               {"track"}));
    add("remove_track", "Delete a track and its clips; linked partners stay in place",
        schema({{"track", "integer"}}, {"track"}));
    add("move_track", "Move a track to the requested index (0 = top, in front in the composition)",
        schema({{"track", "integer"}, {"index", "integer"}}, {"track", "index"}));
    add("group_tracks", "Group tracks in a collapsible timeline group; return group",
        schema({{"tracks", "array"}, {"name", "string"}}, {"tracks"}));
    add("set_group", "Rename, recolor (0xRRGGBB), or collapse a track group",
        schema({{"group", "integer"}, {"name", "string"}, {"color", "integer"}, {"collapsed", "boolean"}}, {"group"}));
    add("ungroup", "Dissolve a group; its tracks stay in place", schema({{"group", "integer"}}, {"group"}));
    add("edit_source",
        "Three-point edit: place a media range [in, out) at start; insert shifts later clips, overwrite "
        "replaces target tracks (video_track/audio_track, 0 = first available); fit mode "
        "fills the work area (or start + duration) by adjusting speed so [in, out) fits",
        schema({{"path", "string"},
                {"duration", "number"},
                {"in", "number"},
                {"out", "number"},
                {"start", "number"},
                {"mode", "string"},
                {"video", "boolean"},
                {"audio", "boolean"},
                {"video_track", "integer"},
                {"audio_track", "integer"}},
               {"path"}));
    add("duplicate_track", "Duplicate a track below itself: clips, effects, animation and mix; return track",
        schema({{"track", "integer"}}, {"track"}));
    add("lift_range", "Lift [begin, end) on all tracks: split boundaries, remove the interior and leave a gap",
        schema({{"begin", "number"}, {"end", "number"}}, {"begin", "end"}));
    add("add_marker", "Add a marker to the timeline ruler",
        schema({{"time", "number"}, {"name", "string"}, {"color", "integer"}, {"note", "string"}, {"duration", "number"}},
               {"time"}));
    add("set_marker", "Move, rename or recolor a marker; duration > 0 makes a region (0: point marker)",
        schema({{"marker", "integer"},
                {"time", "number"},
                {"name", "string"},
                {"color", "integer"},
                {"note", "string"},
                {"duration", "number"}},
               {"marker"}));
    add("remove_marker", "Delete a marker", schema({{"marker", "integer"}}, {"marker"}));
    add("export_marker_stills",
        "Render the frame at each marker; path is the base name (folder/base.png or .exr); files "
        "become base_01_Name.png; returns files",
        schema({{"path", "string"}}, {"path"}));
    add("export_edl",
        "CMX 3600 edit decision list for a video track (track, or the first with clips) for Premiere, Resolve "
        "or Avid; optional path; returns text, events",
        schema({{"path", "string"}, {"track", "integer"}}));
    add("import_edl",
        "Import a CMX 3600 edit decision list onto a new video track (and linked audio track for audio "
        "events); locate media from SOURCE FILE comments, otherwise by name in media_folder (default: the folder of "
        "the EDL), otherwise offline; returns clips, offline",
        schema({{"path", "string"}, {"media_folder", "string"}}, {"path"}));
    add("export_shot_list", "CSV shot list (track, name, timecodes, source, speed, effects); optional path; "
                            "returns text",
        schema({{"path", "string"}}));
    add("export_markers",
        "List markers as YouTube chapters (youtube format) or CSV; optional path writes UTF-8; "
        "returns text",
        schema({{"format", "string"}, {"path", "string"}}));
    add("set_work_area", "Work area / loop; end <= start disables it",
        schema({{"begin", "number"}, {"end", "number"}}, {"begin", "end"}));
    add("add_bus", "Add a stereo mixing bus", schema({{"name", "string"}}));
    add("remove_bus", "Delete a bus, reconnect its sources to master and remove its sends",
        schema({{"bus", "integer"}}, {"bus"}));
    add("set_mix_channel", "Set a track, bus or master channel; output 0 refers to master",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"gain", "number"},
                {"pan", "number"},
                {"output", "integer"},
                {"muted", "boolean"},
                {"solo", "boolean"},
                {"locked", "boolean"},
                {"name", "string"}},
               {"scope"}));
    add("set_send", "Create or set a bus send, before or after the fader",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"bus", "integer"},
                {"gain", "number"},
                {"pre_fader", "boolean"},
                {"enabled", "boolean"}},
               {"scope", "target", "bus"}));
    add("remove_send", "Remove a bus send",
        schema({{"scope", "string"}, {"target", "integer"}, {"bus", "integer"}}, {"scope", "target", "bus"}));
    add("add_mix_effect", "Add a CyDAW 2 DSP to a track, bus or master channel",
        schema({{"scope", "string"}, {"target", "integer"}, {"id", "string"}}, {"scope", "id"}));
    add("set_mix_effect", "Enable or reorder a channel DSP",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"enabled", "boolean"},
                {"position", "integer"}},
               {"scope", "effect"}));
    add("remove_mix_effect", "Remove a channel DSP and its animation",
        schema({{"scope", "string"}, {"target", "integer"}, {"effect", "integer"}}, {"scope", "effect"}));
    add("set_mix_parameter", "Set channel gain/pan or a DSP parameter",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"value", "number"}},
               {"scope", "key", "value"}));
    add("set_mix_keyframe", "Animate a channel or DSP in sequence time",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"value", "number"},
                {"time", "number"},
                {"curve", "integer"},
                {"shape", "number"},
                {"previous_time", "number"}},
               {"scope", "key", "value", "time"}));
    add("remove_mix_keyframe", "Remove a key; without time, restore the channel's base value",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"time", "number"}},
               {"scope", "key"}));
    add("get_audio_graph", "Read the stereo routing/DSP graph, validated by CyDAW 2", schema({}));
    add("set_dsp_graph",
        "Compile and embed a v2 stereo DSP graph; scope clip/track/bus/master, optional effect to "
        "replace",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"graph", "object"}},
               {"scope", "graph"}));
    add("get_dsp_graph", "Read an embedded DSP graph",
        schema({{"scope", "string"}, {"target", "integer"}, {"clip", "integer"}, {"effect", "integer"}},
               {"scope", "effect"}));
    add("add_external_audio",
        "Load a native SDK v1 or VST3 effect in an isolated process; scope clip/track/bus/master",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"kind", "string"},
                {"path", "string"}},
               {"scope", "kind", "path"}));
    add("show_plugin_editor", "Show/hide the VST3 window in its isolated worker",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"visible", "boolean"}},
               {"scope", "effect"}));
    add("capture_plugin_state", "Embed the complete VST3 state",
        schema({{"scope", "string"}, {"target", "integer"}, {"clip", "integer"}, {"effect", "integer"}},
               {"scope", "effect"}));
    add("get_plugin_state", "Read VST3 state and control names",
        schema({{"scope", "string"}, {"target", "integer"}, {"clip", "integer"}, {"effect", "integer"}},
               {"scope", "effect"}));
    add("set_plugin_state", "Restore a base64 VST3 state up to 4 MiB",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"state", "string"}},
               {"scope", "effect", "state"}));
    add("save_plugin_preset", "Save a full VST3 preset",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"path", "string"}},
               {"scope", "effect", "path"}));
    add("load_plugin_preset", "Load a VST3 preset while preserving automation curves",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"path", "string"}},
               {"scope", "effect", "path"}));
    add("add_generator", "Add an animated HLSL video generator",
        schema({{"id", "string"}, {"start", "number"}, {"duration", "number"}}));
    add("add_title",
        "Add a transparent text layer with effects and animation on an available title track at this time "
        "(new_track forces a new track); return clip, track",
        schema({{"text", "string"}, {"start", "number"}, {"duration", "number"}, {"new_track", "boolean"}}));
    add("apply_title_style",
        "Apply a style to title clips: classic (white, outline, shadow), subtitle_yellow (outlined yellow), "
        "boxed (white on black), cinema (serif, soft shadow), neon (cyan glow); return clips",
        schema({{"clips", "array"}, {"style", "string"}}, {"clips", "style"}));
    add("enhance_voice",
        "Enhance audio clip speech: add a gentle noise gate, EQ (reduced bass, increased presence) and "
        "CyDAW compressor to their effects; return clips",
        schema({{"clips", "array"}}, {"clips"}));
    add("add_lower_third",
        "Add a name / role lower third: name, role, start (playhead), duration (5); text at the bottom "
        "left on a dark box, with slide and fade animation; return clip, track",
        schema({{"name", "string"}, {"role", "string"}, {"start", "number"}, {"duration", "number"},
                {"new_track", "boolean"}}));
    add("set_title", "Set text, font, alignment (0 left, 1 center, 2 right) and style",
        schema({{"clip", "integer"},
                {"text", "string"},
                {"font", "string"},
                {"alignment", "integer"},
                {"bold", "boolean"},
                {"italic", "boolean"}},
               {"clip"}));
    add("set_mask", "Set the mask shape (none, rectangle, ellipse) and inversion",
        schema({{"clip", "integer"}, {"shape", "string"}, {"inverted", "boolean"}}, {"clip"}));
    add("set_bezier_mask",
        "Set a closed mask; points [x,y,inX,inY,outX,outY], normalized centered coordinates",
        schema({{"clip", "integer"}, {"points", "array"}}, {"clip", "points"}));
    add("start_mask_tracking",
        "Track mask translation by background correlation (1800 frames maximum)",
        schema({{"clip", "integer"},
                {"begin", "number"},
                {"end", "number"},
                {"radius", "integer"},
                {"effect", "integer"}},
               {"clip", "begin", "end"}));
    add("mask_tracking_status", "Status of the last mask tracking job", schema({}));
    add("cancel_mask_tracking", "Cancel active tracking", schema({}));
    add("apply_mask_tracking", "Apply mask_x/y curves if the clip has not changed",
        schema({{"job", "integer"}}, {"job"}));
    add("set_transition",
        "Incoming transition between the video clip and layers below; reset_progress restores automatic "
        "automatic",
        schema({{"clip", "integer"},
                {"id", "string"},
                {"duration", "number"},
                {"enabled", "boolean"},
                {"reset_progress", "boolean"}},
               {"clip"}));
    add("remove_transition", "Remove the transition and its transition_ parameters/animation",
        schema({{"clip", "integer"}}, {"clip"}));
    add("set_transition_shader",
        "Compile a two-input transition shader and save it in the clip",
        schema({{"clip", "integer"}, {"code", "string"}}, {"clip", "code"}));
    add("set_clip", "Change clip timing, name or 0xRRGGBB color; hold freezes the source at in",
        schema({{"clip", "integer"},
                {"color", "integer"},
                {"start", "number"},
                {"duration", "number"},
                {"in", "number"},
                {"speed", "number"},
                {"fade_in", "number"},
                {"fade_out", "number"},
                {"name", "string"},
                {"enabled", "boolean"},
                {"hold", "boolean"},
                {"reverse", "boolean"}},
               {"clip"}));
    add("ripple_trim",
        "Shorten/extend by delta seconds and shift later clips; left adjusts the source in point",
        schema({{"clip", "integer"}, {"delta", "number"}, {"left", "boolean"}}, {"clip", "delta"}));
    add("normalize_audio",
        "Set audio clip gain so its peak reaches peak dBFS (default -1); return gains",
        schema({{"clips", "array"}, {"peak", "number"}}, {"clips"}));
    add("replace_source",
        "Replace edit: the clip (and linked group using the same media) uses another source starting at in, "
        "keeping its position and duration",
        schema({{"clip", "integer"}, {"path", "string"}, {"in", "number"}}, {"clip", "path"}));
    add("remove_silences",
        "Remove silence from an audio clip (or a video's linked audio), closing gaps across all "
        "tracks; threshold dB (default -40), min_length s (0.5), padding s around speech (0.1); "
        "returns cuts, seconds",
        schema({{"clip", "integer"},
                {"threshold", "number"},
                {"min_length", "number"},
                {"padding", "number"},
                {"spans", "array"}},
               {"clip"}));
    add("auto_duck",
        "Lower music track clip gain during speech on a voice track: amount dB (default -12), "
        "threshold dB (-40), attack and release s (0.25 / 0.5); return spans",
        schema({{"track", "integer"}, {"voice", "integer"}, {"amount", "number"}, {"threshold", "number"},
                {"attack", "number"}, {"release", "number"}},
               {"track", "voice"}));
    add("detect_scenes",
        "Find scene changes in a video clip; split (default true) splits at each one, otherwise add markers; "
        "threshold 0..1 (default 0.15); return times",
        schema({{"clip", "integer"}, {"threshold", "number"}, {"split", "boolean"}, {"times", "array"}}, {"clip"}));
    add("detect_beats",
        "Add a marker at each audio transient (edit to music); sensitivity 0..1 "
        "(default 0.5), min_gap in seconds (default 0.12); return markers",
        schema({{"clip", "integer"}, {"sensitivity", "number"}, {"min_gap", "number"}}, {"clip"}));
    add("import_subtitles", "Import a SubRip (.srt) file as title clips on a new track; return track, clips",
        schema({{"path", "string"}}, {"path"}));
    add("export_subtitles", "Write title clips from a track (otherwise all) as SubRip; return text",
        schema({{"path", "string"}, {"track", "integer"}}));
    add("add_subtitle",
        "Add a subtitle cue using the previous cue's style; stop at the next clip; "
        "returns clip",
        schema({{"track", "integer"}, {"start", "number"}, {"duration", "number"}, {"text", "string"}}, {"track"}));
    add("set_subtitle_style",
        "Apply size (font_size), vertical position (y, -0.5..0.5) and outline to all titles on a track",
        schema({{"track", "integer"}, {"font_size", "number"}, {"y", "number"}, {"outline", "number"},
                {"box", "number"}},
               {"track"}));
    add("normalize_loudness",
        "Measure integrated mix loudness (BS.1770) and set master gain to target LUFS (default -14); "
        "Existing measured/true_peak values skip measurement; limit adds a -1 dBTP master limiter; "
        "returns measured, gain, master, true_peak",
        schema({{"target", "number"}, {"measured", "number"}, {"true_peak", "number"}, {"limit", "boolean"}}));
    add("set_sequence",
        "Change sequence format: even width and height (16..8192), fps (1..240), name",
        schema({{"width", "integer"}, {"height", "integer"}, {"fps", "integer"}, {"name", "string"}}));
    add("fit_clips",
        "Frame video clips with another aspect ratio: fill crops to fill, fit adds letterboxing; "
        "returns scales",
        schema({{"clips", "array"}, {"mode", "string"}}, {"clips"}));
    add("media_status", "Used media (bin and clips): path, online, uses", schema({}));
    add("relink_media",
        "Relink offline media to same-named files in folder (including subfolders); "
        "returns relinked, missing",
        schema({{"folder", "string"}}, {"folder"}));
    add("collect_media",
        "Copy used media into folder and update project paths (archive); return copied, bytes",
        schema({{"folder", "string"}}, {"folder"}));
    add("rename_clips",
        "Rename clips in timeline order: # becomes a zero-padded number, {nom} the "
        "current name; first is the first number (default 1); return names",
        schema({{"clips", "array"}, {"pattern", "string"}, {"first", "integer"}}, {"clips", "pattern"}));
    add("sync_audio",
        "Align a clip (and linked group) to a reference using their audio (envelopes, +/-max_lag s, "
        "default 30); return offset (movement in seconds), confidence 0..1",
        schema({{"clip", "integer"}, {"reference", "integer"}, {"max_lag", "number"}}, {"clip", "reference"}));
    add("apply_animation",
        "Apply an animation preset: fade_in, fade_out, fade_in_out, slide_up, zoom_in, ken_burns "
        "(images), typewriter (titles), ramp_up, ramp_down, ramp_middle (speed ramps); fade/slide duration (0.5 s); "
        "returns clips",
        schema({{"clips", "array"}, {"preset", "string"}, {"duration", "number"}}, {"clips", "preset"}));
    add("paste_attributes",
        "Paste from clip attributes onto clips: effects (append, or replace with replace_effects), "
        "transform (x, y, scale, rotation), opacity, audio (gain, pan), mask; keys follow each clip's start "
        "clip; returns clips",
        schema({{"from", "integer"},
                {"clips", "array"},
                {"effects", "boolean"},
                {"transform", "boolean"},
                {"opacity", "boolean"},
                {"audio", "boolean"},
                {"mask", "boolean"},
                {"replace_effects", "boolean"}},
               {"from", "clips"}));
    add("switch_angle",
        "Multicam: at time (default playhead), split each angle (video tracks with an image, top first) and enable only "
        "the requested angle (1 = topmost) until the next switch; return angles",
        schema({{"angle", "integer"}, {"time", "number"}, {"audio", "boolean"}}, {"angle"}));
    add("auto_grade",
        "Auto grade a video clip at time (default playhead): white balance (gray-world average) and "
        "exposure to 18% middle gray; add or update its correction; return exposure, gains",
        schema({{"clip", "integer"}, {"time", "number"}, {"white_balance", "boolean"}, {"exposure", "boolean"}},
               {"clip"}));
    add("match_grade",
        "Match a video clip (clip) or selection (clips) to a reference clip: average color and "
        "brightness match the reference (white balance and exposure, with a second correction "
        "if needed beyond its range); optional time, reference_time; return clips, exposure, gains, clamped",
        schema({{"clip", "integer"},
                {"clips", "array"},
                {"reference", "integer"},
                {"time", "number"},
                {"reference_time", "number"}},
               {"reference"}));
    add("split_at_markers",
        "Split given clips (or all unlocked tracks) at each marker they cross; "
        "returns cuts",
        schema({{"clips", "array"}}));
    add("close_gaps",
        "Close gaps: on a track (move clips with linked members), or without track, "
        "where all tracks are empty (shorten the sequence); return gaps",
        schema({{"track", "integer"}}));
    add("fit_to_markers",
        "Edit to markers: arrange given clips from the first clip's track in order, starting at the first clip and "
        "each following marker, lasting until the next marker (limited by source duration); return clips",
        schema({{"clips", "array"}}, {"clips"}));
    add("insert_gap",
        "Insert a gap on all tracks: split crossing clips, shift later clips and markers",
        schema({{"time", "number"}, {"duration", "number"}}, {"duration"}));
    add("freeze_frame",
        "Freeze frame: split a video clip at time and insert a held frame for duration; "
        "returns clip",
        schema({{"clip", "integer"}, {"time", "number"}, {"duration", "number"}}, {"clip"}));
    add("trim_clip",
        "Trim a clip and its linked group at an interior time: side start removes the head, end removes the tail; "
        "ripple (default true) closes the gap",
        schema({{"clip", "integer"}, {"time", "number"}, {"side", "string"}, {"ripple", "boolean"}}, {"clip"}));
    add("roll_trim", "Move the cut between adjacent clips without changing total duration",
        schema({{"clip", "integer"}, {"next", "integer"}, {"delta", "number"}}, {"clip", "next", "delta"}));
    add("overlap_transition", "Create an overlap and audio crossfade using available source handles",
        schema({{"clip", "integer"}, {"next", "integer"}, {"duration", "number"}},
               {"clip", "next", "duration"}));
    add("remove_range", "Split and remove a range on all tracks, closing the gap",
        schema({{"begin", "number"}, {"end", "number"}}, {"begin", "end"}));
    add("split_clip", "Split the clip at the specified time",
        schema({{"clip", "integer"}, {"time", "number"}}, {"clip", "time"}));
    add("delete_clip", "Delete a clip and its group; ripple closes its ranges on all tracks",
        schema({{"clip", "integer"}, {"ripple", "boolean"}}, {"clip"}));
    add("select_clips",
        "Select clips and their groups in the interface; an empty list deselects all",
        schema({{"clips", "array"}}, {"clips"}));
    add("move_clips", "Move a selection and its animation by delta seconds, including linked clips",
        schema({{"clips", "array"}, {"delta", "number"}}, {"clips", "delta"}));
    add("delete_clips",
        "Delete a selection; ripple closes the union of its ranges on all tracks without splitting "
        "unselected clips",
        schema({{"clips", "array"}, {"ripple", "boolean"}}, {"clips"}));
    add("duplicate_clips", "Duplicate a selection as a block after its end, preserving links and animation",
        schema({{"clips", "array"}}, {"clips"}));
    add("split_clips", "Split selected clips crossing time, including linked clips",
        schema({{"clips", "array"}, {"time", "number"}}, {"clips", "time"}));
    add("duplicate_clip", "Duplicate a clip and linked group, including animation",
        schema({{"clip", "integer"}}, {"clip"}));
    add("link_clips", "Link two clips or merge their groups for editing operations",
        schema({{"clip", "integer"}, {"other", "integer"}}, {"clip", "other"}));
    add("unlink_clip", "Unlink all clips in the group", schema({{"clip", "integer"}}, {"clip"}));
    add("set_parameter", "Set a clip parameter (opacity/x/y/scale/rotation/gain/pan) or effect parameter",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"key", "string"}, {"value", "number"}},
               {"clip", "key", "value"}));
    add("set_keyframe", "Animation key; interpolation 0 linear, 1 smooth, 2 step, 3 CyDAW curve",
        schema({{"clip", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"value", "number"},
                {"time", "number"},
                {"curve", "integer"},
                {"shape", "number"}},
               {"clip", "key", "value", "time"}));
    add("add_effect", "Add a library HLSL effect",
        schema({{"clip", "integer"}, {"id", "string"}}, {"clip", "id"}));
    add("add_audio_effect", "Add a native CyDAW 2 DSP effect",
        schema({{"clip", "integer"}, {"id", "string"}}, {"clip", "id"}));
    add("set_effect", "Enable an effect and/or reorder it in the video/audio stack (zero-based index)",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"enabled", "boolean"}, {"position", "integer"}},
               {"clip", "effect"}));
    add("remove_effect", "Remove a video/audio effect and its animation",
        schema({{"clip", "integer"}, {"effect", "integer"}}, {"clip", "effect"}));
    add("list_effects", "List available effects and plugins", schema({}));
    add("inspect_ofx", "Describe a float RGBA OFX filter/general plugin in an isolated process",
        schema({{"path", "string"}, {"identifier", "string"}}, {"path"}));
    add("add_ofx_effect", "Add an OFX to a video clip with animatable parameters",
        schema({{"clip", "integer"}, {"path", "string"}, {"identifier", "string"}}, {"clip", "path"}));
    add("set_shader", "Compile and replace an effect shader",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"code", "string"}}, {"clip", "effect", "code"}));
    add("get_graph", "Read an embedded Shaderixel graph",
        schema({{"clip", "integer"}, {"effect", "integer"}}, {"clip", "effect"}));
    add("set_graph", "Compile a Shaderixel graph (one input), native JSON object",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"graph", "object"}},
               {"clip", "effect", "graph"}));
    add("transport", "Play/pause and seek in seconds",
        schema({{"time", "number"}, {"playing", "boolean"}}));
    add("export_video", "Render the sequence as MP4 H264/AAC", schema({{"path", "string"}}, {"path"}));
    add("export_audio", "Render the mix as stereo 48 kHz WAV", schema({{"path", "string"}}, {"path"}));
    add("place_clip",
        "Place video clips as picture-in-picture: position top_left, top_right, bottom_left, bottom_right, center or "
        "full; size (width fraction, default 0.33), margin (0.04); return clips",
        schema({{"clips", "array"}, {"position", "string"}, {"size", "number"}, {"margin", "number"}},
               {"clips", "position"}));
    add("split_screen",
        "Split screen: 2 to 4 video clips, layout side_by_side (left/right), stacked (top/bottom) or grid "
        "(four quadrants); each image fits its cell; return clips",
        schema({{"clips", "array"}, {"layout", "string"}}, {"clips"}));
    add("key_background",
        "Key video clips over a green or blue screen: add a chroma key effect set to color "
        "(green, blue), threshold (default 0.35); return clips",
        schema({{"clips", "array"}, {"color", "string"}, {"threshold", "number"}}, {"clips"}));
    add("audio_cut_fades",
        "Anti-click micro fades at audio track cuts: each boundary receives a fade of "
        "length s (default 0.05) on each side; return cuts",
        schema({{"track", "integer"}, {"length", "number"}}, {"track"}));
    add("add_adjustment_layer",
        "Add an adjustment layer on a new top video track; its effects process the image from tracks "
        "below; return clip, track",
        schema({{"start", "number"}, {"duration", "number"}}));
    add("export_gif",
        "Export a looping animated GIF: range work (work area) or begin/end, width (default 480), fps (15); "
        "returns frames",
        schema({{"path", "string"}, {"range", "string"}, {"begin", "number"}, {"end", "number"}, {"width", "integer"},
                {"fps", "integer"}},
               {"path"}));
    add("export_chapters",
        "Start a chapter export (start/marker to next marker) in folder: kind video (MP4) or "
        "audio (WAV), files named '01 Name'; regions true exports each marker with duration; return jobs",
        schema({{"folder", "string"}, {"kind", "string"}, {"regions", "boolean"}}, {"folder"}));
    add("start_export",
        "Start a background export from a snapshot; kind video (MP4) or audio (WAV); "
        "range work: work area, selection: selected clips' extent, or begin/end in seconds",
        schema({{"path", "string"},
                {"kind", "string"},
                {"preset", "string"},
                {"range", "string"},
                {"begin", "number"},
                {"end", "number"}},
               {"path", "kind"}));
    add("list_export_presets", "Resolution and frame-rate presets for the render queue", schema({}));
    add("prepare_proxy",
        "Prepare an H264 720p preview copy; exports use the original source",
        schema({{"clip", "integer"}, {"path", "string"}}, {"clip"}));
    add("set_preview_proxies", "Enable/disable ready preview proxies",
        schema({{"enabled", "boolean"}}, {"enabled"}));
    add("export_status", "Status of an export, queue and the eight most recent completed renders",
        schema({{"job", "integer"}}));
    add("cancel_export", "Request cancellation before final file replacement; return accepted",
        schema({{"job", "integer"}}, {"job"}));
    add("render_frame", "Renders PNG8, PNG16 or half-float EXR",
        schema({{"path", "string"}, {"time", "number"}, {"format", "string"}}, {"path"}));
    add("undo", "Undo the last change", schema({}));
    add("get_history", "Undoable steps (past, oldest first) and redoable steps (future, "
                       "next first), by name",
        schema({}));
    add("redo", "Redo the change", schema({}));
    return list;
}
Json Protocol::execute(const std::string &name, const Json &a) {
    if (!a.IsNull() && !a.IsObject())
        throw std::runtime_error("Arguments must be a JSON object");
    auto catalogue = tools();
    bool found = false;
    for (auto &tool : catalogue.Elements())
        if (tool["name"].S() == name) {
            found = true;
            auto &s = tool["inputSchema"];
            for (auto &n : s["required"].Elements())
                if (!a.Has(n.S()))
                    throw std::runtime_error("Required argument: " + n.S());
            for (auto &[n, type] : s["properties"].Items())
                if (a.Has(n)) {
                    auto &v = a[n];
                    auto t = type["type"].S();
                    bool valid = t == "string"    ? v.GetType() == Json::Type::String
                                 : t == "boolean" ? v.GetType() == Json::Type::Bool
                                 : t == "object"  ? v.IsObject()
                                 : t == "array"
                                     ? v.IsArray()
                                     : v.GetType() == Json::Type::Number && std::isfinite(v.Num()) &&
                                           (t != "integer" || std::floor(v.Num()) == v.Num());
                    if (!valid)
                        throw std::runtime_error("Invalid type: " + n);
                    if (t == "integer" && (v.Num() < -1 || v.Num() > 2147483647))
                        throw std::runtime_error("Integer out of range: " + n);
                    if (t == "array" && n == "clips") {
                        if (v.Size() > 4096)
                            throw std::runtime_error("4096 clips maximum per selection");
                        for (auto &item : v.Elements())
                            if (item.GetType() != Json::Type::Number || !std::isfinite(item.Num()) ||
                                item.Num() < 1 || item.Num() > 2147483647 ||
                                std::floor(item.Num()) != item.Num())
                                throw std::runtime_error("Invalid selection ID");
                    }
                }
            break;
        }
    if (!found)
        throw std::runtime_error("Unknown tool: " + name);
    auto &s = session;
    s.syncPluginEditors(name == "save_project" || name == "start_export" || name == "export_video" ||
                        name == "export_audio" || name == "open_project" || name == "new_project" ||
                        name == "get_plugin_state");
    auto &p = s.project;
    Project before = p;
    auto previousPath = s.projectPath;
    double previousTime = s.time;
    bool previousPlaying = s.playing, previousDirty = s.dirty;
    Id previousSelected = s.selected;
    auto previousSelection = s.selection;
    bool changed = false;
    // A 720p preview proxy of a medium, exported in the background (path: default proxy location).
    auto startProxy = [&](const std::string &medium, const std::string &label, std::string path) {
        auto &asset = s.media.get(medium);
        if (asset.loading || !asset.video || asset.image || asset.duration <= 0)
            throw std::runtime_error("A loaded video is required to create a proxy");
        if (path.empty())
            path = sx::Narrow(s.media.proxyPath(medium).wstring());
        Project proxy;
        proxy.name = "Proxy · " + label;
        proxy.duration = asset.duration;
        float scale = std::min(1.f, 720.f / asset.frame.height);
        proxy.width = std::max(2, int(asset.frame.width * scale) / 2 * 2);
        proxy.height = std::max(2, int(asset.frame.height * scale) / 2 * 2);
        proxy.fps = p.fps;
        Clip source;
        source.id = proxy.allocate();
        source.source = medium;
        source.duration = asset.duration;
        proxy.addTrack(TrackKind::Video).clips.push_back(source);
        proxy.validate();
        auto job = s.exports.start(proxy, s.renderer.library, ExportKind::Video, path);
        s.proxyJobs.push_back({job, medium, path});
        return job;
    };
    auto clip = [&]() -> Clip & {
        auto *c = p.findClip(a["clip"].U());
        if (!c)
            throw std::runtime_error("Clip not found");
        auto *t = p.owner(c->id);
        if (t->locked && name != "get_graph")
            throw std::runtime_error("Track is locked");
        return *c;
    };
    auto effect = [&]() -> Effect & {
        auto &c = clip();
        int i = a["effect"].I(-1);
        if (i < 0 || i >= int(c.effects.size()))
            throw std::runtime_error("Effect not found");
        return c.effects[i];
    };
    Track *mixTrack = nullptr;
    AudioBus *mixBus = nullptr;
    auto mixChannel = [&]() -> MixChannel & {
        auto scope = a["scope"].S();
        if (scope == "master") {
            if (a["target"].U() != 0)
                throw std::runtime_error("Master uses target 0");
            return p.masterMix;
        }
        bool locked = false;
        MixChannel *channel = nullptr;
        if (scope == "track") {
            mixTrack = p.findTrack(a["target"].U());
            if (!mixTrack || mixTrack->kind != TrackKind::Audio)
                throw std::runtime_error("Audio track not found");
            locked = mixTrack->locked;
            channel = &mixTrack->mix;
        } else if (scope == "bus") {
            mixBus = p.findBus(a["target"].U());
            if (!mixBus)
                throw std::runtime_error("Audio bus not found");
            locked = mixBus->locked;
            channel = &mixBus->mix;
        } else
            throw std::runtime_error("scope attendu : track, bus ou master");
        if (locked && !(name == "set_mix_channel" && a.Has("locked") && a.Size() == 3))
            throw std::runtime_error("Mix channel is locked");
        return *channel;
    };
    Json result = Json::Object();
    result["ok"] = true;
    auto clipIds = [&]() {
        std::vector<Id> ids;
        for (auto &item : a["clips"].Elements())
            ids.push_back(item.U());
        return ids;
    };
    auto resultClips = [&](const std::vector<Id> &ids) {
        result["clips"] = Json::Array();
        for (auto id : ids)
            result["clips"].Push(id);
    };
    try {
        if (name == "export_video" || name == "export_audio")
            for (auto &job : s.exports.list())
                if (job.active())
                    throw std::runtime_error("An export is already running; wait or cancel it");
        if (name == "status") {
            s.tidySelection();
            result["project"] = p.json();
            result["time"] = s.time;
            result["playing"] = s.playing;
            result["dirty"] = s.dirty;
            result["version"] = applicationVersion;
            result["exports"] = Json::Array();
            for (auto &job : s.exports.list())
                result["exports"].Push(job.json());
            result["selected"] = s.selected;
            result["selection"] = Json::Array();
            for (auto id : s.selection)
                result["selection"].Push(id);
            return result;
        } else if (name == "set_motion_widget") {
            if (!a["enabled"].B(true))
                clip().widget = Json();
            else {
                if (!a["layout"].IsObject())
                    throw std::runtime_error("Motion layout required");
                clip().widget = a["layout"];
            }
            changed = true;
        } else if (name == "get_motion_widgets") {
            result["widgets"] = Json::Array();
            double time = a["time"].Num(s.time);
            if (!std::isfinite(time) || time < 0 || time > p.duration)
                throw std::runtime_error("Motion time is outside the sequence");
            for (auto &t : p.tracks)
                for (auto &c : t.clips)
                    if (!c.widget.IsNull()) {
                        auto rect = widgetRect(p, c, time);
                        Json item = Json::Object();
                        item["clip"] = c.id;
                        item["layout"] = c.widget;
                        item["x"] = rect.x;
                        item["y"] = rect.y;
                        item["width"] = rect.width;
                        item["height"] = rect.height;
                        item["visible"] = rect.visible;
                        result["widgets"].Push(item);
                    }
            return result;
        } else if (name == "make_clip_container") {
            p.makeContainer(a["clip"].U());
            changed = true;
        } else if (name == "get_clip_composition") {
            result["project"] = clip().composition;
            return result;
        } else if (name == "set_clip_composition") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video || c.composition.IsNull())
                throw std::runtime_error("A video container is required");
            auto child = Project::fromJson(a["project"]);
            c.composition = child.json();
            changed = true;
        } else if (name == "get_layer_graph") {
            result["graph"] = p.layerGraph.IsNull() ? defaultLayerGraph(p) : p.layerGraph;
            result["explicit"] = !p.layerGraph.IsNull();
            return result;
        } else if (name == "set_layer_graph") {
            decodeLayerGraph(a["graph"], p);
            p.layerGraph = a["graph"];
            changed = true;
        } else if (name == "get_track_graph" || name == "set_track_graph" || name == "add_track_effect") {
            auto *t = p.findTrack(a["track"].U());
            if (!t || t->kind != TrackKind::Video)
                throw std::runtime_error("Video track not found");
            if (name == "get_track_graph") {
                result["graph"] = t->graph.IsNull() ? defaultTrackGraph(p, t->id) : t->graph;
                result["explicit"] = !t->graph.IsNull();
                return result;
            }
            if (name == "set_track_graph") {
                if (!a["graph"].IsNull())
                    decodeTrackGraph(a["graph"], p, t->id);
                t->graph = a["graph"];
            } else {
                auto id = a["id"].S();
                auto def = std::find_if(s.renderer.library.definitions.begin(), s.renderer.library.definitions.end(),
                                        [&](auto &d) { return d.id == id; });
                if (def == s.renderer.library.definitions.end() || def->kind != "effect" || def->generator)
                    throw std::runtime_error("Unknown video effect: " + id);
                auto effect = s.renderer.library.instantiate(id);
                if (effect.id != "grading")
                    effect.code = def->code;
                auto graph = t->graph.IsNull() ? defaultTrackGraph(p, t->id) : t->graph;
                result["node"] = appendTrackEffect(graph, effect, def->name);
                decodeTrackGraph(graph, p, t->id);
                t->graph = graph;
            }
            changed = true;
        } else if (name == "set_layer_parameter" || name == "set_layer_keyframe") {
            if (p.layerGraph.IsNull())
                p.layerGraph = defaultLayerGraph(p);
            if (name == "set_layer_keyframe") {
                if (a["time"].Num() < 0 || a["time"].Num() > p.duration || a["curve"].I() < 0 ||
                    a["curve"].I() > 3)
                    throw std::runtime_error("Composition key is out of range");
                setLayerGraphKey(p.layerGraph, a["node"].U(), a["key"].S(),
                                 {a["time"].Num(), a["value"].F(), Curve(a["curve"].I()), a["shape"].F()});
            } else {
                auto nodes = Json::Array();
                bool found = false;
                for (auto item : p.layerGraph["nodes"].Elements()) {
                    if (item["id"].U() == a["node"].U()) {
                        const auto &read = item;
                        auto param = read["params"].Has(a["key"].S())
                                         ? graphParameter(read["params"][a["key"].S()])
                                         : Parameter{};
                        param.value = a["value"].F();
                        item["params"][a["key"].S()] = parameterJson(param);
                        found = true;
                    }
                    nodes.Push(item);
                }
                if (!found)
                    throw std::runtime_error("Node not found");
                p.layerGraph["nodes"] = nodes;
            }
            decodeLayerGraph(p.layerGraph, p);
            changed = true;
        } else if (name == "add_grade") {
            if (p.owner(clip().id)->kind != TrackKind::Video || clip().effects.size() >= 64)
                throw std::runtime_error("Color grading requires a video clip; 64 stages maximum");
            clip().effects.push_back(makeGrade(a["name"].S("Correction")));
            result["effect"] = int(clip().effects.size() - 1);
            changed = true;
        } else if (name == "video_scopes") {
            double time = a["time"].Num(s.time);
            if (time < 0 || time > p.duration)
                throw std::runtime_error("Scope time is outside the sequence");
            Renderer analysis;
            analysis.initialize(s.renderer.gpu(), s.renderer.gpuContext());
            analysis.library = s.renderer.library;
            float scale = std::min(1.f, std::min(640.f / p.width, 360.f / p.height));
            analysis.render(p, s.media, time, std::max(1u, unsigned(p.width * scale)),
                            std::max(1u, unsigned(p.height * scale)));
            result["scopes"] =
                videoScopes(analysis.readbackFloat(), p.toneMap, p.exposure).json(a["bins"].B());
            return result;
        } else if (name == "list_grade_presets") {
            result["presets"] = Json::Array();
            for (auto title : {"Neutral", "Warm", "Cool", "Black and White", "Cyan shadows / warm highlights"})
                result["presets"].Push(title);
            return result;
        } else if (name == "set_grade" || name == "set_grade_curve" || name == "import_grade_lut" ||
                   name == "neutral_grade" || name == "apply_grade_preset" || name == "save_grade_preset" ||
                   name == "load_grade_preset") {
            auto &e = effect();
            if (e.id != "grading")
                throw std::runtime_error("A color grading correction is required");
            if (name == "set_grade")
                for (auto &[key, value] : a["settings"].Items()) {
                    if (key != "name" && key != "curves" && key != "maskPoints" && key != "lutInput" &&
                        key != "lutOutput" && key != "lut")
                        throw std::runtime_error("Unknown grading data: " + key);
                    e.grading[key] = value;
                }
            else if (name == "set_grade_curve")
                e.grading["curves"][a["curve"].S()] = a["values"];
            else if (name == "neutral_grade")
                neutralGrade(e, {a["r"].F(), a["g"].F(), a["b"].F()});
            else if (name == "apply_grade_preset")
                e = gradePreset(a["name"].S());
            else if (name == "import_grade_lut") {
                e.grading["lut"] = loadLut(sx::Utf8Path(a["path"].S()), a["layout"].S("auto"));
                if (a.Has("input"))
                    e.grading["lutInput"] = a["input"];
                if (a.Has("output"))
                    e.grading["lutOutput"] = a["output"];
            } else if (name == "save_grade_preset") {
                Json document = Json::Object();
                document["format"] = "shaderixframe-grade";
                document["schema"] = 1;
                document["grading"] = e.grading;
                document["enabled"] = e.enabled;
                document["params"] = Json::Object();
                for (auto &[key, parameter] : e.params) {
                    auto &j = document["params"][key];
                    j = Json::Object();
                    j["value"] = parameter.value;
                    j["keys"] = Json::Array();
                    for (auto &k : parameter.keys) {
                        Json item = Json::Array();
                        item.Push(k.time);
                        item.Push(k.value);
                        item.Push(int(k.curve));
                        item.Push(k.shape);
                        j["keys"].Push(item);
                    }
                }
                validateGrade(e);
                AtomicFile file(a["path"].S());
                std::ofstream out(file.path(), std::ios::binary);
                out << document.Dump();
                out.close();
                if (!out)
                    throw std::runtime_error("Unable to write .sgrade");
                file.commit();
                return result;
            } else if (name == "load_grade_preset") {
                auto path = sx::Utf8Path(a["path"].S());
                if (std::filesystem::file_size(path) > 48 * 1024 * 1024)
                    throw std::runtime_error("Preset is too large");
                std::ifstream input(path, std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(input)), {});
                Json doc;
                if (!Json::Parse(text, doc) || doc["format"].S() != "shaderixframe-grade" ||
                    doc["schema"].I() != 1)
                    throw std::runtime_error("Invalid .sgrade file");
                auto replacement = makeGrade();
                replacement.grading = doc["grading"];
                replacement.enabled = doc["enabled"].B(true);
                for (auto &[key, item] : doc["params"].Items()) {
                    Parameter pp{item["value"].F()};
                    for (auto &k : item["keys"].Elements()) {
                        if (k.Size() != 4 || k.At(2).I() < 0 || k.At(2).I() > 3)
                            throw std::runtime_error("Invalid .sgrade key");
                        pp.setKey({k.At(0).Num(), k.At(1).F(), Curve(k.At(2).I()), k.At(3).F()});
                    }
                    replacement.params[key] = pp;
                }
                validateGrade(replacement);
                e = std::move(replacement);
            }
            validateGrade(e);
            s.renderer.validateShader(gradeShader(e, p.workingSpace));
            changed = true;
        } else if (name == "set_color_management") {
            if (a.Has("space"))
                p.workingSpace = a["space"].S();
            if (a.Has("tone"))
                p.toneMap = a["tone"].S();
            if (a.Has("exposure"))
                p.exposure = a["exposure"].F();
            if (a.Has("backgroundAlpha"))
                p.backgroundAlpha = a["backgroundAlpha"].F();
            if (a.Has("imageFormat"))
                p.imageFormat = a["imageFormat"].S();
            changed = true;
        } else if (name == "set_source_color") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video)
                throw std::runtime_error("A video clip is required");
            if (a.Has("space"))
                c.sourceColor = a["space"].S();
            if (a.Has("premultiplied"))
                c.sourcePremultiplied = a["premultiplied"].B();
            changed = true;
        } else if (name == "new_project") {
            Project created;
            created.name = a["name"].S("Untitled");
            created.width = a["width"].I(1280);
            created.height = a["height"].I(720);
            created.fps = a["fps"].I(30);
            created.duration = a["duration"].Num(12);
            created.workingSpace = "linear709";
            if (a.Has("template")) {
                // Format, frame rate and named tracks, top to bottom (V video, A audio).
                struct Template {
                    const char *id, *name;
                    int width, height, fps;
                    double duration;
                    std::vector<std::pair<char, const char *>> tracks;
                };
                static const std::vector<Template> templates = {
                    {"youtube", "YouTube Video", 1920, 1080, 30, 60,
                     {{'V', "Titles"}, {'V', "Video"}, {'A', "Voice"}, {'A', "Music"}}},
                    {"short", "Vertical Video", 1080, 1920, 30, 30,
                     {{'V', "Subtitles"}, {'V', "Video"}, {'A', "Voice"}, {'A', "Music"}}},
                    {"podcast", "Video Podcast", 1920, 1080, 30, 600,
                     {{'V', "Titles"}, {'V', "Camera 2"}, {'V', "Camera 1"}, {'A', "Host"}, {'A', "Guest"}, {'A', "Music"}}},
                    {"cinema", "Short Film", 1920, 1080, 24, 300,
                     {{'V', "Titles"}, {'V', "Image"}, {'A', "Dialogues"}, {'A', "Ambiances"}, {'A', "Music"}}}};
                auto chosen = std::find_if(templates.begin(), templates.end(),
                                           [&](const Template &t) { return a["template"].S() == t.id; });
                if (chosen == templates.end())
                    throw std::runtime_error("Unknown template: youtube, short, podcast or cinema");
                created.name = a["name"].S(chosen->name);
                created.width = chosen->width;
                created.height = chosen->height;
                created.fps = chosen->fps;
                created.duration = a["duration"].Num(chosen->duration);
                for (auto [kind, trackName] : chosen->tracks)
                    created.addTrack(kind == 'V' ? TrackKind::Video : TrackKind::Audio).name = trackName;
            }
            created.validate();
            if (s.dirty)
                s.recovery.preserve(p, s.projectPath, s.time);
            p = std::move(created);
            s.projectPath.clear();
            s.time = 0;
            s.playing = false;
            s.selectClip(0);
            changed = true;
        } else if (name == "open_project") {
            auto opened = Project::load(a["path"].S());
            if (s.dirty)
                s.recovery.preserve(p, s.projectPath, s.time);
            p = std::move(opened);
            s.projectPath = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            s.time = 0;
            s.playing = false;
            s.selectClip(0);
            for (auto &track : p.tracks)
                if (!track.clips.empty()) {
                    s.selectClip(track.clips.front().id);
                    break;
                }
            changed = true;
        } else if (name == "save_project") {
            std::string path = a["path"].S(s.projectPath);
            if (path.empty())
                throw std::runtime_error("Project path required");
            // The file being replaced is kept among the project's last ten versions.
            if (a["versions"].B(true))
                keepVersion(path);
            p.save(path);
            s.projectPath = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(path)).wstring());
            s.dirty = false;
            s.recovery.clear();
        } else if (name == "list_versions") {
            result["versions"] = Json::Array();
            for (auto &[saved, path] : projectVersions(s.projectPath)) {
                Json item = Json::Object();
                item["path"] = sx::Narrow(path.wstring());
                item["saved"] = saved;
                result["versions"].Push(item);
            }
        } else if (name == "restore_version") {
            auto restored = Project::load(a["path"].S());
            p = std::move(restored);
            s.time = std::min(s.time, p.duration);
            s.selectClip(0);
            changed = true;
        } else if (name == "add_track") {
            auto kind = a["kind"].S();
            if (kind != "video" && kind != "audio")
                throw std::runtime_error("Type attendu: video ou audio");
            auto &t = p.addTrack(kind == "audio" ? TrackKind::Audio : TrackKind::Video);
            t.name = a["name"].S(t.name);
            if (t.kind == TrackKind::Video)
                appendLayerSurface(p, t.id, t.name);
            result["track"] = t.id;
            changed = true;
        } else if (name == "set_track") {
            auto *t = p.findTrack(a["track"].U());
            if (!t)
                throw std::runtime_error("Track not found");
            if (t->locked && (a.Has("name") || a.Has("height") || a.Has("muted") || a.Has("solo")) &&
                !(a.Has("locked") && !a["locked"].B()))
                throw std::runtime_error("Track is locked");
            if (a.Has("name")) {
                auto label = a["name"].S();
                if (label.empty() || label.size() > 256)
                    throw std::runtime_error("Invalid track name");
                t->name = label;
            }
            if (a.Has("color"))
                t->color = a["color"].U();
            if ((a.Has("blend") || a.Has("opacity") || a.Has("blend_effect")) && t->kind != TrackKind::Video)
                throw std::runtime_error("Only a video track has a blend mode");
            if (a.Has("blend")) {
                int mode = -1;
                if (a["blend"].GetType() == Json::Type::Number)
                    mode = a["blend"].I();
                else
                    for (int m = 0; m < int(sx::BlendMode::Count); ++m)
                        if (a["blend"].S() == sx::BlendModeName(sx::BlendMode(m)))
                            mode = m;
                if (mode < 0 || mode >= int(sx::BlendMode::Count))
                    throw std::runtime_error("Unknown blend mode");
                t->blend = mode;
                t->blendEffect = Effect{.id = {}};
            }
            if (a.Has("opacity"))
                t->opacity = std::clamp(a["opacity"].F(), 0.f, 1.f);
            if (a.Has("blend_effect")) {
                auto id = a["blend_effect"].S();
                if (id.empty())
                    t->blendEffect = Effect{.id = {}};
                else {
                    auto *def = s.renderer.library.find(id);
                    if (!def || def->kind != "effect" || def->inputs < 2)
                        throw std::runtime_error("Not a blend effect: " + id);
                    t->blendEffect = s.renderer.library.instantiate(id);
                    t->blendEffect.code = def->code;
                    t->blend = 0;
                }
            }
            if (a.Has("height"))
                t->height = a["height"].F();
            if (a.Has("muted"))
                t->muted = a["muted"].B();
            if (a.Has("solo"))
                t->solo = a["solo"].B();
            if (a.Has("locked"))
                t->locked = a["locked"].B();
            if (a.Has("group") && a["group"].U() != t->group) {
                Id joined = a["group"].U(), track = t->id;
                if (joined && !p.findGroup(joined))
                    throw std::runtime_error("Group not found");
                Id left = t->group;
                t->group = joined;
                // A track joining a group goes under its last member.
                if (joined) {
                    int last = -1;
                    for (int i = 0; i < int(p.tracks.size()); ++i)
                        if (p.tracks[i].group == joined && p.tracks[i].id != track)
                            last = i;
                    int current = int(std::find_if(p.tracks.begin(), p.tracks.end(),
                                                   [&](const Track &x) { return x.id == track; }) - p.tracks.begin());
                    if (last >= 0)
                        p.moveTrack(track, last >= current ? last : last + 1);
                }
                if (left && std::none_of(p.tracks.begin(), p.tracks.end(), [&](const Track &x) { return x.group == left; }))
                    std::erase_if(p.groups, [&](const TrackGroup &g) { return g.id == left; });
            }
            changed = true;
        } else if (name == "remove_track") {
            p.removeTrack(a["track"].U());
            changed = true;
        } else if (name == "move_track") {
            p.moveTrack(a["track"].U(), a["index"].I());
            changed = true;
        } else if (name == "add_marker" || name == "set_marker") {
            double at = a["time"].Num();
            if (a.Has("time") && (!std::isfinite(at) || at < 0 || at > p.duration))
                throw std::runtime_error("Marker is outside the sequence");
            Marker *marker = nullptr;
            if (name == "add_marker")
                marker = &p.addMarker(at, a["name"].S());
            else if (!(marker = p.findMarker(a["marker"].U())))
                throw std::runtime_error("Marker not found");
            if (a.Has("name") && name == "set_marker")
                marker->name = a["name"].S();
            if (a.Has("color"))
                marker->color = a["color"].U();
            if (a.Has("note"))
                marker->note = a["note"].S();
            if (a.Has("duration")) {
                double length = a["duration"].Num();
                double start = a.Has("time") ? at : marker->time;
                if (!std::isfinite(length) || length < 0 || start + length > p.duration + 1e-6)
                    throw std::runtime_error("Region duration is outside the sequence");
                marker->duration = length;
            }
            Id id = marker->id;
            if (a.Has("time") && name == "set_marker") {
                marker->time = at;
                std::stable_sort(p.markers.begin(), p.markers.end(),
                                 [](const Marker &l, const Marker &r) { return l.time < r.time; });
            }
            result["marker"] = id;
            changed = true;
        } else if (name == "edit_source") {
            Project::SourceEdit edit;
            edit.path = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            auto &asset = s.media.get(edit.path);
            double length = asset.image ? 6 : asset.duration;
            if (length <= 0)
                throw std::runtime_error("Unknown media duration");
            edit.in = a["in"].Num(0);
            edit.out = a["out"].Num(length);
            if (!asset.image && edit.out > length + 1e-6)
                throw std::runtime_error("Out point is beyond the media end");
            edit.at = a["start"].Num(s.time);
            auto mode = a["mode"].S("overwrite");
            if (mode != "insert" && mode != "overwrite" && mode != "fit")
                throw std::runtime_error("Mode : insert, overwrite ou fit");
            edit.insert = mode == "insert";
            if (mode == "fit") {
                // Fit to fill: the work area, unless a start and a duration are given.
                if (a.Has("duration"))
                    edit.fill = a["duration"].Num();
                else if (p.hasWorkArea()) {
                    edit.at = p.workIn;
                    edit.fill = p.workOut - p.workIn;
                } else
                    throw std::runtime_error("Fit to fill: define a work area (I, O)");
                if (!(edit.fill > 0))
                    throw std::runtime_error("Invalid duration to fill");
            }
            edit.video = asset.video && a["video"].B(true);
            edit.audio = asset.hasAudio && a["audio"].B(true);
            edit.videoTrack = a["video_track"].U();
            edit.audioTrack = a["audio_track"].U();
            auto placed = p.placeSource(edit);
            resultClips(placed);
            if (!placed.empty())
                s.setSelection(placed);
            changed = true;
        } else if (name == "duplicate_track") {
            result["track"] = p.duplicateTrack(a["track"].U()).id;
            changed = true;
        } else if (name == "lift_range") {
            p.liftRange(a["begin"].Num(), std::min(a["end"].Num(), p.duration));
            changed = true;
        } else if (name == "group_tracks") {
            std::vector<Id> ids;
            for (auto &item : a["tracks"].Elements())
                ids.push_back(item.U());
            result["group"] = p.groupTracks(ids, a["name"].S()).id;
            changed = true;
        } else if (name == "set_group") {
            auto *group = p.findGroup(a["group"].U());
            if (!group)
                throw std::runtime_error("Group not found");
            if (a.Has("name")) {
                if (a["name"].S().empty() || a["name"].S().size() > 256)
                    throw std::runtime_error("Invalid group name");
                group->name = a["name"].S();
            }
            if (a.Has("color"))
                group->color = a["color"].U();
            if (a.Has("collapsed"))
                group->collapsed = a["collapsed"].B();
            changed = true;
        } else if (name == "ungroup") {
            p.ungroup(a["group"].U());
            changed = true;
        } else if (name == "export_marker_stills") {
            if (p.markers.empty())
                throw std::runtime_error("No markers");
            auto base = sx::Utf8Path(a["path"].S());
            auto extension = base.extension().wstring();
            bool exr = _wcsicmp(extension.c_str(), L".exr") == 0;
            if (!exr && _wcsicmp(extension.c_str(), L".png") != 0)
                extension = L".png";
            result["files"] = Json::Array();
            int index = 0;
            for (auto &marker : p.markers) {
                // File names keep the marker name, without the characters Windows refuses.
                auto label = sx::Utf8Path(marker.name).wstring();
                for (auto &c : label)
                    if (c < 32 || wcschr(L"<>:\"/\\|?*", c))
                        c = L'_';
                if (label.size() > 60)
                    label.resize(60);
                wchar_t number[16];
                swprintf(number, 16, L"_%02d_", ++index);
                auto file = base.parent_path() / (base.stem().wstring() + number + label + extension);
                s.renderer.render(p, s.media, std::min(marker.time, p.duration), p.width, p.height);
                auto path = sx::Narrow(file.wstring());
                if (exr)
                    writeExr(s.renderer.readbackFloat(), path);
                else
                    writePng(s.renderer.readback(), path);
                result["files"].Push(path);
            }
        } else if (name == "import_edl") {
            std::string text;
            if (!sx::ReadTextFile(a["path"].S(), text))
                throw std::runtime_error("Unable to read the EDL");
            auto events = parseEdl(text, p.fps);
            auto edlPath = sx::Utf8Path(a["path"].S());
            auto folder = a.Has("media_folder") ? sx::Utf8Path(a["media_folder"].S()) : edlPath.parent_path();
            // Files of the media folder by lower-case name and stem, for events without a usable source path.
            std::map<std::wstring, std::filesystem::path> byName;
            std::error_code walk;
            int seen = 0;
            for (std::filesystem::recursive_directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied,
                                                                  walk),
                 end;
                 !walk && it != end && seen < 100000; it.increment(walk), ++seen)
                if (it->is_regular_file(walk))
                    for (auto key : {it->path().filename().wstring(), it->path().stem().wstring()}) {
                        CharLowerBuffW(key.data(), DWORD(key.size()));
                        byName.try_emplace(key, it->path());
                    }
            auto title = text.starts_with("TITLE:") ? text.substr(6, text.find('\n') - 6) : std::string("EDL");
            title.erase(0, title.find_first_not_of(' '));
            std::erase(title, '\r');
            auto &video = p.addTrack(TrackKind::Video);
            video.name = "EDL · " + title;
            Id videoId = video.id, audioId = 0;
            appendLayerSurface(p, videoId, video.name);
            std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            int clips = 0, offline = 0;
            for (auto &event : events) {
                if (event.reel == "BL")
                    continue;
                std::error_code missing;
                std::string source;
                if (!event.sourceFile.empty() && std::filesystem::exists(sx::Utf8Path(event.sourceFile), missing))
                    source = event.sourceFile;
                else {
                    auto key = sx::Utf8Path(event.clipName.empty() ? event.reel : event.clipName).filename().wstring();
                    CharLowerBuffW(key.data(), DWORD(key.size()));
                    if (auto found = byName.find(key); found != byName.end())
                        source = sx::Narrow(found->second.wstring());
                    else {
                        // Kept offline under its best known path: « Relier les médias » finds it later.
                        source = !event.sourceFile.empty() ? event.sourceFile
                                                           : sx::Narrow((folder / sx::Utf8Path(event.clipName.empty()
                                                                                                    ? event.reel
                                                                                                    : event.clipName))
                                                                            .wstring());
                        ++offline;
                    }
                }
                Clip c;
                c.id = p.allocate();
                c.name = event.clipName.empty() ? event.reel : event.clipName;
                c.source = source;
                c.start = event.recordIn;
                c.duration = event.recordOut - event.recordIn;
                c.in = event.sourceIn;
                c.speed = std::clamp(event.speed, .05, 16.);
                bool video = event.channels.find('V') != std::string::npos || event.channels == "B";
                bool audio = event.channels.find('A') != std::string::npos || event.channels == "B";
                Id pictureId = 0;
                if (video) {
                    p.findTrack(videoId)->clips.push_back(c);
                    pictureId = c.id;
                }
                if (audio) {
                    if (!audioId) {
                        auto &sound = p.addTrack(TrackKind::Audio);
                        sound.name = "EDL · audio";
                        audioId = sound.id;
                    }
                    Clip heard = c;
                    heard.id = p.allocate();
                    p.findTrack(audioId)->clips.push_back(heard);
                    if (pictureId)
                        p.linkClips(pictureId, heard.id);
                }
                p.duration = std::max(p.duration, c.start + c.duration);
                ++clips;
            }
            if (!clips)
                throw std::runtime_error("No usable events in this EDL");
            result["clips"] = clips;
            result["offline"] = offline;
            changed = true;
        } else if (name == "export_edl" || name == "export_shot_list") {
            auto text = name == "export_edl" ? p.edl(a["track"].U()) : p.shotListCsv();
            if (a.Has("path")) {
                AtomicFile output(a["path"].S());
                {
                    std::ofstream file(output.path(), std::ios::binary);
                    file << text;
                    if (!file)
                        throw std::runtime_error("Unable to write the file");
                }
                output.commit();
            }
            result["text"] = text;
            if (name == "export_edl") {
                // Event lines open with their three-digit number and a space.
                int events = 0;
                for (size_t at = 0; at + 3 < text.size();) {
                    if (std::isdigit((unsigned char)text[at]) && std::isdigit((unsigned char)text[at + 1]) &&
                        std::isdigit((unsigned char)text[at + 2]) && text[at + 3] == ' ')
                        ++events;
                    auto next = text.find('\n', at);
                    if (next == std::string::npos)
                        break;
                    at = next + 1;
                }
                result["events"] = events;
            }
        } else if (name == "export_markers") {
            auto format = a["format"].S("youtube");
            if (format != "youtube" && format != "csv")
                throw std::runtime_error("format : youtube ou csv");
            auto text = format == "csv" ? p.markerCsv() : p.markerChapters();
            if (a.Has("path")) {
                AtomicFile output(a["path"].S());
                {
                    std::ofstream file(output.path(), std::ios::binary);
                    file << text;
                    if (!file)
                        throw std::runtime_error("Unable to write markers");
                }
                output.commit();
            }
            result["text"] = text;
        } else if (name == "remove_marker") {
            p.removeMarker(a["marker"].U());
            changed = true;
        } else if (name == "set_work_area") {
            double begin = a["begin"].Num(), end = a["end"].Num();
            if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end < 0)
                throw std::runtime_error("Invalid work area");
            p.workIn = std::min(begin, p.duration);
            p.workOut = std::min(end, p.duration);
            if (p.workOut <= p.workIn)
                p.workIn = p.workOut = 0;
            changed = true;
        } else if (name == "add_bus") {
            auto &bus = p.addBus();
            bus.name = a["name"].S(bus.name);
            result["bus"] = bus.id;
            changed = true;
        } else if (name == "remove_bus") {
            p.removeBus(a["bus"].U());
            changed = true;
        } else if (name == "set_mix_channel") {
            auto &channel = mixChannel();
            if (mixTrack || mixBus) {
                auto &gain = mixTrack ? mixTrack->gain : mixBus->gain;
                auto &pan = mixTrack ? mixTrack->pan : mixBus->pan;
                auto &muted = mixTrack ? mixTrack->muted : mixBus->muted;
                auto &locked = mixTrack ? mixTrack->locked : mixBus->locked;
                auto &label = mixTrack ? mixTrack->name : mixBus->name;
                if (a.Has("gain"))
                    gain = a["gain"].F();
                if (a.Has("pan"))
                    pan = a["pan"].F();
                if (a.Has("muted"))
                    muted = a["muted"].B();
                if (a.Has("locked"))
                    locked = a["locked"].B();
                if (a.Has("name"))
                    label = a["name"].S();
                if (a.Has("output"))
                    channel.output = a["output"].U();
                if (a.Has("solo")) {
                    if (!mixTrack)
                        throw std::runtime_error("Solo is available only on source tracks");
                    mixTrack->solo = a["solo"].B();
                }
            } else {
                for (auto key : {"output", "muted", "locked", "name", "solo"})
                    if (a.Has(key))
                        throw std::runtime_error("Setting is not available on master");
                if (a.Has("gain"))
                    p.master = a["gain"].F();
                if (a.Has("pan"))
                    channel.params["pan"].value = a["pan"].F();
            }
            changed = true;
        } else if (name == "set_send" || name == "remove_send") {
            auto &channel = mixChannel();
            auto destination = a["bus"].U();
            if (a["scope"].S() == "master" || !destination || !p.findBus(destination))
                throw std::runtime_error("Sends require a track/bus and an existing destination bus");
            auto it = std::find_if(channel.sends.begin(), channel.sends.end(),
                                   [&](const AudioSend &send) { return send.bus == destination; });
            if (name == "remove_send") {
                if (it == channel.sends.end())
                    throw std::runtime_error("Send not found");
                channel.sends.erase(it);
                channel.params.erase("send." + std::to_string(destination));
            } else {
                if (it == channel.sends.end()) {
                    channel.sends.push_back({destination});
                    it = channel.sends.end() - 1;
                }
                if (a.Has("gain"))
                    it->gain = a["gain"].F();
                if (a.Has("pre_fader"))
                    it->preFader = a["pre_fader"].B();
                if (a.Has("enabled"))
                    it->enabled = a["enabled"].B();
            }
            changed = true;
        } else if (name == "set_dsp_graph" || name == "get_dsp_graph") {
            std::vector<Effect> *chain = nullptr;
            if (a["scope"].S() == "clip") {
                auto &selected = clip();
                if (p.owner(selected.id)->kind != TrackKind::Audio)
                    throw std::runtime_error("Audio clip required");
                chain = &selected.effects;
            } else
                chain = &mixChannel().effects;
            int index = a["effect"].I(-1);
            if (index >= int(chain->size()) || (a.Has("effect") && index < 0))
                throw std::runtime_error("DSP graph not found");
            if (name == "get_dsp_graph") {
                if (index < 0 || (*chain)[index].id != "cydaw.graph")
                    throw std::runtime_error("DSP graph required");
                result["graph"] = (*chain)[index].graph;
            } else {
                decodeDspGraph(a["graph"]);
                Effect item;
                item.id = "cydaw.graph";
                item.graph = a["graph"];
                if (index < 0) {
                    chain->push_back(item);
                    index = int(chain->size() - 1);
                } else {
                    if ((*chain)[index].id != "cydaw.graph")
                        throw std::runtime_error("A standard DSP cannot be replaced by a graph");
                    (*chain)[index] = item;
                }
                result["effect"] = index;
                changed = true;
            }
        } else if (name == "add_external_audio") {
            auto kind = a["kind"].S();
            auto path = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            IsolatedAudio plugin(kind, path);
            Effect item;
            item.id = kind;
            item.code = path;
            auto values = plugin.parameters();
            for (auto &[key, value] : values)
                if (item.params.size() < 512)
                    item.params[key] = {value};
            item.parameterNames = plugin.parameterNames();
            item.pluginState = plugin.state();
            std::vector<Effect> *chain = nullptr;
            if (a["scope"].S() == "clip") {
                auto &selected = clip();
                if (p.owner(selected.id)->kind != TrackKind::Audio)
                    throw std::runtime_error("Audio clip required");
                chain = &selected.effects;
            } else
                chain = &mixChannel().effects;
            chain->push_back(std::move(item));
            result["effect"] = int(chain->size() - 1);
            changed = true;
        } else if (name == "show_plugin_editor") {
            s.pluginEditors.show(p, a, s.time, a["visible"].B(true));
            result["visible"] = a["visible"].B(true);
        } else if (name == "get_plugin_state") {
            const auto &item = pluginEffect(p, a, false);
            result["state"] = item.pluginState;
            result["parameters"] = Json::Object();
            for (auto &[key, value] : item.parameterNames)
                result["parameters"][key] = value;
        } else if (name == "capture_plugin_state" || name == "save_plugin_preset") {
            auto state = s.pluginEditors.snapshot(p, a, s.time);
            result["state"] = state;
            changed = p.json().Dump(0) != before.json().Dump(0);
            if (name == "save_plugin_preset") {
                Json preset = Json::Object();
                preset["format"] = "shaderixframe.vst3-preset";
                preset["schema"] = 1;
                preset["plugin"] = sx::Narrow(sx::Utf8Path(pluginEffect(p, a).code).filename().wstring());
                preset["state"] = state;
                AtomicFile output(a["path"].S());
                {
                    std::ofstream file(output.path(), std::ios::binary);
                    file << preset.Dump(2);
                    file.flush();
                    if (!file)
                        throw std::runtime_error("Preset cannot be read/written");
                }
                output.commit();
            }
        } else if (name == "set_plugin_state" || name == "load_plugin_preset") {
            std::string state = a["state"].S();
            if (name == "load_plugin_preset") {
                Json preset;
                std::string text;
                if (!sx::ReadTextFile(a["path"].S(), text) || !Json::Parse(text, preset) ||
                    preset["format"].S() != "shaderixframe.vst3-preset" || preset["schema"].I() != 1 ||
                    preset["plugin"].S() !=
                        sx::Narrow(sx::Utf8Path(pluginEffect(p, a).code).filename().wstring()))
                    throw std::runtime_error("Preset is incompatible with this VST3");
                state = preset["state"].S();
            }
            s.pluginEditors.restore(p, a, state, s.time);
            changed = true;
        } else if (name == "add_mix_effect" || name == "set_mix_effect" || name == "remove_mix_effect") {
            auto &chain = mixChannel().effects;
            if (name == "add_mix_effect") {
                chain.push_back(instantiateAudioEffect(a["id"].S()));
                result["effect"] = int(chain.size() - 1);
            } else {
                int index = a["effect"].I(-1);
                if (index < 0 || index >= int(chain.size()))
                    throw std::runtime_error("DSP not found");
                if (name == "remove_mix_effect")
                    chain.erase(chain.begin() + index);
                else {
                    int to = a["position"].I(index);
                    if (to < 0 || to >= int(chain.size()))
                        throw std::runtime_error("Invalid DSP position");
                    if (a.Has("enabled"))
                        chain[index].enabled = a["enabled"].B();
                    auto moved = std::move(chain[index]);
                    chain.erase(chain.begin() + index);
                    chain.insert(chain.begin() + to, std::move(moved));
                }
            }
            changed = true;
        } else if (name == "set_mix_parameter" || name == "set_mix_keyframe" ||
                   name == "remove_mix_keyframe") {
            auto &channel = mixChannel();
            auto *params = &channel.params;
            auto key = a["key"].S();
            float fallback = key == "gain" ? (mixTrack ? mixTrack->gain
                                              : mixBus ? mixBus->gain
                                                       : p.master)
                                           : (mixTrack ? mixTrack->pan
                                              : mixBus ? mixBus->pan
                                                       : 0);
            int index = a["effect"].I(-1);
            if (index >= 0) {
                if (index >= int(channel.effects.size()))
                    throw std::runtime_error("DSP not found");
                auto &effect = channel.effects[index];
                params = &effect.params;
                bool known = false;
                if (externalAudio(effect.id) && externalParameter(key) && effect.params.contains(key)) {
                    fallback = effect.params.at(key).value;
                    known = true;
                }
                for (auto &parameter : audioEffect(effect.id)->parameters)
                    if (key == parameter.key) {
                        fallback = parameter.defaultValue;
                        known = true;
                    }
                if (!known)
                    throw std::runtime_error("Unknown DSP parameter");
            } else if (key != "gain" && key != "pan") {
                bool found = false;
                for (auto &send : channel.sends)
                    if (key == "send." + std::to_string(send.bus)) {
                        fallback = send.gain;
                        found = true;
                        break;
                    }
                if (!found)
                    throw std::runtime_error("Unknown channel parameter");
            }
            if (name == "remove_mix_keyframe") {
                auto it = params->find(key);
                if (it != params->end()) {
                    if (!a.Has("time"))
                        params->erase(it);
                    else
                        std::erase_if(it->second.keys, [&](const Keyframe &k) {
                            return std::abs(k.time - a["time"].Num()) < 1e-7;
                        });
                }
            } else {
                auto &parameter = params->try_emplace(key, Parameter{fallback}).first->second;
                if (name == "set_mix_parameter")
                    parameter.value = a["value"].F();
                else {
                    int curve = a["curve"].I();
                    if (a["time"].Num() < 0 || curve < 0 || curve > 3 || std::abs(a["shape"].F()) > 1)
                        throw std::runtime_error("Invalid mix keyframe");
                    if (a.Has("previous_time"))
                        std::erase_if(parameter.keys, [&](const Keyframe &k) {
                            return std::abs(k.time - a["previous_time"].Num()) < 1e-7;
                        });
                    parameter.setKey({a["time"].Num(), a["value"].F(), Curve(curve), a["shape"].F()});
                }
            }
            changed = true;
        } else if (name == "ripple_trim") {
            p.rippleTrim(a["clip"].U(), a["delta"].Num(), a["left"].B());
            changed = true;
        } else if (name == "normalize_audio") {
            // The peak is read from the decoded source over the clip's span; the gain stays on the CyDAW scale.
            float target = float(a["peak"].Num(-1));
            if (!std::isfinite(target) || target > 0 || target < -60)
                throw std::runtime_error("peak must be between -60 and 0 dBFS");
            result["gains"] = Json::Array();
            for (auto id : clipIds()) {
                auto *c = p.findClip(id);
                if (!c || p.owner(id)->kind != TrackKind::Audio || c->source.empty())
                    continue; // the picture of a linked pair is left as it is
                if (auto it = c->params.find("gain"); it != c->params.end() && !it->second.keys.empty())
                    throw std::runtime_error("Gain is animated: remove keys before normalizing");
                auto &asset = s.media.get(c->source);
                if (asset.loading)
                    throw std::runtime_error("Audio media is loading");
                size_t count = asset.samples();
                size_t begin = std::min(count, size_t(std::max(0., c->in) * 48000) * 2);
                size_t end = std::min(count, size_t(std::max(0., c->in + c->duration * c->speed) * 48000) * 2);
                float peak = 0;
                for (size_t n = begin; n < end; ++n)
                    peak = std::max(peak, std::abs(asset.sample(n)));
                if (peak < 1e-6f)
                    throw std::runtime_error("Silent clip: nothing to normalize");
                float gain = std::clamp(target - 20 * std::log10(peak), -60.f, 12.f);
                c->params.try_emplace("gain", Parameter{0}).first->second.value = gain;
                result["gains"].Push(double(gain));
            }
            if (!result["gains"].Size())
                throw std::runtime_error("Choose audio clips");
            changed = true;
        } else if (name == "replace_source") {
            auto path = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            auto &asset = s.media.get(path);
            if (asset.loading)
                throw std::runtime_error("Media is loading");
            if (!asset.error.empty())
                throw std::runtime_error(asset.error);
            auto &target = clip();
            bool video = asset.video || asset.image;
            if (p.owner(target.id)->kind == TrackKind::Video ? !video : !asset.hasAudio)
                throw std::runtime_error("Media has no video or audio for this clip");
            p.replaceSource(target.id, path, a["in"].Num(0), asset.image ? 0 : asset.duration, video, asset.hasAudio);
            if (std::find(p.bin.begin(), p.bin.end(), path) == p.bin.end())
                p.bin.push_back(path);
            changed = true;
        } else if (name == "remove_silences") {
            // The sound of the clip: itself, or the audio member of its linked group.
            auto *c = &clip();
            if (p.owner(c->id)->kind != TrackKind::Audio)
                for (auto member : p.linked(c->id))
                    if (p.owner(member)->kind == TrackKind::Audio) {
                        c = p.findClip(member);
                        break;
                    }
            if (p.owner(c->id)->kind != TrackKind::Audio || c->source.empty())
                throw std::runtime_error("Choose an audio clip or a video clip with linked audio");
            // Spans measured elsewhere (the editor analyses on a worker thread) are applied as given.
            std::vector<std::pair<double, double>> spans;
            if (a.Has("spans")) {
                for (auto &span : a["spans"].Elements())
                    if (span.Size() == 2 && span.At(1).Num() > span.At(0).Num())
                        spans.push_back({std::max(0., span.At(0).Num()), std::min(p.duration, span.At(1).Num())});
                std::sort(spans.begin(), spans.end());
            } else {
                auto &asset = s.media.get(c->source);
                if (asset.loading)
                    throw std::runtime_error("Audio media is loading");
                spans = silenceSpans(*c, asset, float(std::clamp(a["threshold"].Num(-40), -90., -6.)),
                                     std::max(.05, a["min_length"].Num(.5)), std::clamp(a["padding"].Num(.1), 0., 2.),
                                     p.fps);
            }
            // Closed from the last so the earlier ones keep their times.
            double removed = 0;
            for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
                p.removeRange(it->first, it->second);
                removed += it->second - it->first;
            }
            result["cuts"] = int(spans.size());
            result["seconds"] = removed;
            s.time = std::min(s.time, p.duration);
            changed = !spans.empty();
        } else if (name == "auto_duck") {
            auto *music = p.findTrack(a["track"].U());
            auto *voice = p.findTrack(a["voice"].U());
            if (!music || !voice || music == voice || music->kind != TrackKind::Audio || voice->kind != TrackKind::Audio)
                throw std::runtime_error("Choose two audio tracks: music and voice");
            float amount = float(std::clamp(a["amount"].Num(-12), -48., 0.));
            double attack = std::clamp(a["attack"].Num(.25), 0., 5.), release = std::clamp(a["release"].Num(.5), 0., 10.);
            float threshold = float(std::clamp(a["threshold"].Num(-40), -90., -6.));
            // Where the voice speaks: each voice clip minus its silences, in timeline time, merged.
            std::vector<std::pair<double, double>> speech;
            for (auto &vc : voice->clips) {
                if (vc.source.empty() || !vc.enabled)
                    continue;
                auto &asset = s.media.get(vc.source);
                if (asset.loading)
                    throw std::runtime_error("Voice media is loading");
                double end = vc.in + vc.duration * vc.speed, cursor = vc.in;
                auto toTimeline = [&](double source) { return vc.start + (source - vc.in) / vc.speed; };
                for (auto [from, to] : detectSilences(asset, vc.in, end, threshold, .3)) {
                    if (from > cursor)
                        speech.push_back({toTimeline(cursor), toTimeline(from)});
                    cursor = to;
                }
                if (cursor < end)
                    speech.push_back({toTimeline(cursor), toTimeline(end)});
            }
            std::sort(speech.begin(), speech.end());
            std::vector<std::pair<double, double>> merged;
            for (auto span : speech) {
                if (!merged.empty() && span.first - attack <= merged.back().second + release)
                    merged.back().second = std::max(merged.back().second, span.second);
                else
                    merged.push_back(span);
            }
            for (auto &mc : music->clips) {
                auto &gain = mc.params.try_emplace("gain", Parameter{0}).first->second;
                if (!gain.keys.empty())
                    throw std::runtime_error("Gain is already animated on " + mc.name + " : remove its keys before ducking");
                float base = gain.value;
                double from = mc.start, to = mc.start + mc.duration;
                for (auto [s0, s1] : merged) {
                    if (s1 + release < from || s0 - attack > to)
                        continue;
                    // Down before the voice starts, back up after it stops.
                    gain.setKey({std::max(from, s0 - attack), base, Curve::Smooth, 0});
                    gain.setKey({std::clamp(s0, from, to), base + amount, Curve::Smooth, 0});
                    gain.setKey({std::clamp(s1, from, to), base + amount, Curve::Smooth, 0});
                    gain.setKey({std::min(to, s1 + release), base, Curve::Smooth, 0});
                }
            }
            result["spans"] = int(merged.size());
            changed = !merged.empty();
        } else if (name == "detect_scenes") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video || c.source.empty())
                throw std::runtime_error("Choose a video clip with source media");
            result["times"] = Json::Array();
            Id trackId = p.owner(c.id)->id;
            std::string source = c.source;
            double start = c.start, in = c.in, speed = c.speed;
            std::vector<double> times;
            if (a.Has("times")) {
                // Timeline times found elsewhere (the editor's worker thread).
                for (auto &time : a["times"].Elements())
                    times.push_back(std::round(time.Num() * p.fps) / p.fps);
            } else {
                // A private decoder reads the frames in order, away from the preview's own.
                MediaAsset reader;
                reader.open(c.source, {}, false);
                double end = c.in + c.duration * c.speed;
                for (double cut : detectScenes(reader, c.in, end, std::max(1. / p.fps, c.speed / p.fps),
                                               float(std::clamp(a["threshold"].Num(.15), .02, 1.))))
                    times.push_back(std::round((start + (cut - in) / speed) * p.fps) / p.fps);
            }
            for (double t : times)
                result["times"].Push(t);
            if (a["split"].B(true)) {
                // Each cut splits the piece of the clip that holds it (pieces keep the medium and the track).
                for (double t : times) {
                    Id piece = 0;
                    for (auto &candidate : p.findTrack(trackId)->clips)
                        if (candidate.source == source && candidate.start + 1e-5 < t &&
                            candidate.start + candidate.duration - 1e-5 > t)
                            piece = candidate.id;
                    if (piece)
                        p.splitClips({piece}, t);
                }
            } else
                for (size_t i = 0; i < times.size(); ++i)
                    p.addMarker(times[i], "Shot " + std::to_string(i + 2)).color = 0x8c9eff;
            changed = !times.empty();
        } else if (name == "detect_beats") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Audio || c.source.empty())
                throw std::runtime_error("Choose an audio clip");
            auto &asset = s.media.get(c.source);
            if (asset.loading)
                throw std::runtime_error("Audio media is loading");
            double minGap = std::clamp(a["min_gap"].Num(.12), .03, 10.);
            auto onsets = detectOnsets(asset, c.in, c.in + c.duration * c.speed,
                                       float(std::clamp(a["sensitivity"].Num(.5), 0., 1.)), minGap);
            if (p.markers.size() + onsets.size() > 4096)
                throw std::runtime_error("Too many transients: reduce sensitivity");
            result["markers"] = Json::Array();
            int count = 0;
            Id clipId = c.id;
            for (double source : onsets) {
                auto *owner = p.findClip(clipId);
                double time = owner->start + (source - owner->in) / owner->speed;
                if (time < owner->start || time > std::min(p.duration, owner->start + owner->duration))
                    continue;
                auto &marker = p.addMarker(time, "Transient " + std::to_string(++count));
                marker.color = 0xe8c547;
                result["markers"].Push(marker.id);
            }
            changed = count > 0;
        } else if (name == "import_subtitles") {
            std::string text;
            if (!sx::ReadTextFile(a["path"].S(), text))
                throw std::runtime_error("Unable to read the subtitle file");
            auto name = sx::Narrow(sx::Utf8Path(a["path"].S()).stem().wstring());
            Id track = p.importSubtitles(parseSrt(text), name.empty() ? "Subtitles" : name);
            result["track"] = track;
            result["clips"] = int(p.findTrack(track)->clips.size());
            changed = true;
        } else if (name == "export_subtitles") {
            auto text = p.subtitlesSrt(a["track"].U());
            if (text.empty())
                throw std::runtime_error("No title clips to export");
            if (a.Has("path")) {
                AtomicFile output(a["path"].S());
                {
                    std::ofstream file(output.path(), std::ios::binary);
                    file << text;
                    if (!file)
                        throw std::runtime_error("Unable to write subtitles");
                }
                output.commit();
            }
            result["text"] = text;
        } else if (name == "add_subtitle") {
            result["clip"] = p.addSubtitle(a["track"].U(), a["start"].Num(s.time), a["duration"].Num(2),
                                           a["text"].S("Subtitle cue"));
            s.selectClip(result["clip"].U());
            changed = true;
        } else if (name == "set_subtitle_style") {
            std::map<std::string, float> values;
            if (a.Has("font_size"))
                values["font_size"] = std::clamp(float(a["font_size"].Num()), 4.f, 1024.f);
            if (a.Has("y"))
                values["y"] = std::clamp(float(a["y"].Num()), -.5f, .5f);
            if (a.Has("outline"))
                values["text_outline"] = std::clamp(float(a["outline"].Num()), 0.f, 16.f);
            if (a.Has("box"))
                values["text_box"] = std::clamp(float(a["box"].Num()), 0.f, 1.f);
            result["clips"] = p.setSubtitleStyle(a["track"].U(), values);
            changed = true;
        } else if (name == "normalize_loudness") {
            // The whole mix is rendered offline, after the master fader, then the master moves by the difference.
            double target = std::clamp(a["target"].Num(-14), -40., -5.);
            double measured = a["measured"].Num(0), peak = a["true_peak"].Num(0);
            if (!a.Has("measured")) {
                MediaCache offline;
                std::vector<float> block;
                uint64_t total = uint64_t(p.duration * 48000);
                for (uint64_t at = 0; at < total; at += 4096)
                    offline.mix(p, at / 48000., unsigned(std::min<uint64_t>(4096, total - at)), block);
                measured = offline.loudness.integrated();
                peak = offline.loudness.truePeak(true);
            }
            if (!std::isfinite(measured) || measured < -69)
                throw std::runtime_error("Silent sequence: nothing to normalize");
            double gain = target - measured;
            float before = p.master;
            p.master = float(std::clamp(double(p.master) + gain, -60., 12.));
            result["measured"] = measured;
            result["gain"] = double(p.master - before);
            result["master"] = double(p.master);
            result["true_peak"] = peak + double(p.master - before);
            changed = std::abs(p.master - before) > 1e-4f;
            if (a["limit"].B(false)) {
                // A CyDAW limiter last on the master holds the true peak under -1 dBTP.
                auto &chain = p.masterMix.effects;
                auto limiter = std::find_if(chain.begin(), chain.end(), [](const Effect &e) { return e.id == "cydaw.limiter"; });
                if (limiter == chain.end()) {
                    chain.push_back(instantiateAudioEffect("cydaw.limiter"));
                    limiter = chain.end() - 1;
                }
                limiter->enabled = true;
                limiter->params["ceiling"] = Parameter{-1};
                result["limiter"] = true;
                changed = true;
            }
        } else if (name == "set_sequence") {
            int width = int(a["width"].Num(p.width)), height = int(a["height"].Num(p.height));
            int fps = int(a["fps"].Num(p.fps));
            if (width < 16 || height < 16 || width > 8192 || height > 8192 || width % 2 || height % 2)
                throw std::runtime_error("Width and height must be even, between 16 and 8192");
            if (fps < 1 || fps > 240)
                throw std::runtime_error("Frame rate must be between 1 and 240 fps");
            p.width = width;
            p.height = height;
            p.fps = fps;
            if (a.Has("name"))
                p.name = a["name"].S();
            changed = true;
        } else if (name == "fit_clips") {
            auto mode = a["mode"].S("fill");
            if (mode != "fill" && mode != "fit")
                throw std::runtime_error("mode : fill ou fit");
            result["scales"] = Json::Array();
            double sequence = double(p.width) / p.height;
            for (auto id : clipIds()) {
                auto *c = p.findClip(id);
                if (!c || p.owner(id)->kind != TrackKind::Video || c->source.empty() || c->title.enabled)
                    continue;
                auto &asset = s.media.get(c->source);
                if (asset.loading || !asset.frame.width || !asset.frame.height)
                    throw std::runtime_error("Media is loading");
                // The picture is fitted by default; filling scales it by the ratio of the two aspects.
                double ratio = sequence / (double(asset.frame.width) / asset.frame.height);
                float scale = mode == "fill" ? float(std::max(ratio, 1 / ratio)) : 1.f;
                auto &param = c->params["scale"];
                param.keys.clear();
                param.value = scale;
                result["scales"].Push(double(scale));
            }
            if (!result["scales"].Size())
                throw std::runtime_error("Choose video clips with source media");
            changed = true;
        }
        // Three dispatch chains: one chain of every command nests deeper than MSVC allows.
        if (name == "media_status") {
            result["media"] = Json::Array();
            for (auto &path : p.mediaPaths()) {
                Json item = Json::Object();
                item["path"] = path;
                std::error_code missing;
                item["online"] = std::filesystem::exists(sx::Utf8Path(path), missing);
                int uses = 0;
                for (auto &track : p.tracks)
                    for (auto &clip : track.clips)
                        uses += clip.source == path;
                item["uses"] = uses;
                result["media"].Push(item);
            }
        } else if (name == "relink_media") {
            // Files of the folder by lower-case name; the first one found wins.
            std::map<std::wstring, std::filesystem::path> found;
            std::error_code walk;
            int seen = 0;
            for (std::filesystem::recursive_directory_iterator it(sx::Utf8Path(a["folder"].S()),
                                                                  std::filesystem::directory_options::skip_permission_denied, walk),
                 end;
                 it != end && seen < 200000; it.increment(walk), ++seen) {
                if (walk)
                    break;
                if (!it->is_regular_file(walk))
                    continue;
                auto key = it->path().filename().wstring();
                CharLowerBuffW(key.data(), DWORD(key.size()));
                found.try_emplace(key, it->path());
            }
            std::map<std::string, std::string> moves;
            result["missing"] = Json::Array();
            for (auto &path : p.mediaPaths()) {
                std::error_code missing;
                if (std::filesystem::exists(sx::Utf8Path(path), missing))
                    continue;
                auto key = sx::Utf8Path(path).filename().wstring();
                CharLowerBuffW(key.data(), DWORD(key.size()));
                if (auto it = found.find(key); it != found.end())
                    moves[path] = sx::Narrow(it->second.wstring());
                else
                    result["missing"].Push(path);
            }
            result["relinked"] = int(moves.size());
            if (!moves.empty()) {
                p.relinkMedia(moves);
                changed = true;
            }
        } else if (name == "collect_media") {
            auto folder = sx::Utf8Path(a["folder"].S());
            std::filesystem::create_directories(folder);
            std::map<std::string, std::string> moves;
            std::set<std::wstring> taken;
            double bytes = 0;
            for (auto &path : p.mediaPaths()) {
                auto from = sx::Utf8Path(path);
                std::error_code missing;
                if (!std::filesystem::exists(from, missing) ||
                    std::filesystem::equivalent(from.parent_path(), folder, missing))
                    continue;
                // Two media of the same name from different folders get a number.
                auto target = folder / from.filename();
                for (int n = 2; taken.contains(target.wstring()) ||
                                (std::filesystem::exists(target, missing) &&
                                 std::filesystem::file_size(target, missing) != std::filesystem::file_size(from, missing));
                     ++n)
                    target = folder / (from.stem().wstring() + L" (" + std::to_wstring(n) + L")" + from.extension().wstring());
                taken.insert(target.wstring());
                if (!std::filesystem::exists(target, missing))
                    std::filesystem::copy_file(from, target);
                bytes += double(std::filesystem::file_size(target, missing));
                moves[path] = sx::Narrow(target.wstring());
            }
            result["copied"] = int(moves.size());
            result["bytes"] = bytes;
            if (!moves.empty()) {
                p.relinkMedia(moves);
                changed = true;
            }
        } else if (name == "rename_clips") {
            result["names"] = Json::Array();
            for (auto &renamed : p.renameClips(clipIds(), a["pattern"].S(), int(a["first"].Num(1))))
                result["names"].Push(renamed);
            changed = true;
        } else if (name == "sync_audio") {
            // Each side is heard through its own sound: the clip, or the audio member of its linked group.
            auto soundOf = [&](Id id) -> Clip * {
                for (auto member : p.linked(id))
                    if (auto *c = p.findClip(member); c && p.owner(member)->kind == TrackKind::Audio && !c->source.empty())
                        return c;
                auto *c = p.findClip(id);
                return c && !c->source.empty() ? c : nullptr;
            };
            Clip *moved = soundOf(a["clip"].U()), *reference = soundOf(a["reference"].U());
            if (!moved || !reference || moved == reference)
                throw std::runtime_error("Choose two different clips, each with audio");
            auto &referenceAsset = s.media.get(reference->source);
            auto &movedAsset = s.media.get(moved->source);
            if (referenceAsset.loading || movedAsset.loading)
                throw std::runtime_error("Media is loading");
            double confidence = 0;
            double lag = soundOffset(referenceAsset, reference->in, reference->duration * reference->speed, movedAsset,
                                     moved->in, moved->duration * moved->speed,
                                     std::clamp(a["max_lag"].Num(30), 1., 120.), confidence);
            // The moved content at its own time t sits at reference time t + lag: its start follows.
            double target = reference->start + lag / reference->speed;
            double delta = std::round((target - moved->start) * p.fps) / p.fps;
            Id movedId = moved->id;
            if (std::abs(delta) > 1e-9) {
                if (p.findClip(movedId)->start + delta < 0)
                    throw std::runtime_error("Synchronization would place the clip before the sequence start");
                p.moveClips({movedId}, delta);
            }
            result["offset"] = delta;
            result["confidence"] = confidence;
            changed = std::abs(delta) > 1e-9;
        } else if (name == "apply_animation") {
            auto preset = a["preset"].S();
            static const std::set<std::string> presets{"fade_in",   "fade_out",  "fade_in_out", "slide_up",
                                                       "zoom_in",   "ken_burns", "ramp_up",     "ramp_down",
                                                       "ramp_middle", "typewriter"};
            if (!presets.contains(preset))
                throw std::runtime_error("Unknown animation: " + preset);
            int count = 0;
            // A speed ramp goes to the whole linked group, so the picture and its sound stay together.
            std::vector<Id> targets;
            for (auto id : clipIds())
                for (auto member : preset.starts_with("ramp_") ? p.linked(id) : std::vector<Id>{id})
                    if (std::find(targets.begin(), targets.end(), member) == targets.end())
                        targets.push_back(member);
            for (auto id : targets) {
                auto *c = p.findClip(id);
                if (!c || p.owner(id)->locked)
                    continue;
                bool video = p.owner(id)->kind == TrackKind::Video;
                if ((!video && !preset.starts_with("ramp_")) || (preset == "typewriter" && !c->title.enabled))
                    continue;
                double begin = c->start, end = c->start + c->duration;
                double d = std::clamp(a["duration"].Num(.5), 1. / p.fps, c->duration / 2);
                // Fades add to the keys already there; the other presets replace the curves they set.
                auto curve = [&](const std::string &key, std::initializer_list<std::pair<double, float>> points,
                                 bool replace) {
                    auto &param = c->params.try_emplace(key, Parameter{key == "opacity" || key == "scale" ? 1.f : 0.f})
                                      .first->second;
                    if (replace)
                        param.keys.clear();
                    for (auto [time, value] : points)
                        param.setKey({std::round(time * p.fps) / p.fps, value, Curve::Smooth, 0});
                };
                float opaque = c->params.contains("opacity") && c->params["opacity"].keys.empty()
                                   ? c->params["opacity"].value
                                   : 1.f;
                if (preset == "fade_in" || preset == "fade_in_out")
                    curve("opacity", {{begin, 0.f}, {begin + d, opaque}}, false);
                if (preset == "fade_out" || preset == "fade_in_out")
                    curve("opacity", {{end - d, opaque}, {end, 0.f}}, false);
                if (preset == "slide_up") {
                    curve("y", {{begin, .12f}, {begin + d, 0.f}}, true);
                    curve("opacity", {{begin, 0.f}, {begin + d, opaque}}, false);
                }
                if (preset == "zoom_in")
                    curve("scale", {{begin, 1.f}, {end, 1.15f}}, true);
                if (preset == "ken_burns") {
                    curve("scale", {{begin, 1.f}, {end, 1.18f}}, true);
                    curve("x", {{begin, 0.f}, {end, -.04f}}, true);
                    curve("y", {{begin, 0.f}, {end, -.025f}}, true);
                }
                if (preset == "typewriter") {
                    // About 16 characters a second, at most over 60 % of the clip.
                    double writing = std::min(c->duration * .6, std::max(.2, double(c->title.text.size()) / 16));
                    auto &reveal = c->params["text_reveal"];
                    reveal = Parameter{1};
                    reveal.setKey({begin, 0, Curve::Linear, 0});
                    reveal.setKey({begin + writing, 1, Curve::Linear, 0});
                }
                if (preset == "ramp_up")
                    curve("speed_ramp", {{begin, 1.f}, {end, 3.f}}, true);
                if (preset == "ramp_down")
                    curve("speed_ramp", {{begin, 1.f}, {end, .25f}}, true);
                if (preset == "ramp_middle")
                    curve("speed_ramp", {{begin, 1.f}, {begin + c->duration * .35, .3f}, {begin + c->duration * .65, .3f},
                                         {end, 1.f}},
                          true);
                ++count;
            }
            if (!count)
                throw std::runtime_error("No clips can receive this animation");
            result["clips"] = count;
            changed = true;
        } else if (name == "paste_attributes") {
            auto *from = p.findClip(a["from"].U());
            if (!from)
                throw std::runtime_error("Source clip not found");
            AttributeChoice choice;
            choice.effects = a["effects"].B(true);
            choice.transform = a["transform"].B(true);
            choice.opacity = a["opacity"].B(true);
            choice.audio = a["audio"].B(true);
            choice.mask = a["mask"].B(false);
            choice.replaceEffects = a["replace_effects"].B(false);
            Clip source = *from;
            result["clips"] = p.pasteAttributes(source, p.owner(source.id)->kind, clipIds(), choice);
            changed = true;
        } else if (name == "switch_angle") {
            double time = std::round(a["time"].Num(s.time) * p.fps) / p.fps;
            p.switchAngle(time, int(a["angle"].Num(1)), a["audio"].B(false));
            result["angles"] = int(p.anglesAt(time).size());
            changed = true;
        } else if (name == "auto_grade" || name == "match_grade") {
            // A clip alone, without its effects or transform, measured small: its mean colour in linear Rec.709.
            auto measure = [&](const Clip &c, double at) {
                if (p.owner(c.id)->kind != TrackKind::Video || c.title.enabled || c.adjustment)
                    throw std::runtime_error("Choose video clips");
                double time = std::clamp(at, c.start, c.start + c.duration - 1e-4);
                Project probe;
                probe.width = 160;
                probe.height = std::max(2, int(std::lround(160. * p.height / p.width)) / 2 * 2);
                probe.fps = p.fps;
                probe.duration = p.duration;
                probe.workingSpace = p.workingSpace;
                Clip bare = c;
                bare.id = probe.allocate();
                bare.link = 0;
                bare.effects.clear();
                bare.transition.enabled = false;
                bare.fadeIn = bare.fadeOut = 0;
                for (auto key : {"x", "y", "scale", "rotation", "opacity"})
                    bare.params.erase(key);
                probe.addTrack(TrackKind::Video).clips = {bare};
                s.renderer.clearHistory();
                s.renderer.render(probe, s.media, time, unsigned(probe.width), unsigned(probe.height));
                auto measured = s.renderer.readbackFloat();
                s.renderer.clearHistory();
                double sum[3] = {0, 0, 0}, weight = 0;
                for (size_t i = 0; i + 3 < measured.rgba.size(); i += 4) {
                    // Premultiplied: weighting by alpha leaves the letterbox out.
                    for (int k = 0; k < 3; ++k)
                        sum[k] += measured.rgba[i + size_t(k)];
                    weight += measured.rgba[i + 3];
                }
                if (weight < 1)
                    throw std::runtime_error("Empty image: nothing to measure");
                return convertColor({float(sum[0] / weight), float(sum[1] / weight), float(sum[2] / weight)},
                                    p.workingSpace, "linear709");
            };
            auto lumaOf = [](ColorRGB c) { return c.x * .2126f + c.y * .7152f + c.z * .0722f; };
            // One clip (clip), or a selection matched to one reference (clips): one undo step either way.
            std::vector<Id> targets = name == "match_grade" && a.Has("clips") ? clipIds() : std::vector<Id>{clip().id};
            Id referenceId = a["reference"].U();
            ColorRGB reference{};
            if (name == "match_grade") {
                auto *other = p.findClip(referenceId);
                if (!other)
                    throw std::runtime_error("Choose a reference clip");
                reference = measure(*other, a["reference_time"].Num(other->start + other->duration / 2));
                if (lumaOf(reference) <= 1e-5f)
                    throw std::runtime_error("Reference is too dark to measure");
            }
            bool clamped = false;
            int graded = 0;
            for (auto id : targets) {
                auto *found = p.findClip(id);
                if (!found || id == referenceId || p.owner(id)->locked)
                    continue;
                auto &c = *found;
                // A single clip is measured at the time given (the playhead); a selection at each clip's middle.
                auto mean = measure(c, targets.size() == 1 ? a["time"].Num(s.time) : c.start + c.duration / 2);
                float luma = lumaOf(mean);
                if (luma <= 1e-5f || std::min({mean.x, mean.y, mean.z}) <= 1e-5f)
                    throw std::runtime_error("Image is too dark to measure: " + c.name);
                // The clip's first correction is set, or one is added.
                auto first = size_t(std::find_if(c.effects.begin(), c.effects.end(),
                                                 [](const Effect &e) { return e.id == "grading"; }) -
                                    c.effects.begin());
                if (first == c.effects.size())
                    c.effects.push_back(makeGrade(name == "match_grade" ? "Matched" : "Auto"));
                if (name == "auto_grade") {
                    auto &grade = c.effects[first];
                    if (a["white_balance"].B(true))
                        neutralGrade(grade, mean);
                    if (a["exposure"].B(true))
                        grade.params["exposure"] = Parameter{std::clamp(float(std::log2(.18 / luma)), -4.f, 4.f)};
                } else {
                    // Balance gains carry the reference's tint at the clip's own level, the exposure its level:
                    // together they move the clip's mean colour onto the reference's.
                    float target = lumaOf(reference);
                    float gains[3] = {reference.x / mean.x * luma / target, reference.y / mean.y * luma / target,
                                      reference.z / mean.z * luma / target};
                    float within[3], beyond[3];
                    bool more = false;
                    for (int k = 0; k < 3; ++k) {
                        within[k] = std::clamp(gains[k], .125f, 8.f);
                        beyond[k] = gains[k] / within[k];
                        more = more || std::abs(beyond[k] - 1) > 1e-4f;
                    }
                    // Past the balance's range, a correction right after the first one carries the rest.
                    bool following = first + 1 < c.effects.size() && c.effects[first + 1].id == "grading" &&
                                     c.effects[first + 1].grading["name"].S() == "Matched (continued)";
                    if (more && !following)
                        c.effects.insert(c.effects.begin() + long(first) + 1, makeGrade("Matched (continued)"));
                    const char *keys[] = {"wb_r", "wb_g", "wb_b"};
                    auto &grade = c.effects[first];
                    for (int k = 0; k < 3; ++k)
                        grade.params[keys[k]] = Parameter{within[k]};
                    grade.params["temperature"] = Parameter{0};
                    grade.params["tint"] = Parameter{0};
                    grade.params["exposure"] = Parameter{std::clamp(float(std::log2(target / luma)), -4.f, 4.f)};
                    if (more || following) {
                        auto &rest = c.effects[first + 1];
                        for (int k = 0; k < 3; ++k) {
                            clamped = clamped || beyond[k] < .125f || beyond[k] > 8.f;
                            rest.params[keys[k]] = Parameter{std::clamp(beyond[k], .125f, 8.f)};
                        }
                        rest.params["temperature"] = Parameter{0};
                        rest.params["tint"] = Parameter{0};
                        rest.params["exposure"] = Parameter{0};
                    }
                }
                if (graded++ == 0) {
                    auto &grade = c.effects[first];
                    result["exposure"] = double(grade.params["exposure"].value);
                    result["gains"] = Json::Array();
                    for (auto key : {"wb_r", "wb_g", "wb_b"})
                        result["gains"].Push(double(grade.params[key].value));
                }
            }
            if (!graded)
                throw std::runtime_error("No video clips to grade");
            result["clips"] = graded;
            result["clamped"] = clamped;
            changed = true;
        } else if (name == "split_at_markers") {
            std::vector<Id> ids;
            if (a.Has("clips"))
                ids = clipIds();
            int cuts = p.splitAtMarkers(ids);
            result["cuts"] = cuts;
            changed = cuts > 0;
        } else if (name == "close_gaps") {
            int gaps = p.closeGaps(a["track"].U());
            result["gaps"] = gaps;
            changed = gaps > 0;
        } else if (name == "fit_to_markers") {
            // A video or sound lasts what its source holds after « in »; stills, titles and holds have no limit.
            auto available = [&](const Clip &clip) {
                double unlimited = std::numeric_limits<double>::infinity();
                if (clip.source.empty() || clip.generator || clip.hold || !clip.composition.IsNull())
                    return unlimited;
                auto &asset = s.media.get(clip.source);
                if (asset.loading || !asset.error.empty() || asset.image || asset.duration <= 0)
                    return unlimited;
                return std::max(0., asset.duration - clip.in) / std::max(1e-6, std::abs(clip.speed));
            };
            result["clips"] = p.fitToMarkers(clipIds(), available);
            changed = true;
        } else if (name == "insert_gap") {
            p.insertGap(a["time"].Num(s.time), a["duration"].Num());
            changed = true;
        } else if (name == "freeze_frame") {
            result["clip"] = p.freezeFrame(a["clip"].U(), a["time"].Num(s.time), a["duration"].Num(2));
            changed = true;
        } else if (name == "trim_clip") {
            auto side = a["side"].S("start");
            if (side != "start" && side != "end")
                throw std::runtime_error("side : start ou end");
            p.trimClip(a["clip"].U(), a["time"].Num(s.time), side == "start", a["ripple"].B(true));
            changed = true;
        } else if (name == "roll_trim") {
            p.rollTrim(a["clip"].U(), a["next"].U(), a["delta"].Num());
            changed = true;
        } else if (name == "overlap_transition") {
            p.overlapTransition(a["clip"].U(), a["next"].U(), a["duration"].Num());
            changed = true;
        } else if (name == "remove_range") {
            p.removeRange(a["begin"].Num(), a["end"].Num());
            changed = true;
        } else if (name == "get_audio_graph") {
            auto graph = audioGraph(p);
            auto &j = result["graph"];
            j = Json::Object();
            j["id"] = graph.reference.id;
            j["version"] = graph.reference.version.toString();
            j["schema"] = graph.schemaVersion;
            auto ports = [](const auto &source) {
                auto array = Json::Array();
                for (auto &port : source) {
                    auto item = Json::Object();
                    item["id"] = double(port.id);
                    item["key"] = port.key;
                    item["direction"] =
                        port.direction == cydaw::graph::PortDirection::input ? "input" : "output";
                    item["type"] = "audio";
                    item["layout"] = "stereo";
                    item["channels"] = 2;
                    item["multiple"] = port.acceptsMultipleConnections;
                    array.Push(item);
                }
                return array;
            };
            j["ports"] = ports(graph.ports);
            j["nodes"] = Json::Array();
            for (auto &node : graph.nodes) {
                Json item = Json::Object();
                item["id"] = double(node.id);
                item["name"] = node.displayName;
                item["processor"] = node.processorKey;
                item["ports"] = ports(node.ports);
                j["nodes"].Push(item);
            }
            j["connections"] = Json::Array();
            for (auto &wire : graph.connections) {
                Json item = Json::Object();
                item["source"] = double(wire.source.nodeId);
                item["sourcePort"] = double(wire.source.portId);
                item["destination"] = double(wire.destination.nodeId);
                item["destinationPort"] = double(wire.destination.portId);
                item["channels"] = 2;
                j["connections"].Push(item);
            }
        } else if (name == "add_generator") {
            auto &t = p.addTrack(TrackKind::Video);
            t.name = "HLSL Light";
            Clip c;
            c.id = p.allocate();
            c.generator = true;
            c.name = "Aurora · HLSL";
            if (a.Has("id")) {
                auto *d = s.renderer.library.find(a["id"].S());
                if (!d || !d->generator)
                    throw std::runtime_error("A library HLSL generator is required");
                c.effects = {s.renderer.library.instantiate(d->id)};
                c.name = t.name = d->name;
            }
            c.start = a["start"].Num(s.time);
            c.duration = a["duration"].Num(6);
            t.clips.push_back(c);
            appendLayerSurface(p, t.id, t.name);
            std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            p.duration = std::max(p.duration, c.start + c.duration);
            s.selectClip(c.id);
            result["clip"] = c.id;
            changed = true;
        } else if (name == "add_title" || name == "add_lower_third") {
            Clip c;
            c.id = p.allocate();
            c.title.enabled = true;
            c.start = std::max(0., a["start"].Num(s.time));
            c.duration = std::max(1. / p.fps, a["duration"].Num(5));
            for (auto key : {"text_r", "text_g", "text_b", "text_a"})
                c.params[key] = {1};
            if (name == "add_title") {
                c.title.text = a["text"].S("Your title");
                c.name = "Title";
                c.params["font_size"] = {48};
            } else {
                // Lower third: name and role, left aligned low in the frame on a dark box, sliding in and fading out.
                auto who = a["name"].S("First Last"), role = a["role"].S();
                c.title.text = role.empty() ? who : who + "\n" + role;
                c.title.alignment = 0;
                c.name = "Lower third · " + who;
                c.params["font_size"] = {38};
                c.params["text_box"] = {.72f};
                c.params["text_box_padding"] = {14};
                c.params["y"] = {.36f};
                double d = std::min(.5, c.duration / 4);
                auto &x = c.params["x"];
                x.setKey({c.start, -.06f, Curve::Smooth, 0});
                x.setKey({c.start + d, 0, Curve::Smooth, 0});
                auto &opacity = c.params["opacity"];
                opacity = Parameter{1};
                for (auto [time, value] : {std::pair{c.start, 0.f}, {c.start + d, 1.f}, {c.start + c.duration - d, 1.f},
                                           {c.start + c.duration, 0.f}})
                    opacity.setKey({time, value, Curve::Smooth, 0});
            }
            Id clipId = c.id;
            result["track"] = placeTitle(p, std::move(c), a["new_track"].B(false));
            s.selectClip(clipId);
            result["clip"] = clipId;
            changed = true;
        } else if (name == "apply_title_style") {
            auto style = a["style"].S();
            if (style != "classic" && style != "subtitle_yellow" && style != "boxed" && style != "cinema" &&
                style != "neon")
                throw std::runtime_error("Unknown style: " + style);
            int count = 0;
            for (auto id : clipIds()) {
                auto *c = p.findClip(id);
                if (!c || !c->title.enabled || p.owner(id)->locked)
                    continue;
                auto set = [&](const char *key, float value) { c->params[key] = Parameter{value}; };
                // Every style sets the whole look, so styles replace one another cleanly.
                for (auto key : {"text_outline", "text_shadow", "text_shadow_blur", "text_box", "text_shadow_r",
                                 "text_shadow_g", "text_shadow_b", "text_outline_r", "text_outline_g", "text_outline_b"})
                    set(key, 0);
                set("text_r", 1), set("text_g", 1), set("text_b", 1), set("text_a", 1);
                set("text_shadow_x", 4), set("text_shadow_y", 4);
                c->title.font = "Segoe UI";
                c->title.italic = false;
                if (style == "classic") {
                    c->title.bold = true;
                    set("text_outline", 2), set("text_shadow", .6f), set("text_shadow_blur", 4);
                } else if (style == "subtitle_yellow") {
                    c->title.bold = false;
                    set("text_g", .9f), set("text_b", .2f), set("text_outline", 3);
                } else if (style == "boxed") {
                    c->title.bold = false;
                    set("text_box", .8f), set("text_box_padding", 12);
                } else if (style == "neon") {
                    // A coloured glow: a cyan outline and a centred, blurred cyan shadow around white letters.
                    c->title.bold = true;
                    set("text_outline", 1.5f), set("text_outline_r", .2f), set("text_outline_g", 1), set("text_outline_b", 1);
                    set("text_shadow", 1), set("text_shadow_blur", 14), set("text_shadow_x", 0), set("text_shadow_y", 0);
                    set("text_shadow_r", 0), set("text_shadow_g", .9f), set("text_shadow_b", 1);
                } else {
                    c->title.font = "Georgia";
                    c->title.bold = false;
                    set("text_shadow", .7f), set("text_shadow_blur", 10), set("text_shadow_x", 0), set("text_shadow_y", 3);
                }
                ++count;
            }
            if (!count)
                throw std::runtime_error("Choose title clips");
            result["clips"] = count;
            changed = true;
        } else if (name == "enhance_voice") {
            int count = 0;
            for (auto id : clipIds()) {
                auto *c = p.findClip(id);
                if (!c || p.owner(id)->kind != TrackKind::Audio || p.owner(id)->locked)
                    continue;
                // Gentle gate under the speech, less rumble and more presence, then an even level.
                auto gate = instantiateAudioEffect("cydaw.gate");
                gate.params["threshold"].value = -45;
                gate.params["range"].value = -20;
                gate.params["release"].value = 150;
                auto eq = instantiateAudioEffect("cydaw.eq-three");
                eq.params["low"].value = -6;
                eq.params["low-frequency"].value = 120;
                eq.params["high"].value = 3;
                eq.params["high-frequency"].value = 4000;
                auto compressor = instantiateAudioEffect("cydaw.comp-pro");
                compressor.params["threshold"].value = -22;
                compressor.params["ratio"].value = 3;
                compressor.params["attack"].value = 8;
                compressor.params["release"].value = 140;
                compressor.params["makeup"].value = 4;
                compressor.params["sidechain"].value = 100;
                for (auto &effect : {gate, eq, compressor})
                    c->effects.push_back(effect);
                ++count;
            }
            if (!count)
                throw std::runtime_error("Choose audio clips");
            result["clips"] = count;
            changed = true;
        } else if (name == "set_title") {
            auto &title = clip().title;
            if (!title.enabled)
                throw std::runtime_error("Title clip required");
            if (a.Has("text"))
                title.text = a["text"].S();
            if (a.Has("font"))
                title.font = a["font"].S();
            if (a.Has("alignment"))
                title.alignment = a["alignment"].I();
            if (a.Has("bold"))
                title.bold = a["bold"].B();
            if (a.Has("italic"))
                title.italic = a["italic"].B();
            changed = true;
        } else if (name == "set_transition" || name == "remove_transition" ||
                   name == "set_transition_shader") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video)
                throw std::runtime_error("A transition requires a video track");
            if (name == "remove_transition") {
                c.transition = {};
                std::erase_if(c.params,
                              [](const auto &entry) { return entry.first.starts_with("transition_"); });
            } else {
                if (name == "set_transition_shader" || a.Has("id")) {
                    std::string code, id;
                    if (name == "set_transition_shader") {
                        code = a["code"].S();
                        id = "transition.custom";
                    } else {
                        auto *definition = s.renderer.library.find(a["id"].S());
                        if (!definition || definition->kind != "transition")
                            throw std::runtime_error("Transition shader not found");
                        code = definition->code;
                        id = definition->id;
                    }
                    validateTransitionParameters(code);
                    s.renderer.validateShader(code);
                    c.transition.id = id;
                    c.transition.code = code;
                    c.transition.duration = std::min(c.transition.duration, c.duration);
                    c.transition.enabled = true;
                    for (auto &parameter : effectParameters(code)) {
                        int count = parameter.type == "float2"                                ? 2
                                    : parameter.type == "float3"                              ? 3
                                    : parameter.type == "float4" || parameter.type == "color" ? 4
                                                                                              : 1;
                        for (int index = 0; index < count; ++index)
                            c.params.try_emplace(parameter.key +
                                                     (count > 1 ? "." + std::to_string(index) : ""),
                                                 Parameter{parameter.value[index]});
                    }
                }
                if (a.Has("duration"))
                    c.transition.duration = a["duration"].Num();
                if (a.Has("enabled")) {
                    c.transition.enabled = a["enabled"].B();
                    if (c.transition.enabled && !a.Has("duration"))
                        c.transition.duration = std::min(c.transition.duration, c.duration);
                }
                if (a["reset_progress"].B())
                    c.params.erase("transition_progress");
                if (c.transition.enabled) {
                    auto code = s.renderer.library.transitionCode(c.transition);
                    s.renderer.validateShader(code);
                    c.transition.code = std::move(code);
                }
            }
            changed = true;
        } else if (name == "start_mask_tracking") {
            result["job"] = s.tracking.start(p, a["clip"].U(), a["begin"].Num(), a["end"].Num(),
                                             a["radius"].I(24), a["effect"].I(-1));
        } else if (name == "mask_tracking_status") {
            result["tracking"] = s.tracking.status();
        } else if (name == "cancel_mask_tracking") {
            result["accepted"] = s.tracking.cancel();
        } else if (name == "apply_mask_tracking") {
            s.tracking.apply(p, a["job"].U());
            changed = true;
        } else if (name == "set_bezier_mask") {
            auto &selected = clip();
            if (p.owner(selected.id)->kind != TrackKind::Video)
                throw std::runtime_error("A video clip is required");
            selected.mask.points.clear();
            for (auto &point : a["points"].Elements()) {
                if (!point.IsArray() || point.Size() != 6)
                    throw std::runtime_error("Bezier point requires six coordinates");
                for (auto &value : point.Elements())
                    if (value.GetType() != Json::Type::Number)
                        throw std::runtime_error("Numeric Bezier coordinate required");
                selected.mask.points.push_back({point.At(0).F(), point.At(1).F(), point.At(2).F(),
                                                point.At(3).F(), point.At(4).F(), point.At(5).F()});
            }
            selected.mask.shape = MaskShape::Bezier;
            changed = true;
        } else if (name == "set_mask") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video)
                throw std::runtime_error("A video track is required");
            if (a.Has("shape")) {
                auto shape = a["shape"].S();
                if (shape != "none" && shape != "rectangle" && shape != "ellipse")
                    throw std::runtime_error("Expected shape: none, rectangle, ellipse");
                c.mask.shape = shape == "none"      ? MaskShape::None
                               : shape == "ellipse" ? MaskShape::Ellipse
                                                    : MaskShape::Rectangle;
            }
            if (a.Has("inverted"))
                c.mask.inverted = a["inverted"].B();
            changed = true;
        } else if (name == "import_media") {
            auto path = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            auto &asset = s.media.get(path);
            double start = a["start"].Num(s.time);
            double duration = asset.image ? 6 : asset.duration;
            if (duration <= 0)
                throw std::runtime_error("Unknown media duration");
            result["clips"] = Json::Array();
            auto add = [&](TrackKind k) {
                auto &t = p.addTrack(k);
                Clip c;
                c.id = p.allocate();
                c.name = sx::Narrow(sx::Utf8Path(path).filename().wstring());
                c.source = path;
                c.start = start;
                c.duration = duration;
                t.name = c.name;
                t.clips.push_back(c);
                result["clips"].Push(c.id);
                if (k == TrackKind::Video)
                    std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            };
            if (asset.video)
                add(TrackKind::Video);
            if (asset.hasAudio)
                add(TrackKind::Audio);
            if (result["clips"].Size())
                s.selectClip(result["clips"].At(0).U());
            if (result["clips"].Size() == 2)
                p.linkClips(result["clips"].At(0).U(), result["clips"].At(1).U());
            p.duration = std::max(p.duration, start + duration);
            if (std::find(p.bin.begin(), p.bin.end(), path) == p.bin.end())
                p.bin.push_back(path);
            // A heavy video (beyond 1440 lines) gets its 720p preview proxy at once, unless one is ready.
            // Without a proxy folder (outside the interactive preview) the import simply goes without one.
            if (a["proxy"].B(false) && asset.video && !asset.image && asset.frame.height > 1440) {
                std::filesystem::path proxyFile;
                try {
                    proxyFile = s.media.proxyPath(path);
                } catch (const std::exception &) {
                }
                std::error_code missing;
                if (!proxyFile.empty() && !std::filesystem::exists(proxyFile, missing))
                    result["proxy_job"] = startProxy(path, sx::Narrow(sx::Utf8Path(path).filename().wstring()), {});
            }
            changed = true;
        } else if (name == "remove_unused_media") {
            result["removed"] = Json::Array();
            std::erase_if(p.bin, [&](const std::string &path) {
                for (auto &track : p.tracks)
                    for (auto &clip : track.clips)
                        if (clip.source == path)
                            return false;
                result["removed"].Push(path);
                return true;
            });
            changed = result["removed"].Size() > 0;
        } else if (name == "set_clip") {
            auto &c = clip();
            p.setTiming(c.id, a["start"].Num(c.start), a["duration"].Num(c.duration), a["in"].Num(c.in),
                        a["speed"].Num(c.speed));
            if (a.Has("fade_in"))
                c.fadeIn = a["fade_in"].Num();
            if (a.Has("fade_out"))
                c.fadeOut = a["fade_out"].Num();
            if (a.Has("name"))
                c.name = a["name"].S();
            if (a.Has("color"))
                c.color = a["color"].U();
            if (a.Has("enabled"))
                for (auto id : p.linked(c.id))
                    p.findClip(id)->enabled = a["enabled"].B();
            if (a.Has("hold"))
                c.hold = a["hold"].B();
            if (a.Has("reverse"))
                for (auto id : p.linked(c.id))
                    p.findClip(id)->reverse = a["reverse"].B();
            changed = true;
        } else if (name == "duplicate_clip") {
            auto id = p.duplicate(clip().id);
            result["clip"] = id;
            s.selectClip(id);
            changed = true;
        } else if (name == "select_clips") {
            s.setSelection(clipIds());
            resultClips(s.selection);
        } else if (name == "move_clips") {
            auto ids = clipIds();
            p.moveClips(ids, a["delta"].Num());
            s.setSelection(ids);
            resultClips(s.selection);
            changed = true;
        } else if (name == "duplicate_clips") {
            auto copies = p.duplicateClips(clipIds());
            s.setSelection(copies);
            resultClips(copies);
            changed = true;
        }
        // Third dispatch chain, for the same reason as the second.
        if (name == "split_clips") {
            auto ids = clipIds();
            p.splitClips(ids, a["time"].Num());
            s.setSelection(ids);
            changed = true;
        } else if (name == "delete_clips") {
            p.removeClips(clipIds(), a["ripple"].B());
            s.time = std::min(s.time, p.duration);
            changed = true;
        } else if (name == "link_clips") {
            p.linkClips(clip().id, a["other"].U());
            changed = true;
        } else if (name == "unlink_clip") {
            p.unlink(clip().id);
            changed = true;
        } else if (name == "split_clip") {
            p.split(clip().id, a["time"].Num());
            changed = true;
        } else if (name == "delete_clip") {
            p.removeClips({clip().id}, a["ripple"].B());
            s.time = std::min(s.time, p.duration);
            changed = true;
        } else if (name == "set_parameter" || name == "set_keyframe") {
            auto &ps = a["effect"].I(-1) >= 0 ? effect().params : clip().params;
            auto key = a["key"].S();
            if (key.empty())
                throw std::runtime_error("A parameter key is required");
            if (name == "set_parameter")
                ps[key].value = a["value"].F();
            else {
                if (a["time"].Num() < 0)
                    throw std::runtime_error("Animation time cannot be negative");
                int curve = a["curve"].I();
                if (curve < 0 || curve > 3)
                    throw std::runtime_error("Invalid interpolation");
                ps[key].setKey({a["time"].Num(), a["value"].F(), Curve(curve), a["shape"].F()});
            }
            changed = true;
        } else if (name == "add_audio_effect") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Audio)
                throw std::runtime_error("An audio track is required");
            c.effects.push_back(instantiateAudioEffect(a["id"].S()));
            changed = true;
        } else if (name == "set_effect") {
            auto &e = effect();
            auto &effects = clip().effects;
            int from = a["effect"].I(), to = a["position"].I(from);
            if (to < 0 || to >= int(effects.size()))
                throw std::runtime_error("Effect position outside stack");
            if (a.Has("enabled"))
                e.enabled = a["enabled"].B();
            Effect moved = std::move(e);
            effects.erase(effects.begin() + from);
            effects.insert(effects.begin() + to, std::move(moved));
            changed = true;
        } else if (name == "remove_effect") {
            effect(); // Validate the index and lock before mutation.
            auto &effects = clip().effects;
            effects.erase(effects.begin() + a["effect"].I());
            changed = true;
        } else if (name == "add_effect") {
            if (p.owner(clip().id)->kind != TrackKind::Video)
                throw std::runtime_error("A video track is required");
            clip().effects.push_back(s.renderer.library.instantiate(a["id"].S()));
            changed = true;
        } else if (name == "list_effects") {
            result["effects"] = Json::Array();
            for (auto &d : s.renderer.library.definitions) {
                Json e = Json::Object();
                e["id"] = d.id;
                e["name"] = d.name;
                e["origin"] = d.origin;
                e["kind"] = d.kind;
                e["generator"] = d.generator;
                e["inputs"] = d.inputs;
                result["effects"].Push(e);
            }
            for (auto &d : cydaw::engine::effectCatalogue)
                if (audioInsert(d)) {
                    Json e = Json::Object();
                    e["id"] = "cydaw." + std::string(d.typeKey);
                    e["name"] = std::string(d.displayName);
                    e["origin"] = "CyDAW 2 DSP";
                    e["kind"] = "audio";
                    result["effects"].Push(e);
                }
            return result;
        } else if (name == "set_shader") {
            if (p.owner(clip().id)->kind != TrackKind::Video)
                throw std::runtime_error("A video track is required");
            s.renderer.validateShader(a["code"].S());
            auto &e = effect();
            if (e.id == "ofx" || e.id == "grading")
                throw std::runtime_error("OFX does not contain an editable HLSL shader");
            e.code = a["code"].S();
            e.graph = {};
            changed = true;
        } else if (name == "get_graph") {
            result["graph"] = effect().graph;
            return result;
        } else if (name == "set_graph") {
            if (p.owner(clip().id)->kind != TrackKind::Video)
                throw std::runtime_error("A video track is required");
            Effect e = effect();
            if (e.id == "ofx" || e.id == "grading")
                throw std::runtime_error("OFX does not contain a Shaderixel graph");
            e.graph = a["graph"];
            e.code = effectCode(e);
            s.renderer.validateShader(e.code);
            effect() = e;
            changed = true;
        } else if (name == "transport") {
            if (a.Has("time")) {
                double t = a["time"].Num();
                if (t < 0 || t > p.duration)
                    throw std::runtime_error("Time is outside the sequence");
                s.time = t;
            }
            if (a.Has("playing"))
                s.playing = a["playing"].B();
        } else if (name == "export_video") {
            exportVideo(p, s.media, s.renderer, a["path"].S());
        } else if (name == "export_audio") {
            exportWav(p, s.media, a["path"].S());
        } else if (name == "set_preview_proxies") {
            s.media.proxiesEnabled = a["enabled"].B();
        } else if (name == "prepare_proxy") {
            auto &selected = clip();
            result["job"] = startProxy(selected.source, selected.name, a["path"].S());
        } else if (name == "list_export_presets") {
            result["presets"] = Json::Array();
            for (auto &preset : exportPresets()) {
                Json item = Json::Object();
                item["id"] = preset.id;
                item["name"] = preset.name;
                item["width"] = preset.width;
                item["height"] = preset.height;
                item["fps"] = preset.fps;
                result["presets"].Push(item);
            }
        } else if (name == "start_export") {
            auto kind = a["kind"].S();
            if (kind != "video" && kind != "audio" && kind != "images" && kind != "analysis")
                throw std::runtime_error("Unknown export format: video or audio required");
            ExportRange range;
            if (a["range"].S() == "work") {
                if (!p.hasWorkArea())
                    throw std::runtime_error("No work area to export");
                range = {p.workIn, p.workOut};
            } else if (a["range"].S() == "selection") {
                if (s.selection.empty())
                    throw std::runtime_error("No selected clips to export");
                range = {1e30, 0};
                for (auto id : s.selection)
                    if (auto *c = p.findClip(id)) {
                        range.begin = std::min(range.begin, c->start);
                        range.end = std::max(range.end, std::min(p.duration, c->start + c->duration));
                    }
                if (range.end <= range.begin)
                    throw std::runtime_error("Empty selection");
            } else if (a.Has("range") && a["range"].S() != "all")
                throw std::runtime_error("Export range: work, selection or all");
            else if (a.Has("begin") || a.Has("end")) {
                range = {a["begin"].Num(0), a["end"].Num(p.duration)};
                if (!std::isfinite(range.begin) || !std::isfinite(range.end) || range.begin < 0 ||
                    range.end <= range.begin || range.end > p.duration + 1e-9)
                    throw std::runtime_error("Export range is outside the sequence");
            }
            auto id = s.exports.start(p, s.renderer.library,
                                      kind == "video"    ? ExportKind::Video
                                      : kind == "audio"  ? ExportKind::Audio
                                      : kind == "images" ? ExportKind::Images
                                                         : ExportKind::Analysis,
                                      a["path"].S(), a["preset"].S("project"), range);
            result["job"] = id;
        } else if (name == "place_clip" || name == "split_screen") {
            // A picture is fitted to the frame by default: its share of the frame's width and height follows
            // the two aspects. Placing it means a scale and a centre (offsets are shares of the frame).
            auto shares = [&](const Clip &c) {
                std::pair<float, float> share{1.f, 1.f};
                if (c.source.empty() || c.title.enabled)
                    return share;
                auto &asset = s.media.get(c.source);
                if (asset.loading || !asset.frame.width || !asset.frame.height)
                    throw std::runtime_error("Media is loading");
                float ratio = (float(asset.frame.width) / asset.frame.height) / (float(p.width) / p.height);
                return ratio < 1 ? std::pair{ratio, 1.f} : std::pair{1.f, 1 / ratio};
            };
            auto place = [&](Clip &c, float scale, float x, float y) {
                for (auto [key, value] : {std::pair{"scale", scale}, {"x", x}, {"y", y}}) {
                    auto &param = c.params[key];
                    param.keys.clear();
                    param.value = value;
                }
            };
            std::vector<Clip *> pictures;
            for (auto id : clipIds())
                if (auto *c = p.findClip(id); c && p.owner(id)->kind == TrackKind::Video && !p.owner(id)->locked)
                    pictures.push_back(c);
            if (pictures.empty())
                throw std::runtime_error("Choose video clips");
            float aspect = float(p.width) / p.height;
            if (name == "place_clip") {
                auto position = a["position"].S();
                static const std::set<std::string> positions{"top_left", "top_right", "bottom_left", "bottom_right",
                                                             "center", "full"};
                if (!positions.contains(position))
                    throw std::runtime_error("Unknown position: " + position);
                float size = position == "full" ? 1.f : float(std::clamp(a["size"].Num(.33), .05, 1.));
                float margin = float(std::clamp(a["margin"].Num(.04), 0., .4));
                for (auto *c : pictures) {
                    // size is the share of the frame's width the picture takes (the full frame keeps the fit).
                    auto [wide, high] = shares(*c);
                    float scale = position == "full" ? 1.f : size / wide;
                    float halfWidth = scale * wide / 2, halfHeight = scale * high / 2;
                    float x = position.ends_with("left") ? -(.5f - halfWidth - margin)
                              : position.ends_with("right") ? .5f - halfWidth - margin
                                                            : 0.f;
                    float y = position.starts_with("top") ? -(.5f - halfHeight - margin * aspect)
                              : position.starts_with("bottom") ? .5f - halfHeight - margin * aspect
                                                               : 0.f;
                    place(*c, scale, x, y);
                }
            } else {
                // Split screen: each picture fills its cell, fitted inside it, in timeline track order.
                auto layout = a["layout"].S("side_by_side");
                if (layout != "side_by_side" && layout != "stacked" && layout != "grid")
                    throw std::runtime_error("layout : side_by_side, stacked ou grid");
                size_t cells = layout == "grid" ? 4 : 2;
                if (pictures.size() < 2 || pictures.size() > cells)
                    throw std::runtime_error("Choose 2 to " + std::to_string(cells) + " video clips");
                float cellWidth = layout == "stacked" ? 1.f : .5f, cellHeight = layout == "side_by_side" ? 1.f : .5f;
                for (size_t i = 0; i < pictures.size(); ++i) {
                    auto [wide, high] = shares(*pictures[i]);
                    float scale = std::min(cellWidth / wide, cellHeight / high);
                    float x = layout == "stacked" ? 0.f : (i % 2 ? .25f : -.25f);
                    float y = layout == "side_by_side" ? 0.f
                              : layout == "stacked"    ? (i % 2 ? .25f : -.25f)
                                                       : (i / 2 ? .25f : -.25f);
                    place(*pictures[i], scale, x, y);
                }
            }
            result["clips"] = int(pictures.size());
            changed = true;
        } else if (name == "key_background") {
            auto colour = a["color"].S("green");
            if (colour != "green" && colour != "blue")
                throw std::runtime_error("color : green ou blue");
            int count = 0;
            for (auto id : clipIds()) {
                auto *c = p.findClip(id);
                if (!c || p.owner(id)->kind != TrackKind::Video || p.owner(id)->locked)
                    continue;
                auto key = s.renderer.library.instantiate("chroma");
                key.params["keyColor.0"].value = 0;
                key.params["keyColor.1"].value = colour == "green" ? 1.f : 0.f;
                key.params["keyColor.2"].value = colour == "blue" ? 1.f : 0.f;
                key.params["threshold"].value = float(std::clamp(a["threshold"].Num(.35), .01, 1.));
                c->effects.insert(c->effects.begin(), key); // keyed first, so later effects see the matte
                ++count;
            }
            if (!count)
                throw std::runtime_error("Choose video clips");
            result["clips"] = count;
            changed = true;
        } else if (name == "audio_cut_fades") {
            auto *track = p.findTrack(a["track"].U());
            if (!track || track->kind != TrackKind::Audio || track->locked)
                throw std::runtime_error("Choose an available audio track");
            double length = std::clamp(a["length"].Num(.05), .005, 1.);
            int cuts = 0;
            for (auto &outgoing : track->clips)
                for (auto &incoming : track->clips)
                    if (&outgoing != &incoming && std::abs(outgoing.start + outgoing.duration - incoming.start) < 1e-6) {
                        outgoing.fadeOut = std::max(outgoing.fadeOut, std::min(length, outgoing.duration));
                        incoming.fadeIn = std::max(incoming.fadeIn, std::min(length, incoming.duration));
                        ++cuts;
                    }
            result["cuts"] = cuts;
            changed = cuts > 0;
        } else if (name == "add_adjustment_layer") {
            if (!p.layerGraph.IsNull())
                throw std::runtime_error("Adjustment layers require the automatic layer graph");
            auto &track = p.addTrack(TrackKind::Video);
            track.name = "Adjustment Layer";
            Clip c;
            c.id = p.allocate();
            c.adjustment = true;
            c.name = "Adjustment Layer";
            c.start = std::max(0., a["start"].Num(s.time));
            c.duration = std::max(1. / p.fps, a["duration"].Num(5));
            c.color = 0x8c9eff;
            track.clips.push_back(c);
            appendLayerSurface(p, track.id, track.name);
            std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            p.duration = std::max(p.duration, c.start + c.duration);
            s.selectClip(c.id);
            result["clip"] = c.id;
            result["track"] = p.tracks.front().id;
            changed = true;
        } else if (name == "export_gif") {
            ExportRange range;
            if (a["range"].S() == "work") {
                if (!p.hasWorkArea())
                    throw std::runtime_error("No work area to export");
                range = {p.workIn, p.workOut};
            } else if (a.Has("begin") || a.Has("end"))
                range = {std::max(0., a["begin"].Num(0)), std::min(p.duration, a["end"].Num(p.duration))};
            int fps = std::clamp(int(a["fps"].Num(15)), 1, 50);
            exportGif(p, s.media, s.renderer, a["path"].S(), range, unsigned(a["width"].Num(480)), fps);
            result["frames"] = std::max(1, int(std::floor((range.to(p) - range.from(p)) * fps + 1e-6)));
        } else if (name == "export_chapters") {
            auto kind = a["kind"].S("video");
            if (kind != "video" && kind != "audio")
                throw std::runtime_error("kind : video ou audio");
            if (p.markers.empty())
                throw std::runtime_error("No markers: chapters start at markers");
            // Spans: each region on its own, or chapters from the start (when the first marker is not on it)
            // and each marker to the next one, the last to the end.
            struct Span {
                double begin, end;
                std::string name;
            };
            std::vector<Span> spans;
            if (a["regions"].B()) {
                for (auto &marker : p.markers)
                    if (marker.duration >= 1. / p.fps && marker.time < p.duration - 1. / p.fps)
                        spans.push_back({marker.time, std::min(marker.end(), p.duration), marker.name});
                if (spans.empty())
                    throw std::runtime_error("No regions: give a marker a duration");
            } else {
                if (p.markers.front().time >= 1. / p.fps)
                    spans.push_back({0, 0, p.name});
                for (auto &marker : p.markers)
                    if (marker.time < p.duration - 1. / p.fps)
                        spans.push_back({marker.time, 0, marker.name});
                for (size_t i = 0; i < spans.size(); ++i)
                    spans[i].end = i + 1 < spans.size() ? spans[i + 1].begin : p.duration;
            }
            auto folder = sx::Utf8Path(a["folder"].S());
            std::filesystem::create_directories(folder);
            result["jobs"] = Json::Array();
            for (size_t i = 0; i < spans.size(); ++i) {
                auto label = sx::Utf8Path(spans[i].name).wstring();
                for (auto &c : label)
                    if (c < 32 || wcschr(L"<>:\"/\\|?*", c))
                        c = L'_';
                if (label.size() > 60)
                    label.resize(60);
                wchar_t number[8];
                swprintf(number, 8, L"%02d ", int(i + 1));
                auto file = folder / (number + label + (kind == "video" ? L".mp4" : L".wav"));
                auto id = s.exports.start(p, s.renderer.library, kind == "video" ? ExportKind::Video : ExportKind::Audio,
                                          sx::Narrow(file.wstring()), "project", {spans[i].begin, spans[i].end});
                result["jobs"].Push(id);
            }
        } else if (name == "export_status") {
            if (a.Has("job"))
                result["export"] = s.exports.status(a["job"].U()).json();
            else {
                result["exports"] = Json::Array();
                for (auto &job : s.exports.list())
                    result["exports"].Push(job.json());
            }
        } else if (name == "cancel_export") {
            result["accepted"] = s.exports.cancel(a["job"].U());
            result["export"] = s.exports.status(a["job"].U()).json();
        } else if (name == "inspect_ofx" || name == "add_ofx_effect") {
            auto path = ofxBinary(a["path"].S());
            IsolatedOfx worker(path, a["identifier"].S(), 64, 64);
            auto metadata = worker.metadata();
            result["plugin"] = metadata;
            if (name == "add_ofx_effect") {
                auto &c = clip();
                if (p.owner(c.id)->kind != TrackKind::Video)
                    throw std::runtime_error("A video clip is required");
                Effect added;
                added.id = "ofx";
                added.code = path;
                added.graph = metadata;
                for (auto &parameter : metadata["parameters"].Elements()) {
                    added.params[parameter["key"].S()] = {parameter["default"].F()};
                    added.parameterNames[parameter["key"].S()] = parameter["name"].S();
                }
                validateOfx(added);
                c.effects.push_back(added);
                result["effect"] = int(c.effects.size() - 1);
                changed = true;
            }
        } else if (name == "render_frame") {
            double t = a["time"].Num(s.time);
            if (t < 0 || t > p.duration)
                throw std::runtime_error("Time is outside the sequence");
            s.renderer.render(p, s.media, t, p.width, p.height);
            auto format =
                a["format"].S(sx::Utf8Path(a["path"].S()).extension() == L".exr" ? "exr16" : "png8");
            if (format == "exr16")
                writeExr(s.renderer.readbackFloat(), a["path"].S());
            else if (format == "png16")
                writePng16(s.renderer.readbackFloat(true), a["path"].S());
            else if (format == "png8")
                writePng(s.renderer.readback(), a["path"].S());
            else
                throw std::runtime_error("Unknown image format");
        } else if (name == "get_history") {
            result["past"] = Json::Array();
            for (auto &step : s.history.undoSteps())
                result["past"].Push(step.label.empty() ? "Edit" : step.label);
            result["future"] = Json::Array();
            auto &future = s.history.redoSteps();
            for (auto it = future.rbegin(); it != future.rend(); ++it)
                result["future"].Push(it->label.empty() ? "Edit" : it->label);
        } else if (name == "undo") {
            if (s.history.undo(p)) {
                s.dirty = true;
                s.time = std::min(s.time, p.duration);
            }
        } else if (name == "redo") {
            if (s.history.redo(p)) {
                s.dirty = true;
                s.time = std::min(s.time, p.duration);
            }
        }
        if (changed) {
            p.validate();
            if (name == "new_project" || name == "open_project")
                s.history.clear();
            else
                s.history.checkpoint(before, actionLabel(name));
            s.dirty = name != "open_project";
        }
        if (changed || name == "undo" || name == "redo")
            s.media.resetAudio();
        if (changed || name == "undo" || name == "redo" || name == "open_project" || name == "new_project")
            s.renderer.clearHistory();
        if (name == "open_project" || name == "new_project")
            s.media.clear();
        if (name == "open_project" || name == "new_project" || name == "undo" || name == "redo" ||
            name == "set_effect" || name == "remove_effect" || name == "set_mix_effect" ||
            name == "remove_mix_effect")
            s.pluginEditors.clear();
        s.lastCommand = name;
        s.tidySelection();
    } catch (...) {
        p = before;
        s.projectPath = previousPath;
        s.time = previousTime;
        s.playing = previousPlaying;
        s.dirty = previousDirty;
        s.selected = previousSelected;
        s.selection = previousSelection;
        throw;
    }
    return result;
}
Json Protocol::respond(const Json &request) {
    Json response = Json::Object();
    response["jsonrpc"] = "2.0";
    response["id"] = request["id"];
    auto error = [&](int code, const std::string &message) {
        response["error"] = Json::Object();
        response["error"]["code"] = code;
        response["error"]["message"] = message;
        return response;
    };
    if (!request.IsObject() || request["jsonrpc"].S() != "2.0" ||
        request["method"].GetType() != Json::Type::String)
        return error(-32600, "Invalid Request");
    if (!request.Has("id"))
        return {};
    auto method = request["method"].S();
    Json result = Json::Object();
    if (method == "initialize") {
        auto v = request["params"]["protocolVersion"].S();
        result["protocolVersion"] =
            (v == "2025-06-18" || v == "2025-03-26" || v == "2024-11-05") ? v : "2025-06-18";
        result["serverInfo"] = Json::Object();
        result["serverInfo"]["name"] = "shaderixframe";
        result["serverInfo"]["version"] = applicationVersion;
        result["capabilities"] = Json::Object();
        result["capabilities"]["tools"] = Json::Object();
        result["instructions"] = "The MCP process owns a ShaderixFrame window: the tools and "
                                 "window edit the same sequence.";
    } else if (method == "ping") {
    } else if (method == "tools/list")
        result["tools"] = tools();
    else if (method == "tools/call") {
        Json content = Json::Object();
        content["type"] = "text";
        try {
            auto data = execute(request["params"]["name"].S(), request["params"]["arguments"]);
            content["text"] = data.Dump(0);
            result["isError"] = false;
            result["structuredContent"] = data;
        } catch (const std::exception &e) {
            content["text"] = e.what();
            result["isError"] = true;
        }
        result["content"] = Json::Array();
        result["content"].Push(content);
    } else
        return error(-32601, "Method not found");
    response["result"] = result;
    return response;
}
} // namespace sf

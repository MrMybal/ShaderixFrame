# ShaderixFrame 0.13.0-alpha.1

Development and maintenance: Cyberalien.

**Alpha preview for Windows x64.** Licensed under [GNU AGPL v3](LICENSE).
See [alpha downloads and limitations](docs/RELEASE_ALPHA.md) and [licensing/source information](docs/LICENSING.md).

ShaderixFrame is a native Windows video editor with layered compositing, HLSL effects,
VFX graphs and audio editing. It reuses Shaderixel's theme, shader graph compiler and
composition components, together with CyDAW v2's DSP engine, signal analysis and graph controls.
The application starts in **English**. Imported projects keep their own clip names and text.

Version 0.13 adds six workspaces, a CyDAW-style arrangement timeline and frame filmstrip,
a curve editor, detailed composition zoom, and **406 Shaderixel effects and generators**.
See [the release notes](docs/RELEASE_0.13.md) for version-specific details.

## Start editing

Run `dist/ShaderixFrame/ShaderixFrame.exe`, or the executable from the Windows package.
The distributed application works without the Shaderixel and CyDAW source projects.

1. Use **File > Import Media**, `Ctrl+I`, or drop files onto the window. Video with audio
   creates linked clips on separate tracks. An empty sequence also offers templates.
2. Move clips, drag their edges to trim, and press `S` to split at the playhead.
   `Alt` temporarily disables snapping. Right-click for editing commands and effects.
3. Select a clip and use **Properties** for transforms, masks, effects, audio and transitions.
   **K** adds a keyframe; right-click K to show its automation lane.
4. Double-click a video clip to open its **Clip Editor**, with a local video/audio timeline,
   layer graph, shader graphs and effects. **Apply to Container** updates the parent render;
   saving also applies open clip drafts. **Reload** discards an unapplied draft.
5. Save as `.sframe`. Export video, audio or image sequences through **File** or **Exports**.
   Exports run in the background while editing remains available.

## Workspaces and windows

The top bar selects **Edit** (`F1`), **Effects** (`F2`), **Motion** (`F3`),
**Color Grading** (`F4`), **Audio** (`F5`) and **Deliver** (`F6`). It also contains
transport, loop and timecode controls. Each workspace remembers its open panels,
tab order, sizes and detached windows. **View > Reset This Workspace** restores its layout.

Drag a tab outside the main window to create a native window on another screen.
Close panels freely and reopen them from **View**. The clip editor can be detached
as a whole, and its panels are available from **Clip Windows**.
Existing French panel settings and docking layouts are migrated to their English names.

**Command Palette** (`Ctrl+Shift+P`) finds commands, tools, panels and workspaces by name.
Use the arrows and Enter to select a result. **Keyboard Shortcuts** allows rebinding tools,
trims, markers, the work area and shuttle controls; conflicts are shown and custom bindings
are stored in `keymap.json`.

## Media bin and Source monitor

The **Media** tab in **Media & Effects** displays the project bin. Add media without placing
it, then double-click a card to open it in **Source**. Mark **In** (`I`) and **Out** (`O`),
choose video/audio, then **Insert** (`,`) or **Overwrite** (`.`) at the timeline playhead.
Insert shifts later clips; overwrite replaces the target range. Drag the Source image onto
a timeline track to place it directly. A track header's **C** button chooses target tracks.

Source playback supports Space, K to stop and L to accelerate. Drag the in/out handles
on its range bar. **Fit to Fill** (`Shift+.`) adjusts speed to fill the work area.
**Replace Clip** keeps the selected clip's position and duration while replacing its media.
**Match Frame** (`F` in the timeline) opens the selected clip's source with its range marked.

Hover bin cards for source details and video scrubbing. Preview proxies use H264 at 720p;
automatic proxies can be prepared for high-resolution imports. Exports always use originals.
**Remove Unused** removes unused bin entries while keeping files on disk.

**File > Relink Missing Media** searches by file name, including subfolders.
**Collect Media** copies used sources into an archive folder and updates project references.
The top bar reports offline media. Image/video decoding depends on the codecs available
through Windows Media Foundation and WIC.

## Timeline

The timeline adapts CyDAW's ArrangementView and Cinema interactions to seconds and video frames.

- **Tools:** Select (`V`), Blade (`B`, Shift for all tracks), Slip (`Y`), Roll (`N`),
  Rate Stretch (`R`), Slide (`U`) and Hand (`H`). Slip changes source timing without moving
  the clip; roll moves an adjacent cut; slide adjusts the neighboring clips.
- **Navigation:** drag the strip above the ruler horizontally to pan, down to zoom in,
  up to zoom out; double-click to fit all. Wheel scrolls tracks; Shift scrolls time;
  Ctrl zooms; Alt changes track heights. Middle-drag or Hand pans the view.
  `Z` fits the selection; the fit button frames all tracks and time.
- **Ruler and work area:** click to seek. Drag its upper part, or hold Shift, to define
  a work area; move its edges or body. `I`/`O` set its boundaries, `L` loops it,
  and **Work Area Only** limits exports to it.
- **Filmstrip:** the **FRAME** row shows the final render above the tracks; drag it to scrub.
  Video clips show actual rendered thumbnails with timecodes, including HLSL, titles and
  container content. Zoom up to 20,000 px/s to inspect individual frames.
- **Track headers:** color, rename, video visibility/audio mute, solo, lock, automation and
  audio meters. Resize heights, drag headers to reorder tracks and resize the label column.
  Taller audio tracks show CyDAW gain and pan knobs.
- **Track groups:** `Ctrl+G` groups the selected clips' tracks. Collapse/expand groups,
  mute/solo their tracks, and rename, color or dissolve groups through the context menu.
- **Clips:** effect/speed/container badges, colors, fade handles and transitions.
  The **fx** badge opens the effect stack. Right-click **Add Effect** searches Shaderixel
  video effects or CyDAW audio DSPs. A transition's end handle changes its duration.
- **Audio:** drag the gain line (-60 to +12 dB); double-click for 0 dB. Waveforms follow gain
  and reveal sample min/max, then individual samples, as you zoom in.
- **Selection:** Ctrl/Shift-click toggles linked groups; drag a marquee in empty space;
  `Ctrl+A` selects all, Esc deselects. `A` selects forward from the playhead, `Alt+A` backward.
  `Ctrl+F` finds clips by name, track or source file.
- **Clipboard:** `Ctrl+C`, `Ctrl+X`, `Ctrl+V` at the playhead, or **Paste Here**.
  Tracks, spacing, links, effects and animation are preserved. `Ctrl+D` duplicates a block
  after its end. `Ctrl+Alt+V` pastes selected attributes from a copied clip.
- **Trims and cuts:** `S` splits at the playhead. `Q` trims the head and `W` the tail with
  ripple; Alt keeps the gap. Delete removes selection; Shift+Delete closes its intervals.
  Clips crossing a ripple interval must first be split or included in the selection.
  Linked clips and track locks are checked together; keys follow moves and duplicates.
- **Speed:** `Ctrl+R` opens speed/duration. Rate Stretch changes speed by dragging an edge.
  Reverse playback includes audio. Shift+F inserts a two-second freeze frame; context-menu
  presets also offer one and five seconds. Shift+E disables/enables a clip without moving it.
- **Playback:** arrows move by frame (Shift: second), Up/Down visit edit points, Home/End
  go to sequence boundaries. `J` shuttles backward, `Shift+L` forward at increasing speed,
  and `K` stops; `L` remains loop. Shift+Space plays around the playhead and returns.
- **Automation:** clips show keyframe diamonds. Parameter lanes under tracks support
  double-click to add, drag to move and right-click to delete. Ctrl+arrows visit keys.
  The View control toggles filmstrip, thumbnails, waveforms and keys, and manages heights,
  collapsed groups and automation lanes.

Markers (`M`) have names, notes, colors and optional region durations. Drag them on the ruler,
double-click to rename, or use **Markers** for thumbnails, chapters, CSV and rendered images.
Regions can define the work area and be exported individually. Splitting at markers and
aligning clips to markers support editing to music; audio transient detection creates markers.

Right-click the ruler to **Lift Work Area** (leave a gap), **Extract Work Area** (close it),
insert a gap or clear the work area. Tracks can be duplicated and have all gaps closed.
**Sequence > Close All Gaps** removes ranges where every track is empty.

Additional editing commands include batch rename (`Shot ##`, legacy `{nom}` current-name token),
color-based selection, silence removal, music ducking under a voice track, scene detection,
audio synchronization, automatic white balance/exposure and matching color to a reference.
Analyses run in the background and report progress in the status bar.

## Composition, graphs and motion design

**Composition** supports wheel zoom around the pointer, Fit, 1:1 and 25%-1600% presets.
Use Ctrl+0 to fit, Ctrl+1 for 100%, and Ctrl plus +/- to zoom. Space-drag or middle-drag pans;
the minimap shows and moves the visible region. Rendering follows zoom up to sequence resolution,
with sharp pixel display above 200% and hovered pixel coordinates.

Guides include safe areas, thirds, center and target aspect ratios. **Snapshot** stores image A;
**A/B** compares it with the live B image using a draggable wipe. Right-click to export the
current frame as PNG or EXR half, or use **Copy Image** (`Ctrl+Shift+C`) at sequence resolution.

**Layer Graph** is independent of **Shader Graph**. Video tracks produce transparent surfaces;
connect these to blend, correction, effect and output nodes. Branches can share inputs and
parameters are animatable in sequence time. **Open Shader Graph** edits the selected node's
HLSL graph; **Back to Clip Effects** returns to clip effects. Wheel zooms, middle-drag pans,
right-click opens node actions, and right-clicking an input disconnects it.
Shaderixel's graph validation, port types, drawing and 27 blend modes remain shared components.

**Shaderixel Library** provides generators and effects by name/category. Create a generator
layer, drop it onto the timeline or apply an effect to the selected clip. The shader code and
settings are embedded in projects. Native HLSL supports four inputs, multiple passes and history.
**HLSL** compiles text shaders; **Shader Graph** uses Shaderixel's compiler.

**Motion Design** supplies shapes, text, images/video and groups, with hierarchy, anchors,
pivots, visibility and child clipping. Position and size are animatable. Select, move and
resize widgets in the rendered canvas, or open their clip content for VFX and nested timelines.

**Animation** overlays selected clip/effect curves, each with its own range. Click or marquee
selects keys; Ctrl/Shift extends selection. Drag keys on the frame grid (Alt: free), double-click
to add, Delete to remove, Ctrl+C/V to paste at the playhead, and Alt-drag a segment to bend it.
Interpolation can be Linear, Smooth, Step or CyDAW. Wheel zooms time; Shift-wheel or middle-drag
pans; F fits; K adds a key at the playhead. The value table remains available.

Titles support multiline text, installed Windows fonts, bold/italic/alignment, animated size,
color, outline, shadow and background box. Lower thirds and title styles are included.
Masks support rectangle, ellipse or closed Bezier paths, inversion, transforms and feathering.
Mask guides allow direct manipulation; translation tracking uses a contrasting region.

Transitions work on overlapping clips on one track or across tracks, after clip effects and
transforms. Fade and wipe shaders are editable and animatable; node transitions accept two
images and animated progress. Audio uses its own fades. Adjustment layers apply their effects
to lower tracks. Presets include fades, slide, zoom, Ken Burns, typewriter and speed ramps.

## Color and precision

Color grading includes primaries, wheels, curves, qualifiers, local masks, LUTs, presets,
eyedroppers, before/after comparison and video scopes. Corrections retain their own masks and
curves. Import 1D/3D `.cube` LUTs, 2D RG image tables or 2D atlases representing 3D LUTs;
data is embedded in projects and is available in container graphs too.

Compositing, effects and containers use RGBA16F intermediate surfaces. Working spaces include
sRGB, linear Rec.709 and linear Rec.2020. PNG16 and EXR half export before display quantization.
The current video decoding/encoding paths remain 8-bit. This is not a complete OCIO/ACES or
HDR-monitor workflow; see [architecture](docs/ARCHITECTURE.md) and [roadmap](docs/ROADMAP.md).

## Audio, DSP and plugins

**Mixer & Audio Graph** exposes tracks, buses, sends and master. **DSP Graph** edits embedded
stereo graphs using CyDAW v2's AudioRenderPlan, graph validation and controls. Add DSPs by search,
connect ports, arrange nodes, automate parameters and **Compile and Apply** the draft as one
undoable edit. The internal catalogue contains 15 DSP effects. Native SDK effects and VST3
plugins run in isolated workers; plugin windows, opaque state and presets remain accessible.

Channels support gain/pan, mute/solo, output routing and pre/post-fader sends. Routing cycles
are rejected even through disabled sends. Clip and track pan combine before track DSP;
bus pan follows its DSP. Automation can address channels and individual DSP nodes.
Latency compensation, time stretching, spectrum, LUFS and true-peak analysis are available.
**Normalize Loudness** measures the mix in the background and offers broadcast/podcast/web
levels with an optional -1 dBTP master limiter. **Enhance Voice** adds a gate, EQ and compressor.
Effects continue processing silence through the end of the sequence so their tails can render.

**Properties > + Plugin OFX** loads supported Windows x64 RGBA float filters. OFX has its own
worker and preserves HDR values and alpha. VST3 requires its separately supplied CyDAW worker.
See [plugins](docs/PLUGINS.md) and [the native SDK](sdk/README.md) for supported contracts/limits.

## Export, interchange and recovery

Exports include MP4 H264/AAC, stereo 48 kHz WAV, uncompressed RGB/PCM AVI, PNG8/PNG16/EXR half
sequences, animated GIFs and loudness JSON. Presets include HD, UHD, vertical and square formats.
Export the full sequence, selection, work area, marker chapters or regions.

An export captures a copy of the sequence and catalogue at launch; later edits do not change it.
Sources remain referenced by file path. The queue accepts 32 active/waiting jobs, processes
one at a time and keeps the eight most recent completed jobs. Cancel before finalization to
preserve an existing destination. Closing the application cancels pending work and joins workers.

Import/export CMX 3600 EDLs, shot lists and marker lists; import/export SubRip SRT subtitles.
The **Subtitles** panel edits cues and track-wide styling. **Camera Angles** provides multicam
switching with keys 1-4 and optional linked audio following the selected angle.

Each save keeps the previous project among ten versions; **File > Previous Versions** restores
one as an undoable change. **History** lists named undo/redo steps. Unsaved recovery snapshots
are separate from saved projects and are captured periodically and on exit/document changes.
Use **File > Recover Session** to reopen **Session Recovery**.

Press Enter to pre-render the work area into memory. A green ruler bar marks cached frames;
editing invalidates them. Clip editors keep separate documents, selections and undo histories.

## Examples

| Project | Demonstrates |
|---|---|
| `FirstLight.sframe` | Procedural light, title, animation and original audio |
| `CutStudy.sframe` | Linked video/title/audio editing |
| `GraphStudy.sframe` | Shader graph and automation |
| `LayerGraphStudy.sframe` | Layer composition, color branches and animated blends |
| `MotionStudy.sframe` / `ClipStudy.sframe` | Widget hierarchy and nested composition |
| `GradingStudy.sframe` | HDR primaries, curves and local corrections |
| `DspStudy.sframe` / `ParallelEQ.json` | Parallel CyDAW EQ processing |
| `MixerStudy.sframe` | Bus routing, sends and effect tails |
| `PrecisionStudy.sframe` | Linear HDR gradient and half-float export |
| `TransitionStudy.sframe` / `MaskStudy.sframe` | Transitions and animated masks |

Examples are in `examples/`. Their demo soundtrack is generated locally.

## Build and package

Install Visual Studio 2022 with C++ tools, CMake, Ninja and Python 3.

```powershell
python tools/prepare_dependencies.py --audio-only
build.bat Release
```

To compile without running tests: `build.bat Release --safe-compile`.
Use `--warp` or `SF_FORCE_WARP=1` for software D3D11. Tests create media in ignored build folders;
VST3 tests explicitly report missing optional fixtures. The MSVC runtime is statically linked.
Source dependencies are copied in `vendor/`; binary workers and generated media stay separate.
See [BUILDING.md](docs/BUILDING.md) for restoring the optional worker and fixtures.

```powershell
python tools/package.py
python tools/make_installer.py
```

The portable ZIP includes workers, licences and SDK headers. The installer is per-user.
Binary/media downloads, builds, caches and local settings are excluded from Git; Git LFS is not required.

## MCP and shared components

`ShaderixFrame.exe --mcp` starts the built-in stdio JSON-RPC server. The client and that process's
window share one sequence; it does not attach to a separate running instance. `--hidden` supports
automation and tests. The current catalogue exposes **160 tools**, with English descriptions,
covering editing, graphs, motion, color, audio, media, analyses, exports and history.
See [MCP configuration and tool contracts](docs/MCP.md).

`upstream.lock.json` records relative provenance and SHA-256 fingerprints. Shared snapshots stay
unchanged; video-specific adapters and English presentation are generated outside `vendor/`.
Source roots can be configured with `SF_SHADERIXEL_ROOT`, `SF_CYDAW_ROOT` and `SF_JUCE_ROOT`.

```powershell
python tools/sync_upstream.py --check
python tools/import_shaderixel.py --check
python tools/extract_composition.py --check
python tools/verify_snapshots.py
```

Updates are explicit, keep previous copies in `vendor-history/`, refuse local vendor edits and
require regeneration, rebuilding and compatibility tests. Shaderixel and CyDAW are only read.
Licences and third-party provenance are retained in [NOTICE.md](NOTICE.md).

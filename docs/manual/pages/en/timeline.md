---
id: timeline
title: Timeline, tracks and navigation
description: Read the arrangement and navigate from sequence level down to individual frames and samples.
section: Editing and graphics
order: 6
revision: 1
---

Read the arrangement and navigate from sequence level down to individual frames and samples.

## Using this feature {#usage}

```image
{"id": "timeline-image-1", "src": "assets/editing.png", "alt": "The final-frame filmstrip sits above the video and audio tracks.", "controls": "minimal", "annotations": [{"id": "navigator", "type": "rect", "x": 0.31, "y": 0.54, "toX": 0.77, "toY": 0.59, "label": "Navigation strip"}]}
```
The final-frame filmstrip sits above the video and audio tracks.

- Drag the navigator strip above the ruler left/right to pan and down/up to zoom in/out. Double-click it to fit the sequence. Ctrl+wheel zooms time, Shift+wheel moves through time, wheel scrolls tracks and Alt+wheel changes track height.
- Click the ruler to seek. Drag its upper region, or Shift-drag, to mark a work area; drag its edges or body to resize or move it. I/O set its boundaries; L loops it.
- The FRAME/IMAGE row previews the final composition; clip thumbnails preview individual content, including HLSL and containers. Zoom reveals individual frames. Audio waveform detail progresses from min/max envelopes to samples.
- Z fits the selected clips. Middle-drag or the Hand tool pans. The View control toggles thumbnails, filmstrip, waveforms, keys and automation lanes. Resize the header column and track borders for space.

Track headers provide visibility or mute, solo, lock, automation, color and rename. Drag headers to reorder. Ctrl+G groups the selected clips’ tracks; groups can collapse and expose group mute/solo. Track locks also protect linked editing, so a locked linked member can prevent a cut or move.

[Cut, trim and retime clips](doc:editing)

[Show the navigator annotation](doc:timeline?media=timeline-image-1&note=navigator)

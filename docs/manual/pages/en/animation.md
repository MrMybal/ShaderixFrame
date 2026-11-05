---
id: animation
title: Keyframes, curves and automation
description: Animate clip, graph, widget, channel and device parameters over time.
section: Editing and graphics
order: 14
revision: 1
---

Animate clip, graph, widget, channel and device parameters over time.

## Using this feature {#usage}

```steps
{"items": [{"title": "Step 1", "body": "Select the clip or processing node, place the playhead and set a parameter. Use its K button to write a key at that time. Move the playhead, change the value and add another key."}, {"title": "Step 2", "body": "Right-click K to reveal a timeline automation lane when offered. Double-click the lane to add a point, drag points to move them and right-click to delete."}, {"title": "Step 3", "body": "Open Animation for multiple parameter curves. Choose visible curves; each has its own value range. Click or marquee selects keys; Ctrl/Shift extends the selection."}, {"title": "Step 4", "body": "Drag on the frame grid (Alt for free movement). Double-click adds keys, Delete removes, Ctrl+C/V copies/pastes relative to the playhead, and Alt-drag a segment bends it."}, {"title": "Step 5", "body": "Choose Linear, Smooth, Step or CyDAW interpolation. Wheel zooms time, Shift+wheel or middle-drag pans, F fits and K adds at the playhead. Check the result during playback."}]}
```

```callout
{"type": "note", "title": "Note", "body": "Clip keys move with clip edits. Channel, bus and master automation stays in sequence time when clips move. A parameter change on an already animated audio-device knob writes a key at the playhead. Check which target owns a curve before moving an edit."}
```

[Automating DSP parameters](doc:dsp)

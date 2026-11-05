---
id: containers
title: Nested clip editor
description: Treat a clip as a container with its own tracks, graphs, effects and local time.
section: Editing and graphics
order: 9
revision: 1
---

Treat a clip as a container with its own tracks, graphs, effects and local time.

## Using this feature {#usage}

```image
{"id": "containers-image-1", "src": "assets/clip-editor.png", "alt": "A container opened with a local composition, audio and timeline.", "controls": "minimal"}
```
A container opened with a local composition, audio and timeline.

```steps
{"items": [{"title": "Step 1", "body": "Double-click a video clip to open Clip Editor. A plain clip can become a container containing its original content."}, {"title": "Step 2", "body": "Use its local Media & Effects, Composition, Timeline, Properties, Animation, Shader Graph and Layer Graph. Add video, images, titles, generators or audio just as in a sequence."}, {"title": "Step 3", "body": "Apply to Container sends the local edit to the parent composition. Saving the parent also applies open clip drafts. Reload deliberately discards an unapplied local draft."}, {"title": "Step 4", "body": "Separate Window detaches the editor; Clip Windows reopens its panels. Local selection, undo and playback belong to the nested document. Check both the local preview and the parent’s timing."}]}
```

```diagram
{"type": "flow", "title": "Local tracks + clips → Local effects + graphs → Apply / parent clip → Parent output", "nodes": [{"id": "node-0", "label": "Local tracks + clips", "x": 20, "y": 60, "width": 170}, {"id": "node-1", "label": "Local effects + graphs", "x": 255, "y": 60, "width": 180}, {"id": "node-2", "label": "Apply / parent clip", "x": 500, "y": 60, "width": 180}, {"id": "node-3", "label": "Parent output", "x": 745, "y": 60, "width": 135}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

```callout
{"type": "note", "title": "Note", "body": "The local playhead is not the parent playhead. Source in-points, speed and the parent clip’s placement map between them. Local Shader Time from Zero changes the nested shader origin; it does not move the parent clip."}
```

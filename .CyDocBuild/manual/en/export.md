---
id: export
title: Export and delivery
description: Render the right interval and format, then verify the resulting file.
section: User guide
parent: welcome
order: 30
revision: 3
---

Render the right interval and format, then verify the resulting file.

## Using this feature {#usage}

| Output | Use |
| --- | --- |
| MP4 H264/AAC | Common viewing and sharing; current video path is 8-bit |
| WAV | Stereo 48 kHz audio delivery |
| AVI RGB/PCM | Uncompressed picture and PCM sound |
| PNG8 / PNG16 sequence | Frame-based image interchange |
| EXR half sequence | High-range half-float frame interchange |
| Animated GIF | Short animated previews |
| Loudness JSON | Audio measurements |

```steps
{"items": [{"title": "Step 1", "body": "Save the project, apply/check container edits and confirm source media is online. Inspect the sequence dimensions, frame rate, working space and duration."}, {"title": "Step 2", "body": "Choose the full sequence, selection, work area or marker region. Work Area Only restricts the interval; clear it when exporting the whole edit."}, {"title": "Step 3", "body": "Choose a format/preset, a destination and output dimensions. HD, UHD, vertical and square presets are starting points. Start a background job from Exports."}, {"title": "Step 4", "body": "Follow progress in Exports or the status bar. Cancel before finalization to preserve an existing destination. After success, play the produced file and check first/last frames, sync, subtitles, transitions and tails."}]}
```

A job captures the sequence and effect catalogue at launch; later edits do not alter that job. Media remains read by reference, so keep source files unchanged. The queue accepts 32 active/waiting jobs, runs one at a time and retains eight recent completed jobs. Closing the application cancels pending work and waits for workers.

```diagram
{"type": "flow", "title": "Sequence snapshot → Render + audio mix → Encode / finalize → Verify output", "nodes": [{"id": "node-0", "label": "Sequence snapshot", "x": 20, "y": 65, "width": 165}, {"id": "node-1", "label": "Render + audio mix", "x": 245, "y": 65, "width": 170}, {"id": "node-2", "label": "Encode / finalize", "x": 475, "y": 65, "width": 170}, {"id": "node-3", "label": "Verify output", "x": 705, "y": 65, "width": 175}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

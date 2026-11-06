---
id: dsp
title: DSP graph editor
description: Build stereo audio processing with CyDAW v2 graph validation and rendering.
section: User guide
parent: devices
order: 26
revision: 3
---

Build stereo audio processing with CyDAW v2 graph validation and rendering.

## Using this feature {#usage}

```image
{"id": "dsp-image-1", "src": "assets/audio-dsp.png", "alt": "A parallel EQ graph: split, two EQ branches, stereo sum, output.", "controls": "minimal"}
```
A parallel EQ graph: split, two EQ branches, stereo sum, output.

```steps
{"items": [{"title": "Step 1", "body": "Open DspStudy.sframe and DSP Graph. Choose the intended channel and graph instance. New Graph prepares a draft; an existing instance loads its embedded graph."}, {"title": "Step 2", "body": "Use Add DSP to search and add processors. Connect stereo ports from STEREO INPUT through processing to STEREO OUTPUT. Arrange or Fit All helps organize the canvas."}, {"title": "Step 3", "body": "For parallel EQ, feed two EQ nodes from the input, connect both to a stereo sum and connect the sum to output. Adjust branch gain to avoid clipping or an unintended doubled level."}, {"title": "Step 4", "body": "Select a node for its parameters; expose or automate the relevant values. Compile and Apply validates the draft and commits it as one undoable edit. Adding a new graph inserts it in the channel chain."}, {"title": "Step 5", "body": "Listen to the processed output and compare bypass. Save the project to preserve topology and parameter automation. Use the supplied ParallelEQ.json to exchange a compatible graph."}]}
```

```callout
{"type": "note", "title": "Note", "body": "Edits in the DSP canvas are a draft until applied. Wheel zooms, middle-drag pans, Tab/double-click adds DSP and Delete removes selected nodes. The graph carries stereo audio, not video surfaces. Feedback cycles are not a supported way to build a delay."}
```

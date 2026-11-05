---
id: layers
title: Layer compositing graph
description: Combine track surfaces through explicit branches, blends and corrections.
section: Editing and graphics
order: 10
revision: 1
---

Combine track surfaces through explicit branches, blends and corrections.

## Using this feature {#usage}

```image
{"id": "layers-image-1", "src": "assets/layer-graph.png", "alt": "Layer Graph is the composition graph, separate from shader compilation.", "controls": "minimal"}
```
Layer Graph is the composition graph, separate from shader compilation.

Each video track produces a transparent surface from its timeline clips. The layer graph connects these surfaces to blend, correction, effect, LUT, Shader Graph and output nodes. The automatic stack follows track order, blend modes and opacity; an explicit graph allows branches, shared inputs and local corrections before blending.

```steps
{"items": [{"title": "Step 1", "body": "Open Layer Graph and frame the graph with Fit All. Rebuild from Tracks constructs a graph from the current stack; use Undo if you wanted to keep an existing custom arrangement."}, {"title": "Step 2", "body": "Connect an output port to a compatible input. For a blend, connect the background image and the foreground layer; supply a mask when needed. End the composition at the output node."}, {"title": "Step 3", "body": "Select a node to edit its properties. Add corrections, LUTs or a Shader Graph node where processing belongs. Animate exposed parameters with keys in sequence time."}, {"title": "Step 4", "body": "Wheel zooms, middle-drag pans and right-click offers node actions. Right-click an input to disconnect it. Inspect live previews and the final image to confirm routing."}]}
```

```callout
{"type": "note", "title": "Note", "body": "Layer Graph decides how images are composed. Shader Graph builds an image-processing shader. DSP Graph processes audio. They share interface components but serve different signal types; they are not interchangeable."}
```

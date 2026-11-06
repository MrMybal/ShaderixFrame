---
id: track-graphs
title: Track graphs and adjustment layers
description: Process the contents of one track before the composition blends that track.
section: User guide
parent: layers
order: 11
revision: 3
---

Process the contents of one track before the composition blends that track.

## Using this feature {#usage}

Track Graph takes the composed content of a video track as its input. Its effect, correction, LUT and Shader Graph nodes process that surface; the result then enters track blending and the layer graph. Side inputs can read lower tracks for mattes or displacement. Each node has a live preview.

```steps
{"items": [{"title": "Step 1", "body": "Select a track and use its FX button or open Track Graph. Drop an effect card on FX or the panel to append processing."}, {"title": "Step 2", "body": "Connect the track input through the chosen nodes to output. Compare with bypass to verify that the graph processes the whole track."}, {"title": "Step 3", "body": "Use the track context menu for opacity, one of the 27 blend modes or a library blend effect. Dropping a blend card on FX assigns that blending behavior."}, {"title": "Step 4", "body": "Use an adjustment layer for a correction applied to the layers below across its duration. Keep it above the intended tracks and trim it to the affected interval."}]}
```

```diagram
{"type": "flow", "title": "Clips on a track → Track Graph → Track blend / opacity → Layer Graph / output", "nodes": [{"id": "node-0", "label": "Clips on a track", "x": 15, "y": 70, "width": 155}, {"id": "node-1", "label": "Track Graph", "x": 230, "y": 70, "width": 170}, {"id": "node-2", "label": "Track blend / opacity", "x": 460, "y": 70, "width": 175}, {"id": "node-3", "label": "Layer Graph / output", "x": 695, "y": 70, "width": 190}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

---
id: masks
title: Masks, keying and tracking
description: Limit an effect to a region or isolate a foreground object.
section: User guide
parent: composition
order: 17
revision: 3
---

Limit an effect to a region or isolate a foreground object.

## Using this feature {#usage}

```steps
{"items": [{"title": "Step 1", "body": "Select a clip and open Mask in Properties. Choose rectangle, ellipse or a closed Bezier path. Enable Mask Guides in Composition for direct editing."}, {"title": "Step 2", "body": "Move, resize or rotate the mask; adjust feathering for the desired edge. Invert swaps inside and outside. For a Bezier mask, place and adjust the closed path and its handles."}, {"title": "Step 3", "body": "Animate supported mask transforms with K. For translation tracking choose a contrasting region and a time range, run the tracker, then inspect its keys throughout the shot."}, {"title": "Step 4", "body": "For a green/blue screen, choose a keying effect and sample/tune its target. Inspect edges and alpha on a contrasting background; use a mask to exclude irrelevant regions."}]}
```

```callout
{"type": "note", "title": "Note", "body": "The tracker estimates translation, not a full camera solve, planar track or automatic rotoscoping. Large rotations, occlusion and weak contrast can require manual corrections. A grading correction has its own local mask, separate from the clip’s overall mask."}
```

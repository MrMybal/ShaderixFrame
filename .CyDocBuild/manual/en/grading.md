---
id: grading
title: Color grading workflow
description: Balance the image, build a look and isolate local corrections.
section: User guide
parent: composition
order: 19
revision: 3
---

Balance the image, build a look and isolate local corrections.

## Using this feature {#usage}

```image
{"id": "grading-image-1", "src": "assets/color-grading.png", "alt": "Independent corrections with primaries and comparison controls.", "controls": "minimal"}
```
Independent corrections with primaries and comparison controls.

```steps
{"items": [{"title": "Step 1", "body": "Open GradingStudy.sframe, select its picture clip and switch to Color Grading (F4). Add or select a correction in its ordered chain."}, {"title": "Step 2", "body": "Start with neutral balance. Use the eyedropper on a neutral area, then adjust temperature, tint and RGB balance. Set exposure and contrast before a creative look."}, {"title": "Step 3", "body": "Use Wheels for tonal color control and Curves for shaped tonal/color response. Add deliberate points; avoid extreme slopes unless intended. Toggle the correction to compare."}, {"title": "Step 4", "body": "Use Selection & Mask to qualify a color or limit the correction spatially. Inspect the matte, refine boundaries/softness and check moving frames rather than a single still."}, {"title": "Step 5", "body": "Keep balancing, look and local retouch as separate corrections. Reorder or bypass them to understand their contribution. LUT & Looks applies an embedded LUT or preset; animate supported values with K."}]}
```

Store Reference records a comparison image; Load Reference reads one from disk. Use compare modes and the split wipe to assess matching. Auto Color estimates balance/exposure and Color Match uses a reference, but both need visual review. Layer and track graphs can apply a correction before blending, after blending or only on one branch.

[Read scopes and precision limits](doc:scopes)

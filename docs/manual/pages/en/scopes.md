---
id: scopes
title: Scopes and image evaluation
description: Use measurements alongside the preview, with the correct interpretation.
section: Editing and graphics
order: 20
revision: 1
---

Use measurements alongside the preview, with the correct interpretation.

## Using this feature {#usage}

```image
{"id": "scopes-image-1", "src": "assets/video-scopes.png", "alt": "Luma waveform and below/above-range counts for a graded composition.", "controls": "minimal"}
```
Luma waveform and below/above-range counts for a graded composition.

Open Video Scopes and choose a measure such as luma waveform or the available color distributions. Watch the horizontal positions, luminance spread and clipping while changing exposure, contrast and saturation. Compare before/after to distinguish a deliberate look from lost detail.

The panel reports its sampling and refresh behavior. Scopes are after the SDR display/tone-mapping path, with limited sampling and roughly 4 Hz refresh; they are not a full-resolution HDR mastering instrument. Below-zero/above-white counts and working-image values can reveal headroom that an SDR preview cannot show.

```steps
{"items": [{"title": "Step 1", "body": "Evaluate a shot at several times, particularly highlights, skin tones and dark transitions."}, {"title": "Step 2", "body": "Use the viewer’s reference wipe for matching neighboring shots. Recheck with grading bypassed."}, {"title": "Step 3", "body": "For high-precision delivery, export a PNG16/EXR half frame and verify it in a compatible image workflow. Do not infer a 10/12-bit video output from the float compositor alone."}]}
```

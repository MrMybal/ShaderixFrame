---
id: loudness
title: Loudness and mastering checks
description: Measure the complete mix and avoid confusing peak level with perceived loudness.
section: Audio and delivery
order: 31
revision: 1
---

Measure the complete mix and avoid confusing peak level with perceived loudness.

## Using this feature {#usage}

LUFS describes measured program loudness; dBTP describes true peak. A short transient can reach the peak ceiling while dialogue remains quiet. Clip peak normalization is different from measuring and normalizing the complete program. Use the full intended delivery interval for meaningful integrated measurements.

```steps
{"items": [{"title": "Step 1", "body": "Use Sequence > Normalize Loudness and choose the provided broadcast, podcast or web target that matches the delivery specification."}, {"title": "Step 2", "body": "Let the background measurement finish. The optional -1 dBTP master limiter controls peaks; inspect its gain reduction and listen to demanding passages."}, {"title": "Step 3", "body": "Measure again after changes to mix, limiter or interval. Export loudness JSON when a report is useful."}, {"title": "Step 4", "body": "Listen at moderate level to the exported movie as well as the edit. Check dialogue intelligibility, stereo balance, cut clicks, clipping and reverberation decay."}]}
```

```callout
{"type": "note", "title": "Note", "body": "The provided targets are presets, not a guarantee of compliance with every distributor. Use that distributor’s current delivery requirements and verify the actual exported file."}
```

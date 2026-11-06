---
id: audio-edit
title: Audio editing and cleanup
description: Trim sound, maintain sync and prepare dialogue or music.
section: User guide
parent: welcome
order: 23
revision: 3
---

Trim sound, maintain sync and prepare dialogue or music.

## Using this feature {#usage}

Audio clips live on timeline tracks and can be linked to their picture. Drag fade handles to avoid abrupt boundaries. Drag the clip gain line from -60 to +12 dB; double-click resets to 0 dB. Zoom waveform detail to find transients and quiet boundaries. Clip gain and pan are applied before the track device chain.

```steps
{"items": [{"title": "Step 1", "body": "Use Normalize Clip Audio for peak normalization (the supplied command targets -1 dBFS). This changes clip level; it is not integrated loudness normalization."}, {"title": "Step 2", "body": "Enhance Voice adds gate, EQ and compressor. Listen for cut syllables, pumping or excessive brightness, then adjust device parameters."}, {"title": "Step 3", "body": "Remove Silence analyzes an audio source and ripples removed intervals across the sequence. Choose a conservative setting first; linked clips and locks are checked. Preview the resulting speech rhythm."}, {"title": "Step 4", "body": "Auto Duck lowers a music track under a voice track through automation. Detect transients to create music markers; synchronize audio clips when recordings need alignment."}, {"title": "Step 5", "body": "Choose a time-adaptation mode for stretched audio and listen for artifacts. Reverse and speed ramps affect sound as described in the editing chapter."}]}
```

Analyses report progress and cancellation in the status bar and apply their result as an undoable change. Ctrl+Z restores the previous edit if a cleanup removed wanted material.

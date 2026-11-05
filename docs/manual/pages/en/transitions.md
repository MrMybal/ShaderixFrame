---
id: transitions
title: Transitions and fades
description: Control changes between shots on one track or across several tracks.
section: Editing and graphics
order: 18
revision: 1
---

Control changes between shots on one track or across several tracks.

## Using this feature {#usage}

```steps
{"items": [{"title": "Step 1", "body": "Place two clips so the transition has the intended temporal overlap. Select the incoming clip and apply a transition from its menu or the Effects Browser."}, {"title": "Step 2", "body": "Drag the transition’s end handle to set duration. Edit its type or progress parameters; use playback around the cut to verify timing."}, {"title": "Step 3", "body": "For a node transition, connect the two images and animated progress to the transition output. Transitions can combine clips on one track or across tracks."}, {"title": "Step 4", "body": "Audio fades are separate: adjust clip fade handles and listen at normal playback speed. Shift+D creates a crossfade at the nearest cut when applicable."}]}
```

Transitions are evaluated after clip effects and transforms. A custom transition shader has a dedicated progress value and two-image contract, and is single-pass. Start from a provided transition rather than adapting an ordinary effect blindly. A blend effect applied as a persistent clip effect can continue outside the automatic transition interval.

---
id: recovery
title: Saving, history and recovery
description: Recover work and distinguish project edits from reading or navigation.
section: Audio and delivery
order: 32
revision: 1
---

Recover work and distinguish project edits from reading or navigation.

## Using this feature {#usage}

Ctrl+Z/ Ctrl+Y undo and redo document edits. History lists named steps. Local clip editors have independent histories until applied to their parent. Selecting, seeking, zooming or reading the manual does not create a project edit.

```steps
{"items": [{"title": "Step 1", "body": "Save regularly with Ctrl+S. Every save keeps the replaced project among up to ten previous versions in the project’s versions folder."}, {"title": "Step 2", "body": "File > Previous Versions lists saved versions. Restoring one is an undoable change; inspect it and save a separate copy if you want to preserve both edits."}, {"title": "Step 3", "body": "File > Recover Session opens Session Recovery. Recovery snapshots are separate from saved versions and are taken periodically and at document/exit transitions."}, {"title": "Step 4", "body": "Open the recovery candidate, verify media and container content, then save it to a clear project filename. Keep a separate backup of important project files and sources."}]}
```

Enter pre-renders the work area in memory and a green ruler bar shows cached frames. Editing invalidates that cache. A render cache is a playback optimization, not a saved project or a replacement for a final export.

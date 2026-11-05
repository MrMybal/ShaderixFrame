---
id: troubleshooting
title: Troubleshooting and current limits
description: Check common causes before rebuilding an edit or discarding work.
section: Reference
order: 36
revision: 2
---

Check common causes before rebuilding an edit or discarding work.

## Using this feature {#usage}

| Symptom | Checks |
| --- | --- |
| Black or transparent preview | Seek beyond a fade; check clip duration, visibility, opacity, output connections, media status and whether a container draft was applied |
| No sound | Sequence > Sound Output; track/clip mute, solo, gain, routing, media audio stream and worker error; status reports device failure |
| Missing media | File > Relink Missing Media; choose the relocated folder; confirm duplicate filenames |
| Slow preview | Use proxies, fit the viewer, reduce heavy effects or press Enter to cache the work area |
| Graph has no effect | Check selected clip/channel/graph instance, output connection, bypass and Compile and Apply for DSP drafts |
| Keys or shortcuts seem wrong | Click the intended panel; text fields, Source, curves and graphs have local keyboard behavior |
| Plugin fails | Check x64 compatibility, required worker, plugin version/path and the host’s supported contract |
| Export differs from later edit | Jobs capture launch-time state; start a new job after changes |
| Missing manual or picture | Restore the complete help folder, including all scripts, styles and assets |

View > Reset This Workspace restores panels if a layout becomes confusing. Recovery and previous versions can restore edits. Keep the source project before testing a workaround. --warp selects software D3D11 for diagnosing GPU problems; it is slower than a working hardware path.

```callout
{"type": "note", "title": "Note", "body": "Current scope: Windows x64, stereo audio, 8-bit video decode/encode, RGBA16F internal composition, limited SDR-display scopes and a supported OFX filter subset. No full OCIO/ACES/HDR-monitor workflow is claimed. The advanced timeline and curve editor retain application-specific interactions even while common controls use CyUI."}
```

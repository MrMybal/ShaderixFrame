---
id: editing
title: Selection, trimming and retiming
description: Edit linked clips precisely while preserving synchronization.
section: Editing and graphics
order: 7
revision: 1
---

Edit linked clips precisely while preserving synchronization.

## Using this feature {#usage}

| Tool | Key | Gesture |
| --- | --- | --- |
| Select | V | Move bodies; trim edges |
| Blade | B | Cut under pointer; Shift for all tracks |
| Slip | Y | Change source in-point while placement stays fixed |
| Roll | N | Move a shared cut |
| Rate Stretch | R | Drag an edge to change playback speed |
| Slide | U | Move a clip and trim neighbors |
| Hand | H | Pan the view |

Ctrl/Shift-click toggles selection, a marquee selects a region and Ctrl+A selects all. Linked video/audio follows ordinary edits. Ctrl+C/X/V copies, cuts and pastes at the playhead; Paste Here uses the context position. Ctrl+D duplicates after the selection. Ctrl+Alt+V pastes chosen attributes, including effects and animation, onto the current selection.

S splits at the playhead. Q trims the head and W the tail, closing the removed interval; Alt keeps the gap. Delete removes selection and Shift+Delete closes its intervals. Ripple operations affect the sequence, markers and moved keys. Split a clip that crosses an interval to be closed, or include it in the selection. Alt temporarily bypasses snapping during a drag.

Ctrl+R sets speed/duration. Reverse reads the marked source range backward, including linked audio. Shift+F inserts a two-second freeze frame; the context menu offers other durations. Shift+E disables a clip without removing it. Speed-ramp automation integrates source time; its audio changes pitch like tape. Use the audio time-adaptation choice for constant-speed stretching when appropriate.

Right-click gaps or the ruler for Insert Gap, Lift Work Area (leave a hole), Extract Work Area (close it) and closing gaps. Sequence > Close All Gaps removes intervals empty across all tracks. Check downstream synchronization after any ripple or gap command.

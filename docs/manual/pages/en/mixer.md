---
id: mixer
title: Mixer, buses and sends
description: Route audio through tracks, shared effects buses and the master.
section: Audio and delivery
order: 24
revision: 1
---

Route audio through tracks, shared effects buses and the master.

## Using this feature {#usage}

```image
{"id": "mixer-image-1", "src": "assets/audio-mixer.png", "alt": "Track, bus, auxiliary and master channel strips.", "controls": "minimal"}
```
Track, bus, auxiliary and master channel strips.

```diagram
{"type": "flow", "title": "Clip gain / pan → Track DSP / fader → Bus / master → Output device → Aux send / reverb", "nodes": [{"id": "node-0", "label": "Clip gain / pan", "x": 15, "y": 20, "width": 160}, {"id": "node-1", "label": "Track DSP / fader", "x": 245, "y": 20, "width": 170}, {"id": "node-2", "label": "Bus / master", "x": 490, "y": 20, "width": 160}, {"id": "node-3", "label": "Output device", "x": 715, "y": 20, "width": 170}, {"id": "node-4", "label": "Aux send / reverb", "x": 245, "y": 145, "width": 170}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}, {"from": "node-0", "to": "node-4"}, {"from": "node-4", "to": "node-2"}]}
```

```steps
{"items": [{"title": "Step 1", "body": "Open Mixer & Audio Graph in Audio (F5). Select a track, bus or master, then set gain, pan, mute/solo and the output destination."}, {"title": "Step 2", "body": "Create a bus to sum related tracks, then route those tracks to it. Add a send to share reverb or delay without inserting a separate copy on every track."}, {"title": "Step 3", "body": "Choose pre-fader or post-fader send and set its level. A pre-fader send is independent of the channel fader; a post-fader send follows it. Keep a parallel reverb’s dry/wet behavior consistent with your routing."}, {"title": "Step 4", "body": "Use graph and signal views to inspect connections, waveform, spectrum and levels. Cycles are rejected even through disabled sends. Automate channel gain/pan or send levels when needed."}]}
```

The engine compensates declared fixed processing latency. Track pan combines with clip pan before track DSP; bus pan follows bus DSP. Effects continue through silence until the sequence ends so tails can render; leave enough sequence duration for a final decay. Sequence > Sound Output selects the playback device.

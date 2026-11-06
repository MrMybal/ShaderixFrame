---
id: mcp
title: Integrated MCP automation
description: Connect an MCP client to the application’s own session over stdio.
section: User guide
parent: api-links
order: 33
revision: 3
---

Connect an MCP client to the application’s own session over stdio.

## Using this feature {#usage}

Start ShaderixFrame.exe with --mcp to expose JSON-RPC 2.0 over standard input/output, one JSON message per line. --hidden supports background automation. The client and that process’s interface share one sequence; this does not attach to a different already-running instance. The 0.14 catalogue exposes 163 tools; tools/list is the authoritative schema source.

```json
{
  "mcpServers": {
    "shaderixframe": {
      "command": "path/to/ShaderixFrame.exe",
      "args": ["--mcp"]
    }
  }
}
```

```steps
{"items": [{"title": "Step 1", "body": "Replace command with the executable’s path in your local MCP client configuration. Use the client’s normal process-launch mechanism and a writable project/media location."}, {"title": "Step 2", "body": "Initialize the MCP connection, list tools, then call status to inspect the current sequence and selection. Query catalogues before choosing effect IDs or parameter names."}, {"title": "Step 3", "body": "Use stable clip/track IDs returned by imports or status. Time is in seconds; effect indexes start at zero. An omitted effect or -1 generally targets clip parameters when the tool contract permits it."}, {"title": "Step 4", "body": "Validate responses: isError reports a rejected operation. Save explicitly with save_project. For long work use start_export, export_status and cancel_export rather than blocking the client."}]}
```

| Area | Representative tools |
| --- | --- |
| Projects / edit | status, open_project, save_project, select_clips, move_clips, split_clips |
| Graphics | get_graph, set_graph, set_shader, add_effect, set_keyframe |
| Mixing | set_mix_channel, set_send, add_mix_effect, set_mix_keyframe |
| Delivery | render_frame, start_export, export_status, cancel_export |
| History | undo, redo |

```callout
{"type": "note", "title": "Note", "body": "For complete per-tool arguments use tools/list in the connected client. The MCP and SDK reference in this reader explains each public tool and callback. MCP is a local process transport; it is not a network control server or a connection to another open editor."}
```

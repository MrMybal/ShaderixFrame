---
id: api-links
title: MCP and SDK reference
description: Automate editing or develop effects through the public interfaces.
section: Developer reference
parent: welcome
order: 100
revision: 1
---

Choose [MCP](doc:mcp) to automate the editor, [the native SDK](doc:reference-sdk) to create stereo audio effects, or [HLSL](doc:reference-hlsl) to write image processing. Each interface has its own data types and execution context.

## Calling conventions {#calling}

The MCP catalogue exposes 163 operations. Connect over stdio, call `tools/list` for current input schemas, then call `tools/call` with `name` and `arguments`. Clip and track IDs are stable identifiers; effect positions are zero-based indices. Time is in seconds unless the operation says otherwise. Query `status` and `list_effects` before reusing IDs.

A successful tool reply sets `isError: false`, returns the operation object in `structuredContent`, and includes the same JSON in `content[0].text`. A rejected tool sets `isError: true` and puts its explanation in `content[0].text`. Invalid JSON-RPC requests use `error` instead. An omitted optional input is interpreted by that operation; it does not imply a universal default.

## A read-only request {#example}

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"status","arguments":{}}}
```

For a long render, use `start_export`, poll `export_status` with the returned job ID and cancel with `cancel_export` if required. Save project edits explicitly with `save_project`. Editing tools and the process's window work on the same sequence.

## Find an operation {#tools}

Expand the task groups below this page in the sidebar, or type an operation name in the global search field. Each operation page contains its exact input schema, required/optional arguments, parameter explanations and result conventions. API identifiers remain unchanged in both languages.

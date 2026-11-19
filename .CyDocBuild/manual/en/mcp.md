---
id: mcp
title: Integrated MCP automation
description: Use the integrated assistant, command line and MCP on the same application operations.
section: User guide
parent: api-links
order: 33
revision: 6
---

Use the integrated assistant, command line and MCP on the same application operations.

## Using this feature {#usage}

Start ShaderixFrame.exe with --mcp to expose JSON-RPC 2.0 over standard input/output, one JSON message per line. --hidden supports background automation. The client and that process’s interface share one sequence; this does not attach to a different already-running instance. The 0.15 editor exposes 163 business operations plus 23 interface tools. The standalone CLI exposes the business operations; tools/list is the authoritative catalogue for the connected instance.

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
{"type": "note", "title": "Alpha 0.15", "body": "The assistant and console companion are included with this alpha. Earlier 0.14 packages do not contain them. Providers, models and credentials are configured separately."}
```

## Integrated assistant {#assistant}

Open **View > Assistant**, or start with `--assistant`. The panel uses CyMCPSDK ChatUI and native CyUI Markdown rendering. Open **Assistant settings**, detect installed providers, select a detected connection or enter its provider ID and settings, then choose **Connect / New chat**. Discovery runs on a worker and sends no prompt. No provider is silently substituted after an error.

Use an existing Ollama, LM Studio or llama.cpp service for local models, a compatible API endpoint, or an installed native CLI such as Codex. Select a model from the actual returned catalogue; a remote API requires an explicit model choice. API keys come from the named environment variable and remain outside conversation files. Executable arguments are a JSON array and are launched without a shell. Models, engines, CLIs and credentials are not bundled or installed automatically.

**AI may use application tools** attaches the shared registry on the next connection. Tool-capable transports include Codex dynamic tools and compatible API function calls. Other native agents need their own configured MCP connection; attaching tools to an unsupported transport reports an error. Native agent permissions remain separate from application permissions.

Messages stream into the transcript. Cancel interrupts an active turn while retaining its partial response. Errors remain visible. Attachments selected with the composer are loaded on its worker and transmitted only when you send. Their supported types depend on the provider/model. Enable **Save conversation history** for private atomic checkpoints under the assistant's conversation directory. Restore uses the selected history ID and the exact saved native session; it never automatically resends a turn. Reconnect preserves that session.

## Command line {#cli}

`ShaderixFrameCLI.exe` is a console companion. `ShaderixFrame.exe --cli` accepts the same commands and bypasses window creation. Existing application launch options remain available outside CLI mode.

```powershell
ShaderixFrameCLI.exe help
ShaderixFrameCLI.exe list
ShaderixFrameCLI.exe describe set_sequence
ShaderixFrameCLI.exe invoke status --project=examples/CutStudy.sframe --warp
ShaderixFrameCLI.exe invoke set_sequence --json='{"name":"Scripted edit"}' --allow-changes --save=edited.sframe --warp
```

Use `--args=arguments.json` instead of inline JSON when shell quoting is inconvenient. Output is JSON with `ok` and `result`, `operations` or `operation`. Errors produce JSON on stdout plus diagnostics on stderr. Exit codes: 0 success, 2 invalid arguments/JSON/schema, 3 permission or confirmation required, 4 authentication, 1 another failure. A standalone invocation owns its sequence; use `--save` for durable edits. Standalone exports keep the process alive until its export jobs finish. They do not attach to a running editor implicitly.

## Connect to the open editor {#instances}

In Assistant settings, **Start local MCP** creates an authenticated loopback server with a random port/token. **Copy connection file path** copies its private descriptor path. Alternatively launch:

```powershell
ShaderixFrame.exe --mcp-http --mcp-info=private/instance.json --assistant
ShaderixFrameCLI.exe invoke status --connect=private/instance.json
```

Choose the descriptor for the intended instance. The file contains a credential: keep it private and do not commit or share it. It is removed on normal shutdown. External MCP clients use its URL plus `/mcp` and a Bearer token; direct SDK JSON requests use `/v1`. There is no unauthenticated endpoint or public-network bind. The SDK server supports current discovery and negotiated legacy MCP sessions. Standard input/output remains available through `--mcp`; `--mcp-compact` exposes the compact gateway instead. `ShaderixFrameCLI.exe mcp` offers the same tools with no window. `serve --info=private/instance.json` hosts a standalone loopback session.

## Permissions and editor state {#permissions}

All 163 existing business operations retain their names and real engine handlers. The chat, CLI and MCP use the same registry and policy. Read operations are permitted by default; **Allow application changes** enables mutations for that instance. A connected CLI cannot override this setting. Calls arriving from workers are queued to the session's owning thread. Project validation, rollback on failure, Undo/Redo and the existing background export/tracking systems remain in use.

Replacing an unsaved project through the new integration requires saving first or explicitly choosing **Allow replacing unsaved project once**. A standalone script may use `--confirm-discard`. The existing dedicated `--mcp` process retains its previous permission/discard behavior for compatibility; its chat and HTTP endpoint share that process policy too. No command is automatically replayed after a timeout, cancellation or expired result reference.

The running GUI also registers editor interface operations described below. A standalone console process has no editor windows; use `--connect` for those operations. Provider account authentication still uses the provider's own login process. No filesystem shell or unrestricted external tool catalogue is exposed.

## Editor interface automation {#interface}

The assistant, connected CLI and MCP can inspect and operate the actual editor interface in addition to the 163 business operations. `get_ui_state` reports open panels, mode, monitor geometry, theme/language, navigation and the active nested editor. `list_ui_commands` discovers stable command IDs and their enabled state. Labels follow the selected UI language; IDs stay stable.

| Need | Operations |
| --- | --- |
| Command palette | `list_ui_commands`, `ui_command` |
| Modes and windows | `ui_workspace`, `ui_panel` (show/close/focus, resize, detach, dock with another panel) |
| View navigation | `ui_view` (timeline, composition, shader/layer/track/DSP graphs, motion) |
| Theme, language and shortcuts | `ui_preferences`, `list_ui_shortcuts`, `ui_shortcut` |
| Effect/channel/source selection | `ui_focus`, `ui_open_source` |
| Nested clip editing | `ui_clip_editor`, `get_ui_clip_project`, `ui_clip_invoke` |
| Visible controls | `get_ui_items`, `ui_input`, `get_ui_input`, `ui_cancel_input` |
| Native plugin controls | `get_ui_native_windows`, `get_ui_native_controls`, `ui_native_control` |
| File selection and help | `ui_file_list`, `ui_show_help` |

All modifying UI calls require **Allow application changes**, including clicks and keyboard input. This prevents UI gestures from enabling their own permission. Business commands triggered through automation retain validation, Undo and unsaved-project confirmation; synthetic input cannot approve the user's confirmation button. File and folder commands accept `file_path`; omitting it reports `PathRequired` instead of opening a blocking native picker. The attachment picker accepts the same path during automated input. Directory discovery reads only the explicitly supplied directory, without recursion or file contents.

Prefer semantic commands and parameters. For a remaining visible control, query `get_ui_items` for its ID and current rectangle, then request a click, text replacement, key, wheel or drag with `ui_input`. The input is delivered across render frames entirely inside this application's ImGui context. Poll `get_ui_input` for the returned job before sending the next gesture, and inspect the resulting document/UI state. Completion confirms delivery, not the success of a business edit. Missing/disabled controls fail; cancellation, revoked permissions and application shutdown release synthetic keys/buttons. Input is never replayed. Only the latest input job is retained, privately for its caller. `include_active_text` optionally reads the focused standard text input; password fields are excluded.

Use `workspace: [containerClipId, ...]` to address an already-open nested clip editor. `ui_clip_invoke` accepts a business operation name and its usual JSON arguments. Changes stay in the real local draft until `ui_clip_editor` with `action: "apply"`; reload refuses unsaved drafts. Panels, commands and views accept the same workspace path.

Native plugin control discovery is limited to this workspace's actual open VST3 worker processes. Windows accessibility provides invoke/value/range/toggle/selection/expand actions where the plugin supports them. Locked channels cannot be edited through this route. Custom plugin surfaces without accessible controls remain controlled through the plugin's exposed parameters and state API. Other applications, browser/account windows and credentials are outside this window bridge.

```powershell
ShaderixFrameCLI.exe invoke ui_workspace --json='{"mode":"audio"}' --connect=private/instance.json
ShaderixFrameCLI.exe invoke ui_panel --json='{"panel":"DSP Graph","visible":true}' --connect=private/instance.json
```

## Independent runtime options {#optimizations}

Pass `--automation-config=FILE.json` to the application or CLI. Start from `docs/assistant-config.example.json`. Each option is independent: `compact`, `disk_spooling`, `notifications`, `performance`, `inline_bytes`, `chunk_bytes`, `memory_bytes`, `disk_bytes`, `retention_seconds` and `cache_seconds`. The default private root is `%LOCALAPPDATA%/ShaderixFrame/Assistant`; `private_directory` overrides it.

Compact chat discovery exposes five tools: search, describe, call, resource read and native literal grep. Search for a command, load its full schema, then call it. Full MCP discovery remains available. `shaderixframe://project` is a bounded immutable JSON snapshot of the current sequence. UI edits refresh that snapshot within about half a second; automation edits refresh it immediately. Resource manifests and `firstUri`/`nextUri` chunks preserve full content during retention. References are private to the calling principal; expired ones fail without rerunning an action. Disk spooling is off by default; when enabled its default quota is 512 MiB. No ripgrep process or arbitrary file tree is enabled.

Registry/resource changes feed the SDK change journal when notifications are enabled. Subscription support depends on the negotiated transport; clients must retain TTL and explicit-refresh fallbacks. Discovery caching uses the configured TTL; mutations are never cached. Opt-in `performance` writes private asynchronous JSONL containing operation names, times, byte counts, cache status and result status. Messages, arguments, results, credentials and caller IDs are excluded.

**Context budget (KiB, 0 = off)** limits the projected history of API providers on the next connection. Full history stays in the application; system instructions, the active turn and complete tool exchanges stay together. No automatic summarizer is installed. Native CLI agents retain their own context management.

## Selected tools from other applications {#remote}

Configure `remote_servers` in the host-selected automation file, then choose **Connect allowed remote tools**. Each entry requires an `id` and explicit `allow_tools` names, plus either `url`/`token_env` for HTTP or `executable`/`arguments` for stdio. Empty allowlists expose nothing. The SDK follows tool pagination and preserves schemas. Selected tools appear as `remote.ID.NAME` in the shared registry and chat. Remote business errors remain errors; a successful HTTP exchange does not imply a successful action. Mutation permission applies to these tools as well. Remote credentials are obtained from the environment, not saved with chat history.

An explicit `allow_resources` URI list also adds `remote.ID.read_resource`. Only those resources and their returned chunk links can be read. Compatible peers use SDK subscriptions to invalidate cached discovery/resource reads before the TTL; other peers retain TTL and explicit reconnection/refresh. Mutations clear the remote read cache. The SDK schema profile is deliberately bounded: unsupported validation keywords such as `$ref`, `pattern` and `format` are rejected rather than treated as supported.


```image
{"src":"assets/assistant.png","alt":"ShaderixFrame with the native Assistant panel beside the composition and timeline.","caption":"Assistant settings, live editor and message composer in alpha 0.15."}
```

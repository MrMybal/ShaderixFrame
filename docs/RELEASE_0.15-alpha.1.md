# ShaderixFrame 0.15.0-alpha.1

[Français](RELEASE_0.15-alpha.1.fr.md) · [Release and downloads](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.15.0-alpha.1)

**Alpha preview for testing and feedback.** This release adds an assistant that
works on the real editor state, a command-line companion and broader MCP access.
Features, project contracts and plugin compatibility may evolve before a stable release.

## Assistant and automation

- Native CyMCPSDK ChatUI in **View > Assistant**, using the application's CyUI/ImGui interface.
- Provider discovery, model selection, streaming, cancellation, error display,
  attachments and optional private conversation history/restoration.
- Existing Ollama, LM Studio and llama.cpp services, compatible APIs and supported
  installed CLI providers. Engines, model weights and credentials are configured separately.
- A shared registry preserves all 163 business operations and adds 23 live editor tools:
  modes, panels/docking, view navigation, preferences, shortcuts, source/effect selection,
  nested clip drafts, visible controls and accessible native plugin controls.
- `ShaderixFrameCLI.exe` and `ShaderixFrame.exe --cli` support help, discovery,
  schemas and JSON invocation. Use `--connect` to target a particular open editor.
- MCP stdio remains available. Authenticated loopback HTTP uses a private instance
  descriptor and Bearer token; connected CLI calls share the editor's permissions.
- Compact discovery, bounded resource/chunk reads, optional disk spooling, cache
  notifications, metadata metrics and an API context window have independent settings.
- Explicitly selected external MCP tools/resources can be used through the same registry.

Modifying operations require application write access. Automated gestures cannot
grant that access or approve the user's unsaved-project replacement confirmation.
Nested edits remain local drafts until explicitly applied. Async input jobs report
delivery status and can be cancelled; they are not replayed after failure.

## Documentation and distribution

The complete English/French CyDoc reader is bundled for offline `file://` use.
It includes the assistant guide and 186 bilingual operation contracts with input/output
schemas, including the GUI-only operation group. The CLI, SDK licences and dependency
notices accompany the portable application and per-user installer.

CyMCPSDK 0.7.0 development sources are pinned to
`791d4d7d7680213b282ec8529d248a10f8494d77`; CyUI 1.2.0 is pinned to
`6ef01cfcdb197f3a89d64618ce5ef8818c704728`. Both build from the included sources.
The existing compatible IPC-4 VST3 worker is retained with its recorded fingerprint.

## Known limitations

- Windows 10/11 x64 remains the native target. The application and installer are unsigned.
- Video decode/encode remains 8-bit; internal RGBA16F composition and PNG16/EXR half
  export do not establish a complete OCIO/ACES or HDR-monitor workflow.
- OFX hosting retains the supported CPU RGBA filter/general subset. Third-party
  plugins are supplied separately; native plugin UI coverage depends on their accessibility.
- Provider authentication and native CLI permissions remain provider-specific.
  Tool support depends on the selected transport; unsupported integrations report an error.
- Validation uses local API/MCP fixtures and real application instances. It does not
  establish live authentication or model quality for every cloud provider.

## Downloads and reports

Use the portable ZIP or per-user installer, matching corresponding source archive
and SHA-256 checksums from the [release](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.15.0-alpha.1).
For a reproducible issue, include version, steps, expected/actual results and a minimal
project in the [issue tracker](https://github.com/MrMybal/ShaderixFrame/issues).
This release does not publish a bug-by-bug list. See [licensing and sources](LICENSING.md)
and the bundled manual for detailed procedures.

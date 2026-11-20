# ShaderixFrame 0.15.0-alpha.2

[Français](RELEASE_0.15-alpha.2.fr.md) · [Release and downloads](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.15.0-alpha.2)

**Alpha preview for testing and feedback.** This update adds configurable local MCP
access profiles and uses Free by default. The assistant, CLI and live editor keep
the same shared operation registry.

## Local MCP access

| Profile | HTTP token | Operation permissions |
| --- | --- | --- |
| Free (`free`, default) | None | All registered operations without another confirmation |
| Local controlled (`local_controlled`) | None | Instance write permission and unsaved-project confirmation |
| Token only (`token_only`) | Required | All registered operations without another confirmation |
| Protected (`protected`) | Required | Instance write permission and unsaved-project confirmation |

Start local MCP in Assistant settings and use **Copy MCP address**. Free clients
connect using that address alone. The server binds to `127.0.0.1`; it starts only
when requested. Argument schemas and project validation remain active in every profile.
Free and Token only use `Policy::allow_all()` for the registered operations.

Configurations without an explicit `access_profile` now use Free. Explicit choices
are preserved. Use `--mcp-profile=free|local_controlled|token_only|protected` to select
a profile for one process, or save it in Assistant settings and restart to apply it.
Controlled profiles retain the write toggle and confirmation for replacing unsaved work.

## CLI and interface

`ShaderixFrameCLI.exe --connect` accepts a loopback address or an instance descriptor.
Tokenless descriptors do not contain a credential. Token-bearing profiles retain their
private token and reject clients without a valid token. `ShaderixFrame.exe --cli` uses
the same behavior. Standalone CLI edits and saves follow the selected profile too.

The native Assistant panel exposes the profile selector and current MCP address.
Free UI automation can save an explicit profile choice; controlled profiles cannot
grant themselves broader access through synthetic input. Nested clip drafts inherit
the profile of their parent editor.

CyMCPSDK 0.7.1 sources are pinned to `34d4bcae126bc83d1a2925b5047dac36e3d09d92`.
The SDK snapshot can be updated explicitly while preserving its preceding version.
The English/French offline CyDoc reader documents all four profiles and the existing
163 business operations plus 23 GUI tools.

## Distribution and limitations

The release provides a Windows x64 portable ZIP, per-user installer, corresponding
source with the exact JUCE dependency, and SHA-256 checksums. The compatible IPC-4
VST3 worker retains its own recorded version and fingerprint.

This remains an unsigned Windows alpha. Video decode/encode remains 8-bit; internal
RGBA16F composition and PNG16/EXR half export retain their existing scope. Provider
authentication and native agent permissions remain separate from MCP access profiles.
Third-party plugin coverage retains the [alpha 1 limitations](RELEASE_0.15-alpha.1.md#known-limitations).

This release does not publish a bug-by-bug list. Report reproducible issues in the
[issue tracker](https://github.com/MrMybal/ShaderixFrame/issues). See
[licensing and corresponding source](LICENSING.md) for component terms.

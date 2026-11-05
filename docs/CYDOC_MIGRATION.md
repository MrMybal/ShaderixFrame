# CyDoc migration

ShaderixFrame is a Windows x64 C++20/Win32 application using one Dear ImGui docking
context, modular CyUI and Direct3D 11. It has no embedded web runtime. Its previous
help reader drew a bilingual JSON book with ImGui and independently generated HTML.
That reader has been replaced with CyDoc 0.1.0's local export, opened by the native
Windows browser association. No WebView, CEF, service or server was added.

## Content and modules

- All 38 guide chapter IDs and both English/French versions were retained as
  Markdown in `docs/manual/pages/`. Nine screenshots, five diagrams, tables,
  tutorials and HLSL/JSON examples are preserved. Reader-specific instructions were
  updated for CyDoc. `docs/manual/migration.json` records the former book fingerprint.
- Existing useful Markdown references, licensing, SDK guide and technical release
  records are included with provenance. Versioned/outdated records are labelled
  archives, in their original language. In particular older MCP tool counts and
  old architecture/version headings are not presented as current specifications.
- The current MCP reference contains the 163 real `tools/list` contracts. The neutral
  `cydoc` API importer generates signatures and parameter tables, with the complete
  original schemas. IDs and server-provided descriptions remain in their original
  English; the French page labels explain this explicitly.
- CyDoc's lexical source index covers only `sdk/` and `shaders/`, which are shipped
  with the application. It includes C/C++ SDK examples and HLSL source lines, not
  private/internal project sources or the full vendor trees. It is not a C++ compiler
  index; Doxygen and TypeDoc are not required by this integration.
- Only `image` is selected: it supplies local zoom, pan, annotations, keyboard
  controls and in-window enlargement. The timeline image has a stable navigator
  annotation and a precise help link. Static processing diagrams use CyDoc's SVG
  compiler and do not need `graph`. No existing video, frame-controlled GIF, mesh
  or media collection requires `video`, `gif`, `mesh` or `gallery`; no Three.js peer
  is installed in the application's documentation toolchain or exported reader.

## Native integration and compatibility

**Help > User Manual**, **Getting Started**, **Troubleshooting**, **MCP and SDK
Reference**, **View > User Manual** and the command palette open local CyDoc pages.
`Ctrl+F1` chooses the active panel's chapter, including nested and detached panels.
F1 remains the Edit workspace. **Timeline Navigator Help** addresses the exact image
and built-in annotation. Windows `UrlCreateFromPathW` encodes filesystem paths, and
CyDoc's supplied C++ helper encodes the fragment route and media state.

`docs/manual/en.html#chapter` and `fr.html#chapter` are thin compatibility redirects.
The old `--manual-topic`, `--manual-language` and `--view=manual` flags remain accepted;
new tooling uses `--help-topic`, `--help-language`, `--help-section`, `--help-media`
and `--help-note`. Hidden checks use `--help-url-report=...` and never launch a browser.
The native ImGui book renderer, JSON book and independent HTML generator are retired.
The old Python build command is retained as a compatibility shim to the CyDoc builder.

## Reader behavior

The application-owned bootstrap uses public `mountViewer`, the exported image
module and CyDoc's local classic-script asset-provider contract. All reader files
ship together under `help/`. The dark theme follows the editor's gray/orange palette;
other built-in themes and focus/touch rules remain available. Reading state, notes
and bookmarks have a stable application namespace. Browser file-origin storage
policies can limit persistence; CyDoc reports that and supports personal-data export.
The reader survives browser Back/Forward cache restoration. The full printable guide
is derived from CyDoc's compiled pages, preserving the previous print/PDF workflow.

## Build, distribution and checks

`frame_help` runs the generator, and `frame_assets` copies its complete output into
the native build directory. The portable packager validates and copies the same
export; the installer embeds that package. The dependency archive/native helper are
fingerprinted in `cydoc.lock.json` and generator dependencies are npm-locked.

Commands, update instructions and test boundaries are in [manual maintenance](manual/README.md).
The generated translation report distinguishes missing, stale and untracked pages;
authored guide translations use explicit revisions. Original-language technical
records and machine-derived API schemas are not described as human translations.

The available browser-control tool rejects `file://` navigation by policy. No HTTP
server, alternate browser surface or security-policy workaround was used. DOM unit
fixtures cover routing, language, search, image controls and reading state; native
tests cover URI construction, help entry points and packaging. These do not prove
visual layout, actual browser file-origin behavior, PDF printing or touch hardware.
Those checks must be completed manually in a modern Windows browser. Other desktop
and mobile platforms are outside the current Windows-only application backend.

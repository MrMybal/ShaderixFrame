# ShaderixFrame 0.14.0-alpha.2

## Integrated documentation

Help > User Manual and Ctrl+F1 open the bundled CyDoc reader at
`Documentation/Documentation.html`. Contextual help chooses the active panel,
including detached panels and nested clip editors. F1 remains the Edit workspace.
Previous page names and manual addresses retain compatibility redirects.

The manual includes 38 English/French guide chapters, nine application screenshots,
five SVG diagrams and a six-view workspace gallery. Navigation is organized by tasks
and subpages. Global header search, Ctrl+K, descriptive tooltips, themes, bookmarks,
notes and reading positions use the updated CyDoc reader. Media enlargement stays
inside the window and Escape reduces it. Print guide opens the complete user guide.

The developer reference documents all 163 MCP operations in both languages and the
public C/C++ native audio SDK signatures, with parameters, result conventions and
short JSON/HLSL examples. It describes the validated sequence limits of 64–8192 pixels
and 1–120 fps. Internal reports, maintenance notes and implementation listings are
excluded from the human reader.

The entire reader and its resources ship locally under Documentation. Reading needs
no server, CDN, Node runtime or Internet connection. Only image and gallery modules
are selected. DocAI.md is a separate assistant sidecar. GitHub Pages builds the same
export through the documentation workflow.

## Platform and compatibility

This is a Windows 10/11 x64 prerelease. Projects and editing workflows remain on the
0.14 format. Video decode/encode remains 8-bit, with RGBA16F internal compositing and
PNG16/EXR half export. OFX retains the supported CPU RGBA filter/general subset;
VST3 requires the bundled compatible worker. Third-party plugin binaries are not
included. The application and per-user installer are unsigned.

Download the portable ZIP or Setup.exe, plus the matching source archive and checksums.
See [licensing](LICENSING.md) and [alpha limitations](RELEASE_ALPHA.md).

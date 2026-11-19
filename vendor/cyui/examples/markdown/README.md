# CyUI Markdown playground / Laboratoire Markdown

A native SDL3 app for Windows, Linux and macOS for testing Discord-style message interaction and
CyMD-style direct Markdown editing. Web/CEF, YouTube and link previews are optional
host adapters; none is required here. No CyMD package support is involved.

## Build and run / Compiler et lancer

On Windows, from the repository root with Visual Studio 2022 C++ tools and CMake installed:

```bat
build_markdown_demo.bat
build\cyui_markdown_test.exe --locale fr
```

- **Chat / Tchat**: Enter sends, Shift+Enter inserts a line. The input grows with
  the draft. Hover or right-click a message for small copy/reply/edit/delete
  actions. Edit your own message directly in its row: Enter saves, Escape
  cancels. Up from an empty input edits your last message. Reply keeps a link
  back to the original message. The channel is a local UI test, with no network
  messaging service.
- **Attachments / Pièces jointes**: use +, drop files from the system file manager, paste an
  image/screenshot or copied files with Ctrl+V. Pending previews can be removed
  before sending; attachment-only messages work. PNG, JPEG, BMP, TGA, PSD and GIF
  images render as native textures with the same codecs on every OS. Click images to enlarge/zoom, click
  `.md`/`.txt` attachments to edit them, or save any attachment with its small
  download icon. GIF uses its first frame. File bytes are snapshotted in memory;
  changing the original file does not silently change a message.
- **Markdown document / Document Markdown**: Live is the default. Type and
  select formatted text directly with Markdown markers hidden, including during
  selection. Use compact formatting buttons or the floating selection toolbar.
  Enter creates a paragraph/continues a list, Backspace joins paragraphs, arrows
  move between them, Ctrl+A selects the whole document. Source, Side by side
  and Reading remain available. Paste HTML or a URL over a selection, insert or
  paste real images, drag files into the document, resize images and edit cells.
- **Files / Fichiers**: Ctrl+O opens Markdown, Ctrl+S saves, Ctrl+Shift+S saves as.
  Saving uses a plain `.md` with referenced images/files beside it in `assets/`.
  UTF-8 and BOM-marked UTF-16 can be opened; saves use UTF-8. Relative local
  images resolve from the document's folder. Unsaved document changes are
  protected before closing or replacing it. Chat messages are temporary.
- All five themes, both densities and English/French can be changed in the top
  bar. Interface language changes preserve document/message contents.

HTTP(S)/mailto links open when clicked. In Live, use Ctrl+click to open a link.
The native app does not fetch remote images or web previews. Web adapters can be
connected separately. SDL3 handles windows, rendering, file drop and native file
dialogs. Small OS-specific adapters handle Windows copied files/HTML and macOS
pasteboard files/HTML/TIFF screenshots; Linux uses SDL MIME clipboard data.
File dialogs deliver results on the UI thread without blocking the event loop.

## CMake — Windows, Linux, macOS

The application option selects its required modules automatically. Install a C++20
compiler and CMake. On Linux, install the X11/Wayland development packages needed
by SDL; the CI workflow lists the Ubuntu packages. An installed SDL3 >= 3.4 is
preferred; otherwise CMake fetches SDL3 3.4.16 at an immutable revision and verifies
its SHA-256. Set `CYUI_FETCH_DEMO_DEPENDENCIES=OFF` to require a local SDL package.
These dependencies are never added to Core or to library consumers.

```sh
cmake -S . -B build/markdown -DCYUI_BUILD_DEMO=OFF -DCYUI_BUILD_AUDIO=OFF -DCYUI_BUILD_GALLERY=OFF -DCYUI_BUILD_MARKDOWN_DEMO=ON -DCYUI_BUILD_TESTS=ON
cmake --build build/markdown --config Release --target cyui_markdown_test
ctest --test-dir build/markdown -C Release -R cyui_markdown_playground --output-on-failure
```

Single-config builds place `cyui_markdown_test` at `build/markdown/`; Visual Studio
places the `.exe` in `build/markdown/Release/`. Add `CYUI_BUILD_PORTABLE_DEMO=ON`
for the full widget gallery `cyui_demo_sdl3`. Keep the staged `licenses/` folder
beside distributed binaries (Dear ImGui, SDL, stb, JetBrains Mono). An installed
shared SDL also needs its runtime library; the fetched build links SDL statically.

The same CTest suite builds/runs in the Windows/Linux/macOS CI matrix, using a
software renderer and Xvfb on Linux. Windows was verified locally; Linux/macOS
interactive IME, clipboard permissions, dialogs and display scaling still need
verification on real target desktops. The optional CEF runtime has its own platform
validation status; it is not used by these applications.

## Automated checks / Vérifications

```bat
build\cyui_markdown_test.exe --self-test
build\cyui_markdown_test.exe --view markdown --document-mode split --locale fr --theme navy --capture build\markdown.bmp
python tests/capture_markdown_demo.py
```

`--self-test` checks sending, attachment-only messages, edit/cancel/reply, portable
image decoding and an actual Markdown + image save/reload round trip. It then renders
the chat and all four editor modes with the native SDL renderer and exits.
`--capture` writes a BMP and exits. Both run with a hidden window and return a
nonzero exit code on failure. `--file path.md` opens a document at startup.
The capture script reviews both views in every theme and density, plus Live,
Source, Reading and the English interface. `--scenario document-edit` exercises
native typing and Shift+Enter in a rich paragraph and verifies that surrounding
text is preserved; the script also captures this active caret in every theme/density.
Outputs stay under ignored `build/`.
`--scenario document-video --frames 64` replays a heading, repeated Shift+Enter,
typing and a focus change. It checks heading glyph visibility on every frame and
the final text/height after leaving the block. `--frames 12` captures the empty
line before typing. The capture script reviews both stages in every theme/density.
The window title identifies this build as `CyUI - Markdown [revision 7 - multiplatform]`.
`--scenario million-chat` generates a million-message source on demand, including
variable-height text and images. The capture checks that only the visible rows
are fetched and that the document cache stays bounded. No million-row vector is allocated.

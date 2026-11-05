# CyDoc documentation maintenance

**[Documentation](https://mrmybal.github.io/ShaderixFrame/#/en/welcome)** ·
[Documentation en français](https://mrmybal.github.io/ShaderixFrame/#/fr/welcome)

These links open the published HTML reader, including its navigation, search and media.

The authoritative user guides are Markdown files in `pages/en/` and `pages/fr/`.
`Help > User Manual` opens the installed `help/index.html` with an encoded `file://`
route. `Ctrl+F1` chooses the active panel. English is the default content locale;
French uses the same page, anchor and media identifiers. The former ImGui book
reader and its JSON content format have been replaced.

## Generate and test

Use Node.js 22+ and Python 3. Restore the pinned generation tools once:

```powershell
npm ci --prefix tools/cydoc --omit=peer --ignore-scripts
python tools/build_cydoc.py --check --test
```

`python tools/build_manual.py` remains a compatible command. CyDoc `check` is a
build-and-validation operation: it writes a fresh `build/help/`, not a read-only
lint result. Do not edit generated HTML, `data.js`, manifests or reader bundles.
CMake's `frame_help` target runs generation and `frame_assets` copies the entire
export into the executable directory. The portable/installer pipeline validates
and distributes the same tree. The installed reader needs neither Node nor npm.
The first npm restore needs a prepared cache or registry access; reading never does.

## GitHub Pages

The `Documentation` workflow in `.github/workflows/documentation.yml` runs the same
generation and DOM checks on Linux when documentation inputs change on `main`.
It can also be run manually from GitHub Actions. CyDoc and its dependencies are
pinned; no checkout of the private CyDoc repository is needed by the workflow.

Only the complete `build/help/` export is uploaded as the Pages artifact. The
repository root, native application, user projects, settings and build caches are
not deployment inputs. Pages uses **GitHub Actions** as its publishing source.
Hash routes and relative resources work under the `/ShaderixFrame/` project path;
the English and French URLs select the corresponding locale explicitly.

The online and installed readers use the same generator and resource provider.
Hosting does not add a network dependency to the installed `file://` reader.
After publication, open the repository README and follow its Documentation link
to check the actual reader, then check search, a screenshot, a diagram and French.

The library is a locally built archive at `vendor/cydoc/cydoc-0.1.0.tgz`, not a public
npm CyDoc dependency. `cydoc.lock.json` records its repository, commit and hashes;
`tools/cydoc/package-lock.json` pins generation dependencies. Three.js is not installed.
To update CyDoc, build/pack the desired checkout using its own instructions, replace
the archive/helper/licence together, refresh their provenance hashes and npm lock,
and rerun all checks. Do not silently update from a remote branch during a build.

## Authoring and translations

- Retain stable `id` values. Add the same explicit section IDs and media IDs in both
  translations. CyDoc Markdown rich fences contain JSON.
- Bump an English page's `revision` when meaning changes. Update the French
  `sourceRevision` only after translating that change. Missing content falls back
  to English and is reported; stale translations must be reviewed before shipping.
- `translation-report.json` is generated on every export. The optional report page
  is enabled. Generation rejects missing, stale or untracked shipped pages.
- Image paths start with `assets/` and refer to `docs/screenshots/`. Use `image`
  fences with `controls: minimal` and translated alt text. Built-in annotation IDs
  must be shared across languages; the navigator annotation is `navigator`.
- Static `diagram` fences compile to SVG and do not require `graph`. The only
  selected reader is `image`; add another reader only with actual content and tests.
- `tools/cydoc/reader.mjs` is the application-owned public-API embedding. It retains
  CyDoc's classic-script local resource provider and supplies the app theme,
  storage namespace and print-guide link. The image reader itself is unmodified.

The builder includes existing reference Markdown and technical histories with source
fingerprints and explicit original-language/archive notices. Their text is preserved
in its original language; it is not claimed to be a reviewed translation. Local
`VALIDATION_*.md` engineering reports remain on disk and retain the existing package
exclusion. The current 38-chapter guide is fully available in English and French.

## MCP and distributed sources

`api/mcp-tools.json` is captured from the real executable's `tools/list`; its provenance
checks the protocol source fingerprint. CyDoc's neutral API importer produces complete
schema signatures and parameter tables. Server-provided identifiers/descriptions stay
in English, including in the explicitly labelled French reference. The source index
covers only the distributed `sdk/` and `shaders/` trees, not internal/vendor sources.
This is lexical indexing, not semantic C++ analysis. Doxygen/TypeDoc are not needed.

After a protocol change, bootstrap the matching executable before refreshing the snapshot:

```powershell
cmake -S . -B build/Release -DSF_BUILD_HELP=OFF
cmake --build build/Release --target ShaderixFrame
python tools/build_cydoc.py --refresh-mcp build/Release/ShaderixFrame.exe --check --test
cmake -S . -B build/Release -DSF_BUILD_HELP=ON
cmake --build build/Release
```

The normal build defaults to `SF_BUILD_HELP=ON`; packages always validate documentation.

## Contextual and legacy links

Programmatic native links use `HelpDocumentation` and CyDoc's supplied C++ route helper.
`UrlCreateFromPathW` encodes the installed path. The route fragment carries language,
page, optional anchor and optional media state. Examples:

```text
help/index.html#/en/quickstart
help/index.html#/fr/layers/usage
help/index.html#/fr/timeline?media=timeline-image-1&note=navigator
```

`docs/manual/en.html#chapter` and `fr.html#chapter` redirect to the same chapter in CyDoc.
Aliases in `cydoc.config.json` preserve earlier page names. The old CLI flags still work;
new tooling can obtain a native URL without opening a browser:

```powershell
ShaderixFrame.exe --hidden --warp --help-topic=timeline --help-language=fr --help-section=usage --help-media=timeline-image-1 --help-note=navigator --help-url-report=help-route.json
```

## Test boundaries

CTest covers native route/path handling, aliases, sections/media, command palette and
contextual Ctrl+F1 without browser launching. DOM fixtures cover language/fallback,
search, image zoom/in-window enlargement, focus, reading positions, print-link language,
licensing and browser-cache lifecycle. These are unit fixtures without navigation,
network/resource loading or a server; they do not validate browser file-origin policy.

The available browser-control tool blocks `file://` by policy. Visual opening, real
image decoding, storage policy, PDF printing and touch hardware therefore still need
manual validation in a modern Windows browser. No HTTP workaround is used. See
[the migration record](../CYDOC_MIGRATION.md) for scope and historical-content decisions.

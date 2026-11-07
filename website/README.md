# ShaderixFrame showcase

A self-contained English/French presentation of the Windows alpha. Maintained
inputs live here; the generated artifact is `dist/website/` and is not committed.
The page uses the existing application logo and nine real captures from
`docs/screenshots/`. Their capture version remains 0.14.0-alpha.1; the current
application release is 0.14.0-alpha.2.

## Build

Use Node.js 22+ and Python 3. Generate the application's genuine manual with its
existing locked CyDoc toolchain, then build the presentation:

```powershell
npm ci --prefix .CyDocBuild --omit=peer --ignore-scripts
python tools/build_cydoc.py --check --test
node website/build.mjs
```

The showcase build uses only Node built-ins. It does not require Git, an account,
a registry package, a release lookup or an external service. Once the manual has
been generated, rebuilding the showcase needs no network access. Missing or stale
manuals fail with an instruction to regenerate them; no substitute reader is made.

The build replaces only `dist/website/`, copies the complete `Documentation/`
export to `dist/website/manual/`, and checks both languages, manual routes,
resources and artifact boundaries. CSS, classic JavaScript, images and the logo
get content-hashed URLs. Edit source files here, not the generated output.

## Open or preview

Open `dist/website/index.html` directly in a browser. Keep the complete directory
together: `assets/`, `manual/`, `licenses/` and `LICENSE` belong with the page.
The presentation and copied manual use local resources. No server, CDN, remote
font or Node runtime is needed for reading. Installer, source and GitHub links
are supplemental online destinations; installers are not bundled in the site.

For a loopback preview under a project-style subpath:

```powershell
node website/preview.mjs --port=4177
```

Open `http://127.0.0.1:4177/showcase/`. Stop the owned preview with Ctrl+C.
`--port=0` chooses a free port; `--prefix=/another-path/` changes the test prefix.
The server binds only to `127.0.0.1` and serves only the generated artifact.

English is the default. `?lang=fr` opens French and `?lang=en` opens English.
Switching preserves chapter anchors and unrelated URL parameters. Unsupported
language values normalize to English. Earlier `#/en/...` and `#/fr/...` manual
routes redirect to the copied reader. Context links use stable manual page IDs.

## Maintain

- `content.mjs`: both complete translations, screenshot catalog, view groups,
  release metadata and optional verified external destinations.
- `template.mjs`: semantic English HTML, local manual links and image controls.
- `main.js`: classic-script language, menu, tab, chapter and image-viewer behavior.
- `styles.css`: graphite/orange styling, responsive layouts and reduced motion.
- `build.mjs`: dependency-free artifact generation and checks.
- `pages.mjs`: public static artifact with compatible root manual URLs.
- `preview.mjs`: optional local HTTP preview; not a requirement for reading.

Keep translation keys and screenshot captions/alternative text complete in both
languages. Check each new manual target against the real export. After replacing
a capture, preserve its frame, update the capture version and review its content.
The release destinations are configured from the published application assets,
not discovered at runtime. Recheck them before changing the release metadata.

Review the first screen at 1280×720, 1366×768 and 1920×1080 in both languages.
Also inspect mobile/tablet, increased zoom, keyboard/touch-sized controls, image
viewer focus and scroll restoration, tabs, menus, local resources and the real
manual. A DOM or build check alone does not establish visual compatibility.

## Distribution and hosting

Copy the complete generated directory beside an application, or to a static
host under any subpath. There are no history routes or server rewrites. Keep
HTML revalidation separate from immutable caching of hashed assets.

For the GitHub Pages artifact, run:

```powershell
node website/pages.mjs
```

This rebuilds `dist/website/` and assembles `dist/pages/`. The showcase occupies
the site root, with its complete reader under `manual/`. An identical copy of
`Documentation.html`, `DocAI.md` and `DocData/` remains at the root for existing
manual links. Root `#/en/...` and `#/fr/...` links redirect to the bundled reader.
The standalone output and the application's embedded manual keep working offline.

The [Documentation and showcase workflow](../.github/workflows/documentation.yml)
generates and checks the reader, builds this artifact and uploads only `dist/pages/`.
It deploys on relevant changes pushed to `main`, or through `workflow_dispatch`.
Application binaries, user projects, downloads and authoring services are not part
of this static artifact. Building locally does not push or publish.

Public presentation: [English](https://mrmybal.github.io/ShaderixFrame/?lang=en)
and [French](https://mrmybal.github.io/ShaderixFrame/?lang=fr).

## Licences

ShaderixFrame's original content and code remain [AGPL-3.0-only](../LICENSE).
The CyDoc showcase reference retains its [MIT licence](licenses/CyDoc-showcase-LICENSE.txt).
[Showcase notices](NOTICE.md) and the complete reader notices accompany the
artifact. This presentation does not change the application's licence.

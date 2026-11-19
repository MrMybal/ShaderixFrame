# CyUI static showcase

This directory is a self-contained website for GitHub Pages. English is the
default (`index.html`); the complete French pages are under `fr/`. Both languages
include the genuine CyDoc reader under `Documentation/`. The former `guide.html`
URLs redirect their existing chapters to this reader in the selected language.
The complete English/French manual opens locally through file:// without a server.
See [the documentation workspace](../.CyDocBuild/README.md) to regenerate it.

## Preview locally

Open `index.html` directly, or serve this directory from the repository root:

```sh
python -m http.server 8000 --bind 127.0.0.1 --directory site
```

Then visit `http://127.0.0.1:8000/`. The HTML, styles, scripts, guide and screenshots
work offline; GitHub source references and release downloads require a connection
and access to the repository. The site makes no external font, script or analytics
requests. JavaScript adds image enlargement, keyboard-accessible workspace tabs,
theme previews, the mobile menu and module filtering. All four workspace examples
remain readable without JavaScript. The CyDoc reader needs JavaScript.

## Update the pages

Edit `tools/build_showcase.py` for bilingual content and document structure.
Edit `site/assets/site.css` and `site/assets/site.js` for presentation and behavior.
Then regenerate the two showcase pages and two compatibility redirects:

```sh
python tools/build_showcase.py
python tools/build_showcase.py --check
python tests/test_showcase.py
```

The generator puts the CSS and JavaScript SHA-256 prefixes, with text line endings normalized to LF, into their
URLs (`?v=...`) on every page. Regeneration is required after those files change;
the check fails on stale generated HTML or resource versions. Query links such as
`?lang=fr` redirect to the corresponding static language page, retaining its
chapter. The language selector also retains the current chapter.

## Refresh the real screenshots

On Windows, build the current gallery and capture its synthetic fixtures:

```bat
build_demo.bat
python tools/capture_showcase.py
python tools/build_showcase.py
```

`capture_showcase.py` requires Pillow. It writes lossless, metadata-free WebP
images, their dimensions and SHA-256 hashes, the gallery tab/theme/language and
the captured source revision to `assets/screenshots/manifest.json`. No other
applications or their assets are used. The main image retains its full aspect
ratio and is bounded by the available first-screen height on desktop.

The two reader captures in `assets/manual/` show the actual bundled CyDoc reader,
opened at `Documentation/Documentation.html#/en/overview` and its `/fr/overview`
translation. Capture the loaded reader at a desktop viewport, convert the image
to metadata-free WebP, and record its actual pixel dimensions, SHA-256, source
revision and documentation manifest hash in `assets/manual/manifest.json`.
Keep these browser captures separate from `assets/screenshots/`: the native
gallery capture script owns that directory and CyDoc imports its images.

## GitHub Pages readiness

Publish **the contents of `site/`** as the Pages artifact when deployment is
requested. Keep `fr/`, `assets/`, `Documentation/` and `.nojekyll` together. All resource/navigation
paths are relative, so the site works at an origin root or a project subpath
such as `/CyUI/`. There are no SPA rewrites or server-side requirements.

The `Publish CyUI showcase` workflow validates the generated pages and deploys
this directory on relevant pushes to `main`, or when started manually from
`main`. GitHub Pages must use **GitHub Actions** as its publishing source.
Only the deployment job receives Pages/OIDC write permissions. Local preview and
a local commit do not publish the site. Existing GitHub source and download links
keep their current permissions.

The copy distinguishes the portable Windows x64 release from Linux/macOS library
source targets, and current-source captures from the versioned 1.2.0 demo. CEF and
Unreal Engine are optional external runtimes/integrations. The repository has no
declared alpha/beta label, so the site states active development instead.

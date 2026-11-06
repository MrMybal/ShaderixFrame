# ShaderixFrame documentation toolchain

This directory contains documentation inputs and generation tools. It is not installed
with the application. Human content lives in `manual/en/` and `manual/fr/`; the generated
reader lives in the sibling `Documentation/` directory. Internal work notes are ignored
under `internal/`, outside the human content roots.

```powershell
npm ci --prefix .CyDocBuild --omit=peer --ignore-scripts
python tools/build_cydoc.py --check --test
```

Both build/check regenerate the export. CMake copies it beside the executable through
`--install-only --install APP_DIR`; packaging uses the same installation boundary.
The compatibility entry point `tools/build_manual.py` accepts the same arguments.

CyDoc is built from its Git checkout with `npm ci` then `npm pack`, and the resulting
archive/helper/licence are pinned together in `cydoc.lock.json`. The package is local,
not an npm registry dependency. Update all hashes and the npm lock when replacing it.

After a protocol change, bootstrap the Release executable with SF_BUILD_HELP=OFF, then
run `python tools/build_cydoc.py --refresh-mcp build/Release/ShaderixFrame.exe`. Review
and translate every changed contract in `api/mcp-fr.json` before enabling help again.
The source fingerprints guard against stale MCP/SDK contracts. `api/parameters.json`
contains bilingual parameter explanations. `api/mcp-en-overrides.json` corrects catalogue wording against verified runtime behavior without changing the protocol. Public SDK neutral imports contain only
signatures and documentation, never implementation bodies.

Preserve page, heading, media and annotation IDs in both languages. Increment the English
revision and the reviewed French sourceRevision together after a translation update.
Keep tree parents identical. Technical identifiers and code examples retain their spelling.
The reader has no build notes, reports, archives, source browser or translation-report page.
`DocAI.md` remains separate. Only image and gallery are selected; no Three.js is installed.

The Pages workflow builds this same export and uploads only `Documentation/`. The root
index.html preserves previous Pages hash links; Documentation.html opens the reader
directly. Do not publish or push until the user requests it.

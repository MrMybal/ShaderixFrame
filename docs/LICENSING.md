# Licensing and corresponding source

Copyright (c) 2026 Cyberalien.
ShaderixFrame's original code is licensed under **GNU AGPL version 3 only**
(SPDX: `AGPL-3.0-only`). The complete licence is in [LICENSE](../LICENSE).
Third-party code retains its copyright and licence; see [NOTICE.md](../NOTICE.md).
There is no warranty; the alpha is supplied for evaluation and feedback.

## Sources for the Windows alpha

The release `v0.14.0-alpha.1` provides the portable ZIP, per-user installer,
`ShaderixFrame-0.14.0-alpha.1-source.zip` and `SHA256SUMS.txt` from the same release.
The source ZIP contains the complete ShaderixFrame source and the exact JUCE
8.0.15 source used to build its VST3 worker, including bundled SDK/dependency
notices. The source pack omits three unused Android demo `google-services.json`
configurations; the worker source and all build dependencies are unchanged. GitHub's automatic source ZIP contains the repository alone; use the
explicit **source.zip** asset when rebuilding the JUCE worker offline.

The JUCE snapshot is upstream commit `91ad83ae34a81e0833b1a2b0866f54846370ae53`.
The worker uses the locked CyDAW IPC-4 source. VST2, AU, LADSPA, web browser and
CURL hosting are disabled in this build; VST3 hosting is enabled.

Extract the source ZIP, open an x64 Visual Studio 2022 developer terminal in
`ShaderixFrame-source`, and run:

```powershell
python tools/prepare_dependencies.py --audio-only
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DSF_BUILD_VST3_WORKER=ON -DSF_JUCE_SOURCE_DIR="$PWD/juce"
cmake --build build/Release --target ShaderixFrame ofx_gain
python tools/package.py --archive-only
python tools/make_installer.py
```

The source archive's `SOURCE-MANIFEST.json` identifies the repository commit,
JUCE commit and hashes of the supplied files. Build products and generated media
are deliberately outside Git and Git LFS. They are produced locally by the build
and example soundtrack generator.

The native plugin SDK is covered by the project's AGPL licence. No separate
proprietary-plugin linking exception is granted. Third-party plugins are supplied
by their users under their own terms; none are licensed or redistributed by this
release. See the AGPL text for the actual permissions and conditions.

## CyUI

CyUI 1.2.0 is pinned at commit `8367401fe45fc22bfed5e227cb3de343177f676b`.
The upstream repository is [CyUI](https://github.com/MrMybal/CyUI).
Its [upstream MIT licence](https://github.com/MrMybal/CyUI/blob/8367401fe45fc22bfed5e227cb3de343177f676b/LICENSE)
is reproduced in the source tree at `vendor/cyui/LICENSE` and in Windows packages
at `licenses/cyui/LICENSE`. Public availability of the repository does not affect
the bundled licence copy. Its TextEditor, Lexilla/Scintilla and SciTE dependencies
retain their respective notices under the same package directory.

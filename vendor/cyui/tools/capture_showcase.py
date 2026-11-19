"""Capture the real CyUI gallery for the static showcase (Windows, Pillow).

Run build_demo.bat first. Only the gallery's synthetic fixtures are displayed.
Output images are lossless WebP with no copied metadata.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PREVIEWS = (
    ("overview", "mid-grey", 2, False, "en"),
    ("overview-fr", "mid-grey", 2, False, "fr"),
    ("theme-light", "light-grey", 2, False, "en"),
    ("theme-amber", "amber-night", 2, False, "en"),
    ("theme-violet", "violet", 2, False, "en"),
    ("theme-navy", "navy", 2, False, "en"),
    ("audio", "mid-grey", 9, True, "en"),
    ("audio-analysis", "mid-grey", 22, True, "en"),
    ("audio-analysis-fr", "mid-grey", 22, True, "fr"),
    ("audio-spectral", "mid-grey", 23, True, "en"),
    ("audio-spectral-fr", "mid-grey", 23, True, "fr"),
    ("nodes", "violet", 7, True, "en"),
    ("image-graph", "violet", 7, True, "en", "--image-graph"),
    ("assets", "mid-grey", 13, True, "en"),
    ("inspector", "navy", 12, True, "en"),
    ("inspector-fr", "navy", 12, True, "fr"),
    ("code", "navy", 10, True, "en"),
    ("diff", "navy", 11, True, "en"),
    ("plots", "mid-grey", 5, True, "en"),
    ("curves", "amber-night", 6, True, "en"),
    ("timeline", "mid-grey", 8, True, "en"),
    ("filtering", "mid-grey", 21, True, "en"),
    ("images", "violet", 14, True, "en"),
    ("commands", "mid-grey", 16, True, "en"),
    ("documents", "navy", 18, True, "en"),
    ("markdown-reader", "mid-grey", 18, True, "en"),
    ("markdown-reader-fr", "mid-grey", 18, True, "fr"),
    ("markdown-live", "mid-grey", 24, True, "en"),
    ("markdown-live-fr", "mid-grey", 24, True, "fr"),
    ("markdown-split", "navy", 24, True, "en", "--document-mode", "split"),
    ("markdown-split-fr", "navy", 24, True, "fr", "--document-mode", "split"),
    ("markdown-zoom", "violet", 18, True, "en", "--document-zoom"),
    ("markdown-zoom-fr", "violet", 18, True, "fr", "--document-zoom"),
    ("office-grid", "mid-grey", 28, True, "en"),
    ("office-grid-fr", "mid-grey", 28, True, "fr"),
    ("office-spreadsheet", "navy", 26, True, "en"),
    ("office-spreadsheet-fr", "navy", 26, True, "fr"),
    ("office-writer", "mid-grey", 27, True, "en"),
    ("office-writer-fr", "mid-grey", 27, True, "fr"),
    ("diagnostics", "navy", 17, True, "en"),
    ("viewport", "mid-grey", 15, True, "en"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build/cyui_demo.exe")
    parser.add_argument("--only", nargs="+", choices=[preview[0] for preview in PREVIEWS],
                        help="Refresh selected captures while preserving the other manifest entries")
    args = parser.parse_args()
    executable = args.exe.resolve()
    if not executable.is_file():
        parser.error("Build the current gallery with build_demo.bat first.")
    target = ROOT / "site/assets/screenshots"
    target.mkdir(parents=True, exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    manifest = {"source_revision": subprocess.check_output(
        ["git", "-c", f"safe.directory={ROOT.as_posix()}", "rev-parse", "HEAD"],
        cwd=ROOT, text=True).strip(),
        "source_dirty": bool(subprocess.check_output(
            ["git", "-c", f"safe.directory={ROOT.as_posix()}", "status", "--porcelain"],
            cwd=ROOT, text=True).strip()), "images": []}
    if args.only and (target / "manifest.json").is_file():
        manifest["images"] = json.loads((target / "manifest.json").read_text("utf-8"))["images"]
    with tempfile.TemporaryDirectory(prefix="showcase-captures-", dir=ROOT / "build") as scratch:
        for name, theme, tab, gallery_only, language, *extra in PREVIEWS:
            if args.only and name not in args.only:
                continue
            bitmap = Path(scratch) / f"{name}.bmp"
            command = [str(executable), "--capture", str(bitmap), "--theme", theme,
                       "--tab", str(tab), "--locale", language]
            if gallery_only:
                command.append("--gallery-only")
            subprocess.run(command + extra, cwd=scratch, startupinfo=startup,
                           check=True, timeout=30)
            with Image.open(bitmap) as captured:
                image = Image.new("RGB", captured.size)
                image.paste(captured.convert("RGB"))
                image.save(target / f"{name}.webp", lossless=True, method=6)
            output = target / f"{name}.webp"
            manifest["images"] = [entry for entry in manifest["images"] if entry["file"] != output.name]
            manifest["images"].append({"file": output.name, "theme": theme, "tab": tab,
                "locale": language, "size": list(image.size),
                "sha256": hashlib.sha256(output.read_bytes()).hexdigest()})
            print(f"{name}: {theme}, {language}, tab {tab}", flush=True)
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()

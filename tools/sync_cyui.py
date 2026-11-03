"""Verify or explicitly refresh the pinned, unmodified CyUI source snapshot."""
import argparse
import hashlib
import io
import json
import re
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor/cyui"
LOCK = ROOT / "cyui.lock.json"
OMITTED = ("third_party/imgui/", "docs/screenshots/", "tests/", ".github/", "integrations/", "python/", "web/")

def fingerprints(folder):
    return {p.relative_to(folder).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(folder.rglob("*")) if p.is_file()}

def verify():
    record = json.loads(LOCK.read_text(encoding="utf-8"))
    actual = fingerprints(VENDOR)
    if actual != record["files"]:
        changed = sorted(k for k in actual.keys() | record["files"].keys() if actual.get(k) != record["files"].get(k))
        raise SystemExit("Modified CyUI snapshot: " + ", ".join(changed))
    return record

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--source", type=Path, help="read-only local CyUI git checkout")
    parser.add_argument("--revision", help="exact 40-character upstream commit SHA")
    args = parser.parse_args()
    old = verify()
    if args.check or not args.source:
        print("CyUI " + old["version"] + " @ " + old["revision"] + ": snapshot verified")
        return
    if not args.revision or not re.fullmatch(r"[0-9a-fA-F]{40}", args.revision):
        parser.error("An explicit full --revision SHA is required for updates")
    source = args.source.resolve()
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", args.revision + "^{commit}"], text=True).strip()
    archive = subprocess.check_output(["git", "-C", str(source), "archive", "--format=zip", commit])
    # Read-only archive: no checkout/reset and no use of uncommitted source files.
    if not (ROOT / "build").resolve().is_relative_to(ROOT.resolve()):
        raise SystemExit("Build staging directory escapes the application workspace")
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cyui-", dir=ROOT / "build") as temporary:
        stage = Path(temporary) / "cyui"
        stage.mkdir()
        with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
            for entry in zipped.infolist():
                path = Path(entry.filename)
                if path.is_absolute() or ".." in path.parts:
                    raise SystemExit("Unsafe archive path")
                if entry.is_dir() or entry.filename.startswith(OMITTED):
                    continue
                target = stage / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(zipped.read(entry))
        cmake = (stage / "CMakeLists.txt").read_text(encoding="utf-8")
        version = re.search(r"project\(CyUI\s+VERSION\s+([0-9.]+)", cmake).group(1)
        record = {"repository": old["repository"], "revision": commit, "version": version, "files": fingerprints(stage)}
        history = ROOT / "vendor-history" / ("cyui-" + old["revision"])
        history.parent.mkdir(exist_ok=True)
        if history.exists():
            raise SystemExit("Previous snapshot backup already exists: " + str(history))
        for target in (VENDOR, history, stage):
            if not target.resolve().is_relative_to(ROOT.resolve()):
                raise SystemExit("Snapshot move escapes the application workspace")
        VENDOR.rename(history)
        try:
            stage.rename(VENDOR)
        except BaseException:
            history.rename(VENDOR)
            raise
        LOCK.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("Updated CyUI to " + commit + "; rebuild and run compatibility tests before publishing")

if __name__ == "__main__":
    main()

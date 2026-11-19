"""Pin buildable CyMCPSDK sources from a read-only local Git checkout."""
import argparse
import hashlib
import io
import json
import re
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'vendor/cymcp'
LOCK = ROOT / 'cymcp.lock.json'

def fingerprints(folder):
    return {p.relative_to(folder).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(folder.rglob('*')) if p.is_file()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--source', type=Path)
    parser.add_argument('--revision')
    args = parser.parse_args()
    if LOCK.exists():
        record = json.loads(LOCK.read_text('utf-8'))
        if fingerprints(VENDOR) != record['files']:
            raise SystemExit('Modified CyMCPSDK snapshot; preserve local edits before updating')
        if args.check:
            print('CyMCPSDK ' + record['version'] + ' @ ' + record['revision'] + ': snapshot verified')
            return
        raise SystemExit('A snapshot already exists; explicitly review and archive it before replacement')
    if args.check or not args.source or not re.fullmatch(r'[0-9a-f]{40}', args.revision or ''):
        parser.error('Provide --source and an exact --revision for the initial snapshot')
    source = args.source.resolve()
    archive = subprocess.check_output(['git', '-c', 'safe.directory='+source.as_posix(),
                                      '-C', str(source), 'archive', '--format=zip', args.revision])
    if VENDOR.exists():
        raise SystemExit('Refusing to replace an unrecorded vendor directory')
    VENDOR.mkdir(parents=True)
    with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
        for entry in zipped.infolist():
            name = entry.filename
            if not (name.startswith(('src/', 'include/', 'third_party/', 'cmake/')) or
                    name in ('CMakeLists.txt', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'README.md', 'VERSION.md')):
                continue
            path = Path(name)
            if path.is_absolute() or '..' in path.parts or entry.is_dir():
                continue
            target = VENDOR / path
            if not target.resolve().is_relative_to(VENDOR.resolve()):
                raise SystemExit('Unsafe source archive member')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(zipped.read(entry))
    version = re.search(r'project\(CyMCPSDK VERSION ([\d.]+)', (VENDOR/'CMakeLists.txt').read_text()).group(1)
    LOCK.write_text(json.dumps({'repository':'https://github.com/MrMybal/CyMCPSDK',
                               'revision':args.revision, 'version':version,
                               'files':fingerprints(VENDOR)}, indent=2)+'\n', encoding='utf-8')
    print('Pinned CyMCPSDK '+version+' @ '+args.revision)

if __name__ == '__main__':
    main()

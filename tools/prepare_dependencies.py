"""Restore optional binary components separately from the source repository."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path
from demo_audio import write_demo_audio
from upstream_paths import ROOT, resolve_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, help='Extracted Windows package or its ZIP archive')
    parser.add_argument('--tests', action='store_true', help='Also restore the CyDAW VST3 test fixture')
    parser.add_argument('--audio-only', action='store_true', help='Generate demo audio without binary components')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'upstream.lock.json').read_text('utf-8'))
    archive = zipfile.ZipFile(args.bundle) if args.bundle and args.bundle.is_file() else None
    try:
        for key, record in lock['files'].items():
            if args.audio_only:
                continue
            if not record.get('externalArtifact'):
                continue
            if not args.tests and 'CyDAWPluginWorker' not in key:
                continue
            target = ROOT / 'vendor' / key
            expected = record['sha256']
            if target.exists():
                if hashlib.sha256(target.read_bytes()).hexdigest() != expected:
                    raise SystemExit('Existing component differs from the lock: ' + key)
                print('Installed:', target.name)
                continue
            name = target.name
            data = None
            if archive:
                matches = [entry for entry in archive.namelist() if Path(entry).name == name]
                if len(matches) == 1:
                    data = archive.read(matches[0])
            elif args.bundle:
                matches = list(args.bundle.rglob(name))
                if len(matches) == 1:
                    data = matches[0].read_bytes()
            else:
                source = resolve_source(key, record)
                if source.is_file():
                    data = source.read_bytes()
            if data is None:
                raise SystemExit('Component not found: ' + name + '. Supply --bundle or SF_CYDAW_ROOT.')
            if hashlib.sha256(data).hexdigest() != expected:
                raise SystemExit('Component does not match the locked version: ' + name)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            print('Installed:', name)
    finally:
        if archive:
            archive.close()
    audio = ROOT / 'examples/firstlight.wav'
    if not audio.exists():
        audio.parent.mkdir(parents=True, exist_ok=True)
        write_demo_audio(audio)
        print('Created demonstration soundtrack.')


if __name__ == '__main__':
    main()

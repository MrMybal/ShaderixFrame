"""Package the committed application and exact JUCE dependency as corresponding source."""
import argparse
import hashlib
import json
import subprocess
import tempfile
import zipfile
from pathlib import Path
from release_version import release_version

ROOT = Path(__file__).resolve().parents[1]

def git(folder, *args):
    return subprocess.check_output(['git', '-c', 'safe.directory=' + folder.as_posix(), '-C', str(folder), *args])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--juce-source', type=Path, required=True)
    args = parser.parse_args()
    juce = args.juce_source.resolve()
    lock = json.loads((ROOT / 'release-worker.lock.json').read_text('utf-8'))
    if git(ROOT, 'status', '--porcelain').strip():
        raise SystemExit('Commit the reviewed source before packaging it.')
    if git(juce, 'status', '--porcelain').strip():
        raise SystemExit('JUCE has local edits; use the locked unmodified checkout.')
    revision = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    juce_revision = git(juce, 'rev-parse', 'HEAD').decode().strip()
    if juce_revision != lock['juce']['commit']:
        raise SystemExit('JUCE revision differs from the worker lock.')
    version = release_version()
    destination = ROOT / f'dist/ShaderixFrame-{version}-source.zip'
    manifest = {'version': version, 'application_commit': revision, 'juce_commit': juce_revision, 'files': [], 'excluded_unused_demo_configs': []}
    (ROOT / 'build/qa').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='source-package-', dir=ROOT / 'build/qa') as temporary:
        with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
            for index, (folder, prefix) in enumerate(((ROOT, ''), (juce, 'juce/'))):
                archive = Path(temporary) / f'{index}.zip'
                git(folder, 'archive', '--format=zip', '--output=' + str(archive), 'HEAD')
                with zipfile.ZipFile(archive) as source:
                    for entry in source.infolist():
                        if entry.is_dir():
                            continue
                        name = prefix + entry.filename
                        if Path(name).is_absolute() or '..' in Path(name).parts:
                            raise SystemExit('Invalid source archive path')
                        if prefix and Path(entry.filename).name == 'google-services.json':
                            manifest['excluded_unused_demo_configs'].append(name)
                            continue
                        data = source.read(entry)
                        output.writestr('ShaderixFrame-source/' + name, data)
                        manifest['files'].append({'path': name, 'sha256': hashlib.sha256(data).hexdigest()})
            output.writestr('ShaderixFrame-source/SOURCE-MANIFEST.json', json.dumps(manifest, indent=2) + '\n')
    print(destination)
    print('Source files:', len(manifest['files']))

if __name__ == '__main__':
    main()

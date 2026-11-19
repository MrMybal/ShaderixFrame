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
    dependency = parser.add_mutually_exclusive_group(required=True)
    dependency.add_argument('--juce-source', type=Path)
    dependency.add_argument('--juce-archive', type=Path,
                            help='Prior corresponding-source ZIP containing the locked JUCE and SOURCE-MANIFEST.json')
    args = parser.parse_args()
    juce = args.juce_source.resolve() if args.juce_source else None
    lock = json.loads((ROOT / 'release-worker.lock.json').read_text('utf-8'))
    if git(ROOT, 'status', '--porcelain').strip():
        raise SystemExit('Commit the reviewed source before packaging it.')
    if juce and git(juce, 'status', '--porcelain').strip():
        raise SystemExit('JUCE has local edits; use the locked unmodified checkout.')
    revision = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    previous = None
    if juce:
        juce_revision = git(juce, 'rev-parse', 'HEAD').decode().strip()
    else:
        previous = zipfile.ZipFile(args.juce_archive)
        previous_manifest = json.loads(previous.read('ShaderixFrame-source/SOURCE-MANIFEST.json'))
        juce_revision = previous_manifest['juce_commit']
    if juce_revision != lock['juce']['commit']:
        raise SystemExit('JUCE revision differs from the worker lock.')
    version = release_version()
    destination = ROOT / f'dist/ShaderixFrame-{version}-source.zip'
    manifest = {'version': version, 'application_commit': revision, 'juce_commit': juce_revision, 'files': [], 'excluded_unused_demo_configs': []}
    (ROOT / 'build/qa').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='source-package-', dir=ROOT / 'build/qa') as temporary:
        with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
            sources = [(ROOT, '')] + ([(juce, 'juce/')] if juce else [])
            for index, (folder, prefix) in enumerate(sources):
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
            if previous:
                expected = {item['path']: item['sha256'] for item in previous_manifest['files']
                            if item['path'].startswith('juce/')}
                actual = {name.removeprefix('ShaderixFrame-source/'): name for name in previous.namelist()
                          if name.startswith('ShaderixFrame-source/juce/') and not name.endswith('/')}
                if not expected or set(expected) != set(actual):
                    raise SystemExit('JUCE archive entries differ from its source manifest.')
                for name in sorted(expected):
                    if Path(name).is_absolute() or '..' in Path(name).parts:
                        raise SystemExit('Invalid JUCE archive path')
                    data = previous.read(actual[name])
                    if hashlib.sha256(data).hexdigest() != expected[name]:
                        raise SystemExit('JUCE source fingerprint differs: ' + name)
                    output.writestr('ShaderixFrame-source/' + name, data)
                    manifest['files'].append({'path': name, 'sha256': expected[name]})
                manifest['excluded_unused_demo_configs'] = previous_manifest.get('excluded_unused_demo_configs', [])
                previous.close()
            output.writestr('ShaderixFrame-source/SOURCE-MANIFEST.json', json.dumps(manifest, indent=2) + '\n')
    print(destination)
    print('Source files:', len(manifest['files']))

if __name__ == '__main__':
    main()

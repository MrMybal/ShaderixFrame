"""Inspect indexed source files and Git history before publishing."""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GIT = ['git', '-c', 'safe.directory=' + ROOT.as_posix()]


def git(*args):
    return subprocess.run(GIT + list(args), cwd=ROOT, check=True, capture_output=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true', help='Inspect additions/changes in the index only')
    args = parser.parse_args()
    scanner = os.environ.get('GITLEAKS') or shutil.which('gitleaks')
    if not scanner:
        local = ROOT / 'build/qa/github/tools/gitleaks.exe'
        scanner = str(local) if local.is_file() else None
    if not scanner:
        raise SystemExit('Install Gitleaks or set GITLEAKS to its executable path.')
    query = ['diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'] if args.staged else ['ls-files', '-z']
    files = [name.decode('utf-8') for name in git(*query).split(b'\0') if name]
    if not files:
        raise SystemExit('No indexed source files to inspect.')
    qa = ROOT / 'build/qa/publication'
    qa.mkdir(parents=True, exist_ok=True)
    manifest = []
    excluded_suffixes = {'.exe', '.dll', '.vst3', '.ofx', '.pdb', '.zip', '.wav', '.mp4', '.mov', '.avi', '.db', '.sqlite'}
    with tempfile.TemporaryDirectory(prefix='source-', dir=qa) as temporary:
        snapshot = Path(temporary)
        for name in files:
            path = Path(name)
            if (path.parts[0] in ('build', 'dist', 'test-output', 'recovery', 'cache', '.codex', '.claude')
                    or path.suffix.lower() in excluded_suffixes or name in ('.mcp.json', '.env')
                    or path.name.startswith('VALIDATION_')):
                raise SystemExit('Local resource indexed: ' + name)
            data = git('show', ':' + name)
            if len(data) > 100 * 1024 * 1024:
                raise SystemExit('File exceeds the source repository limit: ' + name)
            if re.search(rb'[A-Z]:[/\\](?:Users|_Project)[/\\]', data, re.I):
                raise SystemExit('Workstation path indexed: ' + name)
            if path.name == '.gitattributes' and b'filter=lfs' in data:
                raise SystemExit('LFS resources must remain separate from the source repository.')
            target = snapshot / path
            assert target.resolve().is_relative_to(snapshot.resolve())
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            manifest.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        command = [scanner, 'dir', str(snapshot), '--config', str(ROOT / '.gitleaks.toml'),
                   '--redact=100', '--no-banner', '--report-format', 'json', '--report-path', str(qa / 'sources.json')]
        subprocess.run(command, cwd=ROOT, check=True)
    if not args.staged:
        env = os.environ.copy()
        count = int(env.get('GIT_CONFIG_COUNT', '0'))
        env['GIT_CONFIG_COUNT'] = str(count + 1)
        env[f'GIT_CONFIG_KEY_{count}'] = 'safe.directory'
        env[f'GIT_CONFIG_VALUE_{count}'] = ROOT.as_posix()
        subprocess.run([scanner, 'git', str(ROOT), '--log-opts=--all', '--config', str(ROOT / '.gitleaks.toml'),
                        '--redact=100', '--no-banner', '--report-format', 'json', '--report-path', str(qa / 'history.json')],
                       cwd=ROOT, env=env, check=True)
    (qa / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print('Indexed source files inspected:', len(manifest))


if __name__ == '__main__':
    main()

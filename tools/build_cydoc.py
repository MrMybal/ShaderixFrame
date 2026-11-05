"""Generate ShaderixFrame's CyDoc file:// reader. Check mode also writes a fresh export."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_digest(path):
    # Git checks out LF on Linux; existing Windows trees can still contain CRLF.
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def refresh_mcp(executable):
    executable = executable.resolve()
    request = json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/list'}) + '\n'
    process = subprocess.run([str(executable), '--hidden', '--warp', '--mcp'], input=request,
                             capture_output=True, text=True, encoding='utf-8', timeout=40,
                             creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0,
                             cwd=ROOT / 'build')
    if process.returncode:
        raise SystemExit('Cannot capture MCP contracts: ' + process.stderr)
    tools = json.loads(process.stdout)['result']['tools']
    target = ROOT / 'docs/manual/api'
    target.mkdir(parents=True, exist_ok=True)
    (target / 'mcp-tools.json').write_text(json.dumps(tools, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')
    (target / 'provenance.json').write_text(json.dumps({
        'producer': 'ShaderixFrame tools/list', 'tools': len(tools),
        'source': 'src/mcp/Protocol.cpp', 'source_sha256': source_digest(ROOT / 'src/mcp/Protocol.cpp'),
        'source_line_endings': 'lf',
        'catalogue_sha256': digest(target / 'mcp-tools.json')
    }, indent=2) + '\n', encoding='utf-8', newline='\n')


def validate_export(output):
    config = json.loads((ROOT / 'cydoc.config.json').read_text('utf-8'))
    manifest = json.loads((output / 'manifest.json').read_text('utf-8'))
    migration = json.loads((ROOT / 'docs/manual/migration.json').read_text('utf-8'))
    assert config['modules'] == ['image']
    assert manifest['defaultLocale'] == 'en'
    assert [item['code'] for item in manifest['locales']] == ['en', 'fr']
    assert (output / 'media-image.js').is_file()
    for module in ('video', 'gif', 'mesh', 'graph', 'gallery'):
        assert not (output / ('media-' + module + '.js')).exists(), module
    assert not (ROOT / 'tools/cydoc/node_modules/three').exists(), 'Mesh peer must not enter app dependencies'
    page_map = {(page['id'], page['locale']): page for page in manifest['pages']}
    for topic in migration['preserved_chapter_ids']:
        en, fr = page_map[(topic, 'en')], page_map[(topic, 'fr')]
        assert {item['id'] for item in en['headings']} == {item['id'] for item in fr['headings']}, topic
        assert 'usage' in {item['id'] for item in en['headings']}
    report = json.loads((output / 'translation-report.json').read_text('utf-8'))
    assert not report['missing'] and not report['outdated'] and not report['untracked'], report
    for path in ('index.html', 'data.js', 'viewer-app.js', 'viewer.css', 'shaderixframe.css',
                 'licenses/CyDoc-LICENSE.txt', 'print-en.html', 'print-fr.html'):
        assert (output / path).is_file(), path
    scripts = (output / 'index.html').read_text('utf-8')
    assert 'type="module"' not in scripts and 'http://' not in scripts and 'https://' not in scripts
    for file in ('viewer-app.js', 'media-image.js'):
        assert 'fetch(' not in (output / file).read_text('utf-8'), file
    print(f'CyDoc verified: {len(migration["preserved_chapter_ids"])} bilingual guides, '
          f'{len(manifest["pages"])} page/locale entries, image only, translations current')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Build, validate and write a fresh export (CyDoc check semantics)')
    parser.add_argument('--refresh-mcp', type=Path, metavar='EXE', help='Refresh actual MCP contracts after a protocol change')
    parser.add_argument('--test', action='store_true', help='Run DOM unit fixtures after export validation')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'cydoc.lock.json').read_text('utf-8'))
    for name, expected in lock['files'].items():
        assert digest(ROOT / 'vendor/cydoc' / name) == expected, 'Changed pinned CyDoc component: ' + name
    if args.refresh_mcp:
        refresh_mcp(args.refresh_mcp)
    provenance = json.loads((ROOT / 'docs/manual/api/provenance.json').read_text('utf-8'))
    assert provenance['source_line_endings'] == 'lf'
    assert provenance['source_sha256'] == source_digest(ROOT / provenance['source']), 'MCP changed: use --refresh-mcp with the matching executable'
    assert provenance['catalogue_sha256'] == digest(ROOT / 'docs/manual/api/mcp-tools.json')
    node = os.environ.get('SF_NODE') or shutil.which('node')
    if not node:
        raise SystemExit('CyDoc generation requires Node.js 22+. Install it or set SF_NODE; reading does not need Node.')
    major = int(subprocess.check_output([node, '-p', 'process.versions.node.split(".")[0]'], text=True).strip())
    assert major >= 22, 'CyDoc generation requires Node.js 22+'
    package = ROOT / 'tools/cydoc/node_modules/cydoc/package.json'
    if not package.is_file():
        npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')
        npm_cli = Path(npm).resolve().parent / 'node_modules/npm/bin/npm-cli.js' if npm else Path('missing')
        if not npm_cli.is_file():
            raise SystemExit('Run npm ci --prefix tools/cydoc --omit=peer --ignore-scripts, then regenerate.')
        subprocess.run([node, str(npm_cli), 'ci', '--prefix', str(ROOT / 'tools/cydoc'), '--omit=peer',
                        '--ignore-scripts', '--no-audit', '--no-fund'], cwd=ROOT, check=True)
    assert json.loads(package.read_text('utf-8'))['version'] == lock['version']
    subprocess.run([node, str(ROOT / 'tools/cydoc/build.mjs')], cwd=ROOT, check=True)
    validate_export(ROOT / 'build/help')
    if args.test:
        subprocess.run([node, '--test', str(ROOT / 'tools/cydoc/test.mjs')], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()

"""Build the bilingual local CyDoc reader and optionally install it beside the app."""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

WORK = Path(__file__).resolve().parent
ROOT = WORK.parent
OUTPUT = ROOT / 'Documentation'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_digest(path):
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def refresh_mcp(executable):
    request = json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/list'}) + '\n'
    process = subprocess.run([str(executable.resolve()), '--hidden', '--warp', '--mcp'], input=request,
                             capture_output=True, text=True, encoding='utf-8', timeout=40,
                             creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0,
                             cwd=ROOT / 'build')
    if process.returncode:
        raise SystemExit('Cannot capture MCP contracts: ' + process.stderr)
    tools = json.loads(process.stdout)['result']['tools']
    target = WORK / 'api'
    (target / 'mcp-tools.json').write_text(json.dumps(tools, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')
    (target / 'provenance.json').write_text(json.dumps({
        'producer': 'ShaderixFrame tools/list', 'tools': len(tools),
        'source': 'src/mcp/Protocol.cpp', 'source_sha256': source_digest(ROOT / 'src/mcp/Protocol.cpp'),
        'source_line_endings': 'lf', 'catalogue_sha256': digest(target / 'mcp-tools.json')
    }, indent=2) + '\n', encoding='utf-8', newline='\n')


def validate_export():
    data = OUTPUT / 'DocData'
    manifest = json.loads((data / 'manifest.json').read_text('utf-8'))
    assert manifest['defaultLocale'] == 'en'
    assert [item['code'] for item in manifest['locales']] == ['en', 'fr']
    assert manifest['logo'] == 'assets/app-logo.png'
    assert {p.name for p in OUTPUT.iterdir()} == {'Documentation.html', 'DocAI.md', 'DocData', 'index.html'}
    for name in ('data.js', 'viewer.js', 'viewer-app.js', 'viewer.css', 'shaderixframe.css', 'media-image.js', 'media-gallery.js',
                 'licenses/CyDoc-LICENSE.txt', 'print-en.html', 'print-fr.html'):
        assert (data / name).is_file(), name
    for module in ('video', 'gif', 'mesh', 'graph'):
        assert not (data / ('media-' + module + '.js')).exists(), module
    assert not (WORK / 'node_modules/three').exists(), 'Mesh peer must not enter app dependencies'
    by_locale = {locale: {p['id']: p for p in manifest['pages'] if p['locale'] == locale} for locale in ('en', 'fr')}
    assert set(by_locale['en']) == set(by_locale['fr'])
    for id, en in by_locale['en'].items():
        fr = by_locale['fr'][id]
        assert en['text'].strip() and fr['text'].strip(), id
        assert en['text'] != fr['text'], 'Untranslated page: ' + id
        assert en.get('parent') == fr.get('parent'), id
        assert {h['id'] for h in en['headings']} == {h['id'] for h in fr['headings']}, id
        assert set(en.get('anchors', [])) == set(fr.get('anchors', [])), id
        assert en.get('kind') != 'source' and not en.get('sourcePageId'), id
        assert not any(x in id for x in ('reference-release', 'translation-report', 'manual-maintenance', 'migration', 'source-sdk', 'source-shaders')), id
    assert sum(p['id'].startswith('mcp-api-') for p in by_locale['en'].values()) == 163
    assert sum(p['id'].startswith('sdk-api-') for p in by_locale['en'].values()) == 7
    for page in manifest['pages']:
        assert not page.get('source'), page['id']
        assert not any(x in page['html'] for x in ('Original document retained', 'Document original conservé', 'sourceRevision:')), page['id']
        for asset in re.findall(r'(?:src|href)="(assets/[^"?#]+)', page['html']):
            assert (data / asset).is_file(), asset
    report = json.loads((data / 'translation-report.json').read_text('utf-8'))
    assert not report['missing'] and not report['outdated'] and not report['untracked'], report
    html = (OUTPUT / 'Documentation.html').read_text('utf-8')
    assert '<iframe' not in html and 'type="module"' not in html
    assert 'http://' not in html and 'https://' not in html
    for file in ('viewer-app.js', 'media-image.js', 'media-gallery.js'):
        assert 'fetch(' not in (data / file).read_text('utf-8'), file
    allowed = {'Documentation.html', 'DocAI.md', 'index.html'}
    allowed.update('DocData/' + name for name in ('manifest.json', 'translation-report.json', 'data.js', 'viewer.js',
        'viewer-app.js', 'viewer.css', 'shaderixframe.css', 'media-image.js', 'media-gallery.js',
        'licenses/CyDoc-LICENSE.txt', 'print-en.html', 'print-fr.html'))
    allowed.update('DocData/assets/' + p.relative_to(WORK / 'assets').as_posix()
                   for p in (WORK / 'assets').rglob('*') if p.is_file())
    for path in OUTPUT.rglob('*'):
        assert not path.is_symlink()
        if path.is_file():
            assert path.relative_to(OUTPUT).as_posix() in allowed, 'Unexpected documentation resource: ' + str(path)
            assert path.suffix.lower() not in ('.cpp', '.h', '.hpp', '.hlsl', '.tgz', '.sframe', '.log', '.map', '.py'), path
    print(f'CyDoc verified: {len(by_locale["en"])} pages in each language; complete EN/FR, image + gallery, public API only')


def install_export(destination):
    destination = destination.resolve()
    assert destination.is_relative_to(ROOT) and destination != ROOT, 'Installation must be an application build/package inside this workspace'
    shutil.copytree(OUTPUT, destination / 'Documentation', dirs_exist_ok=True)
    # Remove only unchanged files that belonged to the former generated manual.
    legacy = json.loads((WORK / 'legacy-installation.json').read_text('utf-8'))
    for name, expected in legacy.items():
        path = (destination / name).resolve()
        assert path.is_relative_to(destination)
        if path.is_file() and digest(path) == expected:
            path.unlink()
    (destination / 'help').mkdir(parents=True, exist_ok=True)
    shutil.copy2(WORK / 'compat/help-index.html', destination / 'help/index.html')
    # Keep the former language landing addresses as compatibility redirects.
    (destination / 'docs/manual').mkdir(parents=True, exist_ok=True)
    for locale in ('en', 'fr'):
        shutil.copy2(ROOT / 'docs/manual' / (locale + '.html'), destination / 'docs/manual' / (locale + '.html'))
    print('Installed CyDoc:', destination / 'Documentation/Documentation.html')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Build and validate a fresh export; this command writes files')
    parser.add_argument('--refresh-mcp', type=Path, metavar='EXE')
    parser.add_argument('--test', action='store_true', help='Run DOM and export checks')
    parser.add_argument('--install', type=Path, metavar='APP_DIR', help='Copy the validated export and retire unchanged legacy files')
    parser.add_argument('--install-only', action='store_true', help='Validate/copy an already generated export without rebuilding')
    args = parser.parse_args()
    lock = json.loads((WORK / 'cydoc.lock.json').read_text('utf-8'))
    for name, expected in lock['files'].items():
        assert digest(WORK / 'vendor/cydoc' / name) == expected, 'Changed pinned CyDoc component: ' + name
    if args.refresh_mcp:
        refresh_mcp(args.refresh_mcp)
    for file in ('provenance.json', 'sdk-provenance.json'):
        provenance = json.loads((WORK / 'api' / file).read_text('utf-8'))
        assert provenance['source_sha256'] == source_digest(ROOT / provenance['source']), 'Public API changed; refresh its documentation: ' + file
    provenance = json.loads((WORK / 'api/provenance.json').read_text('utf-8'))
    assert provenance['catalogue_sha256'] == digest(WORK / 'api/mcp-tools.json')
    tools = json.loads((WORK / 'api/mcp-tools.json').read_text('utf-8'))
    french = json.loads((WORK / 'api/mcp-fr.json').read_text('utf-8'))
    assert set(french) == {t['name'] for t in tools}, 'Translate every public MCP operation'
    english = json.loads((WORK / 'api/mcp-en-overrides.json').read_text('utf-8'))
    for tool in tools:
        translation = french[tool['name']]
        expected = hashlib.sha256(json.dumps(tool, sort_keys=True, ensure_ascii=False).encode()).hexdigest()
        assert translation['source_sha256'] == expected, 'Review changed French contract: ' + tool['name']
        if tool['name'] in english:
            assert english[tool['name']]['source_sha256'] == expected, 'Review changed English contract: ' + tool['name']
        assert translation['description'].strip() and translation['description'] != tool['description']
    node = os.environ.get('SF_NODE') or shutil.which('node')
    if not node:
        raise SystemExit('CyDoc generation requires Node.js 22+. Reading needs no Node runtime.')
    assert int(subprocess.check_output([node, '-p', 'process.versions.node.split(".")[0]'], text=True).strip()) >= 22
    package = WORK / 'node_modules/cydoc/package.json'
    if not package.is_file():
        npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')
        npm_cli = Path(npm).resolve().parent / 'node_modules/npm/bin/npm-cli.js' if npm else Path('missing')
        if not npm_cli.is_file():
            raise SystemExit('Run npm ci --prefix .CyDocBuild --omit=peer --ignore-scripts, then regenerate.')
        subprocess.run([node, str(npm_cli), 'ci', '--prefix', str(WORK), '--omit=peer', '--ignore-scripts', '--no-audit', '--no-fund'], cwd=ROOT, check=True)
    assert json.loads(package.read_text('utf-8'))['version'] == lock['version']
    if not args.install_only:
        subprocess.run([node, str(WORK / 'build.mjs')], cwd=ROOT, check=True)
    validate_export()
    if args.test:
        subprocess.run([node, '--test', str(WORK / 'test.mjs')], cwd=ROOT, check=True)
    if args.install:
        install_export(args.install)


if __name__ == '__main__':
    main()

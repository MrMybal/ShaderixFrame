"""Verify the portable archive, native UI and per-user setup inside build/qa only."""
from release_version import release_version
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QA = ROOT / 'build/qa'
QA.mkdir(parents=True, exist_ok=True)
version = release_version()
archive = ROOT / f'dist/ShaderixFrame-{version}-windows-x64.zip'
setup = ROOT / f'dist/ShaderixFrame-{version}-Setup.exe'
target = Path(tempfile.mkdtemp(prefix=f'portable-{version}-', dir=QA)).resolve()
assert target.is_relative_to(QA.resolve())
with zipfile.ZipFile(archive) as zipped:
    names = zipped.namelist()
    assert all(name.startswith('ShaderixFrame/') and '..' not in Path(name).parts for name in names)
    assert not any(any(part in name for part in ('test-output', 'recovery/', 'cache/', 'workspace.ini', '.pending')) for name in names)
    for required in ('cyui.lock.json', 'licenses/cyui/LICENSE', 'licenses/cyui/THIRD_PARTY_NOTICES.md', 'licenses/cyui/third_party/texteditor/LICENSE', 'licenses/cyui/third_party/lexilla/License.txt', 'licenses/cyui/third_party/scintilla/License.txt', 'licenses/cyui/third_party/SciTE-Properties-License.txt', 'LICENSE', 'CYDAW-LICENSE.txt', 'release-worker.lock.json', 'THIRD-PARTY-LICENSES.txt', 'docs/LICENSING.md', 'docs/RELEASE_ALPHA.md', 'ShaderixFramePluginWorker.exe', 'ShaderixFrameOfxWorker.exe', 'CyDAWPluginWorker.exe', 'native_gain.dll', 'ofx_gain.ofx', 'OPENFX-LICENSE.txt',
                     'JUCE-LICENSE.md', 'sdk/ShaderixFramePlugin.h', 'examples/VfxStudy.sframe',
                     'examples/ParallelEQ.json', 'examples/GradingStudy.sframe', 'examples/LayerGraphStudy.sframe',
                     'examples/MotionStudy.sframe', 'examples/ClipStudy.sframe', 'docs/BUILDING.md',
                     'docs/RELEASE_0.13.md', 'docs/RELEASE_0.14.md', 'help/index.html', 'help/manifest.json',
                     'help/data.js', 'help/viewer-app.js', 'help/media-image.js', 'help/licenses/CyDoc-LICENSE.txt',
                     'docs/manual/en.html', 'docs/manual/fr.html', 'docs/screenshots/editing.png',
                     'plugins/shaderixel-library/plugin.json', 'sdk/openfx/ofxImageEffect.h'):
        assert 'ShaderixFrame/' + required in names
    help_data = json.loads(zipped.read('ShaderixFrame/help/manifest.json'))
    assert help_data['defaultLocale'] == 'en' and [item['code'] for item in help_data['locales']] == ['en', 'fr']
    assert 'ShaderixFrame/docs/manual/book.json' not in names
    assert not any('media-'+module+'.js' in name for name in names for module in ('mesh','video','gif','graph','gallery'))
    for page in help_data['pages']:
        for asset in re.findall(r'src="(assets/[^"?]+)', page['html']):
            assert 'ShaderixFrame/help/' + asset in names
    zipped.extractall(target)
exe = target / 'ShaderixFrame/ShaderixFrame.exe'
assert hashlib.sha256(exe.read_bytes()).digest() == hashlib.sha256((ROOT/'build/Release/ShaderixFrame.exe').read_bytes()).digest()
worker_lock = json.loads((exe.parent/'release-worker.lock.json').read_text('utf-8'))
assert hashlib.sha256((exe.parent/'CyDAWPluginWorker.exe').read_bytes()).hexdigest() == worker_lock['sha256']

def check_media(project):
    for track in project['tracks']:
        for clip in track['clips']:
            if clip.get('source'):
                assert not Path(clip['source']).is_absolute()
                assert (exe.parent/'examples'/clip['source']).resolve().is_relative_to(exe.parent)
                assert (exe.parent/'examples'/clip['source']).is_file()
            if clip.get('composition'):
                check_media(clip['composition'])
for example in ('GradingStudy', 'MotionStudy', 'ClipStudy'):
    check_media(json.loads((exe.parent/f'examples/{example}.sframe').read_text('utf-8')))
subprocess.run([sys.executable, str(ROOT/'tests/mcp_smoke.py'), str(exe)], cwd=target, check=True, timeout=60,
               creationflags=subprocess.CREATE_NO_WINDOW)

def capture(view, project, clip, time):
    snapshot = QA / f'{view}-{version}.png'
    frame = QA / f'{view}-frame-{version}.png'
    requests = [('select_clips', {'clips': [clip]}), ('transport', {'time': time, 'playing': False}),
                ('render_frame', {'path': str(frame), 'time': time})]
    process = subprocess.Popen([str(exe), '--hidden', '--warp', '--mcp', '--view='+view, '--ui-snapshot='+str(snapshot),
                                str(exe.parent/'examples'/project)], cwd=target,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding='utf-8', creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        for index, (tool, arguments) in enumerate(requests, 1):
            process.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': index, 'method': 'tools/call',
                                           'params': {'name': tool, 'arguments': arguments}})+'\n')
        process.stdin.flush()
        for index in range(1, len(requests)+1):
            response = json.loads(process.stdout.readline())
            assert response['id'] == index and not response['result'].get('isError', False), response
        assert process.wait(timeout=20) == 0, process.stderr.read()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        process.stdin.close()
        process.stdout.close()
        process.stderr.close()
    assert snapshot.stat().st_size > 1000 and frame.stat().st_size > 1000
    return str(snapshot)

snapshots = [capture('properties', 'VfxStudy.sframe', 2, 4), capture('audio', 'MixerStudy.sframe', 4, 4), capture('dsp', 'DspStudy.sframe', 4, 4)]
snapshots += [capture('grading', 'GradingStudy.sframe', 2, 4), capture('scopes', 'GradingStudy.sframe', 2, 4)]
snapshots += [capture('layers', 'LayerGraphStudy.sframe', 2, 4)]
snapshots += [capture('motion', 'MotionStudy.sframe', 6, 4), capture('clip', 'ClipStudy.sframe', 2, 4),
              capture('library', 'MotionStudy.sframe', 6, 4)]
help_routes = []
for language, topic in (('en', 'quickstart'), ('fr', 'layers')):
    report = QA / f'cydoc-{language}-{version}.json'
    subprocess.run([str(exe), '--hidden', '--warp', '--help-language='+language,
                    '--help-topic='+topic, '--help-url-report='+str(report)], cwd=target,
                   check=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
    route = json.loads(report.read_text('utf-8'))
    assert not route['error'] and route['url'].startswith('file:///') and route['url'].endswith('#/'+language+'/'+topic)
    help_routes.append(route['url'])
viewports = QA / f'viewports-{version}.json'
subprocess.run([str(exe), '--hidden', '--warp', '--viewport-smoke='+str(viewports)], cwd=target,
               check=True, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
viewport_report = json.loads(viewports.read_text('utf-8'))
assert viewport_report['nativeWindowRendered'] and viewport_report['secondaryClosePreservesMain']
assert viewport_report['remainingViewports'] == 1 and not viewport_report['mainCloseRequested']
clip_viewports = QA / f'clip-viewports-{version}.json'
subprocess.run([str(exe), '--hidden', '--warp', '--view=clip', '--viewport-smoke='+str(clip_viewports),
                str(exe.parent/'examples/ClipStudy.sframe')], cwd=target, check=True, timeout=20,
               creationflags=subprocess.CREATE_NO_WINDOW)
clip_viewport_report = json.loads(clip_viewports.read_text('utf-8'))
assert clip_viewport_report['nativeWindowRendered'] and clip_viewport_report['secondaryClosePreservesMain']
assert clip_viewport_report['remainingViewports'] == 1 and clip_viewport_report['compositionVisible']
install = Path(tempfile.mkdtemp(prefix=f'install-{version}-', dir=QA)).resolve()
assert install.is_relative_to(QA.resolve())
subprocess.run([str(setup), '--install-dir', str(install), '--silent', '--no-shortcuts'], check=True, timeout=30,
               creationflags=subprocess.CREATE_NO_WINDOW)
manifest = (install/'installed-files.txt').read_text('utf-8-sig').splitlines()
assert len(manifest) >= 40
for line in manifest:
    digest, relative = line.split('\t')
    installed = (install / relative).resolve()
    assert installed.is_relative_to(install)
    assert hashlib.sha256(installed.read_bytes()).hexdigest().upper() == digest
subprocess.run([sys.executable, str(ROOT/'tests/mcp_smoke.py'), str(install/'ShaderixFrame.exe')],
               cwd=install, check=True, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
personal = install/'my-project.sframe'
personal.write_text('Personal project preservation fixture', encoding='utf-8')
modified = install/'README.md'
modified.write_text(modified.read_text('utf-8')+'\nPersonal modification\n', encoding='utf-8')
subprocess.run([str(setup), '--uninstall', str(install), '--silent'], check=True, timeout=30,
               creationflags=subprocess.CREATE_NO_WINDOW)
assert personal.read_text('utf-8') == 'Personal project preservation fixture'
assert modified.read_text('utf-8').endswith('Personal modification\n')
assert not (install/'ShaderixFrame.exe').exists() and not (install/'installed-files.txt').exists()
result = {'version': version, 'archive': str(archive), 'entries': len(names), 'isolated': str(target),
          'snapshots': snapshots, 'help_routes': help_routes, 'installer': str(setup), 'installed_files': len(manifest),
          'installation': str(install), 'personal_files_preserved': True, 'native_windows': viewport_report,
          'native_clip_editor': clip_viewport_report}
(QA/f'release-{version}-verification.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, indent=2))

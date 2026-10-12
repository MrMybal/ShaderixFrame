"""Package an already tested Release build without touching the source applications."""
import shutil
import re
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
version = re.search(r'project\(ShaderixFrame VERSION ([\d.]+)', (ROOT / 'CMakeLists.txt').read_text('utf-8')).group(1)
destination = ROOT / 'dist' / 'ShaderixFrame'
destination.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='package-', dir=ROOT / 'dist') as temporary:
    staging = Path(temporary).resolve()
    assert staging.is_relative_to((ROOT / 'dist').resolve())
    package = staging / 'ShaderixFrame'
    package.mkdir()
    for binary in ('ShaderixFrame.exe','ShaderixFramePluginWorker.exe','ShaderixFrameOfxWorker.exe','CyDAWPluginWorker.exe','native_gain.dll','ofx_gain.ofx'):
        shutil.copy2(ROOT / 'build/Release' / binary, package)
    for folder in ('shaders', 'plugins', 'examples', 'docs','sdk'):
        shutil.copytree(ROOT / folder, package / folder,
                        ignore=shutil.ignore_patterns('VALIDATION_*.md', '__pycache__', '*.pyc', '.env', '*.log'))
    for name in ('README.md', 'NOTICE.md', 'upstream.lock.json', 'openfx.lock.json'):
        shutil.copy2(ROOT / name, package)
    shutil.copy2(ROOT / 'vendor/imgui/LICENSE.txt', package / 'IMGUI-LICENSE.txt')
    shutil.copy2(ROOT / 'vendor/juce-license/LICENSE.md', package / 'JUCE-LICENSE.md')
    shutil.copy2(ROOT / 'vendor/openfx/LICENSE.md', package / 'OPENFX-LICENSE.txt')
    shutil.copytree(ROOT / 'vendor/openfx/include', package / 'sdk/openfx')
    shutil.make_archive(str(ROOT / f'dist/ShaderixFrame-{version}-windows-x64'), 'zip', staging, 'ShaderixFrame')
    # Update the launchable folder while preserving personal projects and recoveries.
    shutil.copytree(package, destination, dirs_exist_ok=True)
    # Remove only unchanged internal reports from earlier packages; keep edited files.
    for report in (destination / 'docs').glob('VALIDATION_*.md'):
        original = ROOT / 'docs' / report.name
        assert report.resolve().is_relative_to(destination.resolve())
        if original.is_file() and report.read_bytes() == original.read_bytes():
            report.unlink()
print(destination / 'ShaderixFrame.exe')

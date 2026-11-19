"""Package an already tested Release build without touching the source applications."""
from release_version import release_version
from juce_notices import dependency_notices
import argparse
import shutil
import re
import subprocess
import sys
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--archive-only', action='store_true', help='Build the ZIP without updating an open portable installation')
parser.add_argument('--build-dir', type=Path, help='Tested build directory; default: build/Release')
parser.add_argument('--output-dir', type=Path, help='Package destination root; default: dist')
args = parser.parse_args()

ROOT = Path(__file__).resolve().parents[1]
build_directory = args.build_dir.resolve() if args.build_dir else ROOT / 'build/Release'
output_directory = args.output_dir.resolve() if args.output_dir else ROOT / 'dist'
version = release_version()
subprocess.run([sys.executable, str(ROOT / 'tools/build_manual.py'), '--check'], check=True)
destination = output_directory / 'ShaderixFrame'
destination.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='package-', dir=output_directory) as temporary:
    staging = Path(temporary).resolve()
    assert staging.is_relative_to(output_directory.resolve())
    package = staging / 'ShaderixFrame'
    package.mkdir()
    for binary in ('ShaderixFrame.exe','ShaderixFrameCLI.exe','ShaderixFramePluginWorker.exe','ShaderixFrameOfxWorker.exe','CyDAWPluginWorker.exe','native_gain.dll','ofx_gain.ofx'):
        shutil.copy2(build_directory / binary, package)
    for folder in ('shaders', 'plugins', 'examples', 'docs','sdk'):
        shutil.copytree(ROOT / folder, package / folder,
                        ignore=shutil.ignore_patterns('VALIDATION_*.md', '__pycache__', '*.pyc', '.env', '*.log'))
    subprocess.run([sys.executable, str(ROOT / 'tools/build_cydoc.py'), '--install-only', '--install', str(package)], check=True)
    for name in ('README.md', 'README.fr.md', 'LICENSE', 'NOTICE.md', 'upstream.lock.json', 'openfx.lock.json', 'release-worker.lock.json', 'cyui.lock.json', 'cymcp.lock.json'):
        shutil.copy2(ROOT / name, package)
    (package / 'licenses/cydoc').mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / '.CyDocBuild/vendor/cydoc/LICENSE', package / 'licenses/cydoc/LICENSE')
    shutil.copy2(ROOT / 'vendor/cydaw2/LICENSE', package / 'CYDAW-LICENSE.txt')
    shutil.copy2(ROOT / 'vendor/imgui/LICENSE.txt', package / 'IMGUI-LICENSE.txt')
    for notice in ('LICENSE', 'THIRD_PARTY_NOTICES.md', 'third_party/texteditor/LICENSE',
                   'third_party/lexilla/License.txt', 'third_party/scintilla/License.txt',
                   'third_party/SciTE-Properties-License.txt', 'third_party/md4c/LICENSE.md', 'third_party/jetbrains-mono/OFL.txt'):
        target = package / 'licenses/cyui' / notice
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / 'vendor/cyui' / notice, target)
    for notice in ('LICENSE', 'THIRD_PARTY_NOTICES.md', 'third_party/nlohmann/LICENSE.MIT', 'third_party/httplib/LICENSE', 'third_party/picosha2/LICENSE'):
        target = package / 'licenses/cymcp' / notice
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / 'vendor/cymcp' / notice, target)
    shutil.copy2(ROOT / 'vendor/juce-license/LICENSE.md', package / 'JUCE-LICENSE.md')
    shutil.copy2(ROOT / 'vendor/openfx/LICENSE.md', package / 'OPENFX-LICENSE.txt')
    shutil.copytree(ROOT / 'vendor/openfx/include', package / 'sdk/openfx')
    (package / 'THIRD-PARTY-LICENSES.txt').write_text(dependency_notices(), encoding='utf-8')
    archive_base = output_directory / f'ShaderixFrame-{version}-windows-x64'
    shutil.make_archive(str(archive_base), 'zip', staging, 'ShaderixFrame')
    if not args.archive_only:
        # Update the launchable folder while preserving personal projects and recoveries.
        shutil.copytree(package, destination, dirs_exist_ok=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/build_cydoc.py'), '--install-only', '--install', str(destination)], check=True)
        # Remove only unchanged internal reports from earlier packages; keep edited files.
        for report in (destination / 'docs').glob('VALIDATION_*.md'):
            original = ROOT / 'docs' / report.name
            assert report.resolve().is_relative_to(destination.resolve())
            if original.is_file() and report.read_bytes() == original.read_bytes():
                report.unlink()
print(Path(str(archive_base) + '.zip') if args.archive_only else destination / 'ShaderixFrame.exe')

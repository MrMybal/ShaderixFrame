"""Capture the running Windows demo for the README (requires Pillow).

Build with build_demo.bat first. Images contain only the gallery's synthetic
fixtures, and are saved as RGB PNGs without copying image metadata.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PREVIEWS = (
    ('mid-grey', 2, False, 'mid-grey.png'),
    ('light-grey', 2, False, 'light-grey.png'),
    ('violet', 2, False, 'violet.png'),
    ('navy', 2, False, 'navy.png'),
    ('mid-grey', 9, True, 'audio-views.png'),
    ('violet', 7, True, 'nodes.png'),
    ('violet', 7, True, 'image-graph.png', '--image-graph'),
    ('mid-grey', 13, True, 'content-browser.png'),
    ('navy', 12, True, 'details-inspector.png'),
    ('navy', 12, True, 'localization-fr.png', '--locale', 'fr'),
    ('mid-grey', 21, True, 'filtering.png'),
    ('navy', 10, True, 'code-editor.png'),
    ('navy', 11, True, 'code-diff.png'),
    ('mid-grey', 5, True, 'plots.png'),
    ('amber-night', 6, True, 'curves.png'),
    ('mid-grey', 8, True, 'timeline.png'),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'build/cyui_demo.exe')
    parser.add_argument('--output', type=Path, default=ROOT/'docs/screenshots')
    parser.add_argument('--only', nargs='+', choices=[preview[3] for preview in PREVIEWS],
                        help='Regenerate only these named screenshots')
    args = parser.parse_args()
    executable = args.exe.resolve()
    if not executable.is_file():
        parser.error('Build the Windows demo with build_demo.bat first.')
    args.output.mkdir(parents=True, exist_ok=True)
    scratch = ROOT/'build'
    scratch.mkdir(exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with tempfile.TemporaryDirectory(prefix='readme-captures-', dir=scratch) as temporary:
        for theme, tab, gallery_only, name, *extra_flags in PREVIEWS:
            if args.only and name not in args.only:
                continue
            bitmap = Path(temporary)/(name+'.bmp')
            command = [str(executable), '--capture', str(bitmap), '--theme', theme, '--tab', str(tab)]
            if gallery_only:
                command.append('--gallery-only')
            command.extend(extra_flags)
            subprocess.run(command, cwd=ROOT, startupinfo=startup, check=True, timeout=30)
            if not bitmap.is_file():
                raise RuntimeError(f'The demo did not capture {name}')
            with Image.open(bitmap) as captured:
                rgb = captured.convert('RGB')
                image = Image.new('RGB', rgb.size)
                image.paste(rgb)
                image.save(args.output/name, optimize=True)
            print(f'{name}: theme={theme}, tab={tab}', flush=True)


if __name__ == '__main__':
    main()

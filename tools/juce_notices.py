"""Collect dependency notices from the JUCE source used by this build."""
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def juce_source():
    configured = os.environ.get('SF_JUCE_ROOT')
    if configured:
        return Path(configured)
    bundled = ROOT / 'juce'
    if bundled.is_dir():
        return bundled
    cache = (ROOT / 'build/Release/CMakeCache.txt').read_text('utf-8')
    return Path(re.search(r'^SF_JUCE_SOURCE_DIR:PATH=(.+)$', cache, re.M)[1].strip())

def dependency_notices():
    supplied = ROOT / 'vendor/juce-license/THIRD-PARTY-LICENSES.txt'
    if supplied.is_file():
        return supplied.read_text('utf-8')
    root = juce_source()
    paths = [
        'modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt',
        'modules/juce_graphics/fonts/harfbuzz/COPYING',
        'modules/juce_graphics/unicode/sheenbidi/LICENSE',
        'modules/juce_graphics/image_formats/pnglib/LICENSE',
        'modules/juce_graphics/image_formats/jpglib/README',
        'modules/juce_core/zip/zlib/README',
    ]
    parts = ['Third-party notices for the JUCE 8.0.15 VST3 worker.\n'
             'The matching source archive contains the complete dependencies and their notices.\n']
    for name in paths:
        parts.append('\n' + '=' * 72 + '\n' + name + '\n' + '=' * 72 + '\n')
        parts.append((root / name).read_text('utf-8', errors='replace'))
    return '\n'.join(parts)

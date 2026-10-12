"""Resolve developer source locations without publishing workstation paths."""
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source_roots():
    shaderixel = Path(os.environ.get('SF_SHADERIXEL_ROOT', ROOT.parent / 'Shaderixel'))
    cydaw = Path(os.environ.get('SF_CYDAW_ROOT', ROOT.parent / 'CyDAW_Project'))
    return {
        'shaderixel': shaderixel,
        'cydaw2': cydaw,
        'imgui': shaderixel / 'external/imgui',
        'juce-license': Path(os.environ.get('SF_JUCE_ROOT', cydaw / 'build/desktop/_deps/juce-src')),
    }


def resolve_source(key, record):
    source = Path(record['source'])
    if source.is_absolute():
        return source  # Read older local lock files without changing their fingerprints.
    component = key.split('/', 1)[0]
    return source_roots()[component] / source

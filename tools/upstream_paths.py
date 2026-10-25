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


def acceptable_hashes(key, record):
    """Accept the upstream artifact or the source-built worker for this release."""
    import json
    hashes = {record['sha256']}
    current = ROOT / 'release-worker.lock.json'
    if record.get('externalArtifact') and key.endswith('/CyDAWPluginWorker.exe') and current.is_file():
        hashes.add(json.loads(current.read_text('utf-8'))['sha256'])
    return hashes

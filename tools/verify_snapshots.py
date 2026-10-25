"""Read-only audit: snapshot integrity and upstream updates are separate results."""
import hashlib
import json
import re
from pathlib import Path
from upstream_paths import resolve_source, acceptable_hashes

ROOT = Path(__file__).resolve().parents[1]
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
upstream = json.loads((ROOT/'upstream.lock.json').read_text('utf-8'))
available, checked, missing_artifacts = [], 0, []
for key, item in upstream['files'].items():
    target = ROOT/'vendor'/key
    if item.get('externalArtifact') and not target.exists():
        missing_artifacts.append(key)
        continue
    assert digest(target) in acceptable_hashes(key, item), f'Snapshot modified: {key}'
    source = resolve_source(key, item)
    if source.exists() and digest(source) != item['sha256']:
        available.append(key)
    checked += 1
ofx = json.loads((ROOT/'openfx.lock.json').read_text('utf-8'))
for key, item in ofx['files'].items():
    expected = item['sha256'] if isinstance(item, dict) else item
    assert digest(ROOT/'vendor/openfx'/key) == expected, f'OpenFX snapshot modified: {key}'
result = {'snapshot_integrity': True, 'upstream_files': checked,
          'available_upstream_updates': available, 'openfx_commit': ofx['commit'],
          'openfx_files': len(ofx['files']), 'missing_external_artifacts': missing_artifacts}
qa = ROOT/'build/qa'
qa.mkdir(parents=True, exist_ok=True)
version = re.search(r'project\(ShaderixFrame VERSION ([\d.]+)', (ROOT/'CMakeLists.txt').read_text('utf-8')).group(1)
(qa/f'snapshots-{version}-verification.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, indent=2))

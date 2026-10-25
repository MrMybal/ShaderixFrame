"""Workspace modes in the real application: native viewports, saved settings, two launches.

The first launch detaches Composition to its own native window, visits the six modes twice and
saves their layouts. The second launch must load those layouts from its ini and do it again.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix='shaderixframe-modes-'))
report = work / 'modes.json'
for launch in (1, 2):
    run = subprocess.run([str(exe), '--hidden', '--warp', f'--mode-smoke={report}'], cwd=exe.parent,
                         timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
    data = json.loads(report.read_text('utf-8'))
    visits = [v for v in data['visits'] if 'mode' in v]
    assert run.returncode == 0 and data['ok'], data
    assert len(visits) == 12 and sorted({v['mode'] for v in visits}) == list(range(6)), visits
    assert all(v['timelineDocked'] and v['compositionDetached'] for v in visits), visits
    stored_at_start = data['visits'][0]['storedAtStart']
    if launch == 2:
        assert stored_at_start >= 5, f'layouts not reloaded from the ini: {stored_at_start}'
    print(f'launch {launch}: 12 mode visits, {data["storedLayouts"]} stored layouts, {stored_at_start} at start')
print('workspace modes smoke passed')

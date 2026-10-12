"""Build a native, validated grading example through ShaderixFrame's own MCP tools."""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
base = json.loads((ROOT / 'examples/PrecisionStudy.sframe').read_text('utf-8'))
for track in base['tracks']:
    for item in track['clips']:
        if item.get('source') and not Path(item['source']).is_absolute():
            item['source'] = str((ROOT/'examples'/item['source']).resolve())
base['name'] = 'Étude étalonnage · corrections et lumières HDR'
base['color']['tone'] = 'aces'
title = next(c for t in base['tracks'] for c in t['clips'] if c['id'] == 7)
title['title']['text'] = 'ÉTALONNAGE\nPRIMAIRES · COURBES · LOOKS'
clip = next(c for t in base['tracks'] for c in t['clips'] if c['id'] == 2)
clip['name'] = 'Lumière HDR · corrections locales'
source = (ROOT / 'src/render/Renderer.cpp').read_text('utf-8')
generator = re.search(r'static const char \*generator = R"\((.*?)\)";', source, re.S).group(1)
generator = generator.replace('float4(saturate(c),1)', 'float4(c*2,1)')
clip['effects'] = [{'id': 'custom', 'enabled': True, 'code': generator, 'graph': None, 'params': {}}]
draft = ROOT / 'build/qa/grading-draft.sframe'
draft.write_text(json.dumps(base, ensure_ascii=False, indent=2), encoding='utf-8')
exe = ROOT / 'build/Release/ShaderixFrame.exe'
process = subprocess.Popen([str(exe), '--hidden', '--mcp'], cwd=exe.parent,
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           text=True, encoding='utf-8', creationflags=subprocess.CREATE_NO_WINDOW)
serial = 0

def call(tool_name, **arguments):
    global serial
    serial += 1
    request = {'jsonrpc': '2.0', 'id': serial, 'method': 'tools/call',
               'params': {'name': tool_name, 'arguments': arguments}}
    process.stdin.write(json.dumps(request)+'\n')
    process.stdin.flush()
    response = json.loads(process.stdout.readline())
    assert not response['result'].get('isError', False), response
    return response['result']['structuredContent']

try:
    call('open_project', path=str(draft))
    primary = call('add_grade', clip=2, name='01 · Balance et primaires')['effect']
    for key, value in {'temperature': 450, 'contrast': 1.07, 'vibrance': .12}.items():
        call('set_parameter', clip=2, effect=primary, key=key, value=value)
    call('set_keyframe', clip=2, effect=primary, key='exposure', value=-.2, time=0)
    call('set_keyframe', clip=2, effect=primary, key='exposure', value=.1, time=12)
    curves = call('add_grade', clip=2, name='02 · Courbe et ombres cyan')['effect']
    call('set_grade_curve', clip=2, effect=curves, curve='master', values=[[0, 0], [.18, .17], [.5, .55], [1, 1]])
    for key, value in {'shadow_b': .012, 'shadow_g': .008}.items():
        call('set_parameter', clip=2, effect=curves, key=key, value=value)
    local = call('add_grade', clip=2, name='03 · Lumière locale')['effect']
    for key, value in {'mask_mode': 2, 'mask_x': .27, 'mask_y': -.19, 'mask_width': .30,
                       'mask_height': .42, 'mask_feather': .1, 'exposure': .3, 'high_r': .035}.items():
        call('set_parameter', clip=2, effect=local, key=key, value=value)
    example = ROOT/'examples/GradingStudy.sframe'
    call('save_project', path=str(example))
    # Projects saved by the runtime use resolved paths; distributed examples use sibling assets.
    document = json.loads(example.read_text('utf-8'))
    for track in document['tracks']:
        for item in track['clips']:
            if item.get('source'):
                item['source'] = Path(item['source']).relative_to(ROOT/'examples').as_posix()
    example.write_text(json.dumps(document, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
finally:
    process.stdin.close()
    try:
        process.wait(timeout=20)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    process.stdout.close()
    process.stderr.close()
assert process.returncode == 0
print('GradingStudy.sframe: native schema8, HDR, animated primaries, curve and independent local mask.')

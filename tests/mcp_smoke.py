"""Exercise the real executable over stdio, including GUI/session integration."""
import json
import math
import subprocess
import sys
import time
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
sandbox = exe.parent / 'test-output' / 'mcp'
sandbox.mkdir(parents=True, exist_ok=True)
recovery_dir = sandbox / 'recovery'
recovery_dir.mkdir(exist_ok=True)
previous_recoveries = set(recovery_dir.glob('*.sframe'))
process = subprocess.Popen([str(exe), '--mcp', '--hidden', '--recovery-dir=' + str(recovery_dir)], stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
counter = 0
completed = False

def rpc(method, params=None):
    global counter
    counter += 1
    request = {'jsonrpc': '2.0', 'id': counter, 'method': method}
    if params is not None:
        request['params'] = params
    process.stdin.write(json.dumps(request) + '\n')
    process.stdin.flush()
    line = process.stdout.readline()
    assert line, f'Server ended before {method}; exit={process.poll()}'
    response = json.loads(line)
    assert response['id'] == counter and 'error' not in response, response
    return response['result']

def call(name, args=None, error=False):
    response = rpc('tools/call', {'name': name, 'arguments': args or {}})
    assert response.get('isError', False) == error, response
    return response.get('structuredContent', {})

def wait_export(job):
    deadline = time.monotonic() + 15
    progress = 0
    while time.monotonic() < deadline:
        current = call('export_status', {'job': job})['export']
        assert progress <= current['progress'] <= 1
        progress = current['progress']
        if current['state'] in ('completed', 'cancelled', 'failed'):
            return current
        time.sleep(0.02)
    raise AssertionError('Background export timeout')

try:
    assert rpc('initialize', {'protocolVersion': '2025-06-18', 'capabilities': {},
                              'clientInfo': {'name': 'integration-test', 'version': '1'}})['protocolVersion'] == '2025-06-18'
    process.stdin.write('{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
    process.stdin.flush()
    catalogue = rpc('tools/list')['tools']
    assert len(catalogue) == 160
    call('new_project', {'name':'Clip container MCP','width':64,'height':64,'duration':2})
    container=call('add_generator',{'duration':2})['clip']
    call('make_clip_container',{'clip':container})
    local=call('get_clip_composition',{'clip':container})['project']
    assert local['schema']==10 and local['tracks'][0]['clips'][0]['generator']
    local['name']='Local timeline'
    call('set_clip_composition',{'clip':container,'project':local})
    assert call('get_clip_composition',{'clip':container})['project']['name']=='Local timeline'
    bad=dict(local,width=1)
    call('set_clip_composition',{'clip':container,'project':bad},error=True)
    assert call('get_clip_composition',{'clip':container})['project']['width']==64
    call('undo')
    assert call('get_clip_composition',{'clip':container})['project']['name']!='Local timeline'
    call('set_motion_widget',{'clip':container,'layout':{'type':'container','anchorX':.5,'anchorY':.5,'pivotX':.5,'pivotY':.5}})
    call('set_parameter',{'clip':container,'key':'widget_width','value':32})
    call('set_parameter',{'clip':container,'key':'widget_height','value':32})
    assert call('get_motion_widgets',{'time':1})['widgets'][0]['x']==16
    call('set_motion_widget',{'clip':container,'layout':{'type':'container','parent':container}},error=True)
    assert call('get_motion_widgets',{'time':1})['widgets'][0]['x']==16
    call('new_project', {'name': 'Mixer MCP', 'width': 320, 'height': 180, 'duration': .1})
    mix_track = call('add_track', {'kind': 'audio', 'name': 'Dialogue'})['track']
    mix_bus = call('add_bus', {'name': 'Dialogue bus'})['bus']
    mix_aux = call('add_bus', {'name': 'Delay bus'})['bus']
    channel = {'scope': 'track', 'target': mix_track}
    bus_channel = {'scope': 'bus', 'target': mix_bus}
    call('set_mix_channel', dict(channel, output=mix_bus, gain=-6, pan=.2))
    call('set_send', dict(channel, bus=mix_aux, gain=-12, pre_fader=True))
    call('add_mix_effect', dict(bus_channel, id='cydaw.eq-three'))
    call('set_mix_keyframe', dict(bus_channel, effect=0, key='low', value=-3, time=0))
    call('set_mix_keyframe', dict(bus_channel, effect=0, key='low', value=0, time=.1))
    call('add_mix_effect', dict(bus_channel, id='cydaw.limiter'))
    call('set_mix_effect', dict(bus_channel, effect=0, position=1))
    mix_state = call('status')['project']
    assert mix_state['schema'] == 10 and len(mix_state['buses']) == 2
    mix_stack = next(b['mix']['effects'] for b in mix_state['buses'] if b['id'] == mix_bus)
    assert mix_stack[1]['id'] == 'cydaw.eq-three' and len(mix_stack[1]['params']['low']['keys']) == 2
    call('set_mix_parameter', {'scope': 'master', 'key': 'gain', 'value': -3})
    graph_mix = call('get_audio_graph')['graph']
    assert graph_mix['version'] == '2.0.0' and any(n['processor'] == 'cydaw.eq-three' for n in graph_mix['nodes'])
    frozen_mix = call('status')['project']
    call('set_mix_channel', dict(bus_channel, output=mix_bus), error=True)
    call('set_mix_parameter', dict(bus_channel, effect=0, key='ceiling', value=99), error=True)
    call('set_mix_channel', dict(channel, output=999999), error=True)
    call('add_mix_effect', dict(bus_channel, id='color'), error=True)
    assert call('status')['project'] == frozen_mix
    call('set_mix_channel', dict(bus_channel, locked=True))
    call('remove_mix_effect', dict(bus_channel, effect=0), error=True)
    call('set_mix_channel', dict(bus_channel, locked=False))
    call('set_mix_keyframe', dict(channel, key='gain', value=-12, time=0))
    call('set_mix_keyframe', dict(channel, key='gain', value=0, time=.1))
    before_point = call('status')['project']
    call('set_mix_keyframe', dict(channel, key='gain', value=-1, time=.08, previous_time=.1))
    call('undo')
    assert call('status')['project'] == before_point
    call('remove_mix_keyframe', dict(channel, key='gain'))
    call('remove_mix_keyframe', dict(bus_channel, effect=1, key='low', time=0))
    call('remove_send', dict(channel, bus=mix_aux))
    call('remove_mix_effect', dict(bus_channel, effect=1))
    call('remove_bus', {'bus': mix_bus})
    assert next(t for t in call('status')['project']['tracks'] if t['id'] == mix_track)['mix']['output'] == 0
    call('undo')
    assert any(b['id'] == mix_bus for b in call('status')['project']['buses'])
    selection_schema = next(t for t in catalogue if t['name'] == 'select_clips')['inputSchema']['properties']['clips']
    assert selection_schema['items']['type'] == 'integer' and selection_schema['maxItems'] == 4096
    call('new_project', {'name': 'MCP test', 'width': 320, 'height': 180, 'duration': 0.5})
    clip = call('add_generator', {'duration': 0.5})['clip']
    call('add_effect', {'clip': clip, 'id': 'vignette'})
    call('set_keyframe', {'clip': clip, 'key': 'opacity', 'time': 0, 'value': 0, 'curve': 1})
    call('set_keyframe', {'clip': clip, 'key': 'opacity', 'time': 0.5, 'value': 1})
    before = call('status')['project']
    call('set_clip', {'clip': clip, 'start': -5}, error=True)
    assert call('status')['project'] == before
    call('set_shader', {'clip': clip, 'effect': 0, 'code': 'not HLSL'}, error=True)
    graph = {'name': 'Désaturation Shaderixel', 'category': 'Tests', 'inputs': 1,
             'premultiplied': False, 'nextId': 5, 'nodes': [
                 {'id': 1, 'op': 'input', 'pos': [35, 35], 'in': [], 'value': [0, 0, 0, 0]},
                 {'id': 2, 'op': 'desaturate', 'pos': [275, 65], 'in': [[1, 1], [3, 0]], 'value': [0, 0, 0, 0]},
                 {'id': 3, 'op': 'param', 'name': 'amount', 'pos': [35, 210], 'in': [], 'value': [0.25, 0, 0, 0]},
                 {'id': 4, 'op': 'output', 'pos': [535, 65], 'in': [[0, 0], [2, 0], [0, 0]], 'value': [0, 0, 0, 0]},
             ]}
    call('set_graph', {'clip': clip, 'effect': 0, 'graph': graph})
    assert call('get_graph', {'clip': clip, 'effect': 0})['graph'] == graph
    call('split_clip', {'clip': clip, 'time': 0.25})
    call('undo')
    call('redo')
    effects = call('list_effects')['effects']
    assert len(effects) == 429 and any(e['id'] == 'cydaw.limiter' for e in effects)
    assert {e['id'] for e in effects if e['kind'] == 'transition'} == {'transition.crossfade', 'transition.wipe'}
    title = call('add_title', {'text': 'Été · VFX\nMontage vidéo', 'start': 0, 'duration': 0.5})['clip']
    call('set_title', {'clip': title, 'font': 'Segoe UI', 'alignment': 1, 'bold': True, 'italic': True})
    call('set_parameter', {'clip': title, 'key': 'font_size', 'value': 24})
    call('set_keyframe', {'clip': title, 'key': 'text_r', 'time': 0, 'value': 0.8})
    call('set_mask', {'clip': title, 'shape': 'ellipse', 'inverted': False})
    call('set_transition', {'clip': title, 'id': 'transition.wipe', 'duration': 0.15})
    call('set_parameter', {'clip': title, 'key': 'transition_direction', 'value': 2})
    call('set_keyframe', {'clip': title, 'key': 'transition_progress', 'time': 0, 'value': 0})
    call('set_keyframe', {'clip': title, 'key': 'transition_progress', 'time': 0.15, 'value': 1})
    transition_code = 'float4 Effect(EffectContext ctx){return lerp(InputUV(1,ctx.uv),InputUV(0,ctx.uv),ctx.transitionProgress);}'
    call('set_transition_shader', {'clip': title, 'code': transition_code})
    transition_project = call('status')['project']
    title_transition = next(c for t in transition_project['tracks'] for c in t['clips'] if c['id'] == title)['transition']
    assert transition_project['schema'] == 10 and title_transition['code'] == transition_code and title_transition['enabled']
    call('remove_transition', {'clip': title})
    call('undo')
    assert call('status')['project'] == transition_project
    call('set_transition', {'clip': title, 'reset_progress': True})
    call('set_parameter', {'clip': title, 'key': 'mask_width', 'value': 0.9})
    call('set_keyframe', {'clip': title, 'key': 'mask_rotation', 'time': 0.5, 'value': 25})
    call('add_effect', {'clip': title, 'id': 'film-grain'})
    call('add_effect', {'clip': title, 'id': 'color'})
    call('set_effect', {'clip': title, 'effect': 1, 'position': 0, 'enabled': False})
    before = call('status')['project']
    title_state = next(c for t in before['tracks'] for c in t['clips'] if c['id'] == title)
    assert title_state['title']['text'].startswith('Été') and title_state['effects'][0]['id'] == 'color'
    for name, args in (
        ('set_title', {'clip': title, 'alignment': 9}),
        ('set_title', {'clip': clip, 'text': 'Wrong clip'}),
        ('set_mask', {'clip': title, 'shape': 'triangle'}),
        ('set_parameter', {'clip': title, 'key': 'mask_width', 'value': -1}),
        ('set_keyframe', {'clip': title, 'key': 'font_size', 'time': 0.2, 'value': -4}),
        ('set_effect', {'clip': title, 'effect': 0, 'position': 99}),
        ('set_transition', {'clip': title, 'duration': 1}),
        ('set_transition', {'clip': title, 'id': 'color'}),
        ('set_transition_shader', {'clip': title, 'code': 'Invalid HLSL'}),
        ('set_parameter', {'clip': title, 'key': 'transition_progress', 'value': 2}),
    ):
        call(name, args, error=True)
        assert call('status')['project'] == before
    call('remove_effect', {'clip': title, 'effect': 0})
    call('save_project', {'path': str(sandbox / 'session.sframe')})
    call('render_frame', {'path': str(sandbox / 'preview.png'), 'time': 0.2})
    call('export_video', {'path': str(sandbox / 'render.mp4')})
    assert call('export_status')['exports'] == []
    immutable = call('status')
    job = call('start_export', {'kind': 'video', 'path': str(sandbox / 'background.mp4')})['job']
    assert call('status')['project'] == immutable['project']
    call('new_project', {'name': 'Edited while exporting', 'width': 640, 'height': 360, 'duration': 0.1})
    editing = call('add_generator', {'duration': 0.1})['clip']
    call('set_parameter', {'clip': editing, 'key': 'opacity', 'value': 0.2})
    exported = wait_export(job)
    assert exported['state'] == 'completed', exported
    assert exported['project'] == 'MCP test' and exported['width'] == 320 and exported['duration'] == 0.5
    assert exported['progress'] == 1 and not exported['cancellable']
    assert call('status')['project']['name'] == 'Edited while exporting'
    assert call('cancel_export', {'job': job})['accepted'] is False
    call('open_project', {'path': str(sandbox / 'session.sframe')})
    video_clips = call('import_media', {'path': str(sandbox / 'render.mp4')})['clips']
    assert len(video_clips) == 2
    def imported():
        project = call('status')['project']
        return {c['id']: c for t in project['tracks'] for c in t['clips'] if c['id'] in video_clips}
    clips = imported()
    assert clips[video_clips[0]]['link'] == clips[video_clips[1]]['link'] != 0
    call('set_clip', {'clip': video_clips[0], 'start': 0.5, 'fade_in': 0.1, 'fade_out': 0.1})
    clips = imported()
    assert all(c['start'] == 0.5 for c in clips.values())
    call('set_keyframe', {'clip': video_clips[0], 'key': 'opacity', 'time': 0.6, 'value': 0.8})
    call('set_clip', {'clip': video_clips[0], 'start': 0.4})
    assert abs(imported()[video_clips[0]]['params']['opacity']['keys'][0]['time'] - 0.5) < 1e-7
    before = call('status')['project']
    call('set_clip', {'clip': video_clips[0], 'fade_in': 99}, error=True)
    assert call('status')['project'] == before
    call('split_clip', {'clip': video_clips[0], 'time': 0.6})
    call('undo')
    copied = call('duplicate_clip', {'clip': video_clips[0]})['clip']
    call('delete_clip', {'clip': copied})
    call('unlink_clip', {'clip': video_clips[0]})
    assert all(c['link'] == 0 for c in imported().values())
    call('link_clips', {'clip': video_clips[0], 'other': video_clips[1]})
    call('add_audio_effect', {'clip': video_clips[1], 'id': 'cydaw.limiter'})
    call('add_audio_effect', {'clip': video_clips[1], 'id': 'cydaw.eq-three'})
    call('set_effect', {'clip': video_clips[1], 'effect': 1, 'position': 0})
    assert imported()[video_clips[1]]['effects'][0]['id'] == 'cydaw.eq-three'
    call('remove_effect', {'clip': video_clips[1], 'effect': 1})
    before_failure = call('status')['project']
    call('set_transition', {'clip': video_clips[1], 'id': 'transition.crossfade'}, error=True)
    assert call('status')['project'] == before_failure
    selected = call('select_clips', {'clips': [video_clips[1], video_clips[0], video_clips[1]]})['clips']
    state = call('status')
    assert set(selected) == set(video_clips) and state['selection'] == selected
    assert state['selected'] == video_clips[1] and state['version'] == '0.13.0-alpha.1'
    # The selection is shared UI state and never creates a document/history entry.
    assert call('select_clips', {'clips': []})['clips'] == []
    assert call('status')['selected'] == 0
    call('select_clips', {'clips': video_clips})
    batch_before = call('status')
    for ids in ([0], [-1], [1.5], ['2'], [True], [2147483648], [999999], [video_clips[0], 999999], [video_clips[0]] * 4097):
        call('select_clips', {'clips': ids}, error=True)
        current = call('status')
        assert current['project'] == batch_before['project'] and current['selection'] == batch_before['selection']
        assert current['selected'] == batch_before['selected'] and current['dirty'] == batch_before['dirty']
    call('move_clips', {'clips': video_clips + video_clips, 'delta': 0.1})
    assert all(abs(c['start'] - 0.5) < 1e-7 for c in imported().values())
    call('undo')
    assert call('status')['project'] == batch_before['project']
    copies = call('duplicate_clips', {'clips': video_clips + video_clips})['clips']
    assert len(copies) == 2 and call('status')['selection'] == copies
    call('delete_clips', {'clips': copies})
    assert call('status')['selection'] == []
    call('split_clips', {'clips': video_clips, 'time': 0.6})
    call('undo')
    for name, args in (('move_clips', {'clips': video_clips, 'delta': -100}),
                       ('move_clips', {'clips': [], 'delta': 1}),
                       ('delete_clips', {'clips': []}),
                       ('duplicate_clips', {'clips': []}),
                       ('split_clips', {'clips': video_clips, 'time': 0})):
        before_failure = call('status')
        call(name, args, error=True)
        after_failure = call('status')
        assert after_failure['project'] == before_failure['project'] and after_failure['selection'] == before_failure['selection']
    call('transport', {'time': 0.1, 'playing': False})
    call('open_project', {'path': str(sandbox / 'session.sframe')})
    assert call('status')['project']['name'] == 'MCP test'
    opened = call('status')
    call('undo')
    assert call('status')['project'] == opened['project'] and not call('status')['dirty']
    call('new_project', {'name': 'Ripple test', 'width': 320, 'height': 180, 'duration': 8})
    first = call('add_generator', {'start': 0, 'duration': 2})['clip']
    middle = call('add_generator', {'start': 2, 'duration': 2})['clip']
    tail = call('add_title', {'text': 'Tail', 'start': 4, 'duration': 2})['clip']
    call('set_keyframe', {'clip': tail, 'key': 'opacity', 'time': 4.5, 'value': 0.7})
    intact = call('status')['project']
    call('delete_clips', {'clips': [middle], 'ripple': True})
    rippled = call('status')['project']
    tail_state = next(c for t in rippled['tracks'] for c in t['clips'] if c['id'] == tail)
    assert rippled['duration'] == 6 and tail_state['start'] == 2
    assert tail_state['params']['opacity']['keys'][0]['time'] == 2.5
    call('undo')
    assert call('status')['project'] == intact
    call('redo')
    assert call('status')['project'] == rippled
    call('undo')
    crossing = call('add_title', {'text': 'Crossing', 'start': 1, 'duration': 4})['clip']
    before_failure = call('status')['project']
    call('delete_clips', {'clips': [middle], 'ripple': True}, error=True)
    assert call('status')['project'] == before_failure
    call('delete_clip', {'clip': crossing})
    call('delete_clip', {'clip': middle, 'ripple': True})
    assert call('status')['project']['duration'] == 6
    audio_job = call('start_export', {'kind': 'audio', 'path': str(sandbox / 'background.wav')})['job']
    assert wait_export(audio_job)['state'] == 'completed'
    assert (sandbox / 'background.wav').stat().st_size == 44 + 6 * 48000 * 4
    before_failure = call('status')['project']
    count = len(call('export_status')['exports'])
    for name, args in (
        ('start_export', {'kind': 'gif', 'path': str(sandbox / 'bad.gif')}),
        ('start_export', {'kind': 'audio', 'path': str(sandbox / 'bad.mp4')}),
        ('start_export', {'kind': 'audio', 'path': ''}),
        ('export_status', {'job': 99999}),
        ('cancel_export', {'job': 99999}),
    ):
        call(name, args, error=True)
        assert call('status')['project'] == before_failure and len(call('export_status')['exports']) == count
    call('new_project', {'name': 'Cancellation test', 'width': 320, 'height': 180, 'duration': 600})
    call('add_generator', {'duration': 600})
    cancel_path = sandbox / 'cancelled.mp4'
    cancel_path.write_bytes(b'Existing destination')
    cancelled_job = call('start_export', {'kind': 'video', 'path': str(cancel_path)})['job']
    queued_job = call('start_export', {'kind': 'audio', 'path': str(sandbox / 'busy.wav')})['job']
    assert call('export_status', {'job': queued_job})['export']['state'] == 'queued'
    assert call('cancel_export', {'job': queued_job})['accepted'] is True
    call('export_video', {'path': str(cancel_path)}, error=True)
    call('export_audio', {'path': str(sandbox / 'busy.wav')}, error=True)
    cancellation = call('cancel_export', {'job': cancelled_job})
    assert cancellation['accepted'] is True
    assert wait_export(cancelled_job)['state'] == 'cancelled' and cancel_path.read_bytes() == b'Existing destination'
    assert not list(sandbox.glob('*.pending-*'))
    failure_job = call('start_export', {'kind': 'audio', 'path': str(sandbox / 'absent/error.wav')})['job']
    assert wait_export(failure_job)['state'] == 'failed'
    # Exercise every new 0.8 tool against the real executable and its isolated worker.
    call('new_project', {'name': 'Advanced MCP', 'width': 320, 'height': 180, 'duration': 2})
    first = call('add_generator', {'start': 0, 'duration': .5})['clip']
    second = call('duplicate_clip', {'clip': first})['clip']
    before = call('status')['project']
    call('ripple_trim', {'clip': first, 'delta': -.1})
    call('undo')
    assert call('status')['project'] == before
    call('roll_trim', {'clip': first, 'next': second, 'delta': .1})
    call('undo')
    call('overlap_transition', {'clip': first, 'next': second, 'duration': .1})
    assert next(c for t in call('status')['project']['tracks'] for c in t['clips'] if c['id'] == second)['transition']['enabled']
    call('undo')
    call('remove_range', {'begin': .1, 'end': .2})
    assert abs(call('status')['project']['duration'] - 1.9) < 1e-6
    call('undo')
    points = [[-.3, -.3, -.3, -.3, -.3, -.3], [.3, -.3, .3, -.3, .3, -.3], [0, .3, 0, .3, 0, .3]]
    call('set_bezier_mask', {'clip': first, 'points': points})
    saved = call('status')['project']
    call('set_bezier_mask', {'clip': first, 'points': [[0] * 6]}, error=True)
    assert call('status')['project'] == saved
    call('add_effect', {'clip': first, 'id': 'vignette'})
    call('set_shader', {'clip': first, 'effect': 0, 'code': '//@passes 2\nfloat4 Effect(EffectContext c){return Prev(c.px);}'})
    assert len(call('list_export_presets')['presets']) == 8
    dsp = {'schema': 2, 'nodes': [{'id': 1, 'processor': 'cydaw.eq-three'}],
           'connections': [{'source': 0, 'destination': 1}, {'source': 1, 'destination': 0}]}
    dsp_index = call('set_dsp_graph', {'scope': 'master', 'graph': dsp})['effect']
    assert call('get_dsp_graph', {'scope': 'master', 'effect': dsp_index})['graph'] == dsp
    native_index = call('add_external_audio', {'scope': 'master', 'kind': 'native', 'path': str(exe.parent / 'native_gain.dll')})['effect']
    call('set_mix_parameter', {'scope': 'master', 'effect': native_index, 'key': 'p0', 'value': .25})
    call('set_mix_parameter', {'scope': 'master', 'effect': native_index, 'key': 'p0', 'value': 2}, error=True)
    assert wait_export(call('start_export', {'kind': 'audio', 'path': str(sandbox / 'native-master.wav')})['job'])['state'] == 'completed'
    call('new_project', {'name': 'Tracked source', 'width': 320, 'height': 180, 'duration': .4})
    marker = call('add_generator', {'start': 0, 'duration': .4})['clip']
    call('add_effect', {'clip': marker, 'id': 'vignette'})
    call('set_shader', {'clip': marker, 'effect': 0, 'code': 'float4 Effect(EffectContext c){float2 q=abs(c.uv-float2(.5+c.time*.12,.5));return float4(any(q>float2(.025,.04))?float3(.05,.05,.05):float3(1,1,1),1);}'})
    marker_path = sandbox / 'tracking-marker.mp4'
    assert wait_export(call('start_export', {'kind': 'video', 'path': str(marker_path)})['job'])['state'] == 'completed'
    call('new_project', {'name': 'Tracker MCP', 'width': 320, 'height': 180, 'duration': .4})
    tracked = call('import_media', {'path': str(marker_path), 'start': 0})['clips'][0]
    call('set_mask', {'clip': tracked, 'shape': 'rectangle'})
    for key in ('mask_width', 'mask_height'):
        call('set_parameter', {'clip': tracked, 'key': key, 'value': .1})
    tracking_job = call('start_mask_tracking', {'clip': tracked, 'begin': 0, 'end': .35, 'radius': 12})['job']
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        tracking = call('mask_tracking_status')['tracking']
        if tracking['state'] != 'running':
            break
        time.sleep(.02)
    assert tracking['state'] == 'completed' and tracking['keys'] > 8, tracking
    assert call('cancel_mask_tracking')['accepted'] is False
    call('apply_mask_tracking', {'job': tracking_job})
    assert next(c for t in call('status')['project']['tracks'] for c in t['clips'] if c['id'] == tracked)['params']['mask_x']['keys']
    tracked_grade = call('add_grade', {'clip': tracked, 'name': 'Zone suivie'})['effect']
    call('set_parameter', {'clip': tracked, 'effect': tracked_grade, 'key': 'mask_mode', 'value': 2})
    call('set_parameter', {'clip': tracked, 'effect': tracked_grade, 'key': 'mask_width', 'value': .12})
    call('set_parameter', {'clip': tracked, 'effect': tracked_grade, 'key': 'mask_height', 'value': .12})
    call('set_parameter', {'clip': tracked, 'effect': tracked_grade, 'key': 'mask_x', 'value': 0})
    grading_track_job = call('start_mask_tracking', {'clip': tracked, 'effect': tracked_grade, 'begin': 0, 'end': .35, 'radius': 12})['job']
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        tracking = call('mask_tracking_status')['tracking']
        if tracking['state'] != 'running':
            break
        time.sleep(.02)
    assert tracking['state'] == 'completed' and tracking['effect'] == tracked_grade, tracking
    call('apply_mask_tracking', {'job': grading_track_job})
    grade_clip = next(c for t in call('status')['project']['tracks'] for c in t['clips'] if c['id'] == tracked)
    assert grade_clip['effects'][tracked_grade]['params']['mask_x']['keys']
    proxy_job = call('prepare_proxy', {'clip': tracked, 'path': str(sandbox / 'preview-proxy.mp4')})['job']
    assert wait_export(proxy_job)['state'] == 'completed'
    call('set_preview_proxies', {'enabled': True})
    call('set_preview_proxies', {'enabled': False})
    stamp = str(time.time_ns())
    formats = [('video', sandbox / ('advanced-' + stamp + '.avi')),
               ('analysis', sandbox / ('advanced-' + stamp + '.json')),
               ('images', sandbox / ('advanced-png-' + stamp))]
    export_project = call('status')['project']
    jobs = [(kind, path, call('start_export', {'kind': kind, 'path': str(path)})['job']) for kind, path in formats]
    for kind, path, job in jobs:
        finished = wait_export(job)
        assert finished['state'] == 'completed', finished
        if kind == 'video':
            assert path.read_bytes()[:4] == b'RIFF' and finished['format'] == 'avi'
        elif kind == 'analysis':
            assert json.loads(path.read_text('utf-8'))['true_peak_measured'] is True
        else:
            assert json.loads((path / 'sequence.json').read_text('utf-8'))['frames'] == math.ceil(export_project['duration'] * export_project['fps'])
    call('new_project', {'name': 'Precision and OFX', 'width': 64, 'height': 64, 'duration': .1})
    precise_clip = call('add_generator', {'duration': .1})['clip']
    call('set_color_management', {'space': 'linear709', 'tone': 'reinhard', 'backgroundAlpha': 0, 'imageFormat': 'exr16'})
    call('set_source_color', {'clip': precise_clip, 'space': 'auto', 'premultiplied': False})
    ofx_path = str(exe.parent/'ofx_gain.ofx')
    assert call('inspect_ofx', {'path': ofx_path})['plugin']['parameters']
    ofx_index = call('add_ofx_effect', {'clip': precise_clip, 'path': ofx_path})['effect']
    call('set_parameter', {'clip': precise_clip, 'effect': ofx_index, 'key': 'p0', 'value': 1})
    call('render_frame', {'path': str(sandbox/'precise.exr'), 'format': 'exr16', 'time': 0})
    call('render_frame', {'path': str(sandbox/'precise.png'), 'format': 'png16', 'time': 0})
    assert (sandbox/'precise.exr').read_bytes()[:4] == b'\x76\x2f\x31\x01'
    assert (sandbox/'precise.png').read_bytes()[24] == 16
    for precise_format, extension in [('exr16', '.exr'), ('png16', '.png')]:
        call('set_color_management', {'imageFormat': precise_format})
        destination = sandbox/('sequence-'+precise_format+'-'+stamp)
        precise_job = call('start_export', {'kind': 'images', 'path': str(destination)})['job']
        precise_result = wait_export(precise_job)
        assert precise_result['state']=='completed' and precise_result['format']==precise_format, precise_result
        files = list(destination.glob('*'+extension))
        assert len(files)==3
        if extension=='.png':
            assert all(file.read_bytes()[24]==16 for file in files)
    grade_index = call('add_grade', {'clip': precise_clip, 'name': 'Grade MCP'})['effect']
    grade_target = {'clip': precise_clip, 'effect': grade_index}
    assert len(call('list_grade_presets')['presets']) == 5
    call('apply_grade_preset', dict(grade_target, name='Warm'))
    call('set_grade_curve', dict(grade_target, curve='master', values=[[0, 0], [.5, .6], [1, 1]]))
    call('neutral_grade', dict(grade_target, r=.4, g=.2, b=.1))
    cube = sandbox/'identity-grade.cube'
    cube.write_text('LUT_1D_SIZE 2\n0 0 0\n1 1 1\n', encoding='utf-8')
    call('import_grade_lut', dict(grade_target, path=str(cube), input='srgb', output='srgb'))
    call('set_grade', dict(grade_target, settings={'name': 'Grade portable', 'maskPoints': [[-.5, -.5], [.5, -.5], [0, .5]]}))
    call('set_keyframe', dict(grade_target, key='exposure', time=0, value=0))
    call('set_keyframe', dict(grade_target, key='exposure', time=.1, value=1))
    grade_preset = sandbox/'portable.sgrade'
    call('save_grade_preset', dict(grade_target, path=str(grade_preset)))
    call('apply_grade_preset', dict(grade_target, name='Neutre'))
    call('load_grade_preset', dict(grade_target, path=str(grade_preset)))
    restored = next(c for t in call('status')['project']['tracks'] for c in t['clips'] if c['id']==precise_clip)['effects'][grade_index]
    assert restored['grading']['lut']['dimension']==1 and len(restored['params']['exposure']['keys'])==2
    call('set_grade_curve', dict(grade_target, curve='master', values=[[0, 0], [0, 1]]), error=True)
    scopes = call('video_scopes', {'time': 0, 'bins': True})['scopes']
    assert scopes['samples']>0 and len(scopes['waveform'])==256*128 and len(scopes['histogram'])==768
    call('render_frame', {'path': str(sandbox/'graded.exr'), 'format':'exr16', 'time':0})
    automatic = call('get_layer_graph')
    assert automatic['explicit'] is False
    layer_graph = automatic['graph']
    call('set_layer_graph', {'graph': layer_graph})
    blend_node = next(n['id'] for n in layer_graph['nodes'] if n['kind'] == 'blend')
    call('set_layer_parameter', {'node': blend_node, 'key': 'opacity', 'value': .5})
    call('set_layer_keyframe', {'node': blend_node, 'key': 'opacity', 'time': 0, 'value': 0, 'curve': 0})
    call('set_layer_keyframe', {'node': blend_node, 'key': 'opacity', 'time': .1, 'value': 1, 'curve': 0})
    authored = call('get_layer_graph')
    assert authored['explicit'] is True
    blend = next(n for n in authored['graph']['nodes'] if n['id'] == blend_node)
    assert blend['params']['opacity']['value'] == .5 and len(blend['params']['opacity']['keys']) == 2
    before = call('status')['project']
    call('set_layer_parameter', {'node': blend_node, 'key': 'opacity', 'value': 2}, error=True)
    assert call('status')['project'] == before
    call('render_frame', {'path': str(sandbox/'layer-graph.exr'), 'format': 'exr16', 'time': .05})
    assert (sandbox/'layer-graph.exr').read_bytes()[:4] == b'\x76\x2f\x31\x01'
    vst_fixture = Path(__file__).resolve().parents[1]/'vendor/cydaw2/build/desktop-asio/CyDAWTestGain_artefacts/Release/VST3/CyDAW Test Gain.vst3'
    if (vst_fixture/'Contents/x86_64-win/CyDAW Test Gain.vst3').is_file():
        vst_index = call('add_external_audio', {'scope': 'master', 'kind': 'vst3', 'path': str(vst_fixture)})['effect']
        vst_target = {'scope': 'master', 'effect': vst_index}
        call('show_plugin_editor', dict(vst_target, visible=False))
        opaque = call('capture_plugin_state', vst_target)['state']
        assert opaque and call('get_plugin_state', vst_target)['parameters']
        call('set_plugin_state', dict(vst_target, state=opaque))
        preset = str(sandbox/'gain.sfvstpreset')
        call('save_plugin_preset', dict(vst_target, path=preset))
        call('load_plugin_preset', dict(vst_target, path=preset))

    else:
        print('SKIP: optional CyDAW VST3 fixture not installed; use prepare_dependencies.py --tests.')

    call('new_project', {'name': 'Shutdown test', 'width': 320, 'height': 180, 'duration': 600})
    call('add_generator', {'duration': 600})
    # EOF must cancel and join a running job without replacing a destination.
    shutdown_path = sandbox / 'shutdown.mp4'
    shutdown_path.write_bytes(b'Existing destination')
    shutdown_job = call('start_export', {'kind': 'video', 'path': str(shutdown_path)})['job']
    assert call('export_status', {'job': shutdown_job})['export']['state'] in ('queued', 'running')
    completed = True

    print('MCP stdio integration passed: 99 tools, grading/LUT/scopes/presets/local tracking, ripple/roll, Bezier, proxies, isolated SDK, DSP graphs, render queue and media.')
finally:
    process.stdin.close()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    assert process.returncode == 0, f'Server exit: {process.returncode}'
    if completed:
        assert shutdown_path.read_bytes() == b'Existing destination' and not list(sandbox.glob('*.pending-*'))
        new_recoveries = set(recovery_dir.glob('*.sframe')) - previous_recoveries
        assert new_recoveries, 'Switching away from a dirty project must preserve a recovery'
        recovery = json.loads(next(iter(new_recoveries)).read_text(encoding='utf-8'))
        assert recovery['schema'] == 10 and recovery['recovery']['processId'] == process.pid

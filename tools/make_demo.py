"""Build original demo audio and a self-contained example project with embedded shaders."""
import array
import copy
import json
import math
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / 'examples'
EXAMPLES.mkdir(exist_ok=True)
from demo_audio import write_demo_audio
write_demo_audio(EXAMPLES / 'firstlight.wav')


def param(value, keys=None):
    return {'value': value, 'keys': [dict(time=t, value=v, curve=1, shape=0) for t, v in (keys or [])]}

color = {'id': 'color', 'enabled': True, 'code': (ROOT / 'shaders/color.hlsl').read_text('utf-8'),
         'graph': None, 'params': {'exposure': param(0.15, [(0, -0.4), (4, 0.2), (8, 0), (12, 0.3)]),
                                   'saturation': param(1.05), 'contrast': param(1)}}
grain = {'id': 'film-grain', 'enabled': True, 'code': (ROOT / 'plugins/film-grain/grain.hlsl').read_text('utf-8'),
         'graph': None, 'params': {'amount': param(0.025)}}
video = {'id': 2, 'name': 'Aurora · lumière HLSL', 'source': '', 'generator': True,
         'start': 0, 'duration': 12, 'in': 0, 'speed': 1,
         'link': 5, 'fadeIn': 1.2, 'fadeOut': 1.5,
         'params': {'opacity': param(1, [(3, 1), (6, 0.65), (9, 1)]),
                    'scale': param(1), 'x': param(0), 'y': param(0), 'rotation': param(0)},
         'effects': [color, grain]}
audio = {'id': 4, 'name': 'Première lumière · pad original', 'source': 'firstlight.wav', 'generator': False,
         'start': 0, 'duration': 12, 'in': 0, 'speed': 1,
         'link': 5, 'fadeIn': 0.1, 'fadeOut': 0.2,
         'params': {'gain': param(0, [(0, -12), (2, 0), (10, 0), (12, -18)])}, 'effects': []}
def track(id, name, kind, clips):
    return dict(id=id, name=name, kind=kind, muted=False, solo=False, locked=False, gain=0, pan=0, clips=clips,
                mix=dict(output=0, sends=[], effects=[], params={}))

title = dict(id=7, name='Première lumière · titre', source='', generator=False,
             start=0, duration=12, **{'in': 0}, speed=1, link=0, fadeIn=1.8, fadeOut=1.5,
             title=dict(enabled=True, text='PREMIÈRE LUMIÈRE', font='Segoe UI', alignment=1, bold=True, italic=False),
             mask=dict(shape=0, inverted=False),
             params={'font_size': param(44), 'text_r': param(1), 'text_g': param(0.93), 'text_b': param(0.84),
                     'text_a': param(1), 'y': param(0.30), 'x': param(0), 'opacity': param(1)}, effects=[])
project = dict(format='shaderixframe', schema=6, name='First Light', width=1280, height=720,
               fps=30, duration=12, master=0, masterMix=dict(output=0, sends=[], effects=[], params={}), buses=[], nextId=8,
               tracks=[track(6, '01 / Titre', 0, [title]),
                       track(1, '02 / Lumière procédurale', 0, [video]),
                       track(3, '03 / Bande sonore originale', 1, [audio])])
(EXAMPLES / 'FirstLight.sframe').write_text(json.dumps(project, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
study = copy.deepcopy(project)
study['name'] = 'Étude de masque · ellipse animée'
study['tracks'] = study['tracks'][1:]
masked = study['tracks'][0]['clips'][0]
masked['fadeIn'] = 0
masked['mask'] = dict(shape=2, inverted=False)
masked['params'].update({'mask_x': param(0, [(0, -0.15), (6, 0.15), (12, -0.15)]),
                         'mask_y': param(0), 'mask_width': param(0.8), 'mask_height': param(0.9),
                         'mask_rotation': param(-15, [(0, -15), (12, 15)]), 'mask_feather': param(36)})
(EXAMPLES / 'MaskStudy.sframe').write_text(json.dumps(study, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

cuts = copy.deepcopy(project)
cuts.update(name='Étude de montage · trois séquences liées', duration=9)
for t in cuts['tracks']:
    t['clips'] = []
next_id = 8
for index in range(3):
    start = index * 3
    group = next_id
    next_id += 1
    for track_index, source in enumerate((title, video, audio)):
        segment = copy.deepcopy(source)
        segment.update(id=next_id, start=start, duration=3, link=group, fadeIn=.25, fadeOut=.25)
        next_id += 1
        segment['in'] = start
        segment['name'] = f'{index + 1:02} / ' + source['name']
        if track_index == 0:
            segment['title']['text'] = ('PREMIÈRE LUMIÈRE', 'LE TEMPS DU MONTAGE', 'UN NOUVEL HORIZON')[index]
        elif track_index == 1:
            segment['params']['opacity'] = param(1)
            segment['effects'][0]['params']['exposure'] = param(.1 * index, [(start, -.2), (start + 3, .2)])
        else:
            segment['params']['gain'] = param(0)
        cuts['tracks'][track_index]['clips'].append(segment)
cuts['nextId'] = next_id
(EXAMPLES / 'CutStudy.sframe').write_text(json.dumps(cuts, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

graph_study = copy.deepcopy(project)
graph_study['name'] = 'Étude de graphe · désaturation Shaderixel'
graph_study['tracks'] = graph_study['tracks'][1:]
graph = dict(name='Désaturation Shaderixel', category='Études', inputs=1, premultiplied=False, nextId=5,
             nodes=[dict(id=1, op='input', pos=[-100, -80], **{'in': []}, value=[0, 0, 0, 0]),
                    dict(id=2, op='desaturate', pos=[140, -50], **{'in': [[1, 1], [3, 0]]}, value=[0, 0, 0, 0]),
                    dict(id=3, op='param', name='amount', pos=[-100, 95], **{'in': []}, value=[.45, 0, 0, 0]),
                    dict(id=4, op='output', pos=[400, -50], **{'in': [[1, 0], [2, 0], [0, 0]]}, value=[0, 0, 0, 0])])
graph_study['tracks'][0]['clips'][0]['effects'] = [dict(id='graph', enabled=True, code='', graph=graph,
                                                        params={'amount': param(.45, [(0, .1), (12, .85)])})]
(EXAMPLES / 'GraphStudy.sframe').write_text(json.dumps(graph_study, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
transitions = copy.deepcopy(project)
transitions['name'] = 'Étude de transitions · fondu et balayage'
transitions['duration'] = 10
transitions['nextId'] = 23
first, second, third = (copy.deepcopy(video) for _ in range(3))
first.update(id=12, start=0, duration=4, link=0, fadeIn=0, fadeOut=0, name='01 / Lumière initiale')
second.update(id=14, start=3, duration=4, link=0, fadeIn=0, fadeOut=0, name='02 / Fondu · teinte froide')
third.update(id=16, start=6, duration=4, link=0, fadeIn=0, fadeOut=0, name='03 / Balayage · teinte chaude')
for clip in (first, second, third):
    clip['params']['opacity'] = param(1)
    clip['effects'] = []
# Tint actual procedurally generated frames with embedded HLSL for a portable comparison.
second['effects'] = [dict(id='study-cold', enabled=True, graph=None, params={},
                          code='float4 Effect(EffectContext ctx){float4 c=InputUV(0,ctx.uv);c.rgb=c.bgr*float3(.45,.9,1.5);return c;}')]
third['effects'] = [dict(id='study-warm', enabled=True, graph=None, params={},
                         code='float4 Effect(EffectContext ctx){float4 c=InputUV(0,ctx.uv);c.rgb*=float3(1.5,.55,.25);return c;}')]
second['transition'] = dict(enabled=True, id='transition.crossfade', duration=1, code=(ROOT / 'shaders/crossfade.hlsl').read_text('utf-8'))
third['transition'] = dict(enabled=True, id='transition.wipe', duration=1, code=(ROOT / 'shaders/wipe.hlsl').read_text('utf-8'))
third['params'].update(transition_direction=param(0), transition_softness=param(.06),
                       transition_progress=param(0, [(6, 0), (6.4, .25), (7, 1)]))
sound = copy.deepcopy(audio)
sound.update(id=18, duration=10, link=0, fadeIn=.1, fadeOut=1)
sound['params'] = {'gain': param(0)}
caption = copy.deepcopy(title)
caption.update(id=20, duration=10, link=0, fadeIn=0, fadeOut=.5)
caption['title']['text'] = 'FONDU  /  BALAYAGE HLSL'
transitions['tracks'] = [track(19, '01 / Titre', 0, [caption]),
                         track(15, '02 / Plan entrant · balayage', 0, [third]),
                         track(13, '03 / Plan entrant · fondu', 0, [second]),
                         track(11, '04 / Plan initial', 0, [first]),
                         track(17, '05 / Bande sonore', 1, [sound])]
(EXAMPLES / 'TransitionStudy.sframe').write_text(json.dumps(transitions, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
mix_study = copy.deepcopy(project)
mix_study.update(name='Étude de mixage · bus, envois et DSP', nextId=33)
mix_study['tracks'][0]['clips'][0]['title']['text'] = 'BUS  /  ENVOIS  /  DSP'
mix_study['tracks'][2]['name'] = 'Pad original → Bus musique'
mix_study['tracks'][2]['mix'] = dict(output=30, sends=[dict(bus=31, gain=-12, preFader=False, enabled=True)],
                                    effects=[], params={'gain': param(-3, [(0, -9), (3, -3), (10, -3), (12, -18)])})
mix_study['buses'] = [
    dict(id=30, name='Bus musique · égalisation', gain=0, pan=0, muted=False, locked=False,
         mix=dict(output=0, sends=[], params={}, effects=[dict(id='cydaw.eq-three', enabled=True,
              params={'low': param(-2), 'mid': param(1), 'high': param(-3)})])),
    dict(id=31, name='Auxiliaire · échos', gain=-3, pan=0, muted=False, locked=False,
         mix=dict(output=0, sends=[], params={}, effects=[dict(id='cydaw.ping-pong', enabled=True,
              params={'time': param(275), 'feedback': param(.35), 'mix': param(1), 'width': param(1), 'sync': param(0)})]))]
mix_study['masterMix']['effects'] = [dict(id='cydaw.limiter', enabled=True,
                                        params={'gain': param(0), 'ceiling': param(-1), 'release': param(80)})]
(EXAMPLES / 'MixerStudy.sframe').write_text(json.dumps(mix_study, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
mix_study['tracks'][2]['mix']['params']['send.31'] = param(-12, [(0, -30), (3, -12), (10, -6), (12, -30)])
(EXAMPLES / 'MixerStudy.sframe').write_text(json.dumps(mix_study, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
vfx = copy.deepcopy(project)
vfx.update(name='Étude VFX · Bézier, multipasses et historique', nextId=10)
vfx['tracks'][0]['clips'][0]['title']['text'] = 'BÉZIER / EFFETS TEMPORELS'
vfx['tracks'][0]['clips'][0]['params'].update(font_size=param(32), text_outline=param(2),
                                            text_shadow=param(.8), text_shadow_x=param(3),
                                            text_shadow_y=param(5), text_shadow_blur=param(4))
subject = vfx['tracks'][1]['clips'][0]
subject['mask'] = dict(shape=3, inverted=False, points=[
    [-.4,-.25,-.4,-.25,-.2,-.4], [.4,-.25,.2,-.4,.45,-.15],
    [.4,.25,.45,.15,.2,.35], [-.4,.25,-.2,.35,-.45,.15]])
subject['params'].update(mask_width=param(1), mask_height=param(1), mask_feather=param(12))
subject['effects'].append(dict(id='study-temporal', enabled=True, graph=None, params={}, code='''//@passes 2
float4 Effect(EffectContext c){
    float4 value=Prev(c.px);
    if(c.pass==0) return (value+Prev(c.px+float2(2,0))+Prev(c.px-float2(2,0)))/3;
    return HasHistory()?lerp(value,History(c.px),.25):value;
}'''))
back = copy.deepcopy(video)
back.update(id=9, link=0, fadeIn=0)
back['params']['opacity'] = param(.25)
vfx['tracks'].insert(2, track(8, '03 / Fond lumineux', 0, [back]))
vfx['tracks'][3]['name'] = '04 / Graphe DSP parallèle'
vfx['tracks'][3]['mix']['effects'] = [dict(id='cydaw.graph', enabled=True, params={},
                                         graph=json.loads((EXAMPLES/'ParallelEQ.json').read_text('utf-8')))]
(EXAMPLES / 'VfxStudy.sframe').write_text(json.dumps(vfx, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('Original audio and seven example sequences generated.')

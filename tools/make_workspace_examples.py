"""Portable motion hierarchy and nested clip examples, using embedded shaders."""
import json
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def parameter(value, keys=None):
    return {'value': value, 'keys': keys or []}

def animation(values):
    return parameter(values[0][1], [dict(time=t, value=v, curve=1, shape=0) for t, v in values])

def project(name):
    return dict(format='shaderixframe', schema=10, name=name, width=1280, height=720,
                fps=30, duration=8, timeOrigin=0, master=-18, nextId=1, tracks=[], buses=[],
                color=dict(space='linear709', tone='clip', exposure=0, backgroundAlpha=0),
                imageFormat='png16')

def track(p, name, clip, audio=False):
    tid=p['nextId'];clip['id']=tid+1;p['nextId']+=2
    p['tracks'].insert(0,dict(id=tid,name=name,kind=1 if audio else 0,clips=[clip]))
    return clip['id']

def clip(name):
    return dict(name=name,source='',start=0,duration=8,params={},effects=[])

def shape(kind, color):
    distance='length((ctx.uv-.5)*2)-1' if kind=='ellipse' else 'length(max(abs(ctx.uv-.5)-(.5-.04),0))-.04'
    return dict(id='custom',code=f'//@generator\nfloat4 Effect(EffectContext ctx){{float d={distance};return float4({color},1-smoothstep(-.004,.004,d));}}')

def widget(c, kind, width, height, parent=0, anchor=(.5,.5)):
    c['widget']=dict(type=kind,parent=parent,anchorX=anchor[0],anchorY=anchor[1],pivotX=.5,pivotY=.5)
    c['params'].update(widget_width=parameter(width),widget_height=parameter(height))

p=project('Motion design · hiérarchie et animations')
group=clip('Groupe / carte');group['generator']=True;group['effects']=[dict(id='custom',code='//@generator\nfloat4 Effect(EffectContext ctx){return 0;}')]
widget(group,'group',840,360);group['params']['widget_y']=animation([(0,120),(2,0),(6,0),(8,-120)])
parent=track(p,group['name'],group)
card=clip('Carte arrondie');card['generator']=True;card['effects']=[shape('rectangle','.025,.055,.11')];widget(card,'rectangle',800,320,parent);track(p,card['name'],card)
title=clip('Titre');title['title']=dict(enabled=True,text='SHADERIXFRAME',font='Segoe UI',alignment=1,bold=True)
widget(title,'text',700,100,parent,(.5,.35));title['params']['font_size']=parameter(48);track(p,title['name'],title)
caption=clip('Sous-titre');caption['title']=dict(enabled=True,text='TIMELINE  ·  SHADERS  ·  MOTION DESIGN',font='Segoe UI',alignment=1,bold=False)
widget(caption,'text',690,70,parent,(.5,.62));caption['params']['font_size']=parameter(24);track(p,caption['name'],caption)
dot=clip('Accent animé');dot['generator']=True;dot['effects']=[shape('ellipse','.95,.27,.035')];widget(dot,'ellipse',32,32,parent,(.5,.82));dot['params']['widget_width']=animation([(0,8),(2,32),(6,32),(8,8)]);track(p,dot['name'],dot)
background=clip('Fond HLSL');background['generator']=True;background['effects']=[dict(id='custom',code='//@generator\nfloat4 Effect(EffectContext ctx){return float4(.006+ctx.uv.x*.012,.012+ctx.uv.y*.016,.028,1);}')]
track(p,background['name'],background);p['tracks'].append(p['tracks'].pop(0))
audio=clip('Audio dans la composition');audio['source']='firstlight.wav';track(p,audio['name'],audio,True)
(ROOT/'examples/MotionStudy.sframe').write_text(json.dumps(p,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
container=project('Éditeur de clip · composition imbriquée');outer=clip('Carte / conteneur');outer['composition']=deepcopy(p);track(container,outer['name'],outer)
(ROOT/'examples/ClipStudy.sframe').write_text(json.dumps(container,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('MotionStudy.sframe and ClipStudy.sframe generated.')

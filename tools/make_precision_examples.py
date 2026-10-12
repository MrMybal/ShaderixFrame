"""Add 0.9 examples without rewriting the audio assets or upstream applications."""
import copy
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
examples = ROOT/'examples'
base = json.loads((examples/'MixerStudy.sframe').read_text('utf-8'))
graph = json.loads((examples/'ParallelEQ.json').read_text('utf-8'))
graph['layout'] = {'input': [20,200], 'output': [1100,200]}
graph['nodes'][0]['pos'] = [300,80]
graph['nodes'][1]['pos'] = [300,320]
graph['nodes'].append({'id':3,'name':'Somme stéréo','processor':'sf.sum','pos':[800,200],'params':{}})
for wire in graph['connections']:
    if wire['destination']==0:
        wire['destination']=3
graph['connections'].append({'source':3,'destination':0})
base['schema'] = 7
base['name'] = 'Étude DSP · EQ parallèles CyDAW v2'
base['color'] = {'space':'srgb','tone':'clip','exposure':0,'backgroundAlpha':1}
base['imageFormat'] = 'png8'
audio = next(t for t in base['tracks'] if t['kind'] in (1, 'audio'))
audio['mix']['effects'].append({'id':'cydaw.graph','enabled':True,'graph':graph,'code':'','params':{}})
(examples/'DspStudy.sframe').write_text(json.dumps(base,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
precise = copy.deepcopy(base)
precise['name'] = 'Étude précision · HDR linéaire et exports 16 bits'
precise['color'] = {'space':'linear709','tone':'reinhard','exposure':0,'backgroundAlpha':0}
precise['imageFormat'] = 'exr16'
for track in precise['tracks']:
    for clip in track['clips']:
        if clip.get('generator'):
            clip['effects'] = [{'id':'custom','enabled':True,'params':{},'code':'float4 Effect(EffectContext ctx){return float4(4*ctx.uv.x,0.05,0.25+1.5*ctx.uv.y,0.85);}','graph':None}]
(examples/'PrecisionStudy.sframe').write_text(json.dumps(precise,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('DspStudy.sframe and PrecisionStudy.sframe written')

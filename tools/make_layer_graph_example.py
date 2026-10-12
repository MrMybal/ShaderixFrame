"""Build a branched timeline/layer graph with independent shader graphs and audio DSP."""
from pathlib import Path
from copy import deepcopy
import json
root=Path(__file__).resolve().parents[1]
p=json.loads((root/'examples/GradingStudy.sframe').read_text(encoding='utf-8'))
p['schema']=9;p['name']='Timelines, fusions et corrections en graphe'
video=next(t for t in p['tracks'] if t['id']==1)
clip=video['clips'][0]
grades=clip['effects'][1:];clip['effects']=clip['effects'][:1]
base=grades[0];warm=deepcopy(grades[1]);cool=deepcopy(grades[1])
warm['params']['temperature']={'value':.15,'keys':[]}
cool['params']['temperature']={'value':-.15,'keys':[]}
def node(id,kind,name,pos,inputs,**extra):return dict(id=id,kind=kind,name=name,pos=pos,inputs=inputs,**extra)
def correction(id,name,pos,inputs,e):
    effect={k:v for k,v in e.items() if k!='params'}
    return node(id,'effect',name,pos,inputs,effect=effect,params=e.get('params',{}))
shader=dict(name='Shader du calque',category='Graphes',inputs=1,premultiplied=False,nextId=3,
 nodes=[dict(id=1,op='input',pos=[40,80],**{'in':[]},value=[0,0,0,0]),dict(id=2,op='output',pos=[420,80],**{'in':[[1,0]]},value=[0,0,0,0])])
p['layerGraph']=dict(schema=1,engine='shaderixel.composition',output=10,nodes=[
 node(1,'surface','Timeline / Lumière HDR',[20,140],[],surface=1),
 correction(2,'Primaires animées',[235,140],[1],base),
 correction(3,'Branche chaude',[450,5],[2],warm),
 correction(4,'Branche froide',[450,225],[2],cool),
 node(5,'blend','Mélange animé des deux looks',[665,125],[3,4,0],blend=0,opacity=.5,
      params={'opacity':{'value':.5,'keys':[{'time':0,'value':0,'curve':1,'shape':0},{'time':6,'value':1,'curve':1,'shape':0},{'time':12,'value':0,'curve':1,'shape':0}]}}),
 node(6,'effect','Graphe shader indépendant',[880,125],[5],effect={'id':'graph','graph':shader},params={}),
 correction(7,'Correction locale après fusion',[1095,125],[6],grades[2]),
 node(8,'surface','Timeline / Titre',[1095,300],[],surface=6),
 node(9,'blend','Titre sur composition',[1310,125],[7,8,0],blend=0,opacity=1),
 node(10,'output','Sortie vidéo',[1525,125],[9])])
(root/'examples/LayerGraphStudy.sframe').write_text(json.dumps(p,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

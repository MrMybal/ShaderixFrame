"""Copy and adapt the complete Shaderixel shader catalogue, without editing upstream.
--snapshot updates locked copies explicitly; --check verifies generated output.
"""
import argparse,hashlib,json,re
from pathlib import Path
from upstream_paths import source_roots
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--snapshot',action='store_true');p.add_argument('--check',action='store_true');args=p.parse_args()
lock_path=ROOT/'upstream.lock.json';lock=json.loads(lock_path.read_text(encoding='utf-8'))
source=source_roots()['shaderixel']/'shaders/effects';vendor=ROOT/'vendor/shaderixel/shaders/effects'
if args.snapshot:
 for original in sorted(source.glob('*.slang')):
  relative='shaderixel/shaders/effects/'+original.name;target=ROOT/'vendor'/relative;data=original.read_bytes()
  if target.exists() and relative in lock['files'] and hashlib.sha256(target.read_bytes()).hexdigest()!=lock['files'][relative]['sha256']:raise SystemExit('Modified snapshot: '+relative)
  if target.exists() and target.read_bytes()!=data:
   previous=target.read_bytes();backup=ROOT/'vendor-history'/'shaderixel'/hashlib.sha256(previous).hexdigest()[:12]/original.name;backup.parent.mkdir(parents=True,exist_ok=True);backup.write_bytes(previous)
  target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);lock['files'][relative]={'source':'shaders/effects/'+original.name,'sha256':hashlib.sha256(data).hexdigest()}
 lock_path.write_text(json.dumps(lock,indent=2)+'\n',encoding='utf-8')
common=(ROOT/'vendor/shaderixel/shaders/common.slang').read_text(encoding='utf-8')
helpers=common[common.index('float SrgbToLinear1'):]
helpers='float Luma(float3 c){return dot(c,float3(.2126,.7152,.0722));}\n'+helpers
functions=re.findall(r'(?m)^(float\d?\s+\w+\([^\n]+)',helpers)
folder=ROOT/'plugins/shaderixel-library';manifest={'apiVersion':2,'name':'Shaderixel','effects':[]};outputs={}
for original in sorted(vendor.glob('*.slang')):
 key='shaderixel/shaders/effects/'+original.name
 assert hashlib.sha256(original.read_bytes()).hexdigest()==lock['files'][key]['sha256'],key
 text=original.read_text(encoding='utf-8');name=re.search(r'(?m)^//@name\s+(.+)',text);category=re.search(r'(?m)^//@category\s+(.+)',text)
 # Curves and LUT are represented by the native HDR grading node, with its real editors/data.
 special=original.stem in ('curves','color_lookup')
 if special:text='float4 Effect(EffectContext ctx){return Input(0,ctx.px);}\n'
 else:
  lines=[]
  for line in text.splitlines():
   if line.startswith('//@param '):
    match=re.match(r'//@param (\w+) enum ([-\d.]+) "([^"]+)"(.*)',line)
    if match:line=f'//@param {match[1]} int {match[2]} 0 {len(match[3].split("|"))-1}'+match[4]
    match=re.match(r'(//@param \w+ (float2|float3|float4)\s+)([^"\n]+)(.*)',line)
    if match and len(match[3].split())==int(match[2][-1]):line=match[1]+match[3].strip()+' 0 1 '+match[4]
   lines.append(line)
  text='\n'.join(lines)+'\n'
  # Native shared helpers, with only duplicate function definitions omitted.
  chunks=re.split(r'(?m)(?=^float\d?\s+\w+\()',helpers);selected=[]
  for chunk in chunks:
   fn=re.match(r'float\d?\s+(\w+)\(',chunk)
   if not fn or not re.search(r'\bfloat\d?\s+'+fn[1]+r'\s*\(',text):selected.append(chunk)
  text='#define select(c,a,b) ((c)?(a):(b))\n'+''.join(selected)+text
  # Parameter macros cannot share names with arguments in native helper functions.
  parameters=re.findall(r'(?m)^//@param\s+(\w+)',text)
  for parameter in parameters:
   renamed=[]
   for line in text.splitlines():
    if line.startswith('//@param '):line=re.sub(r'(?<=//@param )'+re.escape(parameter)+r'\b','sx_'+parameter,line)
    elif not line.lstrip().startswith('//'):line=re.sub(r'(?<![.\w])'+re.escape(parameter)+r'\b','sx_'+parameter,line)
    renamed.append(line)
   text='\n'.join(renamed)+'\n'
  # Functions may shadow a global parameter (CyPreset imports do this).
  # Rename their local arguments before the GPU compiler creates global parameter macros.
  pattern=r'\b(?:float[234]?|int|uint|bool)\s+\w+\s*\(([^{};]*)\)\s*\{'
  edits=[]
  for function in re.finditer(pattern,text):
   locals=[key for key in parameters if re.search(r'\bsx_'+re.escape(key)+r'\b',function[1])]
   if not locals:continue
   end=function.end();depth=1
   while end<len(text) and depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
   chunk=text[function.start():end]
   for key in locals:chunk=re.sub(r'\bsx_'+re.escape(key)+r'\b','sx_arg_'+key,chunk)
   edits.append((function.start(),end,chunk))
  for start,end,chunk in reversed(edits):text=text[:start]+chunk+text[end:]
 shader=original.stem+'.hlsl' ;outputs[shader]=text.encode('utf-8')
 manifest['effects'].append({'id':'shaderixel.'+original.stem,'name':name[1].strip() if name else original.stem,'category':'Shaderixel / '+(category[1].strip() if category else 'Effets'),'shader':shader})
outputs['plugin.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
if args.check:
 for name,data in outputs.items():assert (folder/name).read_bytes()==data,'Stale Shaderixel adaptation: '+name
else:
 folder.mkdir(parents=True,exist_ok=True)
 for name,data in outputs.items():(folder/name).write_bytes(data)
print(f'{len(manifest["effects"])} Shaderixel shaders '+('verified.' if args.check else 'imported.'))

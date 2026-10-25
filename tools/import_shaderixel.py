"""Copy and adapt the complete Shaderixel shader catalogue, without editing upstream.
--snapshot updates locked copies explicitly; --check verifies generated output.
"""
import argparse,hashlib,json,re
from pathlib import Path
from upstream_paths import source_roots
from english_ui import english, translate_shader
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
  lines=[];curves=[]
  for line in text.splitlines():
   # Shaderixel bakes a curve parameter into <id>_eval(x). ShaderixFrame edits curves with its native
   # grading node, so an embedded curve stays neutral and every other parameter remains available.
   curve=re.match(r'//@param (\w+) curve\b',line)
   if curve:curves.append(curve[1]);continue
   if line.startswith('//@param '):
    match=re.match(r'//@param (\w+) enum ([-\d.]+) "([^"]+)"(.*)',line)
    if match:line=f'//@param {match[1]} int {match[2]} 0 {len(match[3].split("|"))-1}'+match[4]
    match=re.match(r'(//@param \w+ (float2|float3|float4)\s+)([^"\n]+)(.*)',line)
    if match and len(match[3].split())==int(match[2][-1]):line=match[1]+match[3].strip()+' 0 1 '+match[4]
   lines.append(line)
  text=''.join(f'float {name}_eval(float x){{return saturate(x);}}\n' for name in curves)+'\n'.join(lines)+'\n'
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
  # A local variable may shadow a parameter too (Photo RAW's tint): from its declaration to the end of
  # its block, it keeps its own name instead of the parameter macro.
  for key in parameters:
   declaration=re.compile(r'\b(?:float[234]?|int[234]?|uint[234]?|half[234]?|bool)\s+sx_'+re.escape(key)+r'\b(?!\s*\()')
   position=0
   while True:
    found=declaration.search(text,position)
    if not found:break
    depth=0;k=found.start()
    while k>0 and depth>=0:
     k-=1
     if text[k]=='}':depth+=1
     elif text[k]=='{':depth-=1
    if k<=0:position=found.end();continue # global declaration: nothing to scope
    depth=1;end=found.end()
    while end<len(text) and depth:
     depth+=(text[end]=='{')-(text[end]=='}');end+=1
    scoped=re.sub(r'\bsx_'+re.escape(key)+r'\b','sx_local_'+key,text[found.start():end])
    text=text[:found.start()]+scoped+text[end:];position=found.start()+len(scoped)
  # Slang accepts names that HLSL reserves for geometry shader primitives (line, point, triangle...).
  reserved=r'(?<![.\w])(line|point|triangle|lineadj|triangleadj)\b'
  text='\n'.join(line if line.lstrip().startswith('//') else re.sub(reserved,r'sx_\1',line) for line in text.splitlines())+'\n'
  # Slang broadcasts a single constructor argument (float3(x)); HLSL needs a cast for the same meaning.
  for _ in range(4): # nested single-argument constructors need another pass
   out=[];i=0
   for match in re.finditer(r'(?<![\w.])((?:float|int|uint|half|bool)[234])\s*\(',text):
    if match.start()<i:continue
    depth=0;commas=0;end=match.end()-1
    for k in range(end,len(text)):
     if text[k]=='(':depth+=1
     elif text[k]==')':
      depth-=1
      if depth==0:end=k;break
     elif text[k]==',' and depth==1:commas+=1
    line_start=text.rfind('\n',0,match.start())+1
    if commas or not text[match.end():end].strip() or text[line_start:match.start()].lstrip().startswith('//'):continue
    out.append(text[i:match.start()]+'(('+match[1]+')('+text[match.end():end]+'))');i=end+1
   text=''.join(out)+text[i:]
 text=translate_shader(text)
 shader=original.stem+'.hlsl' ;outputs[shader]=text.encode('utf-8')
 manifest['effects'].append({'id':'shaderixel.'+original.stem,'name':english(name[1].strip()) if name else original.stem,'category':'Shaderixel / '+(english(category[1].strip()) if category else 'Effects'),'shader':shader})
outputs['plugin.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
if args.check:
 for name,data in outputs.items():assert (folder/name).read_bytes()==data,'Stale Shaderixel adaptation: '+name
else:
 folder.mkdir(parents=True,exist_ok=True)
 for name,data in outputs.items():(folder/name).write_bytes(data)
print(f'{len(manifest["effects"])} Shaderixel shaders '+('verified.' if args.check else 'imported.'))

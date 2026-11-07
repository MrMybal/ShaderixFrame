import {readFile,writeFile,mkdir,cp,readdir,rm,lstat,realpath} from 'node:fs/promises';
import {resolve,dirname,relative,extname,isAbsolute} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {product,copy,shots as catalogue,groups} from './content.mjs';
import {render} from './template.mjs';

const source=dirname(fileURLToPath(import.meta.url));
const root=resolve(source,'..');
const output=resolve(root,'dist/website');
const manual=resolve(root,'Documentation');
if(process.argv.length>2) throw new Error('Usage: node website/build.mjs');
const hash=data=>createHash('sha256').update(data).digest('hex').slice(0,16);
const fullHash=data=>createHash('sha256').update(data).digest('hex');
const inside=(base,path)=>!isAbsolute(relative(base,path))&&!relative(base,path).startsWith('..');
const sortedKeys=value=>Object.keys(value).sort().join('\n');
if(sortedKeys(copy.en)!==sortedKeys(copy.fr)) throw new Error('English/French translation keys differ');
for(const locale of ['en','fr']) for(const [key,value] of Object.entries(copy[locale])) {
  if(typeof value!=='string'||!value.trim()||/\b(?:TODO|PLACEHOLDER|LOREM IPSUM)\b/i.test(value)) throw new Error(`Incomplete ${locale}: ${key}`);
}
const cmake=await readFile(resolve(root,'CMakeLists.txt'),'utf8');
const version=cmake.match(/project\(ShaderixFrame VERSION ([\d.]+)/)?.[1]+'-'+cmake.match(/set\(SF_VERSION_SUFFIX "([^"]+)"\)/)?.[1];
if(version!==product.version) throw new Error('Showcase release does not match the application version');
for(const destination of Object.values(product.destinations)) if(new URL(destination).protocol!=='https:') throw new Error('External destinations must use HTTPS');
let manifest;
try { manifest=JSON.parse(await readFile(resolve(manual,'DocData/manifest.json'),'utf8')); }
catch { throw new Error('Generate the real manual first: python tools/build_cydoc.py --check --test'); }
if(manifest.version!==product.version) throw new Error('Regenerate the manual for this application version');
const pages=new Set(manifest.pages.map(page=>page.locale+'/'+page.id));
function checkDoc(target) {
  const [id,anchor]=target.split('#');
  for(const locale of ['en','fr']) {
    if(!pages.has(locale+'/'+id)) throw new Error(`Unknown manual route: ${locale}/${id}`);
    if(anchor) {
      const page=manifest.pages.find(page=>page.locale===locale&&page.id===id);
      if(!page.headings?.some(heading=>heading.id===anchor)) throw new Error(`Unknown manual anchor: ${target}`);
    }
  }
}
async function files(directory) {
  const result=[];
  for(const item of await readdir(directory,{withFileTypes:true})) {
    const path=resolve(directory,item.name);
    if(item.isSymbolicLink()) throw new Error('Symlinks are not distributed');
    if(item.isDirectory()) result.push(...await files(path));
    else if(item.isFile()) result.push(path);
  }
  return result.sort();
}
const manualFiles=await files(manual);
for(const path of manualFiles) {
  const name=relative(manual,path).replaceAll('\\','/');
  if(!['Documentation.html','DocAI.md','index.html'].includes(name)&&!name.startsWith('DocData/')) throw new Error('Unexpected documentation file: '+name);
  if(!['.html','.md','.js','.json','.css','.svg','.txt','.png','.jpg','.jpeg','.webp','.woff2'].includes(extname(path))) throw new Error('Unexpected documentation resource: '+name);
}
const required=['Documentation.html','DocAI.md','DocData/viewer.js','DocData/viewer.css','DocData/data.js'];
for(const name of required) if(!manualFiles.includes(resolve(manual,name))) throw new Error('Incomplete manual: '+name);

// This is the only directory replaced. Verify its absolute boundary before removal.
if(output!==resolve(root,'dist','website')||!inside(resolve(root,'dist'),output)) throw new Error('Unsafe generated output directory');
const physicalRoot=await realpath(root);
await mkdir(resolve(root,'dist'),{recursive:true});
if(!inside(physicalRoot,await realpath(resolve(root,'dist')))) throw new Error('Generated output parent escapes the project');
try { if((await lstat(output)).isSymbolicLink()) throw new Error('Refusing a linked output directory'); } catch(error) { if(error.code!=='ENOENT') throw error; }
console.log('Generated output boundary:',output);
await rm(output,{recursive:true,force:true});
await mkdir(resolve(output,'assets'),{recursive:true});
await cp(manual,resolve(output,'manual'),{recursive:true});
const assets={};
for(const key of ['logo','favicon']) {
  const path=resolve(root,product[key]);
  if(!inside(root,path)) throw new Error('Asset escapes source root');
  const data=await readFile(path);
  assets[key]=`./assets/${key}.${hash(data)}${extname(path)}`;
  await writeFile(resolve(output,assets[key]),data);
}
const shots=[];
for(const item of catalogue) {
  if(shots.some(shot=>shot.id===item.id)) throw new Error('Duplicate shot ID');
  for(const locale of ['en','fr']) for(const key of ['alt','caption']) if(!item[locale]?.[key]?.trim()) throw new Error('Missing image translation');
  checkDoc(item.doc);
  const path=resolve(root,item.file);
  if(!inside(root,path)) throw new Error('Screenshot escapes source root');
  const data=await readFile(path);
  if(data.subarray(0,8).toString('hex')!=='89504e470d0a1a0a') throw new Error('Expected the real PNG capture: '+item.file);
  const width=data.readUInt32BE(16),height=data.readUInt32BE(20);
  const src=`./assets/${item.id}.${hash(data)}.png`;
  await writeFile(resolve(output,src),data);
  shots.push({id:item.id,src,width,height,doc:item.doc,en:item.en,fr:item.fr});
}
for(const items of Object.values(groups)) for(const item of items) if(!shots.some(shot=>shot.id===item.shot)) throw new Error('Unknown explorer screenshot');
const runtime=await readFile(resolve(source,'main.js'),'utf8');
const style=await readFile(resolve(source,'styles.css'),'utf8');
if(/\bfetch\s*\(|XMLHttpRequest|requestFullscreen|webkitRequestFullscreen/.test(runtime)) throw new Error('Runtime must remain local and in-window');
const scriptData=`/* ShaderixFrame showcase. See licenses/NOTICE.md. */\n(() => {\n'use strict';\nconst COPY=${JSON.stringify(copy)};\nconst SHOTS=${JSON.stringify(shots)};\nconst GROUPS=${JSON.stringify(groups)};\nconst DESTINATIONS=${JSON.stringify(product.destinations)};\n${runtime}\n})();\n`;
const css=`./assets/showcase.${hash(style)}.css`,script=`./assets/showcase.${hash(scriptData)}.js`;
await writeFile(resolve(output,css),style);
await writeFile(resolve(output,script),scriptData);
const html=render({assets,shots,css,script});
const ids=[...html.matchAll(/\bid="([^"]+)"/g)].map(match=>match[1]);
if(new Set(ids).size!==ids.length) throw new Error('Duplicate HTML anchor');
for(const [,target] of html.matchAll(/data-doc="([^"]+)"/g)) checkDoc(target);
for(const [,key] of html.matchAll(/data-i18n(?:-label)?="([^"]+)"/g)) if(!copy.en[key]||!copy.fr[key]) throw new Error('Untranslated UI: '+key);
for(const [,target] of html.matchAll(/(?:href|src)="([^"]*)"/g)) {
  if(!target||target==='#') throw new Error('Empty destination');
  if(target.startsWith('#')) { if(!ids.includes(target.slice(1))) throw new Error('Unknown chapter: '+target); }
}
await writeFile(resolve(output,'index.html'),html);
await cp(resolve(root,'LICENSE'),resolve(output,'LICENSE'));
await mkdir(resolve(output,'licenses'),{recursive:true});
await cp(resolve(source,'NOTICE.md'),resolve(output,'licenses/NOTICE.md'));
await cp(resolve(source,'licenses/CyDoc-showcase-LICENSE.txt'),resolve(output,'licenses/CyDoc-showcase-LICENSE.txt'));
await cp(resolve(root,'NOTICE.md'),resolve(output,'licenses/APP-NOTICE.md'));
for(const [,target] of html.matchAll(/(?:href|src)="(\.\/[^"?#]+)(?:[?#][^"]*)?"/g)) {
  const path=resolve(output,target);
  if(!inside(output,path)||!(await lstat(path)).isFile()) throw new Error('Missing artifact resource: '+target);
}
for(const path of manualFiles) {
  const copied=resolve(output,'manual',relative(manual,path));
  if(fullHash(await readFile(path))!==fullHash(await readFile(copied))) throw new Error('Manual was modified during copying');
}
const outputFiles=await files(output);
for(const path of outputFiles) {
  const name=relative(output,path).replaceAll('\\','/');
  if(!/^(?:index\.html|LICENSE|assets\/[^/]+|licenses\/[^/]+|manual\/(?:Documentation\.html|DocAI\.md|index\.html|DocData\/.+))$/.test(name)) throw new Error('Artifact boundary violation: '+name);
}
console.log(JSON.stringify({entry:resolve(output,'index.html'),locales:Object.keys(copy),screenshots:shots.length,manualFiles:manualFiles.length,files:outputFiles.length,bytes:(await Promise.all(outputFiles.map(async path=>(await lstat(path)).size))).reduce((a,b)=>a+b,0),css,script},null,2));

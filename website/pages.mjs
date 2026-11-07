// Assemble the public static site while preserving existing manual URLs.
import './build.mjs';
import {cp,lstat,mkdir,readFile,readdir,rm} from 'node:fs/promises';
import {dirname,resolve,relative,isAbsolute} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const standalone=resolve(root,'dist/website');
const output=resolve(root,'dist/pages');
const delta=relative(resolve(root,'dist'),output);
if(isAbsolute(delta)||delta!=='pages') throw new Error('Unsafe Pages output directory');
// build.mjs has already verified the physical dist parent stays in this project.
try { if((await lstat(output)).isSymbolicLink()) throw new Error('Refusing a linked Pages directory'); } catch(error) { if(error.code!=='ENOENT') throw error; }
console.log('Pages output boundary:',output);
await rm(output,{recursive:true,force:true});
await mkdir(output,{recursive:true});
await cp(standalone,output,{recursive:true});
// Keep the original, genuine root reader and all its resources for old links.
for(const name of ['Documentation.html','DocAI.md','DocData']) {
  await cp(resolve(standalone,'manual',name),resolve(output,name),{recursive:true});
}
async function files(path) {
  const result=[];
  for(const entry of await readdir(path,{withFileTypes:true})) {
    const child=resolve(path,entry.name);
    if(entry.isSymbolicLink()) throw new Error('Symlinks are not published');
    if(entry.isDirectory()) result.push(...await files(child));
    else if(entry.isFile()) result.push(child);
  }
  return result;
}
const digest=data=>createHash('sha256').update(data).digest('hex');
for(const source of await files(standalone)) {
  const target=resolve(output,relative(standalone,source));
  if(digest(await readFile(source))!==digest(await readFile(target))) throw new Error('Showcase copy differs');
}
for(const source of await files(resolve(standalone,'manual'))) {
  const name=relative(resolve(standalone,'manual'),source).replaceAll('\\','/');
  if(name==='index.html') continue;
  if(digest(await readFile(source))!==digest(await readFile(resolve(output,name)))) throw new Error('Legacy reader copy differs');
}
const all=await files(output);
for(const path of all) {
  const name=relative(output,path).replaceAll('\\','/');
  if(!/^(?:index\.html|LICENSE|assets\/[^/]+|licenses\/[^/]+|manual\/(?:Documentation\.html|DocAI\.md|index\.html|DocData\/.+)|Documentation\.html|DocAI\.md|DocData\/.+)$/.test(name)) throw new Error('Unexpected Pages file: '+name);
}
console.log(JSON.stringify({entry:resolve(output,'index.html'),files:all.length,bytes:(await Promise.all(all.map(async path=>(await lstat(path)).size))).reduce((a,b)=>a+b,0),legacyReader:'Documentation.html',bundledReader:'manual/Documentation.html'},null,2));

import { readFile, writeFile, mkdir, cp, rm, readdir } from 'node:fs/promises';
import { resolve, dirname, relative, join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { importApi } from 'cydoc/api-import';
import { build } from 'esbuild';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const content = join(root, 'build/cydoc-input/content');
const output = join(root, 'build/help');
for (const target of [content, output]) {
  const delta = relative(root, target);
  if (!delta || delta.startsWith('..') || resolve(target) === root) throw new Error('Unsafe documentation output path');
}
const hash = value => createHash('sha256').update(value).digest('hex');
const slug = value => value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const esc = value => value.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
// Clean only fixed, application-owned build outputs. Never delete authoring inputs.
await mkdir(join(root, 'build/cydoc-input'), { recursive: true });
await rm(content, { recursive: true, force: true });
await rm(output, { recursive: true, force: true });
await cp(join(root, 'docs/manual/pages'), content, { recursive: true });

function frontmatter(id, title, section, order, locale, revision) {
  return `---\nid: ${id}\ntitle: ${title.replaceAll('\n',' ')}\ndescription: ${title.replaceAll('\n',' ')}\nsection: ${section}\norder: ${order}\n${locale==='en'?'revision':'sourceRevision'}: ${revision}\n---\n\n`;
}
const references = [
  ['README.md','reference-readme'], ['sdk/README.md','reference-sdk'], ['NOTICE.md','reference-notices'],
  ['docs/manual/README.md','manual-maintenance'],
  ['LICENSE','license-text'],
  ...(await readdir(join(root, 'docs'))).filter(name=>name.endsWith('.md') && !name.startsWith('VALIDATION_'))
    .map(name=>['docs/'+name,'reference-'+slug(name.replace(/\.md$/,''))])
];
const ids = new Map(references.map(([file,id])=>[resolve(root,file),id]));
const sources = ['sdk/ShaderixFramePlugin.h','sdk/examples/Gain.cpp','sdk/examples/Latency.cpp','sdk/examples/OfxGain.cpp',
  ...(await readdir(join(root,'shaders'))).filter(name=>/\.hlsl$/i.test(name)).map(name=>'shaders/'+name)];
for(const file of sources)ids.set(resolve(root,file),'source-'+slug(file));
const inventory=[];
function rewrite(body, file) {
  // Preserve code fences verbatim. Links in prose target the new manual or its source index.
  return body.split(/(```[\s\S]*?```)/g).map(part=>part.startsWith('```')?part:part
    .replace(/\[!\[([^\]]*)\]\(([^)]+)\)\]\([^)]+\)/g,'![$1]($2)')
    .replace(/(!?)\[([^\]]*)\]\(([^)]+)\)/g,(all,image,label,href)=>{
      if(/^(https?:|mailto:|doc:|#)/.test(href))return all;
      const [path,anchor]=href.split('#');
      const dest=resolve(dirname(join(root,file)),decodeURIComponent(path));
      if(image && dest.startsWith(join(root,'docs/screenshots')))return `![${label}](assets/${dest.split(/[\\/]/).at(-1)})`;
      if(ids.has(dest))return `[${label}](doc:${ids.get(dest)}${anchor?'#'+anchor:''})`;
      if(dest.startsWith(join(root,'docs/manual')) && /\.(html|json)$/.test(dest))return `[${label}](doc:welcome)`;
      if(dest.startsWith(join(root,'docs/screenshots')))return `[${label}](doc:quickstart?media=quickstart-image-1)`;
      return `${label} (\`${href}\`)`;
    })).join('');
}
for (const [file,id] of references) {
  const source=await readFile(join(root,file),'utf8');
  const revision=hash(source).slice(0,16);
  const historical=/RELEASE_|IMPLEMENTATION|ROADMAP|SECURITY_QA|CYUI_VALIDATION|ARCHITECTURE|MCP\.md/.test(file);
  const originalFrench=/ARCHITECTURE|MCP\.md|PLUGINS|RELEASE_0\.(?:8|9|10|11|12|13)|IMPLEMENTATION|ROADMAP|SECURITY_QA/.test(file);
  const body=file==='LICENSE'?'```text\n'+source+'\n```':rewrite(source,file);
  inventory.push({source:file,id,sha256:hash(source),classification:historical?'historical-reference':'reference',originalLanguage:originalFrench?'fr':'en'});
  for(const locale of ['en','fr']) {
    const fr=locale==='fr';
    const section=historical?(fr?'Archives techniques':'Technical archives'):(fr?'Documents de référence':'Reference documents');
    const title=(historical?(fr?'Archive : ':'Archive: '):'')+file;
    let note=fr?`Document original conservé depuis \`${file}\`. Sa langue d’origine est ${originalFrench?'le français':'l’anglais'} ; le texte de référence est préservé sans traduction automatique.`:
      `Original document retained from \`${file}\`. Its source language is ${originalFrench?'French':'English'}; the reference text is preserved without automatic translation.`;
    if(historical)note+=fr?' Ce document décrit son état/version historique et ne remplace pas le guide actuel.':' This is a versioned/historical record, not the current user guide.';
    if(/MCP\.md/.test(file))note+=fr?' Les anciens nombres d’outils sont obsolètes ; consultez la référence des contrats générée depuis le logiciel.':' Historical tool counts are obsolete; use the contract reference generated from the application.';
    await writeFile(join(content,locale,id+'.md'),frontmatter(id,title,section,200,locale,revision)+
      '```callout\n'+JSON.stringify({type:'note',title:fr?'Périmètre du document':'Document scope',body:note})+'\n```\n\n'+
      `[${fr?'Guide actuel':'Current guide'}](doc:welcome) · [${fr?'Contrats MCP actuels':'Current MCP contracts'}](doc:api-links)\n\n`+body,'utf8');
  }
}
// Actual MCP schemas captured from tools/list. The neutral CyDoc adapter supplies
// signatures/parameter tables; prose stays explicitly identified as host-provided English.
const tools=JSON.parse(await readFile(join(root,'docs/manual/api/mcp-tools.json'),'utf8'));
const symbols=tools.map(tool=>({key:tool.name,name:tool.name,language:'json',kind:'tool',
  description:tool.description,signature:JSON.stringify({name:tool.name,inputSchema:tool.inputSchema},null,2),
  parameters:Object.entries(tool.inputSchema.properties||{}).map(([name,property])=>({name,type:property.type||'JSON',optional:!(tool.inputSchema.required||[]).includes(name),description:property.description||'',default:property.default===undefined?'':JSON.stringify(property.default)}))}));
const neutral=join(root,'build/cydoc-input/mcp-api.json');
await writeFile(neutral,JSON.stringify({symbols}));
const pages=await importApi([{format:'cydoc',path:relative(root,neutral),prefix:'mcp-api'}],{base:root,locales:[{code:'en'},{code:'fr'}]});
for(const page of pages) {
  const schema=tools.find(tool=>tool.name===page.title);
  const revision=hash(JSON.stringify(schema)).slice(0,16);
  const notice=page.locale==='fr'?'Contrat JSON réel du serveur MCP. Noms, descriptions et schémas sont fournis par le logiciel en anglais ; les identifiants ne sont pas traduits.':'Actual MCP server contract. Names, descriptions and schemas are provided by the application; identifiers are not translated.';
  await writeFile(join(content,page.locale,page.id+'.md'),frontmatter(page.id,page.title,page.locale==='fr'?'API MCP':'MCP API',150,page.locale,revision).replace('\n---\n\n','\nkind: api\nsymbol: '+page.title+'\n---\n\n')+
    '```callout\n'+JSON.stringify({type:'note',title:'MCP',body:notice})+'\n```\n\n'+page.markdown,'utf8');
}
for(const locale of ['en','fr']) {
  const fr=locale==='fr',revision=hash(JSON.stringify(tools)).slice(0,16);
  const links=pages.filter(p=>p.locale===locale).sort((a,b)=>a.title.localeCompare(b.title)).map(p=>`- [\`${p.title}\`](doc:${p.id}#signature)`).join('\n');
  await writeFile(join(content,locale,'api-links.md'),frontmatter('api-links',fr?'Référence MCP et SDK':'MCP and SDK reference',fr?'Référence':'Reference',145,locale,revision)+
    `## ${fr?'Contrats disponibles':'Available contracts'} {#tools}\n\n${fr?tools.length+' schémas capturés depuis tools/list du véritable exécutable. Les anciens documents MCP sont conservés comme archives.':tools.length+' schemas captured from the real executable’s tools/list. Earlier MCP documents are retained as archives.'}\n\n`+
    `[${fr?'SDK natif':'Native SDK'}](doc:reference-sdk) · [${fr?'Source ABI':'ABI source'}](doc:source-sdk-shaderixframeplugin-h#L1)\n\n`+links,'utf8');
}
await writeFile(join(root,'build/cydoc-input/inventory.json'),JSON.stringify({references:inventory,sourceIndex:sources,mcpTools:tools.length},null,2));
const packageRoot=dirname(dirname(fileURLToPath(import.meta.resolve('cydoc'))));
const cli=JSON.parse(await readFile(join(packageRoot,'package.json'),'utf8')).bin.cydoc;
const run=spawnSync(process.execPath,[join(packageRoot,cli),'check',join(root,'cydoc.config.json')],{stdio:'inherit',windowsHide:true});
if(run.status!==0)throw new Error('CyDoc check failed');
await build({entryPoints:[join(root,'tools/cydoc/reader.mjs')],bundle:true,format:'iife',minify:true,target:'es2020',outfile:join(output,'viewer-app.js'),legalComments:'eof'});
await cp(join(root,'tools/cydoc/reader.html'),join(output,'index.html'));
await cp(join(root,'tools/cydoc/shaderixframe.css'),join(output,'shaderixframe.css'));
await mkdir(join(output,'licenses'),{recursive:true});
await cp(join(root,'vendor/cydoc/LICENSE'),join(output,'licenses/CyDoc-LICENSE.txt'));
const manifest=JSON.parse(await readFile(join(output,'manifest.json'),'utf8'));
const guideIds=JSON.parse(await readFile(join(root,'docs/manual/migration.json'),'utf8')).preserved_chapter_ids;
for(const locale of ['en','fr']) {
  const fr=locale==='fr';
  const pages=guideIds.map(id=>manifest.pages.find(page=>page.id===id&&page.locale===locale));
  if(pages.some(page=>!page))throw new Error('Missing guide translation');
  const body=pages.map(page=>`<section><h1>${esc(page.title)}</h1>${page.html.replaceAll('href="#/','href="index.html#/').replaceAll('loading="lazy"','loading="eager"')}</section>`).join('');
  await writeFile(join(output,`print-${locale}.html`),`<!doctype html><html lang="${locale}"><meta charset="utf-8"><title>ShaderixFrame ${fr?'Guide imprimable':'Printable guide'}</title><link rel="stylesheet" href="viewer.css"><style>body{font:16px/1.6 system-ui;background:white;color:#202020;margin:32px;max-width:1100px}section{break-before:page}section:first-child{break-before:auto}img,svg{max-width:100%;height:auto}pre{white-space:pre-wrap}table{border-collapse:collapse}td,th{border:1px solid #aaa;padding:8px}figure{margin:20px 0;break-inside:avoid}button,.code-toolbar{display:none}@media print{body{font-size:10pt;margin:0}a{color:inherit}img{max-height:15cm;object-fit:contain}}</style><body><p>${fr?'Imprimer / enregistrer PDF : Ctrl+P':'Print / save PDF: Ctrl+P'}</p>${body}</body></html>`,'utf8');
}
await writeFile(join(output,'migration-inventory.json'),JSON.stringify({references:inventory,sourceIndex:sources,mcpTools:tools.length},null,2));
console.log(`ShaderixFrame: preserved ${guideIds.length} bilingual guides, ${inventory.length} reference documents, ${tools.length} MCP contracts; image module only.`);

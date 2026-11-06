import { readFile, writeFile, mkdir, cp, rm, readdir, stat } from 'node:fs/promises';
import { resolve, dirname, relative, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { importApi } from 'cydoc/api-import';
import { build } from 'esbuild';
const work = dirname(fileURLToPath(import.meta.url));
const root = dirname(work), content = join(work, 'generated/manual'), output = join(root, 'Documentation');
const hash = value => createHash('sha256').update(value).digest('hex');
const esc = value => value.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
for (const target of [content, output]) if (!relative(root,target) || relative(root,target).startsWith('..')) throw new Error('Unsafe documentation path');
await mkdir(join(work,'generated'), {recursive:true});
// This is disposable compiler staging, never authoring or internal notes.
await rm(content,{recursive:true,force:true});
await cp(join(work,'manual'),content,{recursive:true});
const tools=JSON.parse(await readFile(join(work,'api/mcp-tools.json'),'utf8'));
const french=JSON.parse(await readFile(join(work,'api/mcp-fr.json'),'utf8'));
const english=JSON.parse(await readFile(join(work,'api/mcp-en-overrides.json'),'utf8'));
const enDescription=tool=>english[tool.name]?.description||tool.description;
const params=JSON.parse(await readFile(join(work,'api/parameters.json'),'utf8'));
const config=JSON.parse(await readFile(join(work,'cydoc.config.json'),'utf8'));
const slug=value=>value.replaceAll('_','-');
const groupNames={
  projects:['Projects and transport','Projets et transport'], media:['Media and source edits','Médias et montage source'],
  tracks:['Tracks and groups','Pistes et groupes'], editing:['Clip editing','Montage des clips'],
  graphs:['Image graphs and shaders','Graphes d’image et shaders'], grading:['Color and LUTs','Couleur et LUT'],
  mixing:['Audio routing and DSP','Routage audio et DSP'], plugins:['External audio and OFX','Audio externe et OFX'],
  motion:['Titles, masks and motion','Titres, masques et animation'], markers:['Markers and interchange','Repères et échanges'],
  delivery:['Rendering and delivery','Rendu et livraison']
};
const groups={
  projects:'status new_project open_project save_project list_versions restore_version set_sequence transport undo redo get_history',
  media:'import_media remove_unused_media edit_source replace_source media_status relink_media collect_media prepare_proxy set_preview_proxies',
  tracks:'add_track set_track remove_track move_track group_tracks set_group ungroup duplicate_track',
  graphs:'get_layer_graph set_layer_graph get_track_graph set_track_graph add_track_effect set_layer_parameter set_layer_keyframe add_generator add_effect set_shader get_graph set_graph list_effects add_adjustment_layer',
  grading:'add_grade set_grade set_grade_curve import_grade_lut neutral_grade list_grade_presets apply_grade_preset save_grade_preset load_grade_preset video_scopes set_color_management set_source_color auto_grade match_grade',
  mixing:'add_bus remove_bus set_mix_channel set_send remove_send add_mix_effect set_mix_effect remove_mix_effect set_mix_parameter set_mix_keyframe remove_mix_keyframe get_audio_graph set_dsp_graph get_dsp_graph add_audio_effect normalize_audio normalize_loudness enhance_voice auto_duck remove_silences audio_cut_fades',
  plugins:'add_external_audio show_plugin_editor capture_plugin_state get_plugin_state set_plugin_state save_plugin_preset load_plugin_preset inspect_ofx add_ofx_effect',
  motion:'set_motion_widget get_motion_widgets add_title apply_title_style add_lower_third set_title set_mask set_bezier_mask start_mask_tracking mask_tracking_status cancel_mask_tracking apply_mask_tracking key_background',
  markers:'add_marker set_marker remove_marker export_edl import_edl export_shot_list export_markers import_subtitles export_subtitles add_subtitle set_subtitle_style detect_scenes detect_beats split_at_markers fit_to_markers switch_angle',
  delivery:'export_video export_audio export_gif export_chapters start_export list_export_presets export_status cancel_export render_frame export_marker_stills'
};
const groupOf=name=>Object.keys(groups).find(key=>groups[key].split(' ').includes(name))||'editing';
function frontmatter({id,title,description,parent,locale,revision='1',kind,order=0}) {
  return `---\nid: ${id}\ntitle: ${title}\ndescription: ${description.replaceAll('\n',' ')}\nsection: ${locale==='fr'?'Référence développeur':'Developer reference'}\n${parent?'parent: '+parent+'\n':''}order: ${order}\n${locale==='en'?'revision':'sourceRevision'}: ${revision}\n${kind?'kind: '+kind+'\n':''}---\n\n`;
}
for (const locale of ['en','fr']) {
  const fr=locale==='fr';
  const symbols=tools.map(tool=>({key:tool.name,name:tool.name,language:'json',kind:'tool',
    description:fr?french[tool.name].description:enDescription(tool),
    // Input schemas are public declarations, not implementation code.
    signature:JSON.stringify({name:tool.name,inputSchema:tool.inputSchema},null,2),
    parameters:Object.entries(tool.inputSchema.properties||{}).map(([name,property])=>({name,type:property.type||'JSON',optional:!(tool.inputSchema.required||[]).includes(name),description:params[name][locale],default:property.default===undefined?'':JSON.stringify(property.default)})),
    returnType:'MCP CallToolResult',
    returns:fr?'content[0].text contient le JSON du résultat ; structuredContent fournit le même objet. isError vaut false en cas de réussite. En cas de refus, isError vaut true et content[0].text explique le problème. Les champs particuliers sont décrits ci-dessus.':'content[0].text contains result JSON; structuredContent provides the same object. isError is false on success. A rejected operation sets isError to true and explains the problem in content[0].text. Operation-specific fields are described above.'}));
  const neutral=join(work,`generated/mcp-${locale}.json`);
  await writeFile(neutral,JSON.stringify({symbols}));
  const pages=await importApi([{format:'cydoc',path:relative(work,neutral),prefix:'mcp-api',locale}],{base:work,locales:config.locales});
  for(const page of pages) {
    const tool=tools.find(tool=>tool.name===page.title);
    const revision=hash(JSON.stringify(tool)).slice(0,16);
    const description=symbols.find(symbol=>symbol.name===page.title).description;
    let body=page.markdown;
    body+=`\n\n[${fr?'Connexion MCP et exemples':'MCP connection and examples'}](doc:mcp) · [${fr?'Règles des appels':'Calling conventions'}](doc:api-links#calling)\n`;
    await writeFile(join(content,locale,page.id+'.md'),frontmatter({...page,description,parent:'mcp-'+groupOf(page.title),revision,order:0})+body,'utf8');
  }
  for(const [group,labels] of Object.entries(groupNames)) {
    const title=labels[fr?1:0], id='mcp-'+group;
    const links=tools.filter(t=>groupOf(t.name)===group).sort((a,b)=>a.name.localeCompare(b.name)).map(t=>`- [\`${t.name}\`](doc:mcp-api-${slug(t.name)}-${slug(t.name)}) — ${fr?french[t.name].description:enDescription(t)}`).join('\n');
    await writeFile(join(content,locale,id+'.md'),frontmatter({id,title,description:fr?'Choisir une opération MCP et consulter ses paramètres et son résultat.':'Choose an MCP operation and inspect its arguments and result.',parent:'api-links',locale})+`## ${fr?'Opérations':'Operations'} {#tools}\n\n`+links+'\n','utf8');
  }
  const sdk=await importApi([{format:'cydoc',path:`api/sdk-${locale}.json`,prefix:'sdk-api',locale}],{base:work,locales:config.locales});
  for(const page of sdk) await writeFile(join(content,locale,page.id+'.md'),frontmatter({...page,description:fr?'Contrat public du SDK audio natif C/C++.':'Public C/C++ native audio SDK contract.',parent:'reference-sdk',revision:'1'})+page.markdown+`\n\n[${fr?'Inclure le SDK et déclarer l’ABI':'SDK include and ABI declaration'}](doc:reference-sdk#L1)\n`,'utf8');
}
// Retired reports/source listings remain addressable without exposing their content.
const legacy=JSON.parse(await readFile(join(work,'legacy-pages.json'),'utf8'));
const active=new Set((await readdir(join(content,'en'))).map(name=>name.replace(/\.md$/,'')));
const aliases={...config.aliases};
for(const id of legacy.pages) if(!active.has(id)) aliases[id]=id.startsWith('source-shaders')?'reference-hlsl#api':id.startsWith('source-')?'reference-sdk#L1':/license|notices/.test(id)?'about-license':id.includes('plugins')?'plugins':id.includes('mcp')?'api-links':'welcome';
// All input paths remain relative to .CyDocBuild, as required by the official CLI.
const generatedConfig=join(work,'export.config.json');
await writeFile(generatedConfig,JSON.stringify({...config,aliases}));
const packageRoot=dirname(dirname(fileURLToPath(import.meta.resolve('cydoc'))));
const cli=JSON.parse(await readFile(join(packageRoot,'package.json'),'utf8')).bin.cydoc;
const run=spawnSync(process.execPath,[join(packageRoot,cli),'check',generatedConfig],{stdio:'inherit',windowsHide:true});
if(run.status!==0)throw new Error('CyDoc check failed');
// Use the public viewer with the application theme, print-guide action and storage namespace.
await build({entryPoints:[join(work,'reader.mjs')],bundle:true,format:'iife',minify:true,target:'es2020',outfile:join(output,'DocData/viewer-app.js'),legalComments:'eof'});
await cp(join(work,'reader.html'),join(output,'Documentation.html'));
await cp(join(work,'shaderixframe.css'),join(output,'DocData/shaderixframe.css'));
await mkdir(join(output,'DocData/licenses'),{recursive:true});
await cp(join(work,'vendor/cydoc/LICENSE'),join(output,'DocData/licenses/CyDoc-LICENSE.txt'));
const manifest=JSON.parse(await readFile(join(output,'DocData/manifest.json'),'utf8'));
for(const locale of ['en','fr']) {
  const fr=locale==='fr';
  const pages=legacy.guides.map(id=>manifest.pages.find(page=>page.id===id&&page.locale===locale));
  if(pages.some(page=>!page))throw new Error('Missing guide translation');
  const body=pages.map(page=>`<section><h1>${esc(page.title)}</h1>${page.html.replaceAll('href="#/','href="../Documentation.html#/').replaceAll('loading="lazy"','loading="eager"')}</section>`).join('');
  await writeFile(join(output,'DocData',`print-${locale}.html`),`<!doctype html><html lang="${locale}"><meta charset="utf-8"><title>ShaderixFrame ${fr?'Guide imprimable':'Printable guide'}</title><link rel="stylesheet" href="viewer.css"><style>body{font:16px/1.6 system-ui;background:white;color:#202020;margin:32px;max-width:1100px}section{break-before:page}section:first-child{break-before:auto}img,svg{max-width:100%;height:auto}pre{white-space:pre-wrap}table{border-collapse:collapse}td,th{border:1px solid #aaa;padding:8px}figure{margin:20px 0;break-inside:avoid}button,.code-toolbar{display:none}@media print{body{font-size:10pt;margin:0}a{color:inherit}img{max-height:15cm;object-fit:contain}}</style><body><p>${fr?'Imprimer / enregistrer PDF : Ctrl+P':'Print / save PDF: Ctrl+P'}</p>${body}</body></html>`,'utf8');
}
await writeFile(join(output,'index.html'),'<!doctype html><meta charset="utf-8"><title>ShaderixFrame Documentation</title><a href="Documentation.html">Documentation</a><script>location.replace("Documentation.html"+location.search+location.hash)</script>');
console.log(`ShaderixFrame: ${legacy.guides.length} bilingual guides, ${tools.length} bilingual MCP contracts, public SDK; image and gallery only.`);

// In-memory DOM fixtures: no browser navigation, server, resource loading or file-origin claims.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { resolve, dirname } from 'node:path';
import { Window } from 'happy-dom';
import { createDocumentation, createLink, parseLink, hasAnchor } from 'cydoc';
import { mountViewer } from 'cydoc/viewer';
import image from 'cydoc/modules/image';
import gallery from 'cydoc/modules/gallery';
const work=dirname(fileURLToPath(import.meta.url)), root=dirname(work);
const data=JSON.parse(await readFile(resolve(root,'Documentation/DocData/manifest.json'),'utf8'));
const contracts=JSON.parse(await readFile(resolve(work,'api/mcp-tools.json'),'utf8'));
const settle=async()=>{await new Promise(resolve=>setImmediate(resolve));await new Promise(resolve=>setImmediate(resolve));};
function fixture(t,route='en/quickstart',extra={}) {
  const window=new Window({url:'https://cydoc-fixture.invalid/Documentation.html#/'+route,settings:{disableJavaScriptFileLoading:true,enableJavaScriptEvaluation:false,enableImageFileLoading:false,enableCSSFileLoading:false}});
  const previous=new Map();
  const bindings={window,document:window.document,localStorage:window.localStorage,location:window.location,navigator:window.navigator,
    MutationObserver:window.MutationObserver,ResizeObserver:window.ResizeObserver,IntersectionObserver:window.IntersectionObserver,
    matchMedia:window.matchMedia.bind(window),requestAnimationFrame:window.requestAnimationFrame.bind(window),cancelAnimationFrame:window.cancelAnimationFrame.bind(window),...extra};
  for(const [key,value] of Object.entries(bindings)){previous.set(key,Object.getOwnPropertyDescriptor(globalThis,key));Object.defineProperty(globalThis,key,{configurable:true,writable:true,value});}
  const host=window.document.createElement('div');host.id='app';window.document.body.append(host);
  const network={calls:0};window.fetch=()=>{network.calls++;throw new Error('No network in fixture');};
  let reader;
  t.after(async()=>{reader?.destroy();window.dispatchEvent(new window.Event('pagehide'));await window.happyDOM.abort();window.close();for(const [key,descriptor] of previous){if(descriptor)Object.defineProperty(globalThis,key,descriptor);else delete globalThis[key];}});
  return {window,host,network,mount:(manifest=data)=>reader=mountViewer(host,manifest,{homePage:'welcome',initialTheme:'dark',assetBase:'DocData/',storageId:'shaderixframe-help-test',modules:{image:async()=>image,gallery:async()=>gallery}})};
}

test('bilingual human content, nested public APIs, stable aliases and no source/maintenance pages',async()=>{
  const docs=createDocumentation(data);
  assert.deepEqual(docs.availableLocales().map(x=>x.code),['en','fr']);
  const ids=(await readdir(resolve(work,'manual/en'))).map(file=>file.replace(/\.md$/,''));
  for(const id of ids){const en=docs.resolve(id,'en').page,fr=docs.resolve(id,'fr').page;assert.notEqual(en.text,fr.text,id);assert.deepEqual(en.headings.map(h=>h.id),fr.headings.map(h=>h.id),id);assert.equal(en.parent,fr.parent);}
  for(const page of data.pages){assert.notEqual(page.kind,'source');assert.ok(!page.source && !page.sourcePageId);assert.ok(!/manual-maintenance|reference-release|translation-report|source-sdk/.test(page.id));}
  assert.equal(docs.resolve('layer-graph','fr').page.id,'layers');
  assert.equal(docs.resolve('source-sdk-shaderixframeplugin-h','fr').page.id,'reference-sdk');
  assert.equal(docs.resolve('manual-maintenance','en').page.id,'welcome');
  assert.ok(hasAnchor(docs.resolve('reference-sdk','fr').page,'L1'));
  const route={locale:'fr',pageId:'timeline',anchor:'usage',state:{media:'timeline-image-1',note:'navigator'}};
  const link=createLink(route,'file:///D:/Example%20App/Documentation/Documentation.html');
  assert.deepEqual(parseLink(link.slice(link.indexOf('#'))),route);
  for(const tool of contracts){const slug=tool.name.replaceAll('_','-'),id=`mcp-api-${slug}-${slug}`;const en=docs.resolve(id,'en').page,fr=docs.resolve(id,'fr').page;assert.ok(en.code.includes('"inputSchema"'));assert.ok(fr.parent.startsWith('mcp-'));assert.notEqual(en.description,fr.description);assert.ok(hasAnchor(fr,'returns'));}
  assert.ok(docs.search('etalonnage','fr').length);
  assert.ok(docs.search('EffectContext','en',30,{language:'hlsl'}).length);
  assert.ok(!docs.search('previous-migration','en').length);
  const incomplete=structuredClone(data);incomplete.pages=incomplete.pages.filter(p=>!(p.id==='quickstart'&&p.locale==='fr'));
  assert.deepEqual(createDocumentation(incomplete).availableLocales().map(x=>x.code),['en']);
});

test('current viewer header search, tree tooltips, language and reading positions',async t=>{
  const {window,host,mount,network}=fixture(t);mount();await settle();
  assert.equal(window.document.documentElement.lang,'en');
  assert.equal(host.dataset.theme,'dark');
  assert.ok(host.querySelector('.topbar [data-header-search]'));
  assert.ok(host.querySelector('.nav-tree .nav-children .nav-children'));
  assert.ok(!host.querySelector('.local-status,.offline-card'));
  const field=host.querySelector('[data-header-search]');field.value='grading';field.dispatchEvent(new window.Event('input',{bubbles:true}));await settle();
  assert.ok(host.querySelector('.search-dialog[open]'));assert.match(host.querySelector('.search-results').textContent,/grading/i);
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'k',ctrlKey:true,bubbles:true}));
  assert.ok(host.querySelector('.search-dialog[open]'));
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));
  host.querySelector('.search-dialog')?.close();
  const language=host.querySelector('.locale-control select');language.value='fr';language.dispatchEvent(new window.Event('change',{bubbles:true}));window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();
  assert.equal(window.document.documentElement.lang,'fr');assert.match(host.querySelector('main').textContent,/Premier montage/);
  const nav=host.querySelector('a[href="#/fr/timeline"]');nav.focus();nav.dispatchEvent(new window.FocusEvent('focusin',{bubbles:true}));await settle();
  const tip=window.document.querySelector('.reader-tooltip');assert.ok(tip);assert.match(tip.textContent,/Timeline, pistes et navigation/);
  let savedScroll=0;window.scrollTo=options=>{savedScroll=typeof options==='object'?options.top||0:0;Object.defineProperty(window,'scrollY',{configurable:true,value:savedScroll});};
  window.scrollTo({top:420});window.location.hash='#/fr/layers';window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();window.location.hash='#/fr/quickstart';window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();assert.equal(savedScroll,420);
  assert.equal(network.calls,0);
});

test('image and gallery resolve DocData assets, keyboard items and in-window enlargement',async t=>{
  const {window,host,mount,network}=fixture(t,'fr/quickstart?media=quickstart-image-1');mount();await settle();
  const media=host.querySelector('[data-cydoc-id="quickstart-image-1"]');assert.ok(media.querySelector('.media-stage'));
  assert.equal(media.querySelector('img').getAttribute('src'),'DocData/assets/editing.png');
  let fullscreen=0;media.requestFullscreen=async()=>{fullscreen++;};
  const enlarge=media.querySelector('[data-action="expand"]');enlarge.focus();enlarge.click();assert.equal(enlarge.getAttribute('aria-expanded'),'true');
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));assert.ok(!media.classList.contains('media-expanded'));assert.equal(fullscreen,0);
  media.querySelector('[data-action="in"]').click();assert.notEqual(media.querySelector('.zoom-value').textContent,'100%');
  window.location.hash='#/fr/workspaces?media=workspaces-gallery&item=2';window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();
  const collection=host.querySelector('[data-cydoc-id="workspaces-gallery"]');assert.equal(collection.querySelector('output').textContent,'3 / 6');assert.ok(!collection.querySelector('.gallery-fallback'));
  collection.querySelector('[role=tab][aria-selected=true]').dispatchEvent(new window.KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));await settle();
  assert.equal(collection.querySelector('output').textContent,'4 / 6');
  const button=collection.querySelector('[role=tab][aria-selected=true]');assert.ok(button.dataset.tooltipDescription);assert.match(button.textContent,/Composition graphique animée/);
  assert.ok(collection.querySelector('.gallery-content img').getAttribute('src').startsWith('DocData/assets/'));
  collection.querySelector('[data-action="expand"]').click();assert.ok(collection.classList.contains('media-expanded'));
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));assert.ok(!collection.classList.contains('media-expanded'));
  assert.equal(network.calls,0);
});

test('app embedding print language, long identity, separate AI sidecar and lifecycle',async t=>{
  const long=structuredClone(data);long.name='ShaderixFrame — '+ 'Long application title '.repeat(7);
  const {window,host}=fixture(t,'en/welcome',{CYDOC_DATA:long,CYDOC_OPTIONS:{homePage:'welcome',modules:['image','gallery'],assets:{}}});
  await import('./reader.mjs');await settle();
  assert.equal(host.dataset.theme,'dark');assert.ok(host.querySelector('[data-print-guide]').href.endsWith('/DocData/print-en.html'));
  assert.ok(host.querySelector('.brand').textContent.includes(long.name));
  const language=host.querySelector('.locale-control select');language.value='fr';language.dispatchEvent(new window.Event('change',{bubbles:true}));window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();
  assert.ok(host.querySelector('[data-print-guide]').href.endsWith('/DocData/print-fr.html'));
  assert.ok(!host.querySelector('.side-nav').textContent.includes('MIT'));
  assert.ok(!host.querySelector('.side-nav').textContent.includes('DocAI'));
  const cached=new window.Event('pagehide');Object.defineProperty(cached,'persisted',{value:true});window.dispatchEvent(cached);assert.ok(host.querySelector('main'));
  const ai=await readFile(resolve(root,'Documentation/DocAI.md'),'utf8');assert.match(ai,/ShaderixFrame/);assert.ok(!data.pages.some(page=>page.title.includes('DocAI')));
});

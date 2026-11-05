// DOM unit fixtures only: no browser navigation, file-origin claims, script loading,
// resource fetching or server. Real file:// browser QA is reported separately.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { resolve, dirname } from 'node:path';
import { Window } from 'happy-dom';
import { createDocumentation, createLink, parseLink, hasAnchor } from 'cydoc';
import { mountViewer } from 'cydoc/viewer';
import image from 'cydoc/modules/image';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const data = JSON.parse(await readFile(resolve(root, 'build/help/manifest.json'), 'utf8'));
const guideIds = JSON.parse(await readFile(resolve(root, 'docs/manual/migration.json'), 'utf8')).preserved_chapter_ids;
const contracts = JSON.parse(await readFile(resolve(root, 'docs/manual/api/mcp-tools.json'), 'utf8'));
const settle = () => new Promise(resolve => setImmediate(resolve));

test('exported guides, legacy routes, bilingual anchors and actual MCP contracts', () => {
  const docs = createDocumentation(data);
  assert.equal(data.defaultLocale, 'en');
  for (const id of guideIds) {
    const en = docs.resolve(id,'en').page, fr = docs.resolve(id,'fr').page;
    assert.equal(docs.resolve(id,'en').fallback, false);
    assert.equal(docs.resolve(id,'fr').fallback, false);
    assert.deepEqual(en.headings.map(h=>h.id),fr.headings.map(h=>h.id));
    assert.ok(hasAnchor(en,'usage'));
  }
  assert.equal(docs.resolve('layer-graph','fr').page.id,'layers');
  const missingFrench=structuredClone(data);
  missingFrench.pages=missingFrench.pages.filter(p=>!(p.id==='quickstart'&&p.locale==='fr'));
  assert.equal(createDocumentation(missingFrench).resolve('quickstart','fr').fallback,true);
  const route={locale:'fr',pageId:'timeline',anchor:'usage',state:{media:'timeline-image-1',note:'navigator'}};
  const link=createLink(route,'file:///D:/Application%20test/help/index.html');
  assert.deepEqual(parseLink(link.slice(link.indexOf('#'))),route);
  const timeline=docs.resolve('timeline','fr').page;
  assert.match(timeline.html,/data-cydoc-id="timeline-image-1"/);
  assert.match(timeline.html,/navigator/);
  assert.ok(docs.search('etalonnage','fr').length);
  assert.ok(docs.search('EffectContext','en',30,{language:'hlsl'}).length);
  for(const tool of contracts) {
    const slug=tool.name.replaceAll('_','-');
    const page=docs.resolve(`mcp-api-${slug}-${slug}`,'en').page;
    assert.ok(page.code.includes('"inputSchema"'));
    assert.ok(page.code.includes(tool.name));
  }
  for(const page of data.pages.filter(p=>p.kind==='source')) {
    assert.match(page.source,/^(sdk|shaders)\//);
    assert.ok(page.anchors.includes('L1'));
  }
});

test('CyDoc viewer languages, keyboard search, image controls, focus and reading history in DOM fixture', async t => {
  const window=new Window({url:'https://cydoc-fixture.invalid/index.html#/en/quickstart',settings:{disableJavaScriptFileLoading:true,enableJavaScriptEvaluation:false,enableImageFileLoading:false,enableCSSFileLoading:false}});
  const previous=new Map();
  const globals={window,document:window.document,localStorage:window.localStorage,location:window.location,navigator:window.navigator,
    ResizeObserver:window.ResizeObserver,IntersectionObserver:window.IntersectionObserver,matchMedia:window.matchMedia.bind(window),
    requestAnimationFrame:window.requestAnimationFrame.bind(window),cancelAnimationFrame:window.cancelAnimationFrame.bind(window)};
  for(const [key,value] of Object.entries(globals)) { previous.set(key,Object.getOwnPropertyDescriptor(globalThis,key));Object.defineProperty(globalThis,key,{configurable:true,writable:true,value}); }
  let network=0,nativeFullscreen=0;
  window.fetch=()=>{network++;throw new Error('No network in documentation fixture');};
  const host=window.document.createElement('div');window.document.body.append(host);
  const reader=mountViewer(host,data,{homePage:'welcome',initialTheme:'dark',storageId:'shaderixframe-help-test',modules:{image:async()=>image}});
  t.after(async()=>{
    reader.destroy();await window.happyDOM.abort();window.close();
    for(const [key,descriptor] of previous) {if(descriptor)Object.defineProperty(globalThis,key,descriptor);else delete globalThis[key];}
  });
  await settle();await settle();
  assert.equal(window.document.documentElement.lang,'en');
  assert.equal(host.dataset.theme,'dark');
  assert.match(host.querySelector('main').textContent,/Your first edit/);
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'k',ctrlKey:true,bubbles:true}));
  assert.ok(host.querySelector('dialog[open]'));
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));
  host.querySelector('dialog')?.close();
  const language=host.querySelector('.locale-control select');language.value='fr';language.dispatchEvent(new window.Event('change',{bubbles:true}));
  window.dispatchEvent(new window.HashChangeEvent('hashchange'));
  await settle();await settle();
  assert.equal(window.document.documentElement.lang,'fr');
  assert.match(host.querySelector('main').textContent,/Premier montage/);
  const media=host.querySelector('[data-cydoc-id="quickstart-image-1"]');
  // Explicit state reveals lazy media, as a native precise-help route would.
  window.location.hash='#/fr/quickstart?media=quickstart-image-1';
  window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();await settle();
  const mounted=host.querySelector('[data-cydoc-id="quickstart-image-1"]');
  assert.ok(mounted.querySelector('.media-stage'));
  mounted.requestFullscreen=async()=>{nativeFullscreen++;};
  const enlarge=mounted.querySelector('[data-action="expand"]');enlarge.focus();enlarge.click();
  assert.equal(enlarge.getAttribute('aria-expanded'),'true');
  assert.ok(mounted.classList.contains('media-expanded'));
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));
  assert.equal(mounted.classList.contains('media-expanded'),false);
  assert.equal(nativeFullscreen,0);
  mounted.querySelector('[data-action="in"]').click();
  assert.notEqual(mounted.querySelector('.zoom-value').textContent,'100%');
  assert.ok(mounted.classList.contains('media-minimal'));
  let savedScroll=0;
  window.scrollTo=options=>{savedScroll=typeof options==='object'?options.top||0:0;Object.defineProperty(window,'scrollY',{configurable:true,value:savedScroll});};
  window.scrollTo({top:420});
  window.location.hash='#/fr/layers';window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();await settle();
  window.location.hash='#/fr/quickstart';window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();await settle();
  assert.equal(savedScroll,420);
  assert.equal(network,0);
});

test('application embedding keeps licensing, printing and browser-cache lifecycle correct in DOM fixture', async t => {
  const window=new Window({url:'https://cydoc-fixture.invalid/index.html#/en/welcome',settings:{disableJavaScriptFileLoading:true,enableJavaScriptEvaluation:false,enableImageFileLoading:false,enableCSSFileLoading:false}});
  const previous=new Map();
  const bindings={window,document:window.document,localStorage:window.localStorage,location:window.location,navigator:window.navigator,
    MutationObserver:window.MutationObserver,ResizeObserver:window.ResizeObserver,IntersectionObserver:window.IntersectionObserver,
    matchMedia:window.matchMedia.bind(window),requestAnimationFrame:window.requestAnimationFrame.bind(window),cancelAnimationFrame:window.cancelAnimationFrame.bind(window),
    CYDOC_DATA:data,CYDOC_OPTIONS:{homePage:'welcome',modules:['image'],assets:{}}};
  for(const [key,value] of Object.entries(bindings)) {previous.set(key,Object.getOwnPropertyDescriptor(globalThis,key));Object.defineProperty(globalThis,key,{configurable:true,writable:true,value});}
  const host=window.document.createElement('div');host.id='app';window.document.body.append(host);
  t.after(async()=>{
    window.dispatchEvent(new window.Event('pagehide'));
    await window.happyDOM.abort();window.close();
    for(const [key,descriptor] of previous) {if(descriptor)Object.defineProperty(globalThis,key,descriptor);else delete globalThis[key];}
  });
  await import('./reader.mjs');await settle();await settle();
  assert.equal(host.dataset.theme,'dark');
  assert.match(host.querySelector('.footer').textContent,/AGPL-3.0-only/);
  assert.match(host.querySelector('.footer').textContent,/CyDoc: MIT/);
  assert.ok(host.querySelector('[data-print-guide]').href.endsWith('/print-en.html'));
  const change=host.querySelector('.locale-control select');change.value='fr';change.dispatchEvent(new window.Event('change',{bubbles:true}));
  window.dispatchEvent(new window.HashChangeEvent('hashchange'));
  await settle();await settle();
  assert.ok(host.querySelector('[data-print-guide]').href.endsWith('/print-fr.html'));
  const cached=new window.Event('pagehide');Object.defineProperty(cached,'persisted',{value:true});window.dispatchEvent(cached);
  assert.ok(host.querySelector('main'));
});

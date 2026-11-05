// Application-owned embedding. Resource loading follows CyDoc's generated offline bootstrap;
// mountViewer and MediaModule are public APIs. No fetch or runtime ESM is used by the export.
import { mountViewer } from 'cydoc/viewer';
const options = globalThis.CYDOC_OPTIONS || {};
const pendingAssets = new Map();
function loadAsset(src) {
  if (pendingAssets.has(src)) return pendingAssets.get(src);
  const file = options.assets?.[src];
  if (!file) return Promise.reject(new Error(`Local asset is not packaged: ${src}`));
  const promise = new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = file;
    script.onload = () => {
      const asset = globalThis.CYDOC_ASSETS?.[src];
      if (globalThis.CYDOC_ASSETS) delete globalThis.CYDOC_ASSETS[src];
      script.remove(); pendingAssets.delete(src);
      if (asset) resolve(asset); else reject(new Error(`Invalid local asset: ${src}`));
    };
    script.onerror = () => { script.remove(); pendingAssets.delete(src); reject(new Error(`Cannot load: ${src}`)); };
    document.head.append(script);
  });
  pendingAssets.set(src, promise);
  return promise;
}
const modules = Object.fromEntries(options.modules.map(name => [name, () => new Promise((resolve, reject) => {
  const script = document.createElement('script'); script.src = `media-${name}.js`;
  script.onload = () => resolve(globalThis[`CyDocMedia_${name}`].default);
  script.onerror = () => { script.remove(); reject(new Error(`Cannot load media-${name}`)); };
  document.head.append(script);
})]));
let preferredTheme;
try { preferredTheme = localStorage.getItem('cydoc:theme'); } catch { /* File storage can be disabled. */ }
const root = document.querySelector('#app');
const reader = mountViewer(root, globalThis.CYDOC_DATA, {
  homePage: options.homePage,
  initialTheme: preferredTheme ? undefined : 'dark',
  storageId: 'shaderixframe-help-v1',
  modules, loadAsset
});
// Preserve the former complete-guide print feature, using CyDoc's compiled pages.
const printLink = () => {
  const fr = document.documentElement.lang === 'fr';
  const licence = root.querySelector('.footer > div > span');
  const label = fr ? 'Application : AGPL-3.0-only · CyDoc : MIT' : 'App: AGPL-3.0-only · CyDoc: MIT';
  if (licence && licence.textContent !== label) licence.textContent = label;
  const toolbar = root.querySelector('.topbar-actions');
  if (!toolbar) return;
  const existing = toolbar.querySelector('[data-print-guide]');
  const link = existing || document.createElement('a');
  link.dataset.printGuide = '';
  link.href = fr ? 'print-fr.html' : 'print-en.html';
  link.className = 'top-link';
  const caption = fr ? 'Imprimer le guide' : 'Print guide';
  if (link.textContent !== caption) link.textContent = caption;
  if (!existing) toolbar.prepend(link);
};
const observer = new MutationObserver(printLink);
observer.observe(root, { childList: true, subtree: true }); printLink();
observer.observe(document.documentElement, { attributes: true, attributeFilter: ['lang'] });
window.addEventListener('pagehide', event => {
  // Keep the reader alive when browser Back/Forward caches this document.
  if (!event.persisted) { observer.disconnect(); reader.destroy(); }
});

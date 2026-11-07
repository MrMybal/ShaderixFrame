// Classic-script interactions, adapted from the CyDoc showcase patterns (MIT).
// COPY, SHOTS, GROUPS and DESTINATIONS are bundled at build time; no runtime fetches.
let language = new URL(location.href).searchParams.get('lang') === 'fr' ? 'fr' : 'en';
const strings = () => COPY[language];
const shotById = id => SHOTS.find(shot => shot.id === id);
const dialog = document.querySelector('#lightbox');
const picture = document.querySelector('#lightbox-image');
const menu = document.querySelector('.menu-button');
const primaryNav = document.querySelector('#primary-nav');
let origin, currentShot = 0, savedScroll, savedOverflow, savedPadding;

function manualUrl(target) {
  const [page, anchor] = target.split('#');
  return `./manual/Documentation.html#/${language}/${encodeURIComponent(page)}${anchor ? '/' + encodeURIComponent(anchor) : ''}`;
}
function updateDialog() {
  const shot = SHOTS[currentShot];
  picture.src = shot.src;
  picture.alt = shot[language].alt;
  document.querySelector('#lightbox-caption').textContent = shot[language].caption;
  document.querySelector('#lightbox-count').textContent = strings()['lightbox.count']
    .replace('{current}', currentShot + 1).replace('{total}', SHOTS.length);
}
function localize() {
  const messages = strings();
  document.documentElement.lang = language;
  document.title = messages['meta.title'];
  document.querySelector('meta[name="description"]').content = messages['meta.description'];
  document.querySelector('meta[property="og:title"]').content = messages['meta.title'];
  document.querySelector('meta[property="og:description"]').content = messages['meta.description'];
  document.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = messages[el.dataset.i18n]; });
  document.querySelectorAll('[data-i18n-label]').forEach(el => { el.setAttribute('aria-label', messages[el.dataset.i18nLabel]); });
  document.querySelectorAll('[data-doc]').forEach(el => { el.href = manualUrl(el.dataset.doc); });
  document.querySelectorAll('[data-lang]').forEach(el => { el.setAttribute('aria-pressed', String(el.dataset.lang === language)); });
  document.querySelectorAll('[data-shot]').forEach(button => {
    const shot = shotById(button.dataset.shot);
    button.setAttribute('aria-label', `${messages['lightbox.open']}: ${shot[language].caption}`);
    const image = button.querySelector('img');
    image.alt = shot[language].alt;
    button.closest('figure').querySelector('[data-caption]').textContent = shot[language].caption;
  });
  menu.setAttribute('aria-label', messages[menu.getAttribute('aria-expanded') === 'true' ? 'nav.close' : 'nav.menu']);
  if (dialog.open) updateDialog();
}
function setLanguage(value) {
  language = value === 'fr' ? 'fr' : 'en';
  const url = new URL(location.href);
  url.searchParams.set('lang', language);
  try { history.replaceState(null, '', url); } catch { /* File-origin history may be restricted. */ }
  localize();
}
function chooseView(group, id, focus = false) {
  const explorer = document.querySelector(`[data-explorer="${group}"]`);
  const item = GROUPS[group].find(view => view.shot === id);
  if (!item) return;
  const panel = explorer.querySelector('[role="tabpanel"]');
  const shot = shotById(item.shot);
  panel.setAttribute('aria-labelledby', `${group}-tab-${id}`);
  panel.querySelector('h3').dataset.i18n = item.title;
  panel.querySelector('.view-body').dataset.i18n = item.body;
  panel.querySelector('[data-doc]').dataset.doc = shot.doc;
  panel.querySelector('[data-shot]').dataset.shot = shot.id;
  const image = panel.querySelector('img');
  image.src = shot.src;
  image.width = shot.width;
  image.height = shot.height;
  explorer.querySelectorAll('[role="tab"]').forEach(button => {
    const selected = button.dataset.view === id;
    button.setAttribute('aria-selected', String(selected));
    button.tabIndex = selected ? 0 : -1;
    if (selected && focus) button.focus();
  });
  localize();
}
function closeMenu(restoreFocus = false) {
  menu.setAttribute('aria-expanded', 'false');
  menu.setAttribute('aria-label', strings()['nav.menu']);
  primaryNav.dataset.open = 'false';
  if (restoreFocus) menu.focus();
}
function openShot(button) {
  origin = button;
  currentShot = SHOTS.findIndex(shot => shot.id === button.dataset.shot);
  savedScroll = [window.scrollX, window.scrollY];
  savedOverflow = document.body.style.overflow;
  savedPadding = document.body.style.paddingRight;
  const gutter = window.innerWidth - document.documentElement.clientWidth;
  document.body.style.overflow = 'hidden';
  if (gutter > 0) document.body.style.paddingRight = gutter + 'px';
  updateDialog();
  dialog.showModal();
  dialog.querySelector('[data-close]').focus();
}
function nextShot(delta) {
  currentShot = (currentShot + delta + SHOTS.length) % SHOTS.length;
  updateDialog();
}
document.addEventListener('click', event => {
  const languageButton = event.target.closest('[data-lang]');
  if (languageButton) setLanguage(languageButton.dataset.lang);
  const button = event.target.closest('[data-shot]');
  if (button) openShot(button);
  const view = event.target.closest('[data-view]');
  if (view) chooseView(view.closest('[data-explorer]').dataset.explorer, view.dataset.view);
  if (event.target.closest('#primary-nav a')) closeMenu();
});
menu.addEventListener('click', () => {
  const expanded = menu.getAttribute('aria-expanded') !== 'true';
  menu.setAttribute('aria-expanded', String(expanded));
  menu.setAttribute('aria-label', strings()[expanded ? 'nav.close' : 'nav.menu']);
  primaryNav.dataset.open = String(expanded);
});
document.querySelectorAll('[data-explorer]').forEach(explorer => {
  explorer.querySelector('[role="tablist"]').addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
    event.preventDefault();
    const group = explorer.dataset.explorer;
    const items = GROUPS[group];
    let index = items.findIndex(view => view.shot === explorer.querySelector('[aria-selected="true"]').dataset.view);
    if (event.key === 'Home') index = 0;
    else if (event.key === 'End') index = items.length - 1;
    else index = (index + (event.key === 'ArrowRight' ? 1 : -1) + items.length) % items.length;
    chooseView(group, items[index].shot, true);
  });
});
dialog.querySelector('[data-close]').addEventListener('click', () => dialog.close());
dialog.querySelector('[data-prev]').addEventListener('click', () => nextShot(-1));
dialog.querySelector('[data-next]').addEventListener('click', () => nextShot(1));
dialog.addEventListener('keydown', event => {
  if (event.key === 'Tab') {
    const controls = [...dialog.querySelectorAll('button')];
    const index = controls.indexOf(document.activeElement);
    event.preventDefault();
    controls[(index + (event.shiftKey ? -1 : 1) + controls.length) % controls.length].focus();
  }
  if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
    event.preventDefault();
    nextShot(event.key === 'ArrowRight' ? 1 : -1);
  }
});
dialog.addEventListener('click', event => {
  if (event.target !== dialog) return;
  const rect = dialog.getBoundingClientRect();
  if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) dialog.close();
});
dialog.addEventListener('close', () => {
  document.body.style.overflow = savedOverflow;
  document.body.style.paddingRight = savedPadding;
  picture.removeAttribute('src');
  origin?.focus({preventScroll:true});
  if (savedScroll) window.scrollTo(...savedScroll);
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape' && !dialog.open && menu.getAttribute('aria-expanded') === 'true') closeMenu(true);
});
const chapterLinks = [...document.querySelectorAll('[data-chapter]')];
let frame;
function updateChapter() {
  frame = undefined;
  const threshold = document.querySelector('.site-header').offsetHeight + document.querySelector('.chapters').offsetHeight + 24;
  let active;
  for (const link of chapterLinks) if (document.getElementById(link.dataset.chapter).getBoundingClientRect().top <= threshold) active = link;
  chapterLinks.forEach(link => {
    if (link === active) link.setAttribute('aria-current', 'location');
    else link.removeAttribute('aria-current');
  });
}
function queueChapter() { if (frame === undefined) frame = requestAnimationFrame(updateChapter); }
window.addEventListener('scroll', queueChapter, {passive:true});
window.addEventListener('resize', queueChapter);
window.addEventListener('popstate', () => {
  language = new URL(location.href).searchParams.get('lang') === 'fr' ? 'fr' : 'en';
  localize();
});
// Earlier public manual links used the domain-root fragment. Keep them meaningful
// if this artifact is later installed at that root, without claiming a deployment.
if (/^#\/(en|fr)\//.test(location.hash)) {
  const url = new URL('./manual/Documentation.html', location.href);
  url.search = location.search;
  url.hash = location.hash;
  location.replace(url.href);
} else {
  const requested = new URL(location.href).searchParams.get('lang');
  if (requested !== null && !['en', 'fr'].includes(requested)) setLanguage('en');
  else localize();
  queueChapter();
}

/* Progressive enhancement only: content, navigation and guides are static HTML. */
(() => {
  'use strict';
  const page = document.documentElement;
  const locale = page.lang;
  const french = locale === 'fr';
  const strings = {
    close: french ? 'Fermer' : 'Close',
    copied: french ? 'Copié' : 'Copied',
    copyFailed: french ? 'Sélectionnez le code pour le copier' : 'Select the code to copy it',
    count: french ? ['module affiché', 'modules affichés'] : ['module shown', 'modules shown'],
  };
  const languageLinks = [...document.querySelectorAll('[data-language]')];
  const requested = new URL(location.href).searchParams.get('lang');
  if ((requested === 'en' || requested === 'fr') && requested !== locale) {
    const destination = languageLinks.find(link => link.dataset.language === requested);
    if (destination) {
      location.replace(destination.getAttribute('href') + location.hash);
      return;
    }
  }
  function preserveChapter() {
    languageLinks.forEach(link => {
      const destination = new URL(link.href, location.href);
      destination.hash = location.hash;
      link.href = destination.href;
    });
  }
  preserveChapter();
  addEventListener('hashchange', preserveChapter);

  const menuButton = document.querySelector('.menu-toggle');
  const menu = document.querySelector('#main-nav');
  const closeMenu = () => {
    menu?.classList.remove('is-open');
    menuButton?.setAttribute('aria-expanded', 'false');
  };
  menuButton?.addEventListener('click', () => {
    const open = menuButton.getAttribute('aria-expanded') !== 'true';
    menuButton.setAttribute('aria-expanded', String(open));
    menu.classList.toggle('is-open', open);
  });
  menu?.addEventListener('click', event => { if (event.target.closest('a')) closeMenu(); });
  document.addEventListener('click', event => {
    if (!event.target.closest('.site-header')) closeMenu();
  });
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && menuButton?.getAttribute('aria-expanded') === 'true') {
      closeMenu(); menuButton.focus();
    }
  });

  const dialog = document.querySelector('#image-dialog');
  const dialogImage = dialog?.querySelector('.lightbox-image');
  const dialogCaption = dialog?.querySelector('#image-caption');
  const dialogCounter = dialog?.querySelector('.lightbox-counter');
  const shots = [...document.querySelectorAll('[data-lightbox]')];
  let activeShots = shots;
  let active = 0;
  let opener;
  function showShot(index) {
    active = (index + activeShots.length) % activeShots.length;
    const image = activeShots[active].querySelector('img');
    dialogImage.src = image.currentSrc || image.src;
    dialogImage.alt = image.alt;
    dialogCaption.textContent = image.alt;
    dialogCounter.textContent = `${active + 1} / ${activeShots.length}`;
  }
  shots.forEach(button => button.addEventListener('click', () => {
    if (!dialog || typeof dialog.showModal !== 'function') {
      window.open(button.querySelector('img').src, '_blank', 'noopener');
      return;
    }
    opener = button;
    activeShots = shots.filter(shot => !shot.closest('[hidden]'));
    showShot(activeShots.indexOf(button));
    dialog.showModal();
    document.body.classList.add('modal-open');
    dialog.querySelector('.lightbox-close').focus();
  }));

  // Complete static panels remain readable without JavaScript. With it, expose
  // the same examples as a keyboard-operable tab set.
  const workspaceTabs = [...document.querySelectorAll('[data-workspace-tab]')];
  const workspacePanels = [...document.querySelectorAll('[data-workspace-panel]')];
  const selectWorkspace = (key, focus = false) => {
    workspaceTabs.forEach(tab => {
      const selected = tab.dataset.workspaceTab === key;
      tab.setAttribute('aria-selected', String(selected));
      tab.tabIndex = selected ? 0 : -1;
      if (selected && focus) tab.focus();
    });
    workspacePanels.forEach(panel => {
      panel.hidden = panel.dataset.workspacePanel !== key;
      panel.setAttribute('role', 'tabpanel');
      panel.setAttribute('aria-labelledby', `workspace-tab-${panel.dataset.workspacePanel}`);
    });
  };
  workspaceTabs.forEach((tab, index) => {
    tab.addEventListener('click', () => selectWorkspace(tab.dataset.workspaceTab));
    tab.addEventListener('keydown', event => {
      let next;
      if (event.key === 'ArrowRight') next = (index + 1) % workspaceTabs.length;
      if (event.key === 'ArrowLeft') next = (index + workspaceTabs.length - 1) % workspaceTabs.length;
      if (event.key === 'Home') next = 0;
      if (event.key === 'End') next = workspaceTabs.length - 1;
      if (next !== undefined) {
        event.preventDefault();
        selectWorkspace(workspaceTabs[next].dataset.workspaceTab, true);
      }
    });
  });
  if (workspaceTabs.length) {
    document.querySelector('.workspace-tabs').hidden = false;
    selectWorkspace(workspaceTabs[0].dataset.workspaceTab);
  }
  dialog?.querySelector('.lightbox-close').addEventListener('click', () => dialog.close());
  dialog?.querySelector('[data-previous]').addEventListener('click', () => showShot(active - 1));
  dialog?.querySelector('[data-next]').addEventListener('click', () => showShot(active + 1));
  dialog?.addEventListener('click', event => {
    if (event.target === dialog || event.target.classList.contains('lightbox-panel')) dialog.close();
  });
  dialog?.addEventListener('keydown', event => {
    if (event.key === 'ArrowLeft') { event.preventDefault(); showShot(active - 1); }
    if (event.key === 'ArrowRight') { event.preventDefault(); showShot(active + 1); }
  });
  dialog?.addEventListener('close', () => {
    document.body.classList.remove('modal-open');
    opener?.focus({ preventScroll: true });
  });

  document.querySelectorAll('[data-theme-image]').forEach(button => button.addEventListener('click', () => {
    const image = document.querySelector('#theme-preview');
    if (!image) return;
    image.src = button.dataset.themeImage;
    image.alt = button.dataset.themeAlt;
    document.querySelector('#theme-caption').textContent = button.dataset.themeAlt;
    image.closest('button').setAttribute('aria-label', `${french ? 'Agrandir' : 'Enlarge'}: ${image.alt}`);
    document.querySelectorAll('[data-theme-image]').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
  }));

  const search = document.querySelector('#module-query');
  const cards = [...document.querySelectorAll('.module-card')];
  const familyButtons = [...document.querySelectorAll('[data-module-family]')];
  let family = 'all';
  const normalize = value => value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase();
  const filterModules = () => {
    const terms = normalize(search.value).trim().split(/\s+/).filter(Boolean);
    let count = 0;
    cards.forEach(card => {
      const text = normalize(card.textContent);
      const matches = (family === 'all' || card.dataset.family === family) && terms.every(term => text.includes(term));
      card.hidden = !matches;
      if (matches) count++;
    });
    document.querySelector('#module-count').textContent = `${count} ${strings.count[count === 1 ? 0 : 1]}`;
    document.querySelector('#module-empty').hidden = count !== 0;
    document.querySelector('.module-grid').hidden = count === 0;
  };
  search?.addEventListener('input', filterModules);
  search?.addEventListener('keydown', event => {
    if (event.key === 'Escape') { search.value = ''; filterModules(); }
  });
  document.querySelector('[data-clear-search]')?.addEventListener('click', () => {
    search.value = ''; filterModules(); search.focus();
  });
  familyButtons.forEach(button => button.addEventListener('click', () => {
    family = button.dataset.moduleFamily;
    familyButtons.forEach(item => item.setAttribute('aria-pressed', String(item === button)));
    filterModules();
  }));

  document.querySelectorAll('.copy-code').forEach(button => button.addEventListener('click', async () => {
    const original = button.textContent;
    const code = button.closest('.code-block').querySelector('code');
    try {
      await navigator.clipboard.writeText(code.textContent);
      button.textContent = strings.copied;
    } catch {
      const selection = getSelection();
      const range = document.createRange();
      range.selectNodeContents(code);
      selection.removeAllRanges(); selection.addRange(range);
      button.textContent = strings.copyFailed;
    }
    setTimeout(() => { button.textContent = original; }, 2400);
  }));

  if ('IntersectionObserver' in window) {
    const links = [...document.querySelectorAll('.chapter-inner a')];
    const sections = [...document.querySelectorAll('section[data-chapter]')];
    const observer = new IntersectionObserver(() => {
      const marker = innerHeight * .25;
      const current = sections.filter(section => section.getBoundingClientRect().top <= marker).pop();
      links.forEach(link => {
        if (current && link.hash === `#${current.id}`) link.setAttribute('aria-current', 'location');
        else link.removeAttribute('aria-current');
      });
    }, { rootMargin: '-25% 0px -74% 0px', threshold: 0 });
    sections.forEach(section => observer.observe(section));
  }
})();

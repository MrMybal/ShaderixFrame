"""Build/check the self-contained bilingual GitHub Pages showcase.

No web framework or network dependency is required. CSS/JS content hashes are
embedded in every page's asset URLs. Run after editing those resources.
"""
import argparse
import hashlib
from html import escape
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
REPO = "https://github.com/MrMybal/CyUI"
RELEASE = REPO + "/releases/tag/v1.2.0"
ZIP = REPO + "/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip"
MODULES = (
    ("Core", "core", "Themes, widgets, localization, layout and quick filters.", "Thèmes, widgets, localisation, disposition et filtres rapides.", "Dear ImGui"),
    ("Audio", "data", "Knobs, faders and peak/RMS meters.", "Potentiomètres, faders et indicateurs de crête/RMS.", "Core"),
    ("Canvas", "data", "Grid, clipping, double-precision coordinates, pan and zoom.", "Grille, découpage, coordonnées double précision, déplacement et zoom.", "Core"),
    ("Plots", "data", "Linear/log plots, series, histograms and heatmaps.", "Graphiques linéaires/logarithmiques, séries, histogrammes et cartes de chaleur.", "Canvas"),
    ("Curves", "data", "Editable keyframes, Bezier handles and snapping.", "Images clés éditables, poignées de Bézier et alignement.", "Plots"),
    ("Nodes", "editor", "Connected cards, pins, selection and graph navigation.", "Cartes reliées, ports, sélection et navigation dans les graphes.", "Canvas"),
    ("Timeline", "data", "Clip lanes, trimming, markers and time ranges.", "Pistes de clips, ajustement des bords, marqueurs et plages temporelles.", "Canvas"),
    ("AudioViews", "data", "Waveforms, filled/band spectra, scrolling spectrograms, spatial/persistent scopes, meters, EQ and dynamics.", "Formes d’onde, spectres remplis/par bandes, spectrogrammes défilants, vues spatiales/persistantes, niveaux, EQ et dynamique.", "Plots"),
    ("Code", "editor", "Shared syntax catalogue, annotations and source views.", "Catalogue syntaxique partagé, annotations et vues du code source.", "Core + Lexilla"),
    ("CodeEditor", "editor", "Native text editing, Undo/Redo and completion hooks.", "Édition de texte native, annulation/rétablissement et points d’extension de complétion.", "Code + TextEditor · C++20"),
    ("Diff", "editor", "Unified/split comparisons and navigation between changes.", "Comparaisons unifiées ou côte à côte et navigation entre les changements.", "Code"),
    ("Inspector", "editor", "Typed properties, validation and draft edits.", "Propriétés typées, validation et brouillons de modification.", "Core"),
    ("Assets", "editor", "Folder trees, grids/lists, thumbnails, search and drag/drop.", "Arborescences, grilles/listes, vignettes, recherche et glisser-déposer.", "Core"),
    ("Images", "media", "Image navigation, checkerboard, crop and gradient stops.", "Navigation dans les images, damier, recadrage et points de dégradé.", "Canvas"),
    ("Viewport", "media", "Host-rendered surfaces, orbit camera and 2D transforms.", "Surfaces rendues par l’application, caméra orbitale et transformations 2D.", "Images"),
    ("Commands", "core", "Searchable keyboard command palette.", "Palette de commandes au clavier avec recherche.", "Core"),
    ("Diagnostics", "core", "Bounded logs, filtering and diagnostic navigation.", "Journaux à taille limitée, filtrage et navigation dans les diagnostics.", "Core"),
    ("DocumentViews", "media", "Native Markdown/GFM, images, spoilers, tasks, tables and transcripts.", "Markdown/GFM natif, images, spoilers, tâches, tableaux et conversations.", "Code, Images"),
    ("DocumentEditor", "media", "Live/source/split/reading editor.", "Éditeur Direct/Source/Côte à côte/Lecture.", "DocumentViews, CodeEditor"),
    ("DocumentIO", "media", "Markdown/CyMD buffers, HTML clipboard.", "Tampons Markdown/CyMD, presse-papiers HTML.", "DocumentViews, miniz"),
    ("DocumentWeb", "media", "Optional existing web backend adapter.", "Adaptateur optionnel vers le moteur web existant.", "DocumentViews, Web"),
    ("Media", "media", "Native playback controls with a host engine.", "Contrôles de lecture natifs avec moteur hôte.", "Images"),
    ("Web", "integration", "Browser surface, messages, origin policy and theme bridge.", "Surface web, messages, politique d’origine et passerelle de thème.", "Core · host backend"),
    ("DataGrid", "editor", "Editable cells, ranges, clipboard and row/column structure.", "Cellules éditables, plages, presse-papiers et lignes/colonnes.", "Core"),
    ("Spreadsheet", "editor", "Worksheets, formula bar and bounded calculation.", "Feuilles, barre de formule et calcul borné.", "DataGrid"),
    ("RichDocument", "editor", "Paginated native documents, tables and page settings.", "Documents natifs paginés, tableaux et mise en page.", "DocumentEditor, DataGrid"),
    ("OfficeIO", "editor", "ODT/ODS, DOCX/XLSX and flat OpenDocument exchange.", "Échanges ODT/ODS, DOCX/XLSX et OpenDocument plat.", "RichDocument, Spreadsheet, DocumentIO, pugixml"),
    ("Gallery", "core", "Interactive widget/module examples and theme editor.", "Exemples interactifs des widgets/modules et éditeur de thème.", "Core + selected modules"),
)
CMAKE = '''set(CYUI_IMGUI_TARGET app_imgui)
set(CYUI_BUILD_DEMO OFF)
set(CYUI_BUILD_AUDIO OFF)
set(CYUI_BUILD_GALLERY OFF)
set(CYUI_MODULES "Nodes;Inspector")
add_subdirectory(external/CyUI)

target_link_libraries(app PRIVATE
    CyUI::Nodes CyUI::Inspector)'''
CPP = '''#include <cyui/Core.h>

// Once, after ImGui::CreateContext():
cyui::Config config;
config.themeKey = "mid-grey";
cyui::apply(config);
cyui::loadFonts();

// During an ImGui frame:
if (cyui::primaryButton(cyui::trLabel("Save").c_str())) {
    saveDocument();
}
cyui::renderNotifications();'''
PYTHON = '''python -m pip install -e .'''
PYTHON_UI = '''import cyui as c
from cyui_modules import plots, inspector

c.set_locale("fr")
# Draw inside your application's ImGui frame.
if c.primary_button(c.tr_label("Save")):
    save_document()'''
UNREAL = '''python tools/package_unreal.py --layout module --output build/unreal-module/CyUI --modules Core Plots Inspector'''

DOC_ROUTES = {
    "start": "integration", "overview": "overview", "modules": "modules",
    "editors": "module-recipes/inspector-and-assets",
    "graphs": "module-recipes/curves-graphs-and-timeline",
    "data": "module-recipes/canvas-and-numeric-plots",
    "audio": "module-recipes/audio-controls-and-prepared-signal-views",
    "code": "code", "diff": "diff", "widgets": "widgets",
    "commands": "module-recipes/commands-logs-and-documents",
    "media": "module-recipes/images-viewport-and-optional-3d-gizmo",
    "filtering": "filtering", "localization": "localization", "design": "design",
    "python": "python", "web": "web", "unreal": "unreal",
    "office-grid": "office/grid", "office-sheet": "office/spreadsheet", "office-writer": "office/writer", "office-formats": "office/formats", "markdown": "native-markdown", "markdown-editor": "native-markdown/editor", "playback": "native-markdown/resources",
    "platforms": "overview/platforms", "tour": "visual-tour",
}
MODULE_DOCS = {
    "Core": "start", "Audio": "audio", "Canvas": "data", "Plots": "data",
    "Curves": "graphs", "Nodes": "graphs", "Timeline": "graphs",
    "AudioViews": "audio", "Code": "code", "CodeEditor": "code", "Diff": "diff",
    "Inspector": "editors", "Assets": "editors", "Images": "media",
    "Viewport": "media", "Commands": "commands", "Diagnostics": "commands",
    "DocumentViews": "markdown", "DocumentEditor": "markdown-editor", "DocumentIO": "markdown",
    "DocumentWeb": "markdown", "Media": "playback", "Web": "web", "Gallery": "widgets", "DataGrid": "office-grid", "Spreadsheet": "office-sheet", "RichDocument": "office-writer", "OfficeIO": "office-formats",
}
WORKSPACES = (
    ("visual", "Visual editors", "Éditeurs visuels", "image-graph",
     "Give connections a clear shape.", "Donnez une forme claire aux connexions.",
     "Cards, typed pins and image previews make a common foundation for node-based tools. Your application supplies textures, connection rules and graph execution.",
     "Cartes, ports typés et aperçus d’images forment une base commune pour les outils à nœuds. Votre application fournit textures, règles de connexion et exécution du graphe.",
     ("Nodes", "Images", "Inspector"), "graphs"),
    ("audio", "Audio & analysis", "Audio et analyse", "audio",
     "Turn prepared signals into useful views.", "Transformez vos signaux en vues utiles.",
     "Combine waveforms and spectra with editable automation and clip lanes. Keep decoding, FFTs, transport and Undo in your audio application.",
     "Associez formes d’onde et spectres aux courbes d’automation et pistes de clips. Gardez décodage, FFT, transport et annulation dans votre application audio.",
     ("Audio", "AudioViews", "Curves", "Timeline"), "audio"),
    ("source", "Code & review", "Code et revue", "code",
     "Read, edit and compare with shared colors.", "Lisez, éditez et comparez avec les mêmes couleurs.",
     "Use the native C++ editor, aligned diffs and navigable diagnostics together. Repository actions and language services stay with your host. CodeEditor requires C++20.",
     "Réunissez éditeur C++ natif, différences alignées et diagnostics navigables. L’application conserve les actions du dépôt et les services de langage. CodeEditor demande C++20.",
     ("CodeEditor", "Diff", "Diagnostics", "Commands"), "code"),
    ("assets", "Assets & scenes", "Ressources et scènes", "assets",
     "Keep resources and their properties together.", "Réunissez ressources et propriétés.",
     "Browse folders and thumbnails, inspect typed properties and display a host-rendered scene. Your engine owns files, asset loading, rendering and validation.",
     "Parcourez dossiers et vignettes, inspectez des propriétés typées et affichez une scène rendue par l’application. Votre moteur possède fichiers, chargement, rendu et validation.",
     ("Assets", "Inspector", "Images", "Viewport"), "editors"),
)


def e(value):
    return escape(str(value), quote=True)


class Page:
    def __init__(self, language, guide=False):
        self.language = language
        self.fr = language == "fr"
        self.guide = guide
        self.assets = "../assets/" if self.fr else "assets/"
        self.manifest = {item["file"]: item for item in json.loads(
            (SITE / "assets/screenshots/manifest.json").read_text(encoding="utf-8"))["images"]}
        self.manual_manifest = {item["locale"]: item for item in json.loads(
            (SITE / "assets/manual/manifest.json").read_text(encoding="utf-8"))["images"]}

    def t(self, en, fr):
        return fr if self.fr else en

    def doc(self, anchor="start"):
        return ("../" if self.fr else "") + f"Documentation/Documentation.html#/{self.language}/" + DOC_ROUTES[anchor]

    def link(self, label, href, cls="text-link"):
        return f'<a class="{e(cls)}" href="{e(href)}">{e(label)} <span aria-hidden="true">↗</span></a>'

    def figure(self, name, en, fr, hero=False, image_id=None):
        alt = self.t(en, fr)
        info = self.manifest[name + ".webp"]
        width, height = info["size"]
        identity = f' id="{image_id}"' if image_id else ""
        priority = 'fetchpriority="high" loading="eager"' if hero else 'loading="lazy" decoding="async"'
        return f'''<figure class="{'hero-frame' if hero else 'screenshot'}">
<button class="shot-button" type="button" data-lightbox aria-label="{e(self.t('Enlarge screenshot: ', 'Agrandir la capture : ') + alt)}">
<img{identity} src="{self.assets}screenshots/{name}.webp" width="{width}" height="{height}" alt="{e(alt)}" {priority}></button>
<figcaption><span{' id="theme-caption"' if image_id == 'theme-preview' else ''}>{e(alt)}</span><span class="enlarge" aria-hidden="true">{self.t('View full size', 'Agrandir')} ↗</span></figcaption></figure>'''

    def block(self, label, code):
        return f'''<div class="code-block"><div class="code-toolbar"><span>{e(label)}</span><button type="button" class="copy-code" aria-live="polite">{self.t('Copy', 'Copier')}</button></div><pre><code>{e(code)}</code></pre></div>'''

    def header(self):
        main = "index.html" if self.guide else ""
        filename = "guide.html" if self.guide else "index.html"
        en = "../" + filename if self.fr else filename
        fr = filename if self.fr else "fr/" + filename
        gh = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 .8a11.2 11.2 0 0 0-3.54 21.82c.56.1.77-.24.77-.54v-2.1c-3.13.68-3.8-1.33-3.8-1.33-.5-1.3-1.25-1.65-1.25-1.65-1.02-.7.08-.68.08-.68 1.13.08 1.72 1.16 1.72 1.16 1 1.72 2.63 1.22 3.27.93.1-.73.4-1.22.71-1.5-2.5-.29-5.13-1.25-5.13-5.57 0-1.23.44-2.24 1.16-3.03-.12-.28-.5-1.43.1-2.99 0 0 .95-.3 3.08 1.15a10.7 10.7 0 0 1 5.6 0c2.13-1.45 3.07-1.15 3.07-1.15.62 1.56.23 2.71.12 3a4.4 4.4 0 0 1 1.15 3.02c0 4.33-2.63 5.28-5.14 5.56.4.35.76 1.04.76 2.1v3.08c0 .3.2.65.77.54A11.2 11.2 0 0 0 12 .8Z"/></svg>'
        return f'''<a class="skip-link" href="#main">{self.t('Skip to content', 'Aller au contenu')}</a>
<header class="site-header" id="top"><div class="wrap header-inner">
<a class="brand" href="index.html" aria-label="{self.t('CyUI home', 'Accueil CyUI')}"><img src="{self.assets}mark.svg" width="34" height="34" alt=""><span>Cy<span>UI</span></span></a>
<nav class="header-nav" id="main-nav" aria-label="{self.t('Main navigation', 'Navigation principale')}">
<a href="{main}#explore">{self.t('Features', 'Fonctionnalités')}</a><a href="{main}#modules">Modules</a><a href="{main}#integrations">{self.t('Integrations', 'Intégrations')}</a><a href="{self.doc('overview')}">{self.t('Documentation', 'Documentation')}</a><a href="{main}#download">{self.t('Downloads', 'Téléchargements')}</a></nav>
<div class="header-actions"><nav class="language" aria-label="{self.t('Language', 'Langue')}"><a href="{en}" data-language="en" lang="en" hreflang="en" {'aria-current="page"' if not self.fr else ''} aria-label="English">EN</a><a href="{fr}" data-language="fr" lang="fr" hreflang="fr" {'aria-current="page"' if self.fr else ''} aria-label="Français">FR</a></nav>
<a class="button github small" href="{REPO}">{gh}<span>GitHub</span><span aria-hidden="true">↗</span></a>
<a class="button primary small" href="{main}#download">{self.t('Try the demo', 'Essayer la démo')} <span aria-hidden="true">↓</span></a></div>
<button class="menu-toggle" type="button" aria-label="{self.t('Toggle navigation', 'Ouvrir ou fermer la navigation')}" aria-controls="main-nav" aria-expanded="false"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 7h16M4 12h16M4 17h16"/></svg></button></div></header>'''

    def footer(self):
        return f'''<footer class="site-footer"><div class="wrap"><div class="footer-inner"><a class="brand" href="index.html"><img src="{self.assets}mark.svg" width="26" height="26" alt=""><span>Cy<span>UI</span></span></a><nav class="footer-links" aria-label="{self.t('Footer', 'Pied de page')}"><a href="{self.doc('overview')}">Documentation</a><a href="{RELEASE}">{self.t('Releases', 'Versions')}</a><a href="{REPO}">GitHub</a><a href="{REPO}/blob/main/LICENSE">MIT</a><a href="#top">{self.t('Back to top', 'Retour en haut')} ↑</a></nav></div><p class="footer-note"><span>© 2026 CyUI · Cyberalien</span><span>{self.t('Built on Dear ImGui. CyUI is MIT licensed; dependencies retain their own notices.', 'Basé sur Dear ImGui. CyUI est sous licence MIT ; les dépendances conservent leurs propres mentions.')}</span></p></div></footer>'''

    def dialog(self):
        return f'''<dialog class="lightbox" id="image-dialog" aria-labelledby="image-caption"><div class="lightbox-panel"><div class="lightbox-toolbar"><span id="image-caption"></span><button class="lightbox-close" type="button" aria-label="{self.t('Close image', 'Fermer l’image')}">×</button></div><img class="lightbox-image" alt=""><div class="lightbox-bottom"><button class="lightbox-nav" type="button" data-previous aria-label="{self.t('Previous screenshot', 'Capture précédente')}">←</button><span class="lightbox-counter" aria-live="polite"></span><button class="lightbox-nav" type="button" data-next aria-label="{self.t('Next screenshot', 'Capture suivante')}">→</button><span>{self.t('Esc to close', 'Échap pour fermer')}</span></div></div></dialog>'''

    def section_head(self, number, label, en_title, fr_title, en_text, fr_text):
        return f'''<div class="section-head"><div><p class="eyebrow">{number} / {e(label)}</p><h2>{e(self.t(en_title, fr_title))}</h2></div><p>{e(self.t(en_text, fr_text))}</p></div>'''

    def image_card(self, name, en_alt, fr_alt, en_title, fr_title, en_text, fr_text, anchor):
        anchor = {"commands": "commands", "diagnostics": "commands", "documents": "markdown",
                  "nodes": "graphs", "curves": "graphs", "timeline": "graphs"}.get(name, anchor)
        return f'''<article class="image-card">{self.figure(name, en_alt, fr_alt)}<h3>{e(self.t(en_title,fr_title))}</h3><p>{e(self.t(en_text,fr_text))}</p>{self.link(self.t('Explore the guide','Découvrir le guide'), self.doc(anchor))}</article>'''

    def hero(self):
        t = self.t
        return f'''<main id="main"><section class="hero" aria-labelledby="hero-title"><div class="wrap hero-layout">
<div class="hero-copy"><p class="eyebrow">{t('A modular Dear ImGui library', 'Bibliothèque modulaire Dear ImGui')}</p><h1 id="hero-title">Cy<span>UI</span></h1>
<p class="hero-lead">{t('A shared interface for the tools you build. Themed widgets, visual editors and optional modules for C++ and Python.', 'Une interface commune pour les outils que vous créez. Widgets thématisés, éditeurs visuels et modules au choix pour C++ et Python.')}</p>
<div class="hero-cta"><a class="button primary" href="#download">↓ {t('Try the Windows demo', 'Essayer la démo Windows')}</a><a class="button" href="#explore">{t('Explore the modules', 'Découvrir les modules')} ↘</a></div>
<p class="hero-meta">MIT · C++ / Python<br>{t('Active development · Demo 1.2.0', 'En développement · Démo 1.2.0')}</p></div>
<div class="hero-media">{self.figure('overview-fr' if self.fr else 'overview', 'CyUI demo — complete workspace, gallery, controls and inspector', 'Démo CyUI — espace complet, galerie, contrôles et inspecteur', True)}</div>
</div></section>'''

    def workspace_explorer(self):
        t = self.t
        tabs, panels = [], []
        for index, (key, en, fr, image, title_en, title_fr, text_en, text_fr, modules, route) in enumerate(WORKSPACES):
            tabs.append(f'<button type="button" id="workspace-tab-{key}" data-workspace-tab="{key}" role="tab" aria-controls="workspace-{key}" aria-selected="{"true" if index == 0 else "false"}" tabindex="{0 if index == 0 else -1}">{e(t(en, fr))}</button>')
            chips = ''.join(f'<a href="{self.doc(MODULE_DOCS[name])}"><code>CyUI::{name}</code></a>' for name in modules)
            panels.append(f'''<section class="workspace-panel" id="workspace-{key}" data-workspace-panel="{key}" aria-labelledby="workspace-title-{key}"><div class="workspace-copy"><p class="eyebrow">{e(t(en, fr))}</p><h3 id="workspace-title-{key}">{e(t(title_en, title_fr))}</h3><p>{e(t(text_en, text_fr))}</p><div class="module-chips" aria-label="{t('Example module selection', 'Exemple de sélection de modules')}">{chips}</div><p class="workspace-note">{t('Core and required CyUI dependencies are linked automatically.', 'Core et les dépendances CyUI nécessaires sont liés automatiquement.')}</p>{self.link(t('Read the integration recipe', 'Lire la recette d’intégration'), self.doc(route))}</div>{self.figure(image, en + ' — native CyUI gallery example', fr + ' — exemple natif de la galerie CyUI')}</section>''')
        return f'''<section class="section wrap workspace-explorer" id="explore" data-chapter>{self.section_head('↳', t('Choose your building blocks', 'Choisissez vos composants'), 'What are you building?', 'Quel outil créez-vous ?', 'Explore useful module combinations, then keep only the parts your application needs. These are examples of the library, with synthetic data.', 'Explorez des combinaisons de modules, puis gardez les composants utiles à votre application. Ces exemples de la bibliothèque utilisent des données synthétiques.')}<div class="workspace-tabs" role="tablist" aria-label="{t('Example workspaces', 'Exemples d’interfaces')}" hidden>{''.join(tabs)}</div><div class="workspace-panels">{''.join(panels)}</div></section>'''

    def documentation_section(self):
        t = self.t
        locale = self.language
        width, height = self.manual_manifest[locale]["size"]
        alt = t('The actual CyDoc reader — CyUI manual, navigation and illustrated guides in English', 'Le véritable lecteur CyDoc — manuel CyUI, navigation et guides illustrés en français')
        return f'''<section class="section documentation-section" id="documentation" data-chapter><div class="wrap"><div class="split"><div class="copy"><p class="eyebrow">09 / CYDOC</p><h2>{t('The guide travels with your tools.', 'Le guide accompagne vos outils.')}</h2><p>{t('Integration recipes, public API references and illustrated module guides in a real documentation reader. English by default, with a complete French edition.', 'Recettes d’intégration, références API publiques et guides illustrés des modules dans un véritable lecteur de documentation. L’anglais par défaut, avec une édition française complète.')}</p><ul class="feature-list"><li>{t('Search text, code and documented symbols', 'Recherchez dans les textes, le code et les symboles documentés')}</li><li>{t('Stable page, section and media links', 'Liens stables vers les pages, sections et médias')}</li><li>{t('Zoom inside the reader, with keyboard controls', 'Agrandissement dans le lecteur, avec commandes au clavier')}</li></ul>{self.link(t('Open the manual', 'Ouvrir le manuel'), self.doc('overview'), 'button primary')}<p class="note">{t('Current source builds include Documentation/Documentation.html and all its resources. Open it directly from disk; no server, CDN or connection is required to read it.', 'Les builds actuels incluent Documentation/Documentation.html et toutes ses ressources. Ouvrez-le depuis le disque : aucun serveur, CDN ou accès Internet n’est requis pour le lire.')}</p></div><figure class="screenshot documentation-preview"><button class="shot-button" type="button" data-lightbox aria-label="{e(t('Enlarge screenshot: ', 'Agrandir la capture : ') + alt)}"><img src="{self.assets}manual/reader-{locale}.webp" width="{width}" height="{height}" loading="lazy" decoding="async" alt="{e(alt)}"></button><figcaption><span>{e(alt)}</span><span class="enlarge" aria-hidden="true">{t('View full size', 'Agrandir')} ↗</span></figcaption></figure></div><div class="manual-links">{self.link(t('Get started', 'Premiers pas'), self.doc('start'))}{self.link(t('Module reference', 'Référence des modules'), self.doc('modules'))}{self.link(t('Localization', 'Localisation'), self.doc('localization'))}{self.link(t('Illustrated tour', 'Visite illustrée'), self.doc('tour'))}</div></div></section>'''

    def main_content(self):
        t = self.t
        chapters = [("explore",t("Explore","Découvrir")),("foundation",t("Foundation","Fondation")),("editors",t("Editor tools","Outils d’édition")),("graphs",t("Graphs","Graphes")),("audio",t("Audio & data","Audio et données")),("code","Code"),("media",t("Media","Médias")),("modules","Modules"),("integrations",t("Integrations","Intégrations")),("documentation","CyDoc"),("download",t("Get started","Commencer"))]
        parts = [self.hero() + f'''<nav class="chapter-bar" aria-label="{t('Page chapters','Chapitres de la page')}"><div class="wrap chapter-inner">''' + "".join(f'<a href="#{key}">{label}</a>' for i,(key,label) in enumerate(chapters,1)) + f'''</div></nav><div class="wrap intro-line"><p>{t('For developers building editors, creative software and technical tools. Share the interface components across your applications; keep each application’s own engine and data.','Pour les développeurs d’éditeurs, de logiciels créatifs et d’outils techniques. Partagez les composants d’interface entre vos applications, tout en gardant leurs moteurs et leurs données propres.')}</p><div class="tech-line"><span>C++17 / C++20</span><span>Python 3.10+</span><span>Dear ImGui ≥ 1.92</span></div></div>''']
        parts.append(self.workspace_explorer())
        themes = [("overview","Mid grey","Gris moyen",""),("theme-light","Light grey","Gris clair","swatch-light"),("theme-amber","Amber night","Nuit ambrée","swatch-amber"),("theme-violet","Violet","Violet","swatch-violet"),("theme-navy","Navy","Bleu nuit","swatch-navy")]
        controls = ''.join(f'''<button type="button" class="theme-button {cls}" data-theme-image="{self.assets}screenshots/{name}.webp" data-theme-alt="{e(t('CyUI gallery — '+en, 'Galerie CyUI — '+fr))}" aria-pressed="{'true' if i==0 else 'false'}"><span class="theme-dot" aria-hidden="true"></span>{e(t(en,fr))}</button>''' for i,(name,en,fr,cls) in enumerate(themes))
        parts.append(f'''<section class="section wrap" id="foundation" data-chapter>{self.section_head('01',t('A shared visual language','Un langage visuel commun'),'Start with a consistent foundation.','Commencez avec une base cohérente.','Buttons, fields, cards and layouts belong together. Five themes, two densities and semantic color roles carry the same visual language into every module.','Boutons, champs, cartes et dispositions forment un ensemble. Cinq thèmes, deux densités et des rôles de couleurs sémantiques unifient tous les modules.')}<div class="theme-controls" role="group" aria-label="{t('Preview a built-in theme','Prévisualiser un thème intégré')}">{controls}</div>{self.figure('overview','CyUI gallery — Mid grey','Galerie CyUI — Gris moyen',image_id='theme-preview')}<div class="triad"><article><h3>{t('A complete everyday toolkit','Les outils du quotidien')}</h3><p>{t('Forms, tables, navigation, status messages and notifications share the active theme. Ordinary Dear ImGui widgets continue to work alongside CyUI.','Formulaires, tableaux, navigation, états et notifications partagent le thème actif. Les widgets Dear ImGui ordinaires fonctionnent aussi aux côtés de CyUI.')}</p></article><article><h3>{t('Made for dense workspaces','Pensé pour les espaces denses')}</h3><p>{t('Choose compact or comfortable density. Semantic sizing and per-context DPI/font configuration let the host adapt the interface to its displays.','Choisissez une densité compacte ou confortable. Les dimensions sémantiques et les réglages DPI/polices par contexte permettent à l’application d’adapter l’interface.')}</p></article><article><h3>{t('English first. French included.','Anglais par défaut. Français inclus.')}</h3><p>{t('Switch language while retaining widget identities. Extend the shared catalogue with your application’s translations and additional languages.','Changez de langue en conservant l’identité des widgets. Complétez le catalogue partagé avec les traductions et les langues de votre application.')}</p></article></div></section>''')
        parts.append(f'''<section class="section" id="editors" data-chapter><div class="wrap"><div class="split"><div class="copy"><p class="eyebrow">02 / {t('Editor tools','Outils d’édition')}</p><h2>{t('Give every asset a place.','Une place pour chaque ressource.')}</h2><p>{t('Build content browsers with real folder navigation, grid or list views, thumbnails and selection metadata. Connect the same resources to clear, typed details panels.','Construisez des navigateurs de contenu avec une arborescence, des vues en grille ou en liste, des vignettes et les métadonnées de sélection. Reliez les mêmes ressources à des panneaux de propriétés clairs et typés.')}</p><ul class="feature-list"><li>{t('Hierarchical navigation and drag/drop','Navigation hiérarchique et glisser-déposer')}</li><li>{t('Editable, grouped properties','Propriétés éditables et regroupées')}</li><li>{t('Optional search in menus and panels','Recherche optionnelle dans les menus et panneaux')}</li></ul>{self.link(t('Build an editor panel','Créer un panneau d’édition'),self.doc('editors'))}</div>{self.figure('assets','Content browser — folders, thumbnails and selected resource','Navigateur de contenu — dossiers, vignettes et ressource sélectionnée')}</div><div class="image-pair">{self.image_card('inspector-fr' if self.fr else 'inspector','Inspector — material preview and grouped properties','Inspecteur — aperçu de matériau et propriétés regroupées','Details without duplication.','Des propriétés sans duplication.','Strings, choices, toggles and numeric fields, with draft edits and validation. Your application owns the object, the preview and its Undo history.','Chaînes, choix, interrupteurs et champs numériques, avec brouillons et validation. Votre application reste propriétaire de l’objet, de l’aperçu et de l’historique d’annulation.','editors')}{self.image_card('filtering','Quick filtering — property section and nested export menu','Filtrage rapide — section de propriétés et menu d’export imbriqué','Find the control. Keep your focus.','Trouvez le réglage. Gardez le fil.','Search across nested actions or properties. Match English and translated captions, ignore Latin accents, and use Ctrl/Cmd+F or Escape in the focused view.','Cherchez parmi les actions imbriquées ou les propriétés. Retrouvez les libellés anglais ou traduits, sans les accents latins, avec Ctrl/Cmd+F ou Échap dans la vue active.','filtering')}</div><div class="image-pair">{self.image_card('commands','Searchable command palette with shortcuts','Palette de commandes avec recherche et raccourcis','Actions at your fingertips.','Les actions à portée de clavier.','A searchable command palette returns stable action IDs. The host decides which commands are enabled and performs the requested operation.','La palette de commandes renvoie des identifiants d’action stables. L’application décide quelles commandes sont disponibles et exécute l’opération demandée.','editors')}{self.image_card('diagnostics','Diagnostics — bounded log view and navigable messages','Diagnostics — journal à taille limitée et messages navigables','Read what your tools report.','Lisez ce que vos outils signalent.','Display bounded logs and diagnostic lists with severity, filtering and navigation. The example messages shown here are synthetic.','Affichez des journaux à taille limitée et des diagnostics avec niveaux, filtrage et navigation. Les messages présentés ici sont des exemples fictifs.','editors')}</div></div></section>''')
        parts.append(f'''<section class="section media-focus" id="graphs" data-chapter><div class="wrap">{self.section_head('03',t('Canvas / nodes / curves','Canvas / nœuds / courbes'),'Think in connections.','Pensez en connexions.','Compose visual editors from connected cards and pins. Navigation, selection, movement and layout share one canvas foundation; your application owns graph compatibility and execution.','Composez des éditeurs visuels avec des cartes reliées par des ports. Navigation, sélection, déplacement et disposition partagent une base de canvas ; l’application gère la compatibilité et l’exécution du graphe.')}{self.figure('image-graph','Image graph — source, color grade, blur and output previews','Graphe d’images — source, étalonnage, flou et aperçus de sortie')}<p class="note bordered">{t('The demo host prepares these synthetic textures. CyUI displays them inside the nodes; it does not execute image-processing or shader algorithms.','L’application de démonstration prépare ces textures fictives. CyUI les affiche dans les nœuds ; la bibliothèque n’exécute pas les algorithmes de traitement d’image ou les shaders.')}</p><div class="image-pair">{self.image_card('nodes','Blueprint-style graph — connected voice modules and colored pins','Graphe de type blueprint — modules de voix reliés et ports colorés','Visual logic, clearly laid out.','Une logique visuelle bien organisée.','Typed-looking pins, curved links and navigable cards give your graph a readable shape. Pin types, validation and evaluation are supplied by the host.','Ports différenciés, liaisons courbes et cartes navigables donnent une forme lisible à votre graphe. Les types de ports, la validation et l’évaluation viennent de l’application.','data')}{self.image_card('curves','Curve editor — keyframes and Bezier handles','Éditeur de courbes — images clés et poignées de Bézier','Make the movement editable.','Rendez le mouvement éditable.','Edit keyframes, handles and selection with snapping and explicit edit transactions. Connect the proposals to your application’s automation and Undo system.','Modifiez les images clés, poignées et sélections avec alignement et transactions d’édition explicites. Reliez ces propositions à l’automation et à l’historique d’annulation de l’application.','data')}</div></div></section>''')
        parts.append(f'''<section class="section" id="audio" data-chapter><div class="wrap"><div class="split reverse">{self.figure('audio','AudioViews — waveform, logarithmic spectrum, oscilloscope and stereo scope','AudioViews — forme d’onde, spectre logarithmique, oscilloscope et vue stéréo')}<div class="copy"><p class="eyebrow">04 / {t('Audio & data','Audio et données')}</p><h2>{t('Make your signals readable.','Rendez vos signaux lisibles.')}</h2><p>{t('Waveforms, spectra, oscilloscopes and stereo scopes sit alongside familiar knobs, faders and meters. Use the same visual system for creative audio tools and technical analysis.','Formes d’onde, spectres, oscilloscopes et vues stéréo côtoient potentiomètres, faders et indicateurs. Utilisez le même système visuel pour les outils audio créatifs et l’analyse technique.')}</p><p>{t('AudioViews draws data prepared by your application. Audio decoding, FFTs and signal processing remain in your engine.','AudioViews dessine les données préparées par votre application. Le décodage audio, les FFT et le traitement du signal restent dans votre moteur.')}</p>{self.link(t('Explore data views','Explorer les vues de données'),self.doc('audio'))}</div></div><div class="split"><div class="copy"><h3>{t('A shared audio analysis workspace.','Un espace commun pour l’analyse audio.')}</h3><p>{t('Inspect time and frequency with a spectrogram, compare LUFS histories, show true peaks and stereo correlation, edit EQ handles over gain/phase curves, and display dynamics with gain reduction. Available in the current sources and Audio analysis gallery tab.','Explorez temps et fréquences avec un spectrogramme, comparez les historiques LUFS, affichez crêtes vraies et corrélation stéréo, éditez les points EQ sur les courbes gain/phase et présentez la dynamique avec réduction de gain. Disponible dans les sources actuelles et l’onglet Analyse audio de la galerie.')}</p></div>{self.figure('audio-analysis-fr' if self.fr else 'audio-analysis','Audio analysis gallery: spectrogram, LUFS, true peak, correlation, EQ/phase and dynamics','Galerie Analyse audio : spectrogramme, LUFS, crête vraie, corrélation, EQ/phase et dynamique')}</div><div class="split reverse">{self.figure('audio-spectral-fr' if self.fr else 'audio-spectral','Spectral displays: filled and band spectra, scrolling spectrograms, spatial stereo and persistent vectorscopes','Vues spectrales : spectres remplis et par bandes, spectrogrammes défilants, vue spatiale stéréo et vectorscopes à persistance')}<div class="copy"><h3>{t('Explore spectral detail and stereo space.','Explorez le détail spectral et l’espace stéréo.')}</h3><p>{t('Use filled curves and frequency bands with a dB floor. Follow scrolling spectrograms in either orientation, with intensity or frequency grading and color legends. Add a spatial stereo cloud or a persistent vectorscope with adjustable fading and glow.','Utilisez des courbes remplies et des bandes fréquentielles avec plancher dB. Suivez les spectrogrammes défilants dans les deux orientations, avec coloration par intensité ou fréquence et légendes. Ajoutez une vue spatiale stéréo ou un vectorscope à persistance avec atténuation et lueur réglables.')}</p></div></div><div class="image-pair">{self.image_card('plots','Plots — frequency response, histogram and heatmap','Graphiques — réponse en fréquence, histogramme et carte de chaleur','From a series to an overview.','De la série à la vue d’ensemble.','Linear and logarithmic axes, lines, points, steps, bars, histograms and heatmaps. Feed bounded or reduced datasets from your application.','Axes linéaires et logarithmiques, lignes, points, paliers, barres, histogrammes et cartes de chaleur. Fournissez des jeux de données limités ou réduits depuis l’application.','data')}{self.image_card('timeline','Timeline — clip lanes, markers, snapping and playhead','Chronologie — pistes de clips, marqueurs, alignement et tête de lecture','Give time a structure.','Donnez une structure au temps.','Move and trim clips, select time ranges and display markers. Time units, transport, media decoding and project state belong to the host.','Déplacez et ajustez les clips, sélectionnez des plages et affichez des marqueurs. Unités de temps, transport, décodage et état du projet appartiennent à l’application.','data')}</div></div></section>''')
        parts.append(f'''<section class="section code-focus" id="code" data-chapter><div class="wrap"><div class="split reverse">{self.figure('code','Code — syntax-colored source, annotations and native editor','Code — coloration syntaxique, annotations et éditeur natif')}<div class="copy"><p class="eyebrow">05 / {t('Code / editing / diff','Code / édition / différences')}</p><h2>{t('Read, edit, compare.','Lisez, éditez, comparez.')}</h2><p>{t('Use a shared syntax catalogue across source views, the native editor and comparisons. Add annotations or register your own language rules and lexer callbacks.','Utilisez un catalogue syntaxique commun aux vues de source, à l’éditeur natif et aux comparaisons. Ajoutez des annotations ou enregistrez vos règles de langage et vos fonctions d’analyse.')}</p><ul class="feature-list"><li>{t('Native C++ editing with Undo/Redo','Édition C++ native avec annulation/rétablissement')}</li><li>{t('Shared language profiles and shader syntax','Profils de langages partagés et syntaxe des shaders')}</li><li>{t('Unified and side-by-side comparisons','Comparaisons unifiées et côte à côte')}</li></ul>{self.link(t('Code integration guide','Guide d’intégration du code'),self.doc('code'))}</div></div><div class="wide-feature"><div class="feature-description"><h3>{t('Keep both sides in view.','Gardez les deux côtés en vue.')}</h3><p>{t('Aligned rows, highlighted changes and navigation between hunks make revisions easier to read. CyUI displays prepared text comparisons; Git, repository access and merge operations stay in the application.','Lignes alignées, changements surlignés et navigation entre les blocs facilitent la lecture des révisions. CyUI affiche les comparaisons préparées ; Git, l’accès au dépôt et les fusions restent dans l’application.')}</p></div>{self.figure('diff','Diff — side-by-side source comparison and highlighted changes','Diff — comparaison de code côte à côte et changements surlignés')}</div></div></section>''')
        parts.append(f'''<section class="section wrap" id="media" data-chapter>{self.section_head('06',t('Images / viewports / documents','Images / vues / documents'),'The rest of the workspace belongs here, too.','Le reste de l’espace de travail a aussi sa place.','Bring image tools, rendered surfaces and readable documents into the same interface. These modules handle presentation and interaction; the host supplies textures, content and services.','Réunissez outils d’image, surfaces rendues et documents lisibles dans une même interface. Ces modules gèrent l’affichage et les interactions ; l’application fournit textures, contenu et services.')}<div class="image-trio">{self.image_card('images','Images — gradient stops, checkerboard and crop tools','Images — points de dégradé, damier et outils de recadrage','Images & gradients','Images et dégradés','Navigate textures, adjust a crop and edit gradient stops. Decoding and image processing stay in your application.','Parcourez les textures, ajustez un recadrage et modifiez les points de dégradé. Le décodage et le traitement d’image restent dans votre application.','media')}{self.image_card('viewport','Viewport — host texture and 2D transform overlay','Vue de scène — texture de l’application et transformation 2D','Host-rendered viewports','Vues rendues par l’application','An orbit-camera model and transform overlays surround a host-rendered surface. The preview texture is synthetic; CyUI is not a 3D engine.','Un modèle de caméra orbitale et des transformations entourent une surface rendue par l’application. La texture d’aperçu est fictive ; CyUI n’est pas un moteur 3D.','media')}{self.image_card('documents','DocumentViews — Markdown content and a sample conversation','DocumentViews — contenu Markdown et conversation d’exemple','Documents & conversations','Documents et conversations','Render native Markdown/GFM with images, spoilers, tasks, code, tables and transcripts. Conversation models, networking and AI providers belong to the host.','Affichez du Markdown/GFM natif avec images, spoilers, tâches, code, tableaux et conversations. Les modèles de conversation, le réseau et les fournisseurs d’IA appartiennent à l’application.','media')}</div></section>''')
        filters = [('all',t('All','Tous')),('core',t('Foundation','Fondation')),('editor',t('Editors','Éditeurs')),('data',t('Data','Données')),('media',t('Media','Médias')),('integration',t('Web','Web'))]
        cards = []
        for name, group, en, fr, deps in MODULES:
            local_deps = deps.replace('host backend','moteur fourni par l’application').replace('selected modules','modules choisis') if self.fr else deps
            anchor = MODULE_DOCS[name]
            cards.append(f'''<article class="module-card" data-family="{group}"><a href="{self.doc(anchor)}"><h3>{name}</h3></a><p>{e(t(en,fr))}</p><code>CyUI::{name}</code><small>{t('Depends on','Dépend de')} : {e(local_deps)}</small></article>''')
        parts.append(f'''<section class="section breakout" id="modules" data-chapter><div class="wrap">{self.section_head('07',t('Build your own combination','Composez votre ensemble'),'Start with Core. Add what you use.','Partez de Core. Ajoutez ce qui vous sert.','Each C++ module is a separate static-library target. Dependencies are selected automatically; a diff viewer does not bring in a browser or an audio engine.','Chaque module C++ est une cible de bibliothèque statique distincte. Les dépendances sont sélectionnées automatiquement ; une vue de différences n’ajoute ni navigateur ni moteur audio.')}<div class="module-tools"><div class="module-search"><label for="module-query">{t('Find a module','Trouver un module')}</label><div class="search-row"><input id="module-query" type="search" placeholder="{t('Name, feature or dependency…','Nom, fonctionnalité ou dépendance…')}" autocomplete="off" aria-controls="module-catalogue"><button type="button" data-clear-search aria-label="{t('Clear module search','Effacer la recherche de modules')}">×</button></div></div><div class="filter-controls" role="group" aria-label="{t('Module families','Familles de modules')}">''' + ''.join(f'<button type="button" data-module-family="{key}" aria-pressed="{"true" if key=="all" else "false"}">{label}</button>' for key,label in filters) + f'''</div></div><p class="module-count" id="module-count" aria-live="polite">{len(MODULES)} {t('modules shown','modules affichés')}</p><div class="module-grid" id="module-catalogue">{''.join(cards)}</div><p class="empty-state" id="module-empty" hidden>{t('No matching module. Try another term or family.','Aucun module correspondant. Essayez un autre terme ou une autre famille.')}</p><p class="note bordered">{t('Additional opt-in adapters: WebCEF uses a separately supplied CEF SDK/runtime; ImGuizmo uses a host-supplied target. Neither is enabled by CYUI_MODULES=All. The Unreal Editor connector is packaged separately.','Adaptateurs supplémentaires au choix : WebCEF utilise un SDK/runtime CEF fourni séparément ; ImGuizmo utilise une cible fournie par l’application. Aucun des deux n’est activé par CYUI_MODULES=All. Le connecteur Unreal Editor se prépare séparément.')}</p></div></section>''')
        integrations = [
            (t('Native','Natif'),'C++',t('Compile against your host’s existing ImGui target. Core needs C++17; the native CodeEditor and tested CEF adapter use C++20.','Compilez avec la cible ImGui déjà utilisée par votre application. Core requiert C++17 ; CodeEditor natif et l’adaptateur CEF testé utilisent C++20.'),'start'),
            ('Python','imgui-bundle',t('Import optional families explicitly. Models are available without a renderer; Python’s editor has multiline editing and a syntax preview.','Importez explicitement les familles optionnelles. Les modèles sont accessibles sans moteur de rendu ; l’éditeur Python propose l’édition multiligne et un aperçu syntaxique.'),'python'),
            ('Web / CEF','HTML + TypeScript',t('Embed a browser surface through a backend or the optional off-screen CEF adapter. Compile TypeScript in your web app and exchange versioned messages.','Intégrez une surface web via un moteur fourni ou l’adaptateur CEF hors écran. Compilez TypeScript dans votre application web et échangez des messages versionnés.'),'web'),
            ('Unreal Editor',t('An optional connector','Un connecteur optionnel'),t('Embed CyUI in Slate panels inside another plugin. Select the modules you need and share a provider or package a uniquely named copy. Editor-only.','Intégrez CyUI dans des panneaux Slate d’un autre plugin. Choisissez les modules nécessaires et partagez un fournisseur ou préparez une copie au nom unique. Éditeur uniquement.'),'unreal'),
        ]
        parts.append(f'''<section class="section wrap" id="integrations" data-chapter>{self.section_head('08',t('One foundation, several hosts','Une fondation, plusieurs hôtes'),'Your application sets the boundaries.','Votre application définit le cadre.','Keep project models, file access, processing and services in the host. CyUI gives them a consistent presentation, with explicit state and edit proposals.','Gardez les modèles du projet, l’accès aux fichiers, les traitements et les services dans l’application. CyUI leur donne un affichage cohérent, avec un état explicite et des propositions de modification.')}<div class="integrations">''' + ''.join(f'<article class="integration"><span class="integration-type">{e(kind)}</span><h3>{e(title)}</h3><p>{e(text)}</p>{self.link(t("Integration guide","Guide d’intégration"),self.doc(anchor))}</article>' for kind,title,text,anchor in integrations) + f'''</div><div class="code-split"><div><h3>{t('Choose dependencies deliberately.','Choisissez vos dépendances.')}</h3><p>{t('This example links Nodes and Inspector through your existing ImGui target. Their required CyUI dependencies follow automatically.','Cet exemple lie Nodes et Inspector à la cible ImGui existante. Leurs dépendances CyUI nécessaires suivent automatiquement.')}</p><p class="note">{t('CMake ≥ 3.16 · Dear ImGui ≥ 1.92. Integration uses add_subdirectory or FetchContent; there is no installed find_package(CyUI) package.','CMake ≥ 3.16 · Dear ImGui ≥ 1.92. L’intégration utilise add_subdirectory ou FetchContent ; il n’existe pas de paquet installé find_package(CyUI).')}</p></div>{self.block('CMakeLists.txt',CMAKE)}</div><div class="guide-callout"><div><h3>{t('From the gallery to your first panel.','De la galerie à votre premier panneau.')}</h3><p>{t('Read the bilingual integration guide, then follow the detailed API references. The HTML guide and its resources also open locally, without a connection.','Lisez le guide d’intégration bilingue, puis consultez les références détaillées des API. Le guide HTML et ses ressources s’ouvrent aussi localement, sans connexion.')}</p></div>{self.link(t('Open documentation','Ouvrir la documentation'),self.doc('start'),'button')}</div></section>''')
        parts.append(self.documentation_section())
        parts.append(f'''<section class="section breakout" id="download" data-chapter><div class="wrap download-layout"><div class="copy"><p class="eyebrow">10 / {t('Try it. Then build with it.','Essayez. Puis intégrez.')}</p><h2>{t('Explore the real thing.','Explorez la vraie interface.')}</h2><p>{t('The portable gallery lets you try CyUI without compiling. For your own software, use the source and select the modules that fit.','La galerie portable permet d’essayer CyUI sans compiler. Pour votre logiciel, utilisez les sources et choisissez les modules adaptés.')}</p><p class="status-line"><span class="status-dot" aria-hidden="true"></span>{t('Active development · Versioned demo 1.2.0','En développement · Démo versionnée 1.2.0')}</p><p class="note">{t('Screenshots show the gallery built from the current source. The downloadable 1.2.0 demo is an earlier release snapshot; newer source builds also include the CyDoc manual.','Les captures montrent la galerie compilée depuis les sources actuelles. La démo 1.2.0 téléchargeable est un instantané antérieur ; les builds plus récents des sources incluent aussi le manuel CyDoc.')}</p><div class="platforms"><div><h3>Windows x64</h3><p>{t('Portable demo + library','Démo portable + bibliothèque')}</p></div><div><h3>Linux</h3><p>{t('Library source; no demo binary','Sources de la bibliothèque ; aucune démo binaire')}</p></div><div><h3>macOS</h3><p>{t('Library source; no demo binary','Sources de la bibliothèque ; aucune démo binaire')}</p></div></div></div><div><article class="download-card"><p class="eyebrow">{t('Portable gallery','Galerie portable')}</p><h3>CyUI Demo 1.2.0</h3><p>Windows x64 · DirectX 11 · ZIP</p><a class="button primary" href="{ZIP}">↓ {t('Download the demo','Télécharger la démo')}</a>{self.link(t('Release notes','Notes de version'),RELEASE)}<p><small>{t('Extract the entire archive, then run cyui_demo.exe. Use Launch-French.cmd for French. No build tools, Python or separate Visual C++ runtime installation required.','Extrayez toute l’archive, puis lancez cyui_demo.exe. Utilisez Launch-French.cmd pour le français. Aucun outil de compilation, Python ou runtime Visual C++ séparé à installer.')}</small></p></article><p class="note bordered">{t('Windows execution is locally validated. Linux/macOS source targets and CI configuration are present, but this repository’s local evidence does not establish execution on those systems. CEF runtimes and Unreal Engine are supplied separately.','L’exécution Windows est validée localement. Les sources et la configuration CI visent Linux/macOS, mais les vérifications locales du dépôt n’établissent pas leur exécution sur ces systèmes. Les runtimes CEF et Unreal Engine sont fournis séparément.')}</p></div></div></section>''')
        faqs = [
            (t('Is CyUI an application or a library?','CyUI est-il une application ou une bibliothèque ?'),t('CyUI is a themed widget and interface-module library built on Dear ImGui. The downloadable application is its interactive gallery. It does not replace your application’s engine, project model or rendering backend.','CyUI est une bibliothèque de widgets et de modules d’interface basée sur Dear ImGui. L’application téléchargeable est sa galerie interactive. La bibliothèque ne remplace ni le moteur, ni le modèle de projet, ni le moteur de rendu de votre application.')),
            (t('Is this an alpha or beta?','Est-ce une version alpha ou bêta ?'),t('The repository identifies the current library and demo as version 1.2.0 and is under active development. It does not declare an alpha or beta designation. Test the modules and adapters against your host and target platforms before distribution.','Le dépôt identifie la bibliothèque et la démo comme version 1.2.0 et leur développement est actif. Il ne déclare pas de statut alpha ou bêta. Testez les modules et adaptateurs dans votre application et sur vos plateformes avant distribution.')),
            (t('Can I use it in commercial software?','Puis-je l’utiliser dans un logiciel commercial ?'),t('CyUI’s own code and adapters use the MIT license. Keep its copyright and permission notice, together with the licenses of the dependencies you distribute. CEF and Unreal Engine retain their separate terms.','Le code propre à CyUI et ses adaptateurs sont sous licence MIT. Conservez les mentions de copyright et d’autorisation, ainsi que les licences des dépendances distribuées. CEF et Unreal Engine gardent leurs conditions distinctes.')),
            (t('Do I have to use all the modules?','Dois-je utiliser tous les modules ?'),t('No. Begin with Core and explicitly select additional targets. Their dependency closure is linked automatically. WebCEF, ImGuizmo and the Unreal Editor connector are optional integrations with separately supplied dependencies.','Non. Commencez par Core et sélectionnez explicitement les autres cibles. Leurs dépendances nécessaires sont liées automatiquement. WebCEF, ImGuizmo et le connecteur Unreal Editor sont des intégrations optionnelles dont les dépendances sont fournies séparément.')),
            (t('Does the documentation work offline?','La documentation fonctionne-t-elle hors ligne ?'),t('Yes. The complete English/French CyDoc reader is included with this site and current source builds. Keep the Documentation folder together and open Documentation.html directly from disk. External references, GitHub and downloads need a connection.','Oui. Le lecteur CyDoc anglais/français complet est inclus avec ce site et les builds actuels. Gardez le dossier Documentation entier et ouvrez Documentation.html depuis le disque. Les références externes, GitHub et les téléchargements nécessitent une connexion.')),
        ]
        parts.append(f'''<section class="section wrap"><div class="faq-layout"><div><p class="eyebrow">{t('Before you start','Avant de commencer')}</p><h2>{t('A few useful answers.','Quelques réponses utiles.')}</h2><p>{t('Scope, platforms and the parts your application keeps.','Périmètre, plateformes et responsabilités de votre application.')}</p></div><div class="faq-list">''' + ''.join(f'<details><summary>{e(q)}</summary><p>{e(a)}</p></details>' for q,a in faqs) + '</div></div></section></main>')
        return '\n'.join(parts)

    def render(self):
        title = self.t('CyUI — Modular interfaces for desktop tools','CyUI — Des interfaces modulaires pour vos outils')
        if self.guide:
            title = self.t('CyUI — Integration guide','CyUI — Guide d’intégration')
        description = self.t('A themed, modular Dear ImGui library for C++ and Python. Explore real widgets, node graphs, audio displays, code editors and a portable Windows demo.','Une bibliothèque Dear ImGui thématisée et modulaire pour C++ et Python. Découvrez widgets, graphes, vues audio, éditeurs de code et démo Windows portable.')
        css = hashlib.sha256((SITE/'assets/site.css').read_text(encoding='utf-8').encode('utf-8')).hexdigest()[:12]
        js = hashlib.sha256((SITE/'assets/site.js').read_text(encoding='utf-8').encode('utf-8')).hexdigest()[:12]
        filename = "guide.html" if self.guide else "index.html"
        en = "../"+filename if self.fr else filename
        fr = filename if self.fr else "fr/"+filename
        return f'''<!doctype html>
<!-- Generated by tools/build_showcase.py; edit the generator or site/assets/. -->
<html lang="{self.language}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta name="color-scheme" content="dark"><meta name="theme-color" content="#131518"><meta name="description" content="{e(description)}"><meta name="referrer" content="strict-origin-when-cross-origin"><title>{e(title)}</title><link rel="icon" href="{self.assets}mark.svg" type="image/svg+xml"><link rel="alternate" hreflang="en" href="{en}"><link rel="alternate" hreflang="fr" href="{fr}"><link rel="stylesheet" href="{self.assets}site.css?v={css}"><script src="{self.assets}site.js?v={js}" defer></script></head><body>{self.header()}<noscript><p class="no-script">{self.t('All content and guides are available. Enable JavaScript for image enlargement, theme previews and module filtering.','Tout le contenu et les guides sont accessibles. Activez JavaScript pour agrandir les images, prévisualiser les thèmes et filtrer les modules.')}</p></noscript>{self.main_content()}{self.footer()}{self.dialog()}</body></html>
'''


def legacy_guide(language):
    reader = ("../" if language == "fr" else "") + "Documentation/Documentation.html"
    routes = {"start": "integration", "editors": "module-recipes/curves-graphs-and-timeline", "data": "module-recipes/canvas-and-numeric-plots", "code": "code", "media": "module-recipes/images-viewport-and-optional-3d-gizmo", "filtering": "filtering", "python": "python", "web": "web", "unreal": "unreal",
    "office-grid": "office/grid", "office-sheet": "office/spreadsheet", "office-writer": "office/writer", "office-formats": "office/formats", "markdown": "native-markdown", "markdown-editor": "native-markdown/editor", "playback": "native-markdown/resources", "platforms": "overview/platforms"}
    mapping = json.dumps(routes, separators=(",", ":"))
    return f'''<!doctype html>
<html lang="{language}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CyUI — Documentation</title></head><body><main id="main"><h1>CyUI Documentation</h1><p><a href="{reader}#/{language}/overview">{"Ouvrir le manuel local" if language == "fr" else "Open the local manual"}</a></p></main><script>
const routes={mapping};
const requested=new URLSearchParams(location.search).get('lang');
const language=['en','fr'].includes(requested)?requested:{json.dumps(language)};
const chapter=location.hash.slice(1);
location.replace({json.dumps(reader)}+'#/'+language+'/'+(routes[chapter]||'overview'));
</script></body></html>
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail if generated pages or resource versions are stale")
    args = parser.parse_args()
    outputs = {SITE/("fr/" if language=="fr" else "")/("guide.html" if guide else "index.html"):
        (legacy_guide(language) if guide else Page(language, False).render()) for language in ("en","fr") for guide in (False,True)}
    for path, content in outputs.items():
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                raise SystemExit(f"Stale page: {path.relative_to(ROOT)}; run python tools/build_showcase.py")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")
        print(f"{'Checked' if args.check else 'Built'} {path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()

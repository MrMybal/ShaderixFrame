# ShaderixFrame 0.12

Développement et maintenance : Cyberalien.

Éditeur vidéo natif Windows, avec composition à calques, effets HLSL et montage audio.
Cette première version utilisable reprend le thème et le compilateur de graphes de
**Shaderixel**, et les traitements DSP, l'analyse du signal et le modèle de graphes de
**CyDAW 2**. La timeline adapte les interactions de `src/v2/ArrangementView` aux secondes
et aux images vidéo.

La 0.12 ajoute le zoom de la composition, la navigation de timeline à la souris,
les fenêtres natives détachables, les clips contenant leur propre montage,
317 effets/générateurs Shaderixel, le graphe DSP CyDAW et le motion design.
Les graphes des calques et des shaders restent indépendants et animables.

Molette pour zoomer ; bouton central pour déplacer la composition et les graphes.
Tirer la barre au-dessus de la timeline : horizontalement pour déplacer la vue,
verticalement pour zoomer. Double-cliquer un clip vidéo pour ouvrir son contenu.
Tirer un onglet hors de la fenêtre pour le placer sur un autre écran ; **Affichage**
permet de rouvrir tous les panneaux fermés.

Ouvrir `examples/MotionStudy.sframe` pour la hiérarchie, les ancrages et l'animation
des widgets, ou `examples/ClipStudy.sframe` pour la timeline incorporée.
**Bibliothèque Shaderixel** rassemble les générateurs et effets ; **Graphe DSP**
ouvre directement les chaînes CyDAW des pistes, clips, bus et master.
Les LUT images 2D, atlas et .cube 3D fonctionnent aussi dans les graphes locaux.

Les nouveautés et limites figurent dans [docs/RELEASE_0.12.md](docs/RELEASE_0.12.md).
Les outils d'étalonnage, VST3, OFX et sorties PNG16/EXR restent disponibles.

Dans **Mixeur & graphe audio**, choisir un graphe puis **Éditer le graphe DSP** :
glisser entre les ports, ajouter un DSP par recherche, déplacer les cartes, zoomer
à la molette et organiser le signal. **Appliquer** valide le graphe en une opération
annulable. Les fenêtres VST3 et presets sont accessibles dans leur pile d'effets.
**Propriétés > + Plugin OFX** charge un filtre float Windows x64 et ses paramètres
animables. **Séquence** règle l'espace de travail, le tone mapping et
la transparence ; **Exports** propose PNG 8/16 bits et EXR half.

`examples/DspStudy.sframe` montre deux EQ CyDAW en parallèle et leur sommation.
`examples/PrecisionStudy.sframe` propose un dégradé HDR linéaire et l'export EXR.
`examples/GradingStudy.sframe` montre des primaires animées, une courbe et une
correction locale ; sélectionner le clip **Lumière HDR**, puis l'onglet **Étalonnage**.

## Lancer

Ouvrir `dist/ShaderixFrame/ShaderixFrame.exe`. Une séquence de démonstration de 12 secondes
est chargée, avec titre transparent, lumière procédurale, couleur animée, grain et bande sonore originale.
La lecture reste en pause. Le dossier distribué fonctionne sans les projets sources.

1. Importer une vidéo, une image ou un son avec **Fichier > Importer**, `Ctrl+I`, ou déposer
   le fichier dans la fenêtre. Une vidéo contenant du son crée deux clips liés sur deux pistes.
2. Déplacer les clips, tirer leurs bords pour les raccourcir et utiliser `S` pour les couper
   à la tête de lecture. `Alt` suspend le magnétisme aux images. Le clic droit permet de
   dupliquer ou supprimer ; les clips peuvent passer sur une autre piste du même type.
   Les déplacements, trims, découpes, duplications et suppressions suivent le groupe lié.
   Le clic droit permet aussi de lier ou délier des clips. Une piste verrouillée bloque
   les opérations sur tout son groupe. Les keyframes de clip et d'effet suivent les déplacements.
   `Ctrl`/`Maj` + clic ajoute ou retire un groupe ; glisser un rectangle dans un espace
   vide sélectionne plusieurs clips, `Ctrl+A` dans la timeline sélectionne tout et
   `Échap` désélectionne. Glisser le corps d'un clip sélectionné déplace le bloc complet ;
   tirer un bord raccourcit uniquement son groupe. `Ctrl+D` duplique le bloc après sa fin
   en conservant les espaces entre clips. Un bloc de plusieurs groupes reste sur ses pistes.
   `Suppr` supprime la sélection ; `Maj+Suppr` referme ses intervalles sur toutes les pistes.
   Un clip non sélectionné qui traverse ces intervalles doit d'abord être découpé ou
   sélectionné. Une piste verrouillée à déplacer bloque l'opération entière.
   `examples/CutStudy.sframe` propose trois séquences de titre/vidéo/son liés pour essayer.
3. Régler les transformations et les effets dans **Propriétés**. `K` ajoute un point
   d'animation. Son clic droit choisit la courbe affichée dans la timeline.
4. Utiliser **Animations** pour régler précisément les temps, valeurs et interpolations
   linéaire, douce, palier ou courbe CyDAW. Double-clic sur la courbe pour ajouter un point ;
   clic droit sur un point pour le retirer.
5. Créer/importer un **Graphe VFX**, ou compiler un shader dans **HLSL**. La pile peut
   combiner effets texte et graphes Shaderixel. Le clip audio propose 15 effets DSP CyDAW.
   Dans le graphe, la molette zoome autour du pointeur, le bouton central déplace le
   canevas et **Tout voir** cadre les nœuds. Le déplacement des nœuds suit le zoom.
   `examples/GraphStudy.sframe` montre un graphe de désaturation et son automation.
6. Enregistrer en `.sframe`, exporter la composition en **MP4 H264/AAC**, ou le mix en
   **WAV stéréo 48 kHz**. Le rendu démarre en arrière-plan : le montage et la lecture
   restent disponibles. **Exports** affiche la progression, le temps écoulé et les erreurs.
   **Annuler cet export**, ou **Annuler export** dans la barre d'état, interrompt le rendu.
   Les sorties MP4, WAV, PNG et projets remplacent leur destination uniquement après
   finalisation ; une erreur ou une annulation préserve le fichier déjà présent.

Un export utilise une copie de la séquence et du catalogue d'effets prise au lancement.
Modifier le montage, les shaders ou ouvrir un autre projet ne change pas ce rendu.
Les fichiers médias restent lus depuis leurs chemins ; leurs contenus ne sont pas copiés.
La file accepte 32 rendus actifs/en attente, exécutés un par un ; les huit derniers
rendus terminés restent visibles dans **Exports**,
accessible aussi par **Affichage > Exports**. L'annulation est disponible avant la
finalisation du remplacement du fichier. Fermer l'application annule les rendus encore
annulables et attend la libération du moteur de rendu.

**Propriétés > Fondus du clip** règle les fondus d'entrée et de sortie en secondes,
séparément pour chaque clip vidéo ou audio. Leurs diagonales apparaissent dans la timeline.
Superposer les clips sur deux pistes permet des transitions par fondu et le mélange audio.

**Propriétés > Transition d'entrée** ajoute un fondu HLSL ou un balayage (quatre directions,
contour progressif). Placer le clip entrant sur un calque au-dessus du plan sortant et
faire chevaucher leurs images pendant la durée de transition. Le bandeau turquoise de
la timeline marque cette durée. La transition mélange la composition avec et sans
ce clip, après ses effets, transformations, masque, opacité et fondus. Les titres
et autres calques placés au-dessus restent visibles. Sans calque dessous, elle part du noir.
Le son se mélange avec ses propres fondus. **Timeline > Montage avancé > Chevauchement**
place automatiquement les clips adjacents et ajoute leurs fondus audio liés.

La progression suit le temps de séquence depuis le début du clip. Le curseur de progression
et ses points `K` remplacent ce calcul ; **Progression automatique** le rétablit. Direction,
contour et paramètres HLSL s'animent aussi. **Éditer le HLSL de transition** ouvre son code
à deux entrées dans le panneau HLSL. Modifier le shader conserve les animations existantes.
Découper conserve la transition sur le segment gauche ; raccourcir limite sa durée,
dupliquer copie le shader et décale ses animations. `examples/TransitionStudy.sframe`
montre le fondu puis le balayage. Les plugins de transition utilisent l'API 2.

Ajouter un **titre** avec **Séquence > Titre / texte**, ou **Médias & effets > Sources**.
Dans **Propriétés > Titre**, éditer le texte multiligne, la police Windows, le gras,
l'italique et l'alignement. La taille, la couleur, le contour et l'ombre s'animent ; `K` à côté de la couleur
ajoute un point sur ses quatre composantes. Les titres utilisent aussi les transformations,
fondus, effets HLSL et graphes VFX. La police doit être installée sur la machine de rendu.

**Propriétés > Masque** propose rectangle, ellipse ou tracé Bézier fermé, inversion, position, taille,
rotation et contour progressif en pixels. `K` anime chaque réglage. Activer **Guides du
masque** dans l'aperçu et glisser son point central pour déplacer le masque. Les paramètres
déjà animés reçoivent un point au temps courant lors de ce déplacement. Le masque suit
les transformations du clip et s'applique après ses effets. Le projet
`examples/MaskStudy.sframe` montre une ellipse animée.

Les boutons **Monter**, **Descendre** et **Retirer** réorganisent les piles HLSL et DSP.
Les animations restent attachées à leur effet ; annuler/rétablir conserve l'ordre.

**Mixeur & graphe audio > Mixeur** propose maintenant les pistes, les bus et le master.
Sélectionner un canal pour régler gain/panoramique, mute/solo de piste, sortie et pile
des 15 DSP CyDAW 2. **+ Bus stéréo** crée un bus ; son nom, son verrouillage et sa
suppression sont disponibles dans ses réglages. Une sortie peut aller au master ou à
un autre bus. Les boucles sont refusées, même pour un envoi désactivé.

**Envois > + Envoi** ajoute un chemin parallèle vers un bus, avant ou après le gain
de canal, avec niveau et bypass propres. Sur une piste, le pan du clip et celui de la
piste restent combinés avant les DSP de piste pour conserver le mix des anciens projets.
Les bus appliquent leur panoramique après leurs DSP. **K** ajoute un point au temps
courant ; son clic droit sélectionne le paramètre dans **Automation de mixage**.
Ce panneau affiche la courbe et permet d'éditer temps, valeur, interpolation et forme
CyDAW, puis de supprimer des points ou de revenir au réglage de base. Entrée valide
les champs numériques. Les automations de canal restent au temps de séquence lors du montage des clips.

Les vu-mètres mesurent chaque canal après son fader ; **CLIP** signale un dépassement
sur ce bloc avant la limitation de sortie. Ce sont des crêtes de blocs audio mis en
file. Le mixeur affiche aussi les LUFS momentanés/intégrés et la latence compensée ;
les crêtes restent des crêtes d'échantillons, sans maintien. Le graphe affiche les bus, les
envois et les DSP actifs, avec ports stéréo validés par CyDAW. Son routage se règle dans
le mixeur. **Graphe DSP : éditer/importer** accepte un graphe JSON stéréo embarqué
dans une pile de clip, piste, bus ou master ; voir `examples/ParallelEQ.json`.
Les effets de piste/bus/master continuent à traiter le silence après la fin des clips,
jusqu'à la fin de séquence. Allonger celle-ci pour exporter toute une queue d'effet.
`examples/MixerStudy.sframe` montre un bus musique, un auxiliaire d'échos et un limiteur master.

Les modifications non enregistrées sont conservées toutes les **5 secondes**, à la
fermeture et avant d'ouvrir une autre séquence, dans `recovery/` à côté de l'exécutable.
Au lancement suivant, le panneau **Récupération des sessions** permet de reprendre ou
supprimer les instantanés. Le menu **Fichier > Récupérer une session** le rouvre.
L'enregistrement du projet retire son instantané courant. Les sessions encore ouvertes
dans un autre processus sont ignorées. La récupération reste distincte du `.sframe` enregistré.

Les panneaux se détachent et se réorganisent. Les calques vidéo placés en haut sont au
premier plan. Les modes Normal, Addition, Produit et Écran sont disponibles. `M` coupe
une piste, `S` dans son en-tête la met en solo. Le mixeur et le spectre analysent le son réel.

## Compiler et vérifier

Visual Studio 2022 avec les outils C++, CMake et Ninja :

```bat
build.bat
build.bat Release --safe-compile
```

Les dépendances de source sont copiées dans `vendor/`. Les binaires VST3 et médias
restent séparés : voir [docs/BUILDING.md](docs/BUILDING.md) pour les récupérer ou les générer. Le runtime MSVC est lié statiquement.
Les codecs utilisés sont ceux de Windows Media Foundation ; un format présent dans le
sélecteur de fichiers n'est pas une garantie de décodage sur chaque installation Windows.

`build.bat` lance les tests du cœur/GPU, les interactions de l'éditeur ImGui sans fenêtre,
et, avec Python, l'intégration MCP du véritable
exécutable. Les tests couvrent les clips, les animations, l'historique, la validation des
graphes, les six effets HLSL, les 15 DSP audio, les pixels GPU, PNG/WAV/MP4 et les commandes
MCP, les groupes liés, fondus, migrations, récupération, annulations d'export, titres
Unicode transparents, masques animés et ordre des effets.
Ils vérifient aussi les sélections groupées, leur annulation atomique, les suppressions
avec fermeture d'espace et les transformations de navigation du graphe.
Les interactions vérifient clic/rectangle, déplacements groupés, annulation en une
étape, zoom/pan et déplacements des nœuds. Consulter l'inspecteur ou sélectionner
un clip n'ajoute pas de paramètres et ne crée pas d'étape d'historique.
Les tests d'export vérifient le moteur COM/GPU séparé, les médias produits, les modifications
concurrentes, les annulations MP4/WAV, les erreurs, l'historique borné et la fermeture.
Les transitions sont vérifiées sur leurs pixels GPU, masques, opacité, directions,
animations, migration de projet, plugins API 2 et MP4 exportés.
Les tests du mixeur vérifient routage, boucles, envois pré/post, headroom, vu-mètres,
DSP de canal, automations, migrations, verrouillages et queues d'effet dans WAV/MP4.
Les essais médias sont créés dans le dossier de build.

```powershell
python tools/make_demo.py
python tools/package.py
python tools/make_installer.py
```

## MCP et plugins

Le serveur est **intégré à l'exécutable** : `ShaderixFrame.exe --mcp`. Le client lance un
processus avec sa fenêtre ; les outils et cette fenêtre partagent la même séquence.
Il ne se connecte pas à une autre instance déjà ouverte. `--hidden` masque la fenêtre pour
les tests et les traitements automatisés. La configuration d'exemple est dans [docs/MCP.md](docs/MCP.md).
Elle n'installe ni n'active automatiquement un serveur dans votre client.

99 outils : graphes de calques et leurs automations, étalonnage/LUT/scopes/presets, état, projet, import, pistes, bus/envois/DSP/automations de mixage, graphe audio, générateur, titres, masques, transitions, sélection et montage groupés, temps de clip,
découpe, liaison, duplication, paramètres, keyframes, piles d'effets vidéo/audio,
shaders, graphes, transport, export avec suivi/annulation et annuler/rétablir.
Voir [docs/MCP.md](docs/MCP.md) et [docs/PLUGINS.md](docs/PLUGINS.md).

## Composants partagés et évolutions

Les deux projets sources sont uniquement lus. `upstream.lock.json` enregistre le chemin
et l'empreinte SHA-256 de chaque fichier copié. Les modifications propres à ShaderixFrame
restent dans `src/adapters/`, `src/core/`, `src/render/` et `src/ui/`.

```powershell
python tools/sync_upstream.py --check
python tools/sync_upstream.py --update --component shaderixel
python tools/sync_upstream.py --update --component cydaw2
build.bat
```

`--check` retourne 1 si une source a changé. `--update` est explicite, conserve les anciennes
copies dans `vendor-history/` et refuse d'écraser un fichier vendor modifié localement.
Une mise à jour ne constitue pas une validation automatique de compatibilité : les
adaptateurs et les tests doivent être vérifiés après compilation.

Les limites de cette version et les étapes suivantes figurent dans
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) et [docs/ROADMAP.md](docs/ROADMAP.md).

La 0.8 ajoute le cache PCM sur disque, l'import et le décodage interactifs en arrière-plan,
les vignettes et proxies d'aperçu, ripple/roll et retrait d'intervalle avec découpe,
les effets HLSL à deux entrées/1–8 passes avec historique, les masques Bézier/suivi,
les graphes DSP, automations d'envoi, compensation de latence, time-stretch et LUFS.
Le SDK natif ABI 1 et les effets VST3 s'exécutent dans des processus séparés.
**Exports** propose aussi AVI RGB/PCM, séquences PNG, rapports JSON et presets.
L'installateur est destiné au compte Windows courant ; le ZIP portable reste disponible.

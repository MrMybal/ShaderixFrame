# ShaderixFrame 0.12.0

Cette version ajoute la navigation des vues, les fenêtres natives détachables,
les compositions de clip et les outils de motion design.

## Navigation et fenêtres

- **Composition** : molette pour zoomer autour du pointeur, bouton central pour
  déplacer l'image, double-clic pour ajuster. Le clic droit propose Ajuster et 1:1.
- **Graphe des calques** : zoom à la molette, déplacement central et menu contextuel
  pour ajouter, copier, renommer, désactiver ou supprimer des nœuds. Le clic droit
  sur une entrée retire sa connexion. Le graphe shader reste un éditeur distinct.
- **Timeline** : molette pour zoomer, Maj + molette pour déplacer la vue.
  Tirer la barre au-dessus de la règle : gauche/droite déplace le temps visible,
  haut/bas change le zoom. Double-clic sur cette barre pour cadrer la séquence.
- Les clips vidéo montrent des vignettes calculées par le moteur de rendu : médias,
  titres, HLSL et contenu de conteneurs. Elles arrivent en arrière-plan.
- Tirer un onglet hors de la fenêtre crée une fenêtre Win32 avec son rendu D3D11,
  déplaçable sur un autre écran. Chaque panneau possède une fermeture et peut
  être rouvert dans **Affichage**. Les états ouverts et le placement sont conservés.

## Éditeur de clip

Double-cliquer un clip vidéo, ou choisir **Éditer le clip** dans la timeline.
Le clip devient un conteneur possédant son document local : pistes vidéo/audio,
timing, composition des calques, graphes shaders, effets et animations.
On peut ouvrir à nouveau un clip de cette timeline locale pour imbriquer les montages.
L'éditeur s'ouvre dans une grande fenêtre indépendante, qui peut aussi être réancrée.

**Appliquer au conteneur** valide les modifications et les transmet au rendu parent.
Enregistrer le projet applique aussi les brouillons ouverts. **Recharger** revient
au contenu appliqué. **Fenêtre séparée** détache tout l'éditeur ; ses panneaux
peuvent également être détachés individuellement et réouverts dans **Fenêtres du clip**.
Les raccourcis de montage suivent la timeline qui a le focus.

La conversion garde le rendu, les couleurs HDR et l'alpha. Les animations existantes
sont décalées au temps local ; l'origine du temps shader garde leur phase globale.
**Temps shader local depuis zéro** permet de choisir une nouvelle phase locale.
Le son interne suit l'entrée et la vitesse du conteneur, ses fondus, gain et pan.
`examples/ClipStudy.sframe` contient le montage de `MotionStudy.sframe`.

## DSP et bibliothèque Shaderixel

**Affichage > Graphe DSP** ouvre directement l'éditeur du véritable moteur
`AudioRenderPlan` de CyDAW v2. Choisir piste, bus, master ou clip audio et l'instance
de graphe. Recherche de DSP, ports, câbles, boutons rotatifs CyDAW, zoom, disposition,
menu contextuel et undo/redo du brouillon sont disponibles.
**K** choisit l'automation d'un paramètre ; ses points sont éditables sous le graphe.
**Compiler / Appliquer** valide et applique le brouillon à la chaîne audio.

**Bibliothèque Shaderixel** apporte les **317 shaders d'effets et de générateurs**
copiés depuis Shaderixel. Recherche et catégories permettent de créer un calque
générateur ou d'ajouter un effet au clip sélectionné. Les générateurs se déposent
également sur la timeline. Les corrections Curves et Color Lookup ouvrent les outils
natifs de courbes et LUT. Les pistes ajoutées sont reliées au graphe de composition
existant sans remplacer ses branches.

## Motion design, LUT et transitions

**Motion design** propose rectangle arrondi, ellipse, texte, image/vidéo et groupe.
La hiérarchie définit parents, ancrages, pivots, visibilité et découpage des enfants.
Position et dimensions sont animables avec **K**. Le canvas montre le rendu réel :
sélection/déplacement, poignée de taille, zoom et déplacement de la vue.
Les widgets restent des clips, avec effets et possibilité d'ouvrir leur contenu.
`examples/MotionStudy.sframe` montre une carte, du texte et des formes animées.

Les nœuds de correction du graphe des calques et les effets d'étalonnage importent
les LUT **3D .cube**, les **atlas d'image 2D représentant une LUT 3D** et les
**tables RG 2D**. Choisir Auto, Atlas ou RG ; les données sont incorporées au projet.
Les mêmes outils sont disponibles dans les graphes et effets des conteneurs.

Les transitions d'entrée fonctionnent entre clips chevauchés d'une même piste,
ordonnés par leur début, et entre plusieurs pistes. Le graphe propose aussi
**+ Transition multitrack**, avec deux images en entrée et une progression animable.
Les transitions HLSL personnalisées restent éditables.

## Contrats et limites

Projet **schema 10**, lecture des schemas **1..10** ; **99 outils MCP**.
Composition et conteneurs utilisent des intermédiaires RGBA16F, avec PNG16/EXR half.
Les codecs vidéo actuels restent 8 bits. Les conteneurs sont limités à huit niveaux
et à 16 Mio de document incorporé ; les médias restent des références de fichiers.
Changer leur vitesse utilise le rééchantillonnage du mix interne.

Le catalogue importe les effets/générateurs shader de Shaderixel ; les scènes 3D,
maillages et projets complets de Shaderixel ne sont pas importés. Motion design
est un éditeur de widgets vidéo, sans moteur Unreal ni ses scripts et widgets interactifs.
Le DSP reprend le moteur CyDAW et ses composants graphiques dans la frontière stéréo
existante ; MIDI, instruments, sidechains et graphes Studio arbitraires restent à porter.
Les limites VST3/OFX et étalonnage des versions précédentes restent applicables.

Les copies upstream sont verrouillées par SHA-256. `tools/import_shaderixel.py --check`
vérifie les 317 adaptations et `tools/extract_composition.py --check` les composants
partagés. La mise à jour explicite conserve les anciennes copies et exige recompilation
et tests. Shaderixel et CyDAW ne sont jamais modifiés par ces outils.

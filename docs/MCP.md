# MCP intégré

Transport stdio JSON-RPC 2.0 : une ligne JSON par message, aucune sortie de diagnostic
sur stdout. Versions négociées : 2025-06-18, 2025-03-26 et 2024-11-05.

```json
{
  "mcpServers": {
    "shaderixframe": {
      "command": "chemin/ShaderixFrame/ShaderixFrame.exe",
      "args": ["--mcp"]
    }
  }
}
```

Le serveur expose `initialize`, `ping`, `tools/list` et `tools/call`. La notification
`notifications/initialized` ne reçoit pas de réponse. À la fermeture de stdin, le
processus termine après avoir traité les messages reçus. Une commande invalide rend
`isError: true` avec un message ; elle ne modifie pas le projet.

| Groupe | Outils |
|---|---|
| Projet | `status`, `new_project`, `open_project`, `save_project` |
| Montage | `import_media`, `add_track`, `add_generator`, `set_clip`, `split_clip`, `delete_clip`, `duplicate_clip`, `link_clips`, `unlink_clip` |
| Sélection groupée | `select_clips`, `move_clips`, `duplicate_clips`, `split_clips`, `delete_clips` |
| Animation | `set_parameter`, `set_keyframe` |
| Canaux audio | `add_bus`, `remove_bus`, `set_mix_channel`, `set_send`, `remove_send` |
| DSP/automation du mixeur | `add_mix_effect`, `set_mix_effect`, `remove_mix_effect`, `set_mix_parameter`, `set_mix_keyframe`, `remove_mix_keyframe`, `get_audio_graph` |
| Composition | `add_title`, `set_title`, `set_mask`, `set_transition`, `set_transition_shader`, `remove_transition` |
| Effets | `add_effect`, `add_audio_effect`, `set_effect`, `remove_effect`, `list_effects`, `set_shader`, `get_graph`, `set_graph` |
| Transport | `transport` |
| Sortie | `export_video`, `export_audio`, `render_frame` |
| Export en arrière-plan | `start_export`, `export_status`, `cancel_export` |
| Historique | `undo`, `redo` |

Les schémas sont fournis par `tools/list`. `clip` est un ID stable retourné par l'import
ou le générateur, `effect` un index à partir de zéro. Un `effect` omis ou égal à -1 cible
un paramètre de clip. `set_graph` accepte l'objet JSON natif `EffectGraph::ToJson()` de
Shaderixel. Les clés DSP audio sont celles du catalogue CyDAW ; leurs ID ont le préfixe
`cydaw.` (par exemple `cydaw.limiter`, `cydaw.eq-pro`, `cydaw.ping-pong`).

`set_clip` accepte `start`, `duration`, `in`, `speed`, `name`, `fade_in` et `fade_out`.
Les quatre propriétés temporelles agissent aussi sur les membres liés. Un changement
de `start` déplace les animations ; les fondus et le nom restent propres au clip ciblé.
`link_clips` prend `clip` et `other`, `unlink_clip` délie le groupe entier. Découpe,
duplication et suppression traitent tout le groupe, avec vérification des verrouillages.
`duplicate_clip` retourne l'ID de la copie du clip ciblé. Les sorties sont finalisées
avant de remplacer une destination existante.

La version 0.12 expose **99 outils**. `status` retourne `selected` (clip principal, ou 0)
et `selection` (IDs des clips sélectionnés). `select_clips` accepte `clips`, une liste
d'IDs entiers positifs, déduplique et étend les groupes liés. Une liste vide désélectionne
tout ; cette opération ne modifie ni le document ni l'historique. La sélection est un
état de session partagé avec l'interface, sans être enregistrée dans le `.sframe`.
Une requête contient au maximum 4096 IDs ; les membres liés sont ajoutés automatiquement.

Les quatre mutations groupées prennent aussi `clips`, qui doit être non vide.
`move_clips` prend `delta` en secondes et déplace chaque membre une seule fois, avec ses
animations. `duplicate_clips` retourne les IDs des copies et conserve les distances
entre clips ; les copies commencent après la fin du bloc et reçoivent de nouveaux
groupes. `split_clips` prend `time` et coupe les groupes qui traversent la tête de lecture ;
il ignore les autres clips de la sélection. Un membre lié ne pouvant pas être coupé
fait échouer toute l'opération. Chaque mutation correspond à une seule annulation.

`delete_clips` et `delete_clip` acceptent `ripple` (faux par défaut). Avec `ripple: true`,
l'union des intervalles supprimés, limitée à la durée de séquence, est refermée sur
toutes les pistes ; la durée diminue et les animations suivent les survivants déplacés.
Un survivant qui traverse une zone à fermer, une piste verrouillée à déplacer, ou des
membres liés qui recevraient des décalages différents font échouer la transaction entière.
Les espaces vides hors de ces intervalles sont conservés.

## Bus et mixage

Les commandes de canal utilisent `scope` (`track`, `bus`, `master`) et `target`, l'ID
stable de piste/bus. Le master accepte `target: 0` ou un target omis. Une piste vidéo
est refusée. `add_bus` accepte `name` et retourne `bus` ; 64 bus maximum.
`set_mix_channel` accepte `gain` (-80 à +12 dB), `pan` (-1 à +1), `output` (0 master,
sinon ID de bus), `muted`, `locked`, `name`, et `solo` pour les pistes sources.
Le master accepte gain/pan uniquement. Un canal verrouillé refuse les mutations ;
la requête limitée à `scope`, `target`, `locked` permet de le déverrouiller.

`set_send` prend `scope`, `target`, `bus` et, en option, `gain`, `pre_fader`, `enabled`.
Un envoi nouveau vaut -12 dB, après fader, actif. Une destination est unique par
canal ; répéter la commande règle le même envoi. Pistes et bus peuvent envoyer vers
des bus ; le master ne peut pas envoyer. `remove_send` retire cette destination.
Les niveaux d'envoi s'animent avec `set_mix_parameter`/`set_mix_keyframe`, clé
`send.<id du bus>` (-80 à +12 dB), sans index `effect`. Les cycles sont refusés
même avec des envois désactivés. `remove_bus` prend `bus`, reconnecte ses sorties
entrantes au master et retire les envois qui le ciblent ; une source verrouillée
fait refuser toute la suppression. Annuler restaure bus, DSP, animations et raccordements.

`add_mix_effect` accepte `id` (DSP `cydaw.*`) et retourne son index `effect`.
`set_mix_effect` prend `effect`, puis `enabled`/`position` ; `remove_mix_effect` retire
ce DSP. Leurs animations suivent les réorganisations. Ces piles concernent les
canaux entiers et complètent les DSP de clips existants. 64 DSP maximum par canal.

`set_mix_parameter` et `set_mix_keyframe` acceptent `key`, `value`, et éventuellement
`effect` : -1/omis cible gain/pan du canal, sinon un paramètre du DSP à cet index.
Les noms/plages DSP viennent du catalogue CyDAW ; une clé inconnue est refusée.
Les keyframes prennent `time` en secondes de séquence, `curve` 0–3 et `shape` -1–1.
`previous_time` déplace un point en une seule transaction ; un point au nouveau
temps est remplacé. `remove_mix_keyframe` prend `key`, éventuellement `effect` et
`time` ; sans `time`, le paramètre/sa courbe est retiré et le réglage de base redevient actif.
Les automations de canal restent fixes dans la séquence quand ses clips sont déplacés.

`get_audio_graph` retourne une vue de topologie en lecture seule, référence
`shaderixframe.audio-mix` version `2.0.0`, schéma CyDAW 2 : `ports`, `nodes` avec
`id/name/processor/ports`, `connections` avec `source/sourcePort/destination/destinationPort/channels`.
L'ID de nœud 0 désigne la frontière du graphe ; les ports sont audio stéréo et déclarent
direction et acceptation de plusieurs câbles. Les IDs de nœud sont reconstruits à chaque
requête. Réglages et courbes complets restent dans `status.project` (schéma 9).
La lecture, les WAV et les MP4 utilisent le même routage ; les rendus en arrière-plan
conservent celui de leur instantané, même si le mixeur est édité pendant le rendu.

`start_export` prend `kind` (`video` : MP4 H264/AAC ou AVI RGB/PCM selon l'extension ;
`audio` : WAV stéréo 48 kHz ; `images` : nouveau dossier PNG ; `analysis` : JSON LUFS),
`path` et un `preset` optionnel (défaut `project`). `list_export_presets` décrit
les cinq presets. AVI non compressé est limité à 2 Gio. Il retourne immédiatement
`job`, sans attendre le rendu ni changer le document, son historique ou le transport.
Les outils peuvent continuer à éditer la séquence ou en ouvrir une autre pendant le rendu.
Le worker utilise une copie de la séquence et du catalogue d'effets au lancement, avec
ses propres décodeurs, DSP et moteur GPU. Les contenus des médias sur disque ne sont
pas copiés dans l'instantané.

`export_status` avec `job` retourne `export`, sans `job` retourne `exports`, du plus
récent au plus ancien. `status` inclut aussi cette liste. Tous les jobs actifs/en attente
et les huit derniers jobs terminés sont retenus ; les IDs sortis de cette liste deviennent introuvables. Chaque état fournit
`job`, `kind`, `format`, `preset`, `state`, `path` absolu, `project`, `width`, `height`, `fps`, `duration`,
`progress` (0–1), `elapsed` (secondes), `cancellable` et `error`.
Les états sont `queued`, `running`, `cancelling`, `finalizing`, `completed`, `cancelled`
ou `failed`. Seul `completed` garantit que le fichier a été finalisé et remplacé ;
sa progression vaut 1. Les erreurs de codec, shader ou fichier apparaissent dans un
job `failed`, sans écraser la destination précédente.

`cancel_export` prend `job` et retourne `accepted` ainsi que l'état `export` courant.
Avec `accepted: true`, l'annulation est enregistrée avant le remplacement final du
fichier. Attendre un état terminal pour constater son nettoyage. Après cette frontière,
pendant `finalizing` ou après la fin, `accepted` vaut faux. Les appels codec/GPU déjà
engagés doivent terminer avant l'annulation ; elle n'est pas forcément instantanée.
Fermer stdin ou l'application annule les jobs encore annulables et attend leur arrêt.

La file accepte 32 jobs actifs/en attente ; un seul s'exécute à la fois. Un chemin
de destination déjà réservé par un job actif est refusé. Un job en attente peut
être annulé immédiatement. Les anciens outils `export_video` et `export_audio`
restent synchrones pour préserver leur contrat ; ils sont refusés pendant un export
en arrière-plan. `render_frame` reste une opération synchrone sur l'aperçu courant.

`add_title` prend `text`, `start` et `duration` et retourne `clip`. `set_title` cible ce
clip et accepte `text`, `font`, `alignment` (0 gauche, 1 centre, 2 droite), `bold` et
`italic`. Les paramètres animables comprennent `text_outline` (0–16), `text_shadow` (0–1),
`text_shadow_x/y` (-256–256), `text_shadow_blur` (0–32), `font_size` (4–1024 pixels de séquence) et
`text_r`, `text_g`, `text_b`, `text_a` (0–1). Les transformations/fondus/effets de clip
s'appliquent aussi au texte ; la police doit être disponible sur la machine.

`set_mask` cible un clip vidéo avec `shape` (`none`, `rectangle`, `ellipse`) et
`inverted`. Utiliser `set_parameter`/`set_keyframe` pour `mask_x` et `mask_y` (offsets
normalisés autour du centre, -2 à 2), `mask_width` et `mask_height` (fractions du cadre,
0.001 à 4), `mask_rotation` (degrés, -36000 à 36000), `mask_feather` (0–1024 pixels).
Les dimensions et couleurs hors plage sont refusées avant de modifier la séquence.

`set_transition` cible un clip vidéo avec `clip`, et accepte `id`, `duration`, `enabled`
et `reset_progress`. Les IDs livrés sont `transition.crossfade` et `transition.wipe` ;
`list_effects` identifie ces shaders par `kind: "transition"`. `id` charge/embarque le code
et active la transition. La durée doit être positive et ne pas dépasser celle du clip.
`enabled: false` suspend la transition en conservant ses réglages. `remove_transition`
retire aussi tous les paramètres/animations dont la clé commence par `transition_`.

Les deux entrées sont des frames complètes : 0 est la composition après ce clip et 1
la composition des calques dessous. Les effets/transformations/masques du clip sont
déjà appliqués. Les calques au-dessus sont composités ensuite. Superposer le plan entrant
au plan sortant pendant la durée voulue ; les outils ne déplacent pas les clips et
ne changent pas leurs fondus audio. `overlap_transition` automatise ce placement
pour deux clips adjacents et leurs sons liés. Une transition peut aussi partir du noir si le
clip ne couvre aucun autre calque vidéo.

`transition_progress` (0–1) remplace la progression automatique calculée depuis le début
du clip. `set_keyframe` anime cette progression, `transition_softness` (0–1) et
`transition_direction` (0 droite, 1 gauche, 2 bas, 3 haut) pour le balayage.
`reset_progress: true` retire valeur et keys de progression pour revenir au temps automatique.
Les déplacements/duplications décalent ces keys comme les autres paramètres de clip.
La découpe garde l'entrée uniquement sur le segment gauche ; le trim limite sa durée.

`set_transition_shader` prend `clip` et `code` et compile le HLSL avant application.
Utiliser `InputUV(0/1, uv)`, `Input(0/1, px)` et `ctx.transitionProgress` ; `HasInput(1)`
est vrai pendant ce passage. Les paramètres déclarés doivent commencer par `transition_`,
à l'exception de `transition_progress`, réservé au calcul dans `ctx.transitionProgress`.
Le code est embarqué et les animations restent conservées lors d'une recompilation.
Ces mutations vérifient les pistes verrouillées et se restaurent par annulation.

`set_effect` prend `clip`, `effect`, et optionnellement `enabled` et `position`.
`position` est l'index final dans la pile ; l'effet déplacé conserve ses animations.
`remove_effect` prend `clip` et `effect`. Ces commandes fonctionnent sur les piles
HLSL et DSP. Après une réorganisation, les index des autres effets peuvent changer.

La récupération automatique est active avec une fenêtre visible. Pour l'activer en
mode `--hidden`, fournir `--recovery-dir=CHEMIN`. Les instantanés sont repris depuis le
panneau de récupération lors du lancement normal. Ouvrir ou créer un projet conserve
la séquence précédente non enregistrée et réinitialise l'historique.

Exemple de paramètres de `tools/call` :

```json
{"name":"set_keyframe","arguments":{"clip":2,"key":"opacity","time":1,"value":0.5,"curve":1}}
```

`curve` : 0 linéaire, 1 douce, 2 palier, 3 CyDAW ; `shape` règle cette dernière de -1 à 1.
Les fichiers produits utilisent les chemins fournis. Le MCP dispose des droits du
processus local, comme l'interface ; l'autorisation d'exécution des outils appartient au
client MCP. Il ne s'agit pas d'un serveur distant ni d'un sandbox de shaders arbitraires.

Implémentation fondée sur les spécifications officielles des
[outils](https://modelcontextprotocol.io/specification/2025-06-18/server/tools) et du
[transport stdio](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports).

## Outils ajoutés en 0.8

| Outil | Arguments et résultat |
|---|---|
| `ripple_trim` | `clip`, `delta` (secondes), `left` optionnel : déplace les clips suivants et la fin de séquence |
| `roll_trim` | `clip`, `next`, `delta` : déplace la coupe entre deux groupes adjacents sur une piste |
| `overlap_transition` | `clip`, `next`, `duration` : avance/étend l'entrée et ajoute transition/fondus liés |
| `remove_range` | `begin`, `end` : découpe les clips qui traversent puis referme l'intervalle |
| `set_bezier_mask` | `clip`, `points` : 3–16 tableaux `[x,y,inX,inY,outX,outY]`, coordonnées centrées [-2,2] |
| `start_mask_tracking` | `clip`, `begin`, `end`, `radius` optionnel (1–64, défaut 24) ; retourne `job` |
| `mask_tracking_status` | Sans arguments ; retourne `tracking` avec état/progression/erreur/nombre de points |
| `cancel_mask_tracking` | Sans arguments ; retourne `accepted` |
| `apply_mask_tracking` | `job` terminé ; applique les keys si le clip n'a pas changé, avec historique |
| `set_dsp_graph` | `scope` clip/track/bus/master, `clip` ou `target`, `graph`, `effect` optionnel ; retourne l'index |
| `get_dsp_graph` | Même cible, `effect` requis ; retourne le `graph` embarqué |
| `add_external_audio` | Même cible, `kind` native/vst3, `path` ; retourne `effect` et contrôles normalisés |
| `prepare_proxy` | `clip` vidéo chargé, `path` optionnel en GUI (requis sans cache interactif) ; retourne le job d'export |
| `set_preview_proxies` | `enabled` ; change seulement le choix de décodeur d'aperçu |
| `list_export_presets` | Sans arguments ; retourne `presets` |

Le graphe DSP est le JSON stéréo de l'adaptateur CyDAW v2 (schema 2), illustré par
`examples/ParallelEQ.json` : nœuds `{id,processor,params}` et connexions
`{source,destination}`, frontière 0. Il accepte les DSP du catalogue et `sf.sum`,
refuse cycles/ports invalides, et reste embarqué dans la pile. Il ne charge pas un
projet CyDAW complet. L'interface propose un canevas visuel et un onglet JSON.

`set_parameter`/`set_keyframe` sur un clip audio acceptent `preserve_pitch` (0/1)
et `stretch_mode` (0 repitch, 1 tonal, 2 rythmes, 3 texture).
Les contrôles natifs sont `p0` à `p63`, les VST3 jusqu'à `p511`, dans [0,1].

## Outils ajoutés en 0.9

| Outil | Arguments et contrat |
|---|---|
| `set_color_management` | `space`: srgb/linear709/linear2020 ; `tone`: clip/reinhard/aces ; `exposure` EV ; `backgroundAlpha` 0–1 ; `imageFormat`: png8/png16/exr16 |
| `set_source_color` | `clip`, `space`: auto/srgb/linear709/linear2020, `premultiplied` bool |
| `inspect_ofx` | `path` .ofx/bundle x64 ; `identifier` facultatif ; retourne `plugin` et ses contrôles |
| `add_ofx_effect` | `clip`, `path`, `identifier` facultatif ; retourne l'index `effect` |
| `show_plugin_editor` | cible audio `scope` clip/track/bus/master, `clip` ou `target`, `effect`, `visible` bool |
| `capture_plugin_state` | même cible ; capture état opaque VST3 et valeurs de contrôles |
| `get_plugin_state` | même cible ; retourne `state` base64 et `parameters` noms des contrôles |
| `set_plugin_state` | même cible + `state` base64 strict (4 Mio décodés maximum) |
| `save_plugin_preset` | même cible + `path` .sfvstpreset ; sauvegarde atomique de l'état |
| `load_plugin_preset` | même cible + `path` ; identité du plugin vérifiée, courbes conservées |

`render_frame` accepte désormais `format` png8/png16/exr16 ; une extension `.exr`
choisit EXR par défaut. Les séquences d'images suivent `project.imageFormat`.
Les OFX utilisent `set_parameter` / `set_keyframe` avec leur index d'effet et des
valeurs en unités du plugin, contrairement aux paramètres VST3 normalisés.
Le format EXR garde les pixels linéaires prémultipliés avant tone mapping ; PNG16
utilise le même affichage SDR que l'aperçu, avec quantification 16 bits et alpha droit.
Les jobs de suivi utilisent au plus 1800 frames et mesurent une translation ; ils
ne remplacent pas une rotoscopie avec déformation. Les rapports LUFS distinguent
crêtes d'échantillons et vraie crête FIR ×4 (`true_peak_measured: true`).

## Étalonnage ajouté en 0.10

Les cibles de correction utilisent `clip` (vidéo) et `effect` (index dans sa vraie pile).
Le graphe d'étalonnage est une chaîne de corrections, indépendante des graphes
Shaderixel. `set_shader` et `set_graph` refusent de remplacer une correction native.

| Outil | Arguments |
| --- | --- |
| `add_grade` | `clip`, `name` facultatif ; retourne l'index `effect` |
| `set_grade` | cible + `settings` : name, curves, maskPoints, lut, lutInput, lutOutput |
| `set_grade_curve` | cible + `curve`, `values` (2..32 paires [x,y], extrémités x=0/1) |
| `import_grade_lut` | cible + `path` .cube, `input`/`output` facultatifs ; données incorporées |
| `neutral_grade` | cible + `r`,`g`,`b` linéaires Rec.709 mesurés avant correction |
| `list_grade_presets` | sans arguments ; cinq presets natifs |
| `apply_grade_preset` | cible + `name` ; remplace les réglages et animations |
| `save_grade_preset` | cible + `path` .sgrade ; écriture atomique |
| `load_grade_preset` | cible + `path` .sgrade ; validation et remplacement |
| `video_scopes` | `time` facultatif, `bins` bool facultatif ; retourne `scopes` |

Les espaces source/LUT incluent `logc3-awg3-ei800`, en plus de srgb/linear709/linear2020.
Le working space du projet reste srgb/linear709/linear2020. Les paramètres numériques
sont animés avec `set_parameter` et `set_keyframe`, et réordonnés/activés avec
`set_effect`. Exemple :

```json
{"name":"add_grade","arguments":{"clip":2,"name":"Balance"}}
{"name":"set_parameter","arguments":{"clip":2,"effect":1,"key":"exposure","value":0.5}}
{"name":"set_grade_curve","arguments":{"clip":2,"effect":1,"curve":"master","values":[[0,0],[0.5,0.6],[1,1]]}}
{"name":"video_scopes","arguments":{"time":1,"bins":false}}
```

L'index retourné par `add_grade` doit remplacer l'index illustratif 1 ci-dessus.
Les noms de contrôles se trouvent dans `src/render/Grading.cpp`. Les courbes de teinte
doivent avoir des y identiques à x=0 et x=1. `mask_mode` : 0 aucun, 1 rectangle,
2 ellipse, 3 polygone ; `maskPoints` contient 3..16 paires x/y centrées, transformées
par position/taille/rotation. Feather en coordonnées normalisées du clip.
`start_mask_tracking` accepte `effect` pour suivre le masque de cette correction ;
sans cet argument, il suit le masque de découpe du clip comme auparavant.

Les bins des scopes sont des comptes en ordre ligne par ligne ; waveform 256×128,
parade trois plans 128×128 (R/G/B), vectorscope 256×256, histogramme trois plans de
256 comptes. La ligne zéro des traces est en haut (blanc) ; la ligne basse est noire.
Le rapport distingue les traces SDR des statistiques linéaires, ignore l'alpha nul
et donne le nombre d'échantillons analysés.

## Graphes des calques ajoutés en 0.11

`get_layer_graph` retourne `graph` et `explicit`. Un graphe implicite est dérivé
sans modifier le document. `set_layer_graph` prend `graph`, valide les connexions
avec Shaderixel et rend la composition explicite, avec undo/redo.

Les nœuds portent `id`, `kind` (`surface`, `effect`, `blend`, `split`, `output`),
`name`, `pos` et `inputs`. `surface` référence une piste vidéo ; `inputChannels`
sélectionne les sorties Split (0 RGBA, 1 R, 2 V, 3 B, 4 A). Un effet incorpore
`effect.id`, `code` ou `graph` EffectGraph, et éventuellement `grading`.
Les paramètres de nœud sont dans `params`, comme nombres ou `{value, keys}`.
`output` désigne le nœud output. Le graphe est identifié par
`schema: 1`, `engine: "shaderixel.composition"`. Voir `examples/LayerGraphStudy.sframe`.

`set_layer_parameter` prend `node`, `key`, `value` et conserve les clés.
`set_layer_keyframe` ajoute/remplace un point avec `node`, `key`, `time`, `value`,
`curve` (0 linéaire, 1 douce, 2 palier, 3 courbée) et `shape` (-1..1).
Les temps sont ceux de la séquence. Pour une fusion, la clé est `opacity`.
Pour un effet, ce sont les paramètres de son shader, dont ceux de l'étalonnage.

```json
{"node": 5, "key": "opacity", "time": 4, "value": 0.7, "curve": 1}
```

Les fusions conservent aussi `blend` (mode Shaderixel 0..26), `clip`, `channelOff`
(bits R/V/B/A conservés), `blendIf` (8 octets gris : quatre poignées calque puis
quatre fond) et `blendIfRGB` (24 octets, mêmes poignées pour R/V/B).
Les graphes DSP acceptent désormais des paramètres animés dans chaque nœud,
avec la même représentation `{value, keys}`.

## Conteneurs et motion design, 0.12

| Outil | Arguments / résultat |
|---|---|
| `make_clip_container` | `clip` vidéo ; incorpore son contenu dans une timeline locale |
| `get_clip_composition` | `clip` ; retourne `project`, null si ce n'est pas un conteneur |
| `set_clip_composition` | `clip`, `project` schema 1..10 ; remplace le document local validé |
| `set_motion_widget` | `clip`, `layout` ; ou `enabled: false` pour retirer le layout |
| `get_motion_widgets` | `time` facultatif ; retourne les layouts et rectangles évalués |

Le layout accepte `type` (rectangle, ellipse, text, image, group, container), `parent`
(ID de clip, 0 ou omis pour le canvas), `anchorX/Y`, `pivotX/Y` dans 0..1,
`hidden` et `clipChildren`. `set_parameter`/`set_keyframe` animent `widget_x`,
`widget_y`, `widget_width`, `widget_height` en pixels. Les cycles sont refusés.
Les sélections de parents incluent leurs descendants pour déplacer/copier/supprimer.

Les projets incorporés ont leur propre `layerGraph`, leurs effets et pistes audio.
Les temps des clés y sont locaux. `timeOrigin` règle l'origine du temps shader global.
Les mutations sont annulables et restaurent tout le projet en cas de validation échouée.
Pour modifier un graphe local : lire le projet incorporé, modifier son `layerGraph`
puis le transmettre à `set_clip_composition`.

`add_generator` accepte désormais `id`, notamment un ID `shaderixel.*` retourné
par `list_effects`. Le shader et ses paramètres sont incorporés au clip. `list_effects`
précise `generator` et `inputs` pour le catalogue vidéo.

`import_grade_lut` accepte `layout` : `auto` (défaut), `atlas` ou `rg`, pour une
image LUT 2D. Les .cube restent des tables 3D ; les données sont incorporées au projet.
Les nœuds de transition du graphe utilisent un shader à deux entrées et une clé
`progress` animable via `set_layer_keyframe`.

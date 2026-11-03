# ShaderixFrame 0.13.0

Cette version refait l'interface sur le modèle de **CyDAW v2** : des modes de travail et une
timeline reprise de l'ArrangementView et de la timeline Cinema. ShaderixFrame reste basé sur
**Shaderixel**, dont la bibliothèque complète d'effets est à nouveau synchronisée.

## Modes de travail

La barre du haut choisit un mode, comme dans CyDAW : **Montage** (F1), **Effets** (F2),
**Motion** (F3), **Étalonnage** (F4), **Audio** (F5) et **Livraison** (F6). Elle porte aussi
le transport, la boucle et le timecode.

- Chaque mode a sa disposition par défaut ; la timeline est présente dans tous.
- Chaque mode retient sa disposition : panneaux ouverts, onglets, tailles et panneaux déplacés,
  d'une session à l'autre. **Affichage > Réinitialiser ce mode** revient à l'origine.
- Un panneau détaché sur un autre écran reste en place quand on change de mode.
- En mode Motion, l'éditeur de courbes est visible au-dessus de la timeline.

## Timeline

- **Navigateur** au-dessus de la règle : glisser pour naviguer, vers le bas pour zoomer.
- **Règle timecode** avec repères (M), **zone de travail** (tirer en haut de la règle, I/O)
  rejouée par la boucle (L), et liste des repères au clic droit.
- **Pellicule image par image**, comme le mode Cinema : la rangée **IMAGE** montre le rendu
  final et se parcourt à la souris ; chaque clip vidéo montre ses images et leur timecode.
  Les vignettes sont alignées sur les images ; jusqu'à 20 000 px/s, chaque image apparaît.
- **En-têtes de piste** : couleur, renommage, masquer/muet, solo, verrou, automation,
  vumètre, hauteur réglable, réordonnancement par glisser, colonne élargissable, piste cible (C)
  et, pour une piste audio haute, boutons rotatifs CyDAW de gain et de panoramique.
- **Fermer l'espace** au clic droit dans un vide entre deux clips ; **Soulever** ou **Extraire**
  la zone de travail depuis la règle ; **Dupliquer la piste** ; `Alt` + flèches décale la sélection
  d'une image (`Maj` : dix).
- **Groupes de pistes** repliables (`Ctrl+G`), comme les dossiers de CyDAW, avec aperçu des
  clips quand le groupe est replié et M/S de groupe.
- **Clips** : couleur au choix, badges (effets, vitesse, conteneur), poignées de fondu,
  points d'animation en losanges, ligne de gain audio sur l'échelle CyDAW, forme d'onde
  exacte au zoom (minimum/maximum, puis échantillons).
- **Outils** : Sélection (V), Lame (B), Slip (Y), Roll (N), Étirement de vitesse (R),
  Glissement (U), Main (H), trim **RIPPLE**, magnétisme aux images et aux bords des clips,
  repères, zone et tête, avec un repère visuel du point d'aimantation.
- **Effets depuis la timeline** : le badge « fx » ouvre la pile d'effets du clip ;
  clic droit > **Ajouter un effet** cherche dans la bibliothèque Shaderixel (ou les DSP CyDAW
  pour l'audio). La poignée au bout d'une transition règle sa durée.
- **Lanes d'automation** sous chaque piste, pour les paramètres des clips et de leurs effets.
- **Désactiver un clip** (`Maj+E` ou clic droit) : il reste en place, hachuré, mais n'est plus
  rendu ni mixé ; le groupe lié suit. **Vitesse / durée** (`Ctrl+R` ou clic droit) règle la
  vitesse en pourcentage ou la durée, la portion de source restant la même.
- **Couper à la tête** : `Q` retire le début du clip jusqu'à la tête, `W` la fin, en refermant
  l'espace (`Alt` : sans refermer). **Normaliser** (clic droit sur un clip audio) amène sa crête
  à -1 dBFS. Côté MCP : `trim_clip` et `normalize_audio`.
- **Arrêt sur image** (`Maj+F`, ou clic droit : 1, 2 ou 5 s) : le clip est coupé à la tête, la
  séquence s'ouvre et l'image figée s'y place (badge ARRÊT, valeurs animées figées, sans son).
  **Insérer un espace** (clic droit sur une piste ou la règle) ouvre la séquence sur toutes les
  pistes ; repères et points d'animation suivent. Côté MCP : `freeze_frame`, `insert_gap`.
- **Navette** : `J` lit à l'envers, `Maj+L` plus vite, chaque nouvel appui double la vitesse (×8),
  `K` arrête ; l'image seule suit, la vitesse s'affiche près du timecode. `L` reste la boucle.
- **Sélection** : `A` choisit tout ce qui suit la tête, `Alt+A` ce qui précède, et le clic droit
  la suite d'une seule piste. **`Ctrl+F`** cherche un clip par son nom, sa piste ou son fichier ;
  Entrée le sélectionne et y amène la tête.
- **Montage sur la musique** : clic droit sur un clip audio > **Repères sur les attaques** (peu,
  normal, beaucoup) pose un repère à chaque attaque ; **Couper aux repères** (clic droit sur un clip,
  la règle ou le panneau Repères) coupe la sélection ou toutes les pistes à chacun. Extraire, le trim
  ripple et la suppression ripple déplacent désormais les repères avec la séquence.
- **Montage automatique** : clic droit sur un clip avec du son > **Supprimer les silences** (prudent,
  normal, serré) retire les silences en gardant une marge autour de la parole, la séquence se
  refermant sur toutes les pistes (`remove_silences`) ; clic droit sur une piste audio > **Baisser sous
  la voix** pose un ducking de -12 dB lissé pendant la parole d'une autre piste (`auto_duck`) ; clic
  droit sur un clip vidéo > **Détecter les plans** le coupe, ou pose un repère, à chaque changement
  de plan (`detect_scenes`). Silences et plans sont analysés dans un thread à part, avec leur
  progression et un bouton d'annulation dans la barre d'état ; le résultat s'applique en une étape.
- **Titres** : un nouveau titre va sur une piste de titres libre à ce moment au lieu d'en créer une à
  chaque fois.
- **Versions** : chaque enregistrement garde le fichier remplacé (dix versions au plus, dossier
  « <projet> versions ») ; **Fichier > Versions précédentes** en rétablit une, annulable par `Ctrl+Z`
  (`list_versions`, `restore_version`).
- **Lire à l'envers** (clic droit sur un clip issu d'un média) : le clip et son groupe lié lisent leur
  portion de source de la fin vers le début, son compris, badge ENVERS (`set_clip` `reverse`).
- **Rampes de vitesse** : le paramètre « Vitesse (rampe) » s'anime dans l'éditeur de courbes ; le
  temps de source suit son intégrale, image et son (le son change de hauteur, comme une bande).
- **Menu des clips réorganisé** : découper, copier, couper, coller, dupliquer et désactiver en haut ; le
  reste rangé dans les sous-menus Montage, Image, Audio, Animation prédéfinie, Effets et Clip ;
  supprimer en bas. « Zoom sur la sélection » depuis ce menu cadre désormais selon la largeur de la
  timeline (il prenait celle du menu).
- **Titres** : une boîte arrondie derrière le texte (opacité, couleur, marge), aussi réglable pour
  toute une piste de sous-titres (« Fond ») ; **Style de titre** (clic droit) : classique, sous-titre
  jaune, boîte noire, cinéma ou néon, l'ombre et le contour pouvant désormais être colorés
  (`apply_title_style`) ; animation **Machine à écrire** (le texte s'écrit sans bouger) ; **Bandeau nom / fonction…** (menu + de la
  timeline ou clic droit sur une piste vidéo, nom et fonction saisis) : deux lignes à gauche en bas sur une boîte sombre, glissées puis
  fondues (`add_lower_third`).
- **Incrustation** (clic droit sur un clip vidéo) : placer en haut ou en bas, à gauche ou à droite, au
  centre ou en plein cadre, la taille suivant le format réel du média (`place_clip`) ; **écran partagé**
  de 2 à 4 clips côte à côte, l'un sur l'autre ou en quatre cadrans (`split_screen`) ; et détourer
  un fond vert ou bleu (`key_background`).
- **Améliorer la voix** (clic droit sur un clip audio) : porte de bruit douce, égaliseur et compresseur
  CyDAW ajoutés en un clic, réglables ensuite (`enhance_voice`).
- **Micro-fondus** (clic droit sur une piste audio) : un fondu de 50 ms de part et d'autre de chaque
  coupe supprime les clics aux raccords (`audio_cut_fades`).
- **Calques d'effets** (clic droit sur une piste vidéo > **Calque d'effets ici**) : un clip sans média
  dont les effets Shaderixel s'appliquent à tout ce qui est en dessous, sur sa durée ; son opacité
  dose le résultat (`add_adjustment_layer`).
- **Livraison** (menu Fichier) : **Exporter la sélection en MP4** (l'étendue des clips choisis),
  **Exporter chaque chapitre en MP4** (un fichier par segment entre repères, `export_chapters`) et
  **Exporter un GIF animé** de la zone de travail, 480 px à 15 ips, rendu en arrière-plan (`export_gif`).
- **Multicam** : plusieurs prises superposées sur des pistes vidéo (synchronisées par le son, par exemple),
  les touches `1` à `4` coupent vers un angle à la tête ; les autres restent en place, désactivés
  (`switch_angle`). Le panneau **Angles** montre l'image de chaque angle sous la tête, l'angle actif
  encadré ; un clic y coupe. **Le son suit l'angle** (coché par défaut) coupe et active aussi le son lié
  de chaque prise.
- **Étalonnage automatique** (clic droit > Image) : balance des blancs par la moyenne grise et exposition
  vers un gris moyen de 18 %, mesurées sur l'image du clip sous la tête (`auto_grade`). Avec deux clips
  sélectionnés, **Assortir les couleurs à l'autre clip** amène la couleur moyenne et la luminosité du clip
  sur celles de l'autre (`match_grade`) ; au-delà de la plage de la balance des blancs, une seconde correction
  prend le relais. Avec trois clips ou plus, **Assortir toute la sélection à ce clip** les règle tous d'un coup.
- **Démarrage** (menu Fichier) : **Nouveau depuis un modèle** (vidéo YouTube, verticale, podcast vidéo, court
  métrage : format, cadence et pistes nommées) et **Projets récents** (dix derniers ouverts ou enregistrés).
  Sur une séquence vide, la Composition affiche un **accueil** : modèles, ouvrir, importer et projets récents.
- **Échange** (menu Fichier) : **Exporter l'EDL (CMX 3600)** de la piste vidéo choisie pour Premiere, Resolve
  ou Avid (`export_edl`), **Importer une EDL…** qui reconstruit la piste et son son, les médias absents restant
  hors ligne pour « Relier les médias » (`import_edl`), et **Exporter la liste des plans (CSV)** pour la production (`export_shot_list`).
- **Préréglages d'export** : vertical 1080p à 60 i/s (Shorts, Reels, TikTok), carré 1080 (Instagram), 4K UHD. Les proxys automatiques valent aussi pour un média glissé sur la timeline.
- **Synchroniser par le son** : deux clips sélectionnés, clic droit sur celui à déplacer > **Synchroniser
  sur l'autre clip par le son** ; la concordance s'affiche (`sync_audio`), pour aligner une caméra et
  un enregistreur séparé.
- **Animations prédéfinies** (clic droit) : fondus entrant et sortant, glisser vers le haut, zoom
  avant lent, Ken Burns, et rampes accélérer, ralentir ou ralenti au milieu (`apply_animation`) ; une
  rampe s'applique au groupe lié. L'éditeur de courbes affiche les noms des paramètres en français.
- **Coller les attributs** (`Ctrl+C` sur un clip, puis `Ctrl+Alt+V` ou clic droit sur d'autres) :
  effets (ajoutés ou remplacés), transformations, opacité, gain et panoramique, masque, au choix ;
  les animations suivent le début de chaque clip (`paste_attributes`).
- **Survol du chutier** : passer la souris sur une carte vidéo fait défiler ses images, avec le
  timecode, décodées par un lecteur à part et limitées à une toutes les 40 ms.
- Lecture vidéo plus sûre : un pas en arrière d'une seule image déclenche désormais la recherche
  (avant, il lisait l'image suivante) ; en ralenti, l'image décodée reste tant qu'elle couvre le temps
  demandé (avant, le décodeur prenait de l'avance puis cherchait en arrière) ; et une vidéo qui
  refuse la recherche est rouverte et relue jusqu'à l'image demandée.
- **Pré-rendu** (`Entrée` dans la timeline, ou Séquence) : la zone de travail, sinon la séquence, est
  rendue en mémoire (1,5 Go au plus) par un thread à part ; la lecture affiche ces images sans
  recalculer les effets. Une barre verte sous la règle montre ce qui est prêt et la progression
  s'affiche près du timecode ; toute modification du document efface le pré-rendu.
- **Fondu enchaîné** à la coupe vidéo la plus proche de la tête (`Maj+D`) : une seconde au plus,
  raccourcie à la poignée de source disponible. `Ctrl` + flèches sautent au point d'animation
  précédent ou suivant de la sélection.
- **Lire autour** (`Maj+Espace`) : deux secondes avant la tête jusqu'à deux secondes après, puis
  retour à la tête.
- **Renommer en série** (clic droit) : modèle `Plan ##` ou `{nom} v#`, premier numéro, aperçu des
  premiers noms ; ordre de la timeline (`rename_clips`). **Sélectionner la même couleur**.
- Copier, couper, coller (`Ctrl+C/X/V`), suivi de lecture, défilement personnalisé et
  bouton **?** listant tous les raccourcis.
- Menu **Vue** : rangée IMAGE, vignettes, formes d'onde et points d'animation affichables ou non,
  hauteurs des pistes, groupes repliés/dépliés d'un coup, lanes masquées. **Z** cadre la sélection.
- Les vignettes de clips gardent leur document isolé d'une image à l'autre : reconstruit dès que la
  structure du clip change, revérifié quelques fois par seconde pour les valeurs de paramètres.
- Molette : pistes ; `Maj` : temps ; `Ctrl` : zoom ; `Alt` : hauteur des pistes.
- **Palette de commandes** (`Ctrl+Maj+P`, Affichage > Palette de commandes) : une cinquantaine de
  commandes (outils, montage, repères, lecture, fichiers, sonorité, modes, panneaux) trouvées par
  leur nom sans tenir compte des accents, avec leur raccourci actuel ; flèches et `Entrée`.
- **Régions** : un repère peut avoir une durée (clic droit > Région jusqu'à la tête, sur la zone de
  travail, ou Ajouter une région sur la zone de travail) ; bande sur la règle et teinte des pistes,
  **Zone de travail sur la région**, bouton de durée dans le panneau Repères. Une insertion
  l'allonge, une extraction la raccourcit (`duration` de `add_marker`/`set_marker`). La fin d'une
  région se tire sur la règle (une étape d'annulation) ; déplacer le repère garde la région dans la
  séquence. Fichier > **Exporter chaque région en MP4** (`export_chapters` avec `regions`), et le
  CSV des repères a une colonne Durée.
- Les noms des repères ont un fond sur la règle : les timecodes ne se lisent plus au travers.
- **Ajuster à l'espace** (Source > Ajuster à la zone, `Maj+.`) : la portion marquée écrase la zone
  de travail avec la vitesse qui la fait durer exactement (×0,05 à ×20 ; `edit_source` mode `fit`).
- La **palette de commandes** reprend aussi les commandes du clip choisi : vitesse/durée, lecture
  à l'envers, étalonnage automatique, normalisation, amélioration de la voix, silences, attaques,
  renommage en série, collage d'attributs, calque d'effets, titre et bandeau.
- **Fermer les espaces** : d'une piste (clic droit sur la piste ; ses clips avancent avec leurs
  membres liés) ou de toute la séquence (menu Séquence, là où aucune piste n'a de clip).
- **Caler sur les repères** (clic droit > Montage) : les clips choisis commencent au premier puis à
  chaque repère et durent jusqu'au suivant, bornés par leur source ; avec les repères sur les
  attaques, un montage au rythme (`close_gaps`, `fit_to_markers`).
- **Raccourcis personnalisables** : le panneau Raccourcis (ou **?** > Personnaliser) réaffecte les
  touches des outils, coupes, sélection, repères, zone de travail et navette par capture ; un même
  accord pris deux fois est signalé, **Rétablir les touches par défaut** revient à l'origine. Seules les
  touches modifiées sont écrites dans `keymap.json`, à côté des réglages des panneaux.

## Chutier et moniteur Source

L'onglet **Médias** devient un chutier en vignettes (recherche, **+ Chutier** pour importer sans
placer). Le panneau **Source** ouvre un média, le fait défiler image par image, marque entrée et
sortie, et le place à la tête de lecture par **Insérer** (la séquence est coupée et la suite décalée
sur toutes les pistes) ou **Écraser** (sur les pistes cibles seulement), ou par glisser sur une piste.
Le bouton **C** d'un en-tête de piste désigne la piste cible des montages (une vidéo, une audio) ;
sans cible, la première piste libre du type est utilisée. L'entrée et la sortie se tirent sur la
barre de Source, et le chutier prépare un proxy de prévisualisation. Source se lit avec le son du média (Espace), K et L (×2, ×4, image seule). **F** dans la timeline
(image correspondante) ouvre le média du clip sélectionné dans Source, à l'image sous la tête.
Côté MCP : `edit_source`. Le projet enregistre son chutier (`bin`, champ facultatif). **Proxys auto**
(case du chutier, cochée par défaut) prépare un proxy 720p de chaque vidéo de plus de 1440 lignes dès
son import ; **Retirer les inutilisés** vide le chutier des médias qu'aucun clip n'utilise
(`remove_unused_media`).
**Remplacer le clip** (Source) donne au clip sélectionné le média ouvert depuis son entrée, à la même
place et pour la même durée, le son lié compris (`replace_source`). Survoler une carte du chutier
affiche le chemin, le type, la taille d'image, la durée, le son, le poids du fichier et ses usages.

## Sous-titres et médias

**Fichier > Importer des sous-titres SRT…** crée une piste de clips titres (bas de l'image, contour
noir), une réplique par clip, éditables comme tout titre ; **Exporter les sous-titres SRT…** écrit
les clips titres au format SubRip. Le panneau **Sous-titres** liste les répliques d'une piste :
texte sur deux lignes modifiable sur place (une étape d'annulation par modification), timecode
(clic : y aller), recherche, **+ Réplique à la tête** au style de la précédente, et taille, hauteur et
contour appliqués à toute la piste. Les médias introuvables sont comptés dans la barre du haut
(« n médias hors ligne ») : un clic, ou **Relier les médias manquants…**, demande un fichier du
dossier où ils sont (sélecteur de dossier Windows) et les relie tous par leur nom. **Rassembler les médias…** copie chaque média
utilisé dans un dossier et y repointe le projet, pour l'archiver ou le déplacer. Côté MCP :
`import_subtitles`, `export_subtitles`, `add_subtitle`, `set_subtitle_style`, `media_status`,
`relink_media`, `collect_media`.

**Séquence > Format** passe la séquence en HD, 4K, 720p, vertical 9:16, carré ou 4:5, et à 24, 25,
30, 50 ou 60 ips (`set_sequence`) ; clic droit sur un clip vidéo > **Cadrage** le remplit ou l'ajuste
au cadre (`fit_clips`), utile pour un plan vertical dans une séquence 16:9 et inversement.

**Séquence > Normaliser la sonorité** mesure le mix entier hors ligne (BS.1770), dans un thread à
part avec sa progression et un bouton d'annulation dans la barre d'état, et règle le master
pour atteindre -14 LUFS (YouTube, Spotify), -16 (podcast), -23 (EBU R128) ou -24 (ATSC) ; la barre
d'état donne la mesure, le gain et la crête vraie estimée. L'option **Limiteur -1 dBTP sur le
master** (cochée par défaut) ajoute le limiteur CyDAW en fin de master ; sans elle, une alerte
signale une crête au-dessus de -1 dBTP (`normalize_loudness`).

## Repères et historique

Le panneau **Repères** (Affichage, ou clic droit sur la règle) liste les repères avec une vignette
du rendu final, leur timecode (clic : y aller), leur nom, une note et leur couleur ; une recherche
filtre les noms et les notes. **Chapitres** copie la liste au format YouTube, **CSV…** l'exporte.
Le panneau **Historique** nomme chaque étape annulable (« Arrêt sur image », « Montage »,
« Courbe d'animation »…) ; un clic revient à une étape, un autre la rétablit. Côté MCP :
`export_markers`, `get_history`, et `note` sur `add_marker` / `set_marker`. **Images…** rend une image
du rendu final par repère (`export_marker_stills`), par exemple pour des miniatures de chapitres.

## Animations

Le panneau **Animations** devient un éditeur de courbes inspiré du `CurveEditor` de CyDAW :
plusieurs paramètres superposés, sélection multiple et au rectangle, déplacement sur les
images, interpolations linéaire/douce/palier/CyDAW, courbure par `Alt` + glisser et barre de
valeurs pour un point. `Ctrl+C` / `Ctrl+V` copient les points choisis et les recollent à la tête
de lecture, espacements conservés, sur le même clip ou un autre. Le tableau des valeurs reste
disponible.

## Vue finale

Le panneau Composition zoome de 25 % à 1600 % (`Ctrl+0`, `Ctrl+1`, molette), se déplace avec
Espace ou le bouton central et affiche une mini-carte et les coordonnées du pixel. Le rendu
suit le zoom jusqu'à la définition de la séquence ; au-delà de 200 %, les pixels sont nets.
**Guides…** affiche les zones de sécurité (action 93 %, titres 90 %), les tiers, le centre et un
format cible assombri autour (2,39:1, 1,85:1, carré, 4:5, 9:16). **Instantané** garde l'image
actuelle comme A : un volet glissant la compare à l'image en cours (B), pratique en étalonnage.
Son clic droit **exporte l'image** courante (PNG ou EXR half, aussi dans le menu Fichier) ou la
**copie** dans le presse-papiers (`Ctrl+Maj+C`), à la définition de la séquence.

## Shaderixel

- **406 effets et générateurs** Shaderixel au lieu de 317, importés par
  `tools/import_shaderixel.py`. L'import adapte trois tournures Slang absentes de HLSL :
  noms réservés (`line`), constructeurs à un argument et variables locales masquant un
  paramètre. La courbe de tons intégrée de Photo RAW reste neutre : les courbes passent par
  l'étalonnage natif.
- Compilateur de graphes mis à jour : les valeurs posées sur les entrées non branchées d'un
  nœud sont prises en compte et réglables dans le Graphe VFX.
- L'éditeur Graphe VFX signale une sortie sans alpha relié (image transparente) et propose de
  la corriger. `examples/GraphStudy.sframe`, qui rendait noir pour cette raison, est corrigé.

## Exports et MCP

- **Exports > Zone de travail seulement** rend la zone, pour tous les formats ; chaque image
  est rendue à son propre temps. `start_export` accepte `range: "work"` ou `begin`/`end`.
- Nouveaux outils MCP : `set_track`, `remove_track`, `move_track`, `add_marker`, `set_marker`,
  `remove_marker`, `set_work_area`, `group_tracks`, `set_group`, `ungroup`, `edit_source`,
  `lift_range`, `duplicate_track`. `set_clip` accepte
  une couleur. Le projet enregistre repères, zone, groupes, couleurs et hauteurs de pistes
  dans des champs facultatifs du schéma 10.

## Effets de piste, son et vue

- **Track Graph** : chaque piste vidéo a son graphe d'effets (comme Shaderixel), appliqué à l'image
  de ses clips avant la fusion avec les pistes du dessous ; vignettes par nœud, Shader Graph et LUT
  par nœud, lecture des pistes du dessous comme entrées secondaires. Bouton **FX** sur l'en-tête de
  piste (allumé s'il y a des effets ; on y dépose un effet de la bibliothèque). MCP :
  `get_track_graph`, `set_track_graph`, `add_track_effect`.
- **Effets CyDAW** : les 50 inserts stéréo de CyDAW (35 nouveaux : reverbs Pro, Halo et à
  convolution, échos à bande et granulaire, compresseur Pression, multibande, de-esser, saturation,
  simulations d'ampli, de baffle et de micro, tranche de console, mastering, transposition et
  correction de justesse, vocodeur, modulateurs d'anneau et de fréquence, résonateurs, utilitaire,
  mesures) ; latence de look-ahead compensée, valeurs lissées comme dans le graphe DSP.
- **Track Effects** : chaîne d'inserts d'une piste audio, d'un bus ou du master en appareils
  CyDAW (marche/arrêt, réordonner par glisser, retirer, boutons rotatifs, catalogue par famille,
  VST3/natif) ; suit le clip son choisi, bouton **FX** des pistes audio ; ouvert en mode Audio.
- **Sortie son** : Séquence > **Sound Output** choisit le périphérique (mémorisé) ; il est rouvert à
  chaque lecture ; un échec n'est plus silencieux (« No sound » dans la barre d'état avec la raison) ;
  un clip hors ligne est sauté au lieu d'arrêter la lecture ; un média déposé depuis le chutier
  pendant son chargement est placé une fois chargé, avec son clip son.
- **Explorateur d'effets** : cartes avec aperçu (rendu en arrière-plan sur une image de référence ou
  sur le clip choisi) pour les effets, fusions, générateurs et transitions Shaderixel, et inserts
  audio CyDAW ; double-clic pour appliquer, glisser vers la timeline ; **Apply LUT File...** pour
  une LUT .cube ou image en correction native.
- **Fusion des pistes** : mode (27 modes Shaderixel), opacité et effet de fusion par piste vidéo
  (menu de la piste, carte de fusion déposée sur **FX**, `set_track` `blend`/`opacity`/
  `blend_effect`), utilisés par le graphe des calques automatique. Une entrée secondaire non
  branchée d'un nœud d'effet est absente (un masque optionnel lit blanc) ; une fusion appliquée en
  effet de clip lit l'image du dessous comme fond.
- **Vue finale** : outil **Main** (`H`), Maj+molette et molette horizontale, glisser dans la marge
  pour se déplacer dans l'image zoomée ; double-clic sur la mini-carte sans retour à l'ajustement.

## Correction du décodage vidéo

Les images H.264 étaient lues à leur taille codée (1080 lignes codées en 1088) : des lignes de
remplissage vertes apparaissaient en bas. Les images sont maintenant recadrées sur la zone
d'affichage du flux.

## Build

Les shaders, plugins et exemples sont copiés dans le dossier de build à chaque build, et non
plus seulement au relink de l'exécutable : une bibliothèque régénérée n'y reste plus périmée.

## Limites

- `Document`/`NodeEditor` (graphes de calques), ImGui et les sources CyDAW n'ont pas été
  resynchronisés : leur version amont a été restructurée et demande d'adapter les extracteurs.
- Une disposition de mode mémorisée avant cette version garde l'ancien placement ;
  **Réinitialiser ce mode** applique le nouveau.

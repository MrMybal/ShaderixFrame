# ShaderixFrame 0.8 — les six chantiers

## Utilisation

- **Médias & effets** : l'import interactif et le décodage d'aperçu s'effectuent dans
  un thread COM séparé. Les vignettes apparaissent après préparation. Le PCM stéréo
  48 kHz est stocké dans `cache/audio`, identifié par chemin, taille et date du fichier,
  puis mappé en lecture seule. Les rendus asynchrones réutilisent aussi ce cache.
  Préparer un proxy du clip sélectionné crée une copie H264 à 720 lignes maximum.
  Le commutateur des proxies concerne l'aperçu ; les exports lisent les originaux.
- **Timeline > Montage avancé** : trim ripple, déplacement de coupe roll, fondu avec
  chevauchement automatique et retrait d'un intervalle avec découpe. Les liaisons,
  animations et verrous sont contrôlés avant la modification. Une transition exige
  une poignée source suffisante avant le clip entrant ; les générateurs/titres n'en
  ont pas besoin. Un ripple ambigu ou un groupe non aligné est refusé.
- **Propriétés** : masques Bézier fermés de 3 à 16 points, poignées entrantes/sortantes,
  translation/rotation/taille/inversion/feather, contours et ombres de texte animables.
  **Suivre le masque** calcule une translation par corrélation, en arrière-plan,
  sur une zone contrastée, avec un rayon de recherche de 1 à 64 pixels sur l'image
  réduite. Limite de 1800 frames. Les résultats deviennent des keyframes `mask_x/y`.
  Le calcul ne s'applique pas si le clip a changé ; il faut le relancer.
- **Audio** : conserver la hauteur active un time-stretch par alignement de grains
  (WSOLA). Les chemins parallèles sont alignés sur la latence déclarée par les plugins,
  avec préchauffage du moteur pour conserver la synchronisation de la séquence.
  Limite de latence cumulée : 10 secondes. Les envois possèdent leurs propres courbes
  `send.<id du bus>`, accessibles avec K comme les autres paramètres du mixeur.
- **Graphe DSP : éditer/importer** : édition JSON d'un graphe embarqué dans une pile
  de clip, piste, bus ou master. Ports stéréo validés par CyDAW v2, branchements
  parallèles et sommes, 128 nœuds/512 connexions maximum. La version initiale accepte
  les 15 DSP du catalogue et `sf.sum`. `examples/ParallelEQ.json` est un exemple.
  Les nœuds conservent des identités stables ; les cycles sont refusés.
  `examples/VfxStudy.sframe` combine ce graphe, un masque Bézier, deux passes HLSL,
  l'historique temporel et un titre avec contour/ombre. `MixerStudy.sframe` anime
  désormais aussi le niveau d'envoi auxiliaire.
- **Plugins natifs/VST3** : choisir un DLL du SDK ou un plugin VST3 dans la pile audio.
  Chaque instance tourne dans son propre processus. Les contrôles normalisés sont
  exposés sous `p0`, `p1`, etc. ; 64 contrôles maximum par instance dans cette version.
  Le fichier et les contrôles sont enregistrés dans le projet. L'éditeur graphique
  propre au VST3 et ses états opaques/presets ne sont pas encore intégrés.
- **Exports** : jusqu'à 32 rendus actifs/en attente, exécutés dans l'ordre. Les huit
  derniers rendus terminés restent visibles. Presets projet, 720p, 1080p web, portrait
  et cinéma 24 fps. MP4 H264/AAC, WAV PCM, AVI RGB/PCM sans compression (2 Gio maximum),
  séquence PNG dans un nouveau dossier avec `sequence.json`, et rapport de sonorité
  JSON. L'annulation d'un job en attente est immédiate. Chaque job conserve son
  instantané ; les médias et DLL doivent rester disponibles pendant son exécution.

## HLSL et formats

Les effets et graphes VFX disposent de deux entrées : `Input(0)` est l'image du clip
avant l'effet et `Input(1)` la composition des calques inférieurs. `//@passes N`
accepte 1–8 passes ; `ctx.pass` commence à 0 et `Prev()` lit la passe précédente.
`History()`/`HistoryUV()` lisent la frame précédente du clip après sa pile d'effets ;
`HasHistory()` permet de gérer la première frame. Une recherche ou une édition remet
cet historique à zéro. Une frame affichée plusieurs fois ne réaccumule pas l'effet.
L'historique est limité à 16 calques ou 256 Mio. La composition reste en BGRA8 à alpha
droit. Les transitions utilisent une seule passe et leurs deux entrées de composition.

Les projets écrivent le schéma 6 et chargent les schémas 1–5. Les fixtures versionnées
dans `tests/fixtures/compat` vérifient les migrations et les graphes des adaptateurs.
`tools/sync_upstream.py --check` vérifie les composants copiés et le moteur VST3.

## Sonorité

La mesure stéréo à 48 kHz implémente la pondération K et les portes de blocs 400 ms
de [l'ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en).
Le mixeur affiche les LUFS momentanés et intégrés ; un export `analysis` calcule un
rapport indépendant. Les crêtes sont des crêtes d'échantillons dBFS. La mesure
true-peak suréchantillonnée et la certification sur la suite officielle restent à faire.
Le time-stretch est testé sur un signal tonal ; les voix, transitoires complexes et
ratios extrêmes demandent encore des essais d'écoute et des améliorations.

## Installation et périmètre vérifié

`ShaderixFrame-0.8.0-Setup.exe` installe pour le compte courant, par défaut dans
`%LOCALAPPDATA%/Programs/ShaderixFrame`, sans élévation. L'archive portable reste
disponible. L'installateur accepte `--install-dir DOSSIER --silent --no-shortcuts`.
`--uninstall DOSSIER` retire uniquement les fichiers installés dont l'empreinte est
inchangée ; projets, récupérations, caches et fichiers modifiés sont conservés.

Les intégrations OFX, HDR/espaces couleur, les projets Shaderixel image/3D complets,
les instruments/MIDI VST3 et le tracking de déformation/rotation ne sont pas compris
dans cette livraison. Aucun SDK OFX exploitable n'a été trouvé dans les sources locales.
Les configurations réellement testées sont consignées dans `VALIDATION_0.8.md`.

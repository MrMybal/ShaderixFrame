# Architecture et contrats — 0.12

## Document et montage

Le cœur C++20 `Project` est indépendant de l'interface. Le `.sframe` JSON écrit le
schéma 10 et charge 1–10 avec valeurs neutres pour les nouveaux champs. Pistes, clips,
groupes et bus ont des IDs stables. Le chargement valide le candidat avant remplacement ;
`AtomicFile` finalise la sauvegarde. Médias/plugins restent référencés par chemin,
résolu depuis le projet quand relatif ; shaders/graphes/contrôles sont embarqués.

Les keyframes sont en secondes absolues : déplacement/duplication les décalent,
trim/roll conservent leurs temps. Groupes liés et verrous sont vérifiés avant une
mutation atomique. Ripple décale l'aval ; roll conserve la fin du clip entrant ;
overlap étend sa poignée et ajoute les fondus liés. `remove_range` découpe les
survivants avant fermeture, tandis que la suppression ripple historique les refuse.
La sélection est un état de session ; l'historique conserve 64 instantanés.
`Recovery` écrit des instantanés séparés toutes les cinq secondes et avant changement
de document/fermeture. Il nécessite un dossier accessible et ne remplace pas les médias.

## Interface et médias

Win32, ImGui docking et Direct3D 11 ; thème Shaderixel adapté et timeline inspirée
de CyDAW v2. WIC lit les images ; Media Foundation décode vidéos BGRA et audio
float stéréo 48 kHz. L'aperçu conserve le ratio et plafonne à 1280×720. `--warp` ou
`SF_FORCE_WARP=1` force D3D11 logiciel pour les essais.

En GUI interactive, `MediaCache::Background` a son thread COM/MF, sa file d'import
et une dernière demande de frame par source. Le thread principal adopte métadonnées,
vignettes, PCM mappé et images via `poll()`. Une génération invalide les anciens
résultats ; huit décodeurs au plus restent côté worker. Pendant une recherche,
l'aperçu peut montrer la dernière frame reçue. MCP/captures utilisent un cache
synchrone pour une vérification déterministe.

Le cache PCM disque est identifié par chemin canonique, taille et date source ;
en-tête/fingerprint/taille sont validés avant mapping Windows en lecture seule.
L'écriture progressive est finalisée atomiquement. Sans cache disque, la limite
RAM reste 512 Mio par média. Il n'y a pas encore de quota/purge automatique.
Les proxies H264 720p sont des jobs de rendu : l'aperçu peut les choisir, les
exports gardent les originaux et les chemins enregistrés ne changent pas.

## Composition et adaptateur Shaderixel

Le compilateur `EffectGraph` copié reste intact. Le prélude vidéo HLSL 5.0 fournit
paramètres `//@param`, quatre entrées à alpha droit et 1–8 passes `//@passes`.
Entrée 0 : clip original de l'effet ; entrée 1 : calques inférieurs. `Prev/PrevUV`
lisent la passe précédente. Le ping-pong GPU sépare lecture et écriture.

`History/HistoryUV` lisent le clip après sa pile à la frame précédente. Deux cibles
par clip sont conservées uniquement si nécessaires, jusqu'à 16 clips/256 Mio.
Répéter une frame ne promeut pas l'historique ; recherche/saut/mutation le remet
à zéro. Composition, effets et historique RGBA16F prémultiplié, avec couleurs de
travail sRGB/Rec.709 linéaire/Rec.2020 linéaire. L'interface shader est en alpha droit
par défaut ; `//@premultiplied` conserve les entrées/sorties prémultipliées. Les RGB
HDR/négatifs ne sont pas écrêtés dans le moteur. Un passage séparé convertit vers
l'aperçu SDR BGRA8 ; PNG16 et EXR half lisent les pixels float avant quantification.
Le cache de bytecode est partagé entre appareils, son verrou ne couvre pas la compilation.
Les transitions gardent une passe distincte avec composition après clip en entrée
0, fond en entrée 1, puis calques supérieurs. `transition_` est un namespace réservé.

DirectWrite/Direct2D rasterisent les titres sur WIC transparent, converti en alpha
droit. Texte/style/paramètres/résolution invalident le cache. Contour noir et ombre
utilisent décalages et échantillons de diffusion ; ils s'animent comme taille/couleur.
Les polices doivent être installées. Masques rectangle/ellipse/Bézier (3–16 points)
suivent les effets, avec inversion/transform/feather animables. Les cubiques Bézier
sont échantillonnées dans le shader.

`TrackingJobs` a un worker COM/MF, un état protégé et une annulation. Corrélation
d'une zone contrastée sur image réduite (320 px maximum), rayon 1–64, 1800 frames.
Les clés de translation ne sont appliquées que si le clip est identique et déverrouillé.
Rotation, déformation et occlusion ne sont pas résolues par ce suivi initial.

Le document conserve un `layerGraph` indépendant des `EffectGraph` des effets.
`ShaderixelComposition` extrait les types/ports/cycles de Document ; `LayerGraph`
lie les Surface aux IDs des timelines et conserve les clés en temps de séquence.
Chaque timeline est rendue dans une cible transparente, puis le DAG est évalué
avec mémorisation des sorties. Le shader de fusion est extrait de composite.slang ;
seules les constantes et entrées/sorties sont adaptées au prélude D3D11. Les nœuds
Effect exécutent un EffectGraph, du HLSL ou une correction colorimétrique.
128 nœuds et 512 Mio maximum de cibles estimées. Les anciens documents génèrent
leur graphe implicitement, avec conservation des modes et transitions historiques.

## Conteneurs, widgets et vignettes

Un clip vidéo peut incorporer un `Project` dans `composition`, jusqu'à huit niveaux.
Le renderer enfant partage le device/context D3D11 du parent et transmet directement
sa texture RGBA16F. Il conserve le ratio et utilise l'espace de travail parent.
Le cache des renderers est invalidé lorsque le contenu change et libéré hors intervalle.
Le mix audio enfant rejoint le master parent après timing, gain, pan et fondus du clip.
Le rééchantillonnage suit sa vitesse ; la timeline interne conserve ses DSP et bus.

Les clés sont locales à chaque document. `timeOrigin` conserve la phase du temps
shader global à la conversion ; `ctx.localTime` reste le temps source. Chaque espace
clip possède Session, historique, sélection, transport et identifiants ImGui distincts.
L'application explicite passe par `set_clip_composition`, avec validation transactionnelle.
Sauvegarde/fermeture incluent les brouillons ouverts ; la récupération les préserve
avant un changement de document ou une annulation du parent.

`widget` décrit type, parent, ancrages, pivots et visibilité/découpage. `widget_x/y/width/height`
sont des paramètres animables. `widgetRect` calcule la hiérarchie au temps courant,
refuse les cycles et compose les bornes de découpage. Les titres sont rasterisés
aux dimensions du widget, puis effets et transformations utilisent le même moteur.

`ClipPreviews` possède son thread COM/MF et device D3D11. Il rend des images 128×72
avec le vrai renderer de clip, incluant HLSL, titres et conteneurs. Le thread principal
adopte les résultats et crée les SRV ; files et cache sont bornés. Le fingerprint
porte sur le document du clip et l'instant demandé, sans modifier la sélection.

L'interface active les viewports ImGui Win32/D3D11 pour détacher chaque panneau.
Les éditeurs enfants ont des dockspaces indépendants et peuvent aussi être détachés.
Les états ouverts sont dans `workspace-panels.json`, les placements dans l'ini ImGui.
Les exécutions masquées ne touchent pas ces réglages utilisateur.

`tools/import_shaderixel.py` adapte les 317 shaders Slang verrouillés en HLSL5,
avec leurs annotations de paramètres et helpers d'origine. Le catalogue est généré
hors vendor ; les corrections Curves/LUT utilisent les nœuds d'étalonnage natifs.
Les LUT images carrées RG et atlas N³ sont converties en volume couleur embarqué,
comme les .cube 3D. PNG/TIFF/EXR préservent les pixels de précision disponibles.

## Audio et adaptateur CyDAW v2

Les 15 DSP, buffers, analyses et types de graphes sont copiés de CyDAW v2. Le routage
du mixeur utilise des ports stéréo validés, un tri de dépendances et le refus des
cycles (même avec sends désactivés). Son graphe global est une vue du mixeur ;
les piles acceptent en plus un graphe DSP JSON schema 2 éditable/importable :
128 nœuds/512 liens, DSP du catalogue et `sf.sum`, branches parallèles et sommes.

Les graphes DSP utilisent AudioGraph/AudioRenderPlan de CyDAW v2 directement,
avec des processeurs de frontière pour les entrées/sommes/sorties stéréo.
Les paramètres des DSP se mettent à jour au temps du bloc de 64 samples.
Les contrôles Widgets sont extraits sans changer leur fonctionnement ; les
cartes et bandeaux MixerView utilisent une frontière sans dépendance à JUCE.
La timeline adapte le dessin ArrangementView. La coque Studio, ses graphes
imbriqués et ses ports MIDI/contrôle ne sont pas inclus dans cette frontière.

Clips : DSP, gain/fondus/pan combiné clip+piste. Pistes : somme, DSP, sends pré,
gain, sortie/sends post. Bus : somme, DSP, sends pré, gain/pan, sorties post.
Master : somme, DSP, gain/pan puis clamp final. Le headroom intermédiaire reste
disponible. Gain/pan/sends sont évalués par sample, DSP internes par 64 samples,
contrôles externes au début d'un bloc (2048 frames maximum). Les DSP de canal
traitent le silence jusqu'à la fin de séquence pour conserver leurs queues.

Les latences fixes déclarées sont propagées dans le DAG ; délais aux entrées
parallèles et préchauffage compensent le retard final (480000 samples cumulés max).
Recherche/mutation MCP réinitialise le DSP. Le stretch aligne des grains à vitesse
unitaire : tonal/rythmes/texture adaptent les fenêtres CyDAW, bornées aux poignées du
clip. `preserve_pitch` historique reste compatible ; `stretch_mode` choisit 0–3.
Les essais couvrent voix synthétique/transitoires et ratios 0,25–4 ; l'écoute de
sources réelles et les ratios au-delà restent à approfondir.

WaveOut possède huit buffers de 1024 frames ; son horloge pilote la vidéo. Les
vu-mètres de canal mesurent RMS/crêtes après fader avec maintien 1,5 s. LUFS utilisent pondération K,
blocs 400 ms et portes absolue/relative. Crêtes sample, LUFS momentanés/intégrés
et court terme 3 s, rapport JSON et true-peak FIR ×4 sont disponibles. La suite
officielle de conformité reste à exécuter.

## Plugins, MCP et upstream

Les paquets HLSL conservent API 1/2. Le SDK C ABI 1 utilise un worker natif isolé ;
le worker CyDAW copié héberge VST3 avec IPC 4. Chaque instance a un processus/mapping
séparé, stéréo 48 kHz, blocs ≤2048, ≤64 contrôles natifs ou ≤512 contrôles VST3.
Crash/timeout produit une erreur sans charger la DLL dans l'éditeur. Interfaces
propres VST3 et états opaques (4 Mio) sont gérés par `PluginEditors` : capture
GUI, sauvegarde/récupération/presets, restitution dans les instances de rendu.
Instruments/MIDI et sidechains restent à ajouter. `IsolatedOfx` transporte deux
images RGBA32F (512 Mio max) vers un worker officiel ABI OpenFX CPU Filter/General ;
conversion d'orientation/prémultiplication et contrôle des valeurs aux frontières.
L'éditeur DSP graphique conserve un brouillon, valide CyDAW puis applique en une étape.

MCP stdio intégré, 99 outils : un thread reçoit les lignes, le thread principal
valide/applique les commandes. Aucun listener réseau ; chaque processus a sa session.
Les outils ont les droits locaux du processus, selon les autorisations du client.

`upstream.lock.json` consigne chemins/SHA-256 ; sync lit seulement les sources,
refuse les modifications vendor et garde les anciennes copies lors d'une update
explicite. Les adaptateurs vivent hors de vendor. Fixtures projets 1–10, graphe
Shaderixel, DSP v2, VST3 de test et OFX Basic officiel vérifient les contrats. `tools/extract_composition.py` régénère les composants portables ; `--check`
vérifie leur correspondance aux sources verrouillées. Une nouvelle empreinte
exige toujours régénération/reconstruction/tests ; elle ne garantit pas seule la compatibilité.

## Rendus et distribution

`ExportJobs` possède un scheduler permanent et un worker. Maximum 32 jobs actifs/en
attente, huit métadonnées terminales conservées. Chaque job copie projet/catalogue
et possède COM/MF, cache/DSP et GPU séparés ; il ne touche ni ImGui ni l'aperçu.
MP4 H264/AAC, WAV, AVI RGB/PCM (2 Gio), PNG8/PNG16/EXR+manifest et analyse JSON utilisent le
même montage/mix. Les médias et DLL restent référencés pendant l'exécution.

Catalogue et jobs ont des mutex distincts, pris catalogue puis job quand combinés.
La cancellation partage le mutex de la vérification avant commit : acceptée,
elle interdit le remplacement ; `finalizing` la refuse. `completed` est publié
après commit/libération. PNG finalise un dossier staged par renommage. L'annulation
attend les appels codec/GPU en cours ; fermeture annule la file et joint le worker
avant d'arrêter les runtimes. Un arrêt brutal peut laisser un temporaire.

Le ZIP portable contient workers/licences/SDK, sans dépendre des projets sources.
Le setup .NET Framework x64 installe pour le compte courant avec manifeste SHA-256.
La désinstallation retire les fichiers inchangés du manifeste et conserve projets,
caches et fichiers modifiés. Le setup n'est pas signé et n'enregistre pas encore
d'entrée dans les paramètres Applications de Windows.

Voir [RELEASE_0.9.md](RELEASE_0.9.md) et
[ROADMAP.md](ROADMAP.md) pour les essais et prochaines extensions.

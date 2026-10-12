# ShaderixFrame 0.9.0

Les quatre chantiers demandés sont intégrés localement. Shaderixel et CyDAW ne sont
pas modifiés. Les reprises CyDAW v2 restent des snapshots avec adaptateurs locaux.

## Graphe DSP

Le mixeur ouvre une fenêtre graphique avec cartes, ports stéréo, câbles courbes,
recherche de DSP, paramètres, sélection par rectangle, déplacement groupé et
historique local. Molette : zoom autour du pointeur ; bouton central : pan ;
Tab/double-clic : ajout ; F : cadrer ; A : organiser ; Suppr : retirer.
Le clic droit sur un câble le déconnecte. Le nœud Somme stéréo accepte plusieurs
entrées. Les boucles et limites de ports sont validées par le moteur CyDAW.
Importer/exporter le JSON préserve les positions ; Appliquer crée une étape dans
l'historique du projet. Plusieurs graphes de la pile peuvent être édités.

## Plugins

VST3 : fenêtre native dans le worker CyDAW IPC 4, contrôles nommés jusqu'à 512,
état opaque embarqué (4 Mio maximum), presets `.sfvstpreset`, capture automatique
des changements de fenêtre avant sauvegarde, récupération et export. Les courbes
restent attachées aux contrôles à la restitution d'un état. Chaque instance audio
de rendu restaure le même état. Latence fixe compensée comme dans la 0.8.

OFX : worker dédié, SDK officiel ASWF versionné, chargement d'un `.ofx` ou bundle
`Contents/Win64`, contextes Filter/General, entrée Source et sortie Output RGBA
32 bits float sur CPU. Paramètres numériques et choix animables, notifications de
changements, alpha prémultiplié, orientation et temps en images adaptés au moteur.
Le plugin ne charge jamais sa DLL dans le processus de l'éditeur. Un crash/timeout
devient une erreur de traitement. `ofx_gain.ofx` et son source sont livrés.

Cette livraison n'est pas une compatibilité universelle avec tous les plugins.
VST3 instruments/MIDI, sidechains et automation par sample restent à ajouter.
OFX GPU, overlays interactifs, accès temporel aux autres images, générateurs,
transitions à plusieurs entrées et édition des paramètres custom/string restent
à ajouter. Les essais utilisent le VST3 CyDAW et deux OFX dont le Basic ASWF
non modifié ; aucun catalogue de plugins commerciaux n'a été certifié.

## Précision et couleur

Composition, effets HLSL, transitions et historique utilisent RGBA16F : valeurs HDR
et RGB négatives sont conservées. Accumulation prémultipliée Porter-Duff, interfaces
shader en alpha droit par défaut ; `//@premultiplied` permet une interface prémultipliée.
Espaces sRGB historique, Rec.709 linéaire et Rec.2020 linéaire ; interprétation couleur
et alpha par source. Les anciens projets gardent leur espace historique, les nouveaux
projets MCP utilisent Rec.709 linéaire. Aperçu SDR séparé, exposition et courbes
clip/Reinhard/ACES fitted (la dernière n'est pas une chaîne ACES complète).

PNG 16 bits : RGBA entier, image après tone mapping SDR, alpha droit.
EXR : RGBA half linéaire prémultiplié, chromaticités Rec.709/2020, valeurs de travail
avant tone mapping. Import EXR limité aux scanlines non compressées single-part RGB/A
half/float ; les autres variantes sont refusées explicitement. L'import PNG haute
précision conserve les pixels float WIC. Le décodeur vidéo, le MP4 H264 et l'affichage
restent 8 bits SDR : les codecs HDR 10/12 bits, PQ/HLG et écran HDR sont une suite distincte.

## Audio

Modes repitch, tonal (grains 80 ms), rythmes (28 ms), texture (180 ms), adaptés du
fonctionnement CyDAW. Fenêtres bornées aux poignées du clip, grains à vitesse unitaire,
recherche de corrélation pour tonal/texture. Les 12 combinaisons des modes à hauteur
conservée avec ×0,25/×0,5/×2/×4 vérifient voix synthétique/transitoires, énergie,
indépendance des blocs et recherche. Cela ne remplace pas une écoute comparative
sur des enregistrements et des ratios extrêmes.

Vu-mètres RMS/crête avec maintien 1,5 s ; LUFS court terme 3 s en plus des mesures
momentanées/intégrées ; true-peak FIR ×4 utilisant les coefficients de l'annexe 2
de [ITU-R BS.1770-4](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-4-201510-S!!PDF-E.pdf).
Les rapports incluent `true_peak_dbtp`, `true_peak_measured` et `short_term_lufs`.
La suite officielle de conformité broadcast et la mesure LRA restent à réaliser.

## Projet et maintenance

Schéma 7, migration 1–6 ; 80 outils MCP. Nouveaux contrats et limites documentés
dans [MCP.md](MCP.md) et [PLUGINS.md](PLUGINS.md). Audit lecture seule :
`python tools/verify_snapshots.py`. Les différences présentes dans les sources
upstream signalent une mise à jour disponible, sans modifier les snapshots validés.
`tools/sync_upstream.py --update` conserve les anciennes copies et exige de nouveaux
tests de compatibilité avant livraison. Voir.

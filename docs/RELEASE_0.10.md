# ShaderixFrame 0.10.0 · Étalonnage

Sélectionner un clip vidéo, ouvrir l'onglet **Étalonnage**, puis **+ Correction**.
Le graphe affiche la vraie pile du clip : sélectionner une carte, monter/descendre,
activer ou retirer une correction. Les autres effets HLSL/OFX conservent leur ordre.
Le moteur, les exports et le MCP utilisent la même pile. Shaderixel et CyDAW restent
inchangés ; cette extension appartient à ShaderixFrame.

## Corrections

- Balance relative à 6500 K, teinte vert/magenta, gains RGB et pipette neutre.
- Exposition en EV, contraste avec pivot linéaire, noirs/blancs, saturation et vibrance.
- Roues ombres, moyens, hautes lumières et offset global, avec luminance et RGB.
- Courbes maître et RGB, teinte→teinte/saturation/luminance et luminance→saturation.
  Double clic pour ajouter, glisser pour déplacer, clic droit pour retirer.
  Maximum 32 points par courbe. Interpolation linéaire ; les courbes RGB extrapolent
  leurs segments aux extrémités pour conserver les valeurs HDR. Les courbes de
  teinte bouclent à 0/1. Les modifications de teinte sont calculées sur la partie
  chromatique positive puis ajoutées au RGB signé pour conserver le résidu négatif.
- Qualifier HSL : centre/largeur de teinte, saturation et luminance, douceur,
  inversion, pipette couleur et vue de la sélection.
- Masque indépendant par correction : rectangle, ellipse, polygone de 3 à 16
  sommets, position, taille, rotation, contour progressif et inversion. Il dose
  uniquement la correction : aucune découpe d'alpha. Le qualifier multiplie le masque.
- Suivi de translation par corrélation, en arrière-plan, avec annulation et
  application de clés mask_x/y. Une zone contrastée est nécessaire ; une perte
  de suivi est signalée. Maximum 1800 images, sans suivi perspective ni déformation.
  La correction suivie doit rester inchangée pendant le calcul.
- LUT `.cube` 1D (2..16384 entrées) ou 3D (2..65), domaine min/max par composante,
  intensité et espaces entrée/sortie explicites. Données incorporées au projet et
  aux presets, texture GPU float32, interpolation linéaire/trilinéaire calculée
  explicitement en float pour éviter les fractions limitées du sampler matériel.
  Les LUT bornent l'entrée à leur domaine. Les fichiers combinant un shaper 1D
  et une LUT 3D dans un seul `.cube` ne sont pas pris en charge.
- Cinq looks natifs, sauvegarde et chargement `.sgrade`, incluant LUT et clés.
  Charger un preset remplace la correction sélectionnée et ses animations.

**K** ajoute une clé au temps courant. Quand un réglage est déjà animé, le glisser
écrit une clé au temps courant. Clic droit sur le curseur : choisir sa courbe
dans **Animations**. Intensité et activation permettent de doser ou contourner
chaque étape. L'ordre interne d'une correction est balance → primaires/roues →
courbes → LUT ; son masque et son qualifier mélangent ce résultat avec l'entrée.

La pipette utilise une moyenne de 5×5 pixels de l'entrée de la correction,
avant les transformations de composition du clip. Le point cliqué est ramené
dans les coordonnées du clip. La balance neutre remplace les gains et leurs clés.
La température est une balance créative relative, pas une calibration spectrale
de caméra.

## Analyse et comparaison

**Scopes vidéo** propose waveform luminance, parade RGB, vectorscope et histogramme
RGB. Les traces représentent l'image affichée SDR après le tone mapping du projet ;
min/max et les comptes sous zéro/au-dessus du blanc analysent les données linéaires.
Le nombre d'échantillons est limité à environ 262144 et l'affichage actualisé à 4 Hz.
Ce sont des mesures de travail, pas un moniteur matériel HDR calibré.

Modes image corrigée, bypass de toutes les corrections, séparation avant/après
et comparaison avec une référence capturée ou chargée. La référence reste en mémoire
pour la session. Elle ne s'enregistre pas dans le projet. Ces vues et la matte
ne changent pas les exports ; désactiver un effet dans la pile, en revanche, le change.

## Couleur et formats

Calcul HLSL float32, intermédiaires de composition **RGBA16F**. Les primaires et
roues opèrent en Rec.709 linéaire, avec conversion depuis/vers sRGB historique et
Rec.2020 linéaire. L'ancien filtre `color.hlsl` du catalogue conserve désormais
le HDR ; les anciens projets gardent leurs shaders embarqués.

**Propriétés → Couleurs de la source** ajoute **ARRI LogC3/AWG3 EI800**.
La même transformation est proposée aux entrées/sorties LUT. Courbe d'exposition
et matrice linéaire provenant du [document technique ARRI](https://www.arri.com/resource/blob/31918/66f56e6abb6e5b6553929edf9aa7483e/2017-03-alexa-logc-curve-in-vfx-data.pdf),
avec matrice inverse calculée. Choisir explicitement ce profil : pas de détection
automatique de métadonnées caméra. LogC4, S-Log, V-Log, autres EI et configurations
OpenColorIO restent à intégrer. La version n'embarque pas de runtime OpenColorIO.
Le décodage vidéo Media Foundation et les codecs d'export restent 8 bits ; les
images float et les séquences PNG16/EXR profitent du traitement haute précision.

Schéma projet **8**, lecture des schémas 1..8. Format de correction natif versionné
indépendamment du graphe Shaderixel, pour préserver la compatibilité des snapshots.
`examples/GradingStudy.sframe` contient des lumières HDR, des primaires animées,
une courbe et une correction locale.

## MCP

90 outils, dont `add_grade`, `set_grade`, `set_grade_curve`, `import_grade_lut`,
`neutral_grade`, `list_grade_presets`, `apply_grade_preset`, `save_grade_preset`,
`load_grade_preset` et `video_scopes`. Les outils existants d'effet/paramètre/clé
règlent l'ordre, l'activation et les animations. `start_mask_tracking` accepte
désormais `effect` pour cibler le masque d'une correction. Les mutations invalides
sont annulées avant de créer un point d'historique.

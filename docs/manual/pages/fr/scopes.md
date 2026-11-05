---
id: scopes
title: Scopes et évaluation de l’image
description: Utilisez les mesures avec l’aperçu en les interprétant correctement.
section: Montage et graphisme
order: 20
sourceRevision: 1
---

Utilisez les mesures avec l’aperçu en les interprétant correctement.

## Utilisation {#usage}

```image
{"id": "scopes-image-1", "src": "assets/video-scopes.png", "alt": "Forme d’onde luminance et compte des valeurs hors plage sur la composition étalonnée.", "controls": "minimal"}
```
Forme d’onde luminance et compte des valeurs hors plage sur la composition étalonnée.

Ouvrez Video Scopes et choisissez une mesure, par exemple la forme d’onde de luminance ou les distributions couleur proposées. Observez position horizontale, répartition lumineuse et écrêtage en changeant exposition, contraste et saturation. Comparez avant/après pour distinguer intention et perte de détail.

Le panneau indique échantillonnage et rafraîchissement. Les scopes suivent le chemin d’affichage SDR/tone mapping, avec échantillonnage limité et environ 4 Hz ; ils ne constituent pas un instrument de mastering HDR pleine résolution. Comptages sous zéro/au-dessus du blanc et valeurs de travail peuvent révéler une marge invisible dans l’aperçu SDR.

```steps
{"items": [{"title": "Étape 1", "body": "Évaluez plusieurs instants, surtout hautes lumières, peaux et transitions sombres."}, {"title": "Étape 2", "body": "Utilisez la séparation avec référence pour raccorder les plans. Revérifiez avec l’étalonnage désactivé."}, {"title": "Étape 3", "body": "Pour la haute précision, exportez une image PNG16/EXR half et vérifiez-la dans un outil compatible. Le compositeur flottant ne garantit pas une vidéo 10/12 bits."}]}
```

---
id: library
title: Navigateur d’effets et bibliothèque
description: Trouvez et appliquez effets, fusions, générateurs, transitions et traitements audio.
section: Guide utilisateur
parent: composition
order: 13
sourceRevision: 3
---

Trouvez et appliquez effets, fusions, générateurs, transitions et traitements audio.

## Utilisation {#usage}

Le catalogue Shaderixel embarqué contient 406 effets et générateurs. Shaderixel Library recherche par nom/catégorie ; Effects Browser propose des cartes avec aperçus calculés en arrière-plan, familles de fusions/transitions et effets audio CyDAW compris. Preview on selected clip évalue les cartes sur la sélection au lieu de la scène de référence.

| Type de carte | Résultat du double-clic |
| --- | --- |
| Effet vidéo | Ajoute un effet au clip sélectionné |
| Générateur | Crée un nouveau calque/clip |
| Fusion | Règle la fusion de la piste du clip |
| Transition | Ajoute une transition entrante au clip |
| Effet audio | Ajoute au canal de Track Effects |

Les cartes se glissent sur la timeline ; les effets peuvent viser FX d’une piste ou un graphe. Le badge fx du clip ouvre sa pile. Utilisez le bypass pour comparer et choisissez soigneusement l’ordre, car il change le résultat. Apply LUT File charge une table .cube ou image comme correction native.

```callout
{"type": "note", "title": "Remarque", "body": "La génération des aperçus est asynchrone : une carte peut attendre pendant le calcul d’autres aperçus. Le code est embarqué à l’application ; mettre à jour la bibliothèque ne remplace pas silencieusement l’effet déjà conservé dans un projet."}
```

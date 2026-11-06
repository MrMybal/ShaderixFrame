---
id: composition
title: Vue de composition et comparaison
description: Inspectez l’image finale sans modifier l’échelle des clips.
section: Guide utilisateur
parent: welcome
order: 8
sourceRevision: 3
---

Inspectez l’image finale sans modifier l’échelle des clips.

## Utilisation {#usage}

La molette zoome autour du pointeur. Fit cadre l’image, 1:1 affiche ses pixels et le menu propose 25–1600 %. Ctrl+0 cadre, Ctrl+1 passe à 100 %, Ctrl avec plus/moins zoome. H sur l’image choisit Main ; glisser du milieu, Espace-glisser, Maj+molette, défilement horizontal et glisser dans la marge déplacent la vue. La minicarte indique et déplace la région visible.

Le rendu d’aperçu suit le zoom jusqu’à la résolution de séquence. Au-delà de 200 %, l’affichage net des pixels facilite l’inspection. Le zoom de vue est une navigation : utilisez Scale dans les propriétés du clip pour redimensionner son contenu. Les guides montrent zones sûres, tiers, centre et format cible.

```steps
{"items": [{"title": "Étape 1", "body": "Snapshot mémorise l’image A. Activez A/B et déplacez la séparation pour comparer à l’image B actuelle. Un instantané est une référence, pas un nouveau clip."}, {"title": "Étape 2", "body": "Le clic droit exporte l’image courante en PNG ou EXR half. Copy Image (Ctrl+Maj+C) place une image à la résolution de séquence dans le presse-papiers Windows."}, {"title": "Étape 3", "body": "Pour l’étalonnage, utilisez aussi avant/après et référence dans Color Grading. Interprétez les valeurs HDR en tenant compte des limites de précision et de scopes."}]}
```

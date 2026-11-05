---
id: masks
title: Masques, détourage et suivi
description: Limitez un effet à une région ou isolez un premier plan.
section: Montage et graphisme
order: 17
sourceRevision: 1
---

Limitez un effet à une région ou isolez un premier plan.

## Utilisation {#usage}

```steps
{"items": [{"title": "Étape 1", "body": "Sélectionnez un clip puis Mask dans Properties. Choisissez rectangle, ellipse ou chemin Bézier fermé. Activez Mask Guides dans Composition pour le manipuler."}, {"title": "Étape 2", "body": "Déplacez, redimensionnez ou tournez ; réglez le contour progressif. Invert échange intérieur et extérieur. Pour Bézier, placez/ajustez chemin fermé et poignées."}, {"title": "Étape 3", "body": "Animez les transformations proposées avec K. Pour un suivi en translation, choisissez une région contrastée et une plage temporelle, lancez le suivi puis inspectez toutes les clés."}, {"title": "Étape 4", "body": "Pour fond vert/bleu, choisissez un effet de keying et réglez la couleur cible. Inspectez contour et alpha sur un fond contrasté ; un masque exclut les zones inutiles."}]}
```

```callout
{"type": "note", "title": "Remarque", "body": "Le suivi estime la translation, pas une reconstruction caméra, un suivi planaire ni une rotoscopie automatique. Rotation forte, occultation et faible contraste peuvent exiger des corrections manuelles. Une correction colorimétrique possède son masque local, distinct du masque global du clip."}
```

---
id: motion
title: Motion design et hiérarchie de widgets
description: Construisez des compositions animées avec textes, formes, images et groupes.
section: Guide utilisateur
parent: composition
order: 15
sourceRevision: 3
---

Construisez des compositions animées avec textes, formes, images et groupes.

## Utilisation {#usage}

```image
{"id": "motion-image-1", "src": "assets/motion-design.png", "alt": "Hiérarchie de widgets, sélection du titre et composition rendue.", "controls": "minimal"}
```
Hiérarchie de widgets, sélection du titre et composition rendue.

```steps
{"items": [{"title": "Étape 1", "body": "Ouvrez examples/MotionStudy.sframe et passez à Motion (F3). La hiérarchie liste groupes, formes, titre et éléments d’accent."}, {"title": "Étape 2", "body": "Créez rectangle, ellipse, texte, image/vidéo ou groupe. Sélectionnez dans l’arbre ou le canevas. Déplacez/redimensionnez avec les poignées."}, {"title": "Étape 3", "body": "Choisissez Parent pour la hiérarchie. Anchor définit l’attache dans le parent ; Pivot définit l’origine de transformation. Réglez position, largeur et hauteur en observant le canevas."}, {"title": "Étape 4", "body": "Visibilité et découpage des enfants contrôlent les groupes. Animez position/taille à plusieurs instants avec K puis affinez dans Animation."}, {"title": "Étape 5", "body": "Ouvrez le clip d’un élément pour des effets locaux ou timelines imbriquées si nécessaire. Appliquez au parent puis contrôlez la composition finale."}]}
```

Pour un bandeau réutilisable, groupez un fond avec le nom et la fonction. Ancrez près d’une zone sûre, animez arrivée et sortie puis gardez l’ensemble dans un conteneur. Testez un texte long et le format de sortie avant l’export.

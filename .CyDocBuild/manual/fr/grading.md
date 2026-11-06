---
id: grading
title: Étalonnage colorimétrique
description: Équilibrez l’image, construisez un rendu et isolez les corrections locales.
section: Guide utilisateur
parent: composition
order: 19
sourceRevision: 3
---

Équilibrez l’image, construisez un rendu et isolez les corrections locales.

## Utilisation {#usage}

```image
{"id": "grading-image-1", "src": "assets/color-grading.png", "alt": "Corrections indépendantes avec primaires et commandes de comparaison.", "controls": "minimal"}
```
Corrections indépendantes avec primaires et commandes de comparaison.

```steps
{"items": [{"title": "Étape 1", "body": "Ouvrez GradingStudy.sframe, sélectionnez son clip image et passez à Color Grading (F4). Ajoutez ou choisissez une correction dans sa chaîne ordonnée."}, {"title": "Étape 2", "body": "Commencez par neutraliser. Échantillonnez une zone neutre puis ajustez température, teinte et balance RGB. Réglez exposition et contraste avant le look créatif."}, {"title": "Étape 3", "body": "Wheels règle les couleurs par tonalité ; Curves modèle la réponse tonale/colorée. Ajoutez peu de points maîtrisés ; évitez les pentes extrêmes sauf intention. Désactivez la correction pour comparer."}, {"title": "Étape 4", "body": "Selection & Mask qualifie une couleur ou limite spatialement la correction. Inspectez le matte, affinez limites et douceur puis vérifiez plusieurs images en mouvement."}, {"title": "Étape 5", "body": "Séparez équilibrage, look et retouche locale en corrections distinctes. Réordonnez ou désactivez pour comprendre leur contribution. LUT & Looks applique une LUT ou un preset ; les valeurs proposées s’animent avec K."}]}
```

Store Reference mémorise une référence ; Load Reference en charge une. Comparez avec les modes dédiés et la séparation. Auto Color estime balance/exposition et Color Match utilise une référence, mais un contrôle visuel reste nécessaire. Les graphes placent une correction avant/après mélange ou seulement sur une branche.

[Lire les scopes et les limites de précision](doc:scopes)

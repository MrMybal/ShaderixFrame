---
id: transitions
title: Transitions et fondus
description: Contrôlez les changements de plans sur une piste ou entre plusieurs pistes.
section: Guide utilisateur
parent: editing
order: 18
sourceRevision: 3
---

Contrôlez les changements de plans sur une piste ou entre plusieurs pistes.

## Utilisation {#usage}

```steps
{"items": [{"title": "Étape 1", "body": "Placez deux clips avec le chevauchement temporel voulu. Sélectionnez le clip entrant et appliquez une transition depuis son menu ou Effects Browser."}, {"title": "Étape 2", "body": "Glissez la poignée de fin de transition pour sa durée. Réglez type ou progression et relisez autour de la coupe."}, {"title": "Étape 3", "body": "Pour une transition nodale, branchez les deux images et la progression animée à la sortie. Les transitions combinent des clips sur une piste ou entre pistes."}, {"title": "Étape 4", "body": "Les fondus audio sont distincts : ajustez les poignées et écoutez à vitesse normale. Maj+D crée un fondu à la coupe la plus proche quand possible."}]}
```

Les transitions sont évaluées après effets et transformations de clip. Un shader de transition possède une progression et un contrat à deux images, en une passe. Partez d’un exemple fourni plutôt que d’un effet ordinaire. Une fusion appliquée comme effet de clip permanent peut continuer hors de l’intervalle de transition automatique.

---
id: track-graphs
title: Graphes de pistes et calques de réglage
description: Traitez le contenu d’une piste avant son mélange dans la composition.
section: Montage et graphisme
order: 11
sourceRevision: 1
---

Traitez le contenu d’une piste avant son mélange dans la composition.

## Utilisation {#usage}

Track Graph reçoit le contenu composé d’une piste vidéo. Ses nœuds effet, correction, LUT et Shader Graph traitent cette surface ; le résultat passe ensuite au mélange de pistes et au graphe de calques. Les entrées secondaires peuvent lire les pistes inférieures pour masques ou déplacement. Chaque nœud dispose d’un aperçu.

```steps
{"items": [{"title": "Étape 1", "body": "Sélectionnez une piste et son bouton FX, ou ouvrez Track Graph. Déposez une carte d’effet sur FX ou le panneau pour ajouter un traitement."}, {"title": "Étape 2", "body": "Reliez l’entrée piste aux nœuds choisis puis à la sortie. Comparez avec le bypass pour vérifier le traitement de toute la piste."}, {"title": "Étape 3", "body": "Le menu de piste règle opacité, l’un des 27 modes de fusion ou une fusion de bibliothèque. Déposer une carte de fusion sur FX lui attribue ce comportement."}, {"title": "Étape 4", "body": "Utilisez un calque de réglage pour corriger les calques inférieurs pendant sa durée. Placez-le au-dessus des pistes visées et rognez-le à l’intervalle voulu."}]}
```

```diagram
{"type": "flow", "title": "Clips d’une piste → Graphe de piste → Fusion / opacité piste → Graphe calques / sortie", "nodes": [{"id": "node-0", "label": "Clips d’une piste", "x": 15, "y": 70, "width": 155}, {"id": "node-1", "label": "Graphe de piste", "x": 230, "y": 70, "width": 170}, {"id": "node-2", "label": "Fusion / opacité piste", "x": 460, "y": 70, "width": 175}, {"id": "node-3", "label": "Graphe calques / sortie", "x": 695, "y": 70, "width": 190}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

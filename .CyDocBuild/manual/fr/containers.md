---
id: containers
title: Éditeur de clips imbriqués
description: Utilisez un clip comme conteneur de pistes, graphes, effets et temps local.
section: Guide utilisateur
parent: composition
order: 9
sourceRevision: 3
---

Utilisez un clip comme conteneur de pistes, graphes, effets et temps local.

## Utilisation {#usage}

```image
{"id": "containers-image-1", "src": "assets/clip-editor.png", "alt": "Conteneur ouvert avec composition, audio et timeline locaux.", "controls": "minimal"}
```
Conteneur ouvert avec composition, audio et timeline locaux.

```steps
{"items": [{"title": "Étape 1", "body": "Double-cliquez un clip vidéo pour ouvrir Clip Editor. Un clip simple peut devenir un conteneur conservant son contenu d’origine."}, {"title": "Étape 2", "body": "Utilisez ses panneaux locaux Media & Effects, Composition, Timeline, Properties, Animation, Shader Graph et Layer Graph. Ajoutez vidéo, images, titres, générateurs ou son comme dans une séquence."}, {"title": "Étape 3", "body": "Apply to Container transmet le montage local à la composition parente. Enregistrer le parent applique aussi les brouillons ouverts. Reload abandonne volontairement un brouillon local non appliqué."}, {"title": "Étape 4", "body": "Separate Window détache l’éditeur ; Clip Windows rouvre ses panneaux. Sélection, annulation et lecture locales appartiennent au document imbriqué. Vérifiez aperçu local et placement temporel dans le parent."}]}
```

```diagram
{"type": "flow", "title": "Pistes + clips locaux → Effets + graphes locaux → Appliquer / clip parent → Sortie parente", "nodes": [{"id": "node-0", "label": "Pistes + clips locaux", "x": 20, "y": 60, "width": 170}, {"id": "node-1", "label": "Effets + graphes locaux", "x": 255, "y": 60, "width": 180}, {"id": "node-2", "label": "Appliquer / clip parent", "x": 500, "y": 60, "width": 180}, {"id": "node-3", "label": "Sortie parente", "x": 745, "y": 60, "width": 135}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

```callout
{"type": "note", "title": "Remarque", "body": "La tête locale n’est pas la tête parente. Point d’entrée source, vitesse et placement du conteneur relient leurs temps. Local Shader Time from Zero change l’origine temporelle des shaders imbriqués, pas le placement du parent."}
```

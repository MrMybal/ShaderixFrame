---
id: layers
title: Graphe de mélange des calques
description: Combinez les surfaces de pistes par branches, mélanges et corrections explicites.
section: Guide utilisateur
parent: composition
order: 10
sourceRevision: 3
---

Combinez les surfaces de pistes par branches, mélanges et corrections explicites.

## Utilisation {#usage}

```image
{"id": "layers-image-1", "src": "assets/layer-graph.png", "alt": "Layer Graph est le graphe de composition, distinct de la compilation des shaders.", "controls": "minimal"}
```
Layer Graph est le graphe de composition, distinct de la compilation des shaders.

Chaque piste vidéo produit une surface transparente à partir de ses clips temporels. Le graphe de calques relie ces surfaces à des nœuds de mélange, correction, effet, LUT, Shader Graph et sortie. La pile automatique suit ordre des pistes, modes de fusion et opacité ; un graphe explicite permet branches, entrées partagées et corrections locales avant mélange.

```steps
{"items": [{"title": "Étape 1", "body": "Ouvrez Layer Graph puis Fit All. Rebuild from Tracks reconstruit depuis la pile actuelle ; utilisez Annuler si vous vouliez garder un arrangement personnalisé."}, {"title": "Étape 2", "body": "Reliez une sortie à une entrée compatible. Pour une fusion, branchez le fond et le calque de premier plan ; ajoutez un masque au besoin. Terminez au nœud de sortie."}, {"title": "Étape 3", "body": "Sélectionnez un nœud pour ses propriétés. Ajoutez correction, LUT ou Shader Graph à l’endroit voulu. Animez les paramètres exposés avec des clés en temps de séquence."}, {"title": "Étape 4", "body": "Molette pour zoomer, glisser du milieu pour déplacer et clic droit pour les actions. Clic droit sur une entrée pour déconnecter. Contrôlez vignettes et image finale."}]}
```

```callout
{"type": "note", "title": "Remarque", "body": "Layer Graph décide comment composer les images. Shader Graph construit un shader de traitement d’image. DSP Graph traite le son. Ils partagent des composants d’interface mais leurs signaux sont différents ; ils ne sont pas interchangeables."}
```

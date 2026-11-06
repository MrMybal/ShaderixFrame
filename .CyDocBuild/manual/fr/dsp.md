---
id: dsp
title: Éditeur graphique DSP
description: Construisez un traitement stéréo avec validation et rendu de graphes CyDAW v2.
section: Guide utilisateur
parent: devices
order: 26
sourceRevision: 3
---

Construisez un traitement stéréo avec validation et rendu de graphes CyDAW v2.

## Utilisation {#usage}

```image
{"id": "dsp-image-1", "src": "assets/audio-dsp.png", "alt": "Graphe EQ parallèle : séparation, deux égaliseurs, somme stéréo et sortie.", "controls": "minimal"}
```
Graphe EQ parallèle : séparation, deux égaliseurs, somme stéréo et sortie.

```steps
{"items": [{"title": "Étape 1", "body": "Ouvrez DspStudy.sframe puis DSP Graph. Choisissez canal et instance. New Graph prépare un brouillon ; une instance existante charge son graphe embarqué."}, {"title": "Étape 2", "body": "Add DSP recherche et ajoute des processeurs. Branchez les ports stéréo de STEREO INPUT aux traitements puis STEREO OUTPUT. Arrange et Fit All organisent le canevas."}, {"title": "Étape 3", "body": "Pour un EQ parallèle, alimentez deux EQ depuis l’entrée, branchez-les à une somme stéréo puis à la sortie. Réglez les branches pour éviter écrêtage ou doublement involontaire."}, {"title": "Étape 4", "body": "Sélectionnez un nœud pour ses paramètres ; exposez/animez les valeurs utiles. Compile and Apply valide et applique en une modification annulable. Un nouveau graphe s’insère dans la chaîne du canal."}, {"title": "Étape 5", "body": "Écoutez puis comparez au bypass. Enregistrez pour conserver topologie et automation. ParallelEQ.json sert d’exemple d’échange compatible."}]}
```

```callout
{"type": "note", "title": "Remarque", "body": "Le canevas DSP est un brouillon jusqu’à application. La molette zoome, le milieu déplace, Tab/double-clic ajoute un DSP et Suppr retire les nœuds. Le signal est audio stéréo, pas une surface vidéo. Les boucles de rétroaction ne constituent pas une manière prise en charge de créer un délai."}
```

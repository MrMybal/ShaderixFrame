---
id: export
title: Export et livraison
description: Rendez la bonne portion au bon format et vérifiez le fichier obtenu.
section: Audio et livraison
order: 30
sourceRevision: 1
---

Rendez la bonne portion au bon format et vérifiez le fichier obtenu.

## Utilisation {#usage}

| Sortie | Usage |
| --- | --- |
| MP4 H264/AAC | Lecture et partage ; chemin vidéo actuel 8 bits |
| WAV | Audio stéréo 48 kHz |
| AVI RGB/PCM | Image non compressée et son PCM |
| Séquence PNG8 / PNG16 | Échange d’images individuelles |
| Séquence EXR half | Échange demi-flottant à plage étendue |
| GIF animé | Courts aperçus animés |
| JSON de sonie | Mesures audio |

```steps
{"items": [{"title": "Étape 1", "body": "Enregistrez, appliquez/vérifiez les conteneurs et confirmez les médias en ligne. Contrôlez dimensions, cadence, espace de travail et durée."}, {"title": "Étape 2", "body": "Choisissez séquence entière, sélection, zone ou région. Work Area Only limite l’intervalle ; désactivez-le pour tout exporter."}, {"title": "Étape 3", "body": "Choisissez format/preset, destination et dimensions. HD, UHD, vertical et carré sont des points de départ. Lancez le travail en arrière-plan depuis Exports."}, {"title": "Étape 4", "body": "Suivez Exports ou la barre d’état. Annulez avant finalisation pour préserver une destination existante. Après succès, relisez premières/dernières images, synchro, sous-titres, transitions et résonances."}]}
```

Un travail capture séquence et catalogue à son lancement ; les modifications suivantes ne le changent pas. Les sources restent lues par référence : gardez-les inchangées. La file accepte 32 travaux actifs/en attente, en exécute un à la fois et garde huit résultats récents. Fermer le logiciel annule l’attente et rejoint les workers.

```diagram
{"type": "flow", "title": "Copie de séquence → Rendu + mixage → Encoder / finaliser → Vérifier la sortie", "nodes": [{"id": "node-0", "label": "Copie de séquence", "x": 20, "y": 65, "width": 165}, {"id": "node-1", "label": "Rendu + mixage", "x": 245, "y": 65, "width": 170}, {"id": "node-2", "label": "Encoder / finaliser", "x": 475, "y": 65, "width": 170}, {"id": "node-3", "label": "Vérifier la sortie", "x": 705, "y": 65, "width": 175}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

---
id: recovery
title: Enregistrement, historique et récupération
description: Récupérez votre travail et distinguez édition, lecture et navigation.
section: Audio et livraison
order: 32
sourceRevision: 1
---

Récupérez votre travail et distinguez édition, lecture et navigation.

## Utilisation {#usage}

Ctrl+Z/Ctrl+Y annulent et rétablissent les modifications. History liste les étapes nommées. Les éditeurs de clips possèdent leur historique jusqu’à application au parent. Sélection, déplacement de tête, zoom et lecture du manuel ne modifient pas le projet.

```steps
{"items": [{"title": "Étape 1", "body": "Enregistrez régulièrement avec Ctrl+S. Chaque enregistrement conserve le projet remplacé parmi dix versions antérieures au maximum, dans le dossier de versions du projet."}, {"title": "Étape 2", "body": "File > Previous Versions liste ces versions. Restaurer est annulable ; vérifiez et enregistrez séparément pour garder les deux montages."}, {"title": "Étape 3", "body": "File > Recover Session ouvre Session Recovery. Les instantanés de récupération sont distincts des versions enregistrées et sont pris périodiquement et aux changements de document/sortie."}, {"title": "Étape 4", "body": "Ouvrez le candidat, vérifiez médias et conteneurs, puis enregistrez sous un nom clair. Gardez une sauvegarde séparée des projets et sources importants."}]}
```

Entrée précalcule la zone de travail en mémoire ; une barre verte indique les images en cache. Une modification invalide ce cache. C’est une optimisation de lecture, pas un projet enregistré ni un export final.

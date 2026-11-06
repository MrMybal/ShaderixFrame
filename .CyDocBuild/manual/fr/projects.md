---
id: projects
title: Projets, séquences et fichiers
description: Comprenez le contenu d’un .sframe et le déplacement d’un projet.
section: Guide utilisateur
parent: welcome
order: 3
sourceRevision: 3
---

Comprenez le contenu d’un .sframe et le déplacement d’un projet.

## Utilisation {#usage}

Un projet contient dimensions, cadence, durée, pistes, clips, graphes, sources de shaders embarquées, effets, animations et compositions imbriquées. Les médias restent généralement référencés par leur chemin. File > New from Template propose vidéo horizontale, verticale, podcast et cinéma ; Sequence > Format change le format de séquence.

```steps
{"items": [{"title": "Étape 1", "body": "Enregistrez avec Ctrl+S ; un nouveau nom de projet conserve une copie de travail indépendante. Enregistrez avant une grande réorganisation."}, {"title": "Étape 2", "body": "Avant un transfert, utilisez Collect Media et choisissez un dossier d’archive. Vérifiez le projet copié et ses références relatives depuis ce dossier."}, {"title": "Étape 3", "body": "Si des médias sont absents, utilisez File > Relink Missing Media et sélectionnez leur nouveau dossier. La recherche parcourt les sous-dossiers et compare les noms ; contrôlez les résultats si plusieurs fichiers portent le même nom."}]}
```

Le temps de séquence est mesuré en secondes et affiché heures:minutes:secondes:images. Le point d’entrée source choisit un décalage dans le média ; déplacer un clip change son début dans la séquence. Un conteneur possède sa propre séquence locale. Évitez de modifier un média sur disque pendant sa lecture ou son export.

[Versions et récupération](doc:recovery)

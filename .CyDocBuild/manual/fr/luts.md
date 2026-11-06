---
id: luts
title: LUT 1D, 2D et 3D
description: Importez des tables de couleurs dans les traitements de clip, piste, calques ou conteneur.
section: Guide utilisateur
parent: grading
order: 21
sourceRevision: 3
---

Importez des tables de couleurs dans les traitements de clip, piste, calques ou conteneur.

## Utilisation {#usage}

Les entrées prises en charge comprennent .cube 1D/3D, tables image RG 2D et atlas représentant une table 3D. Apply LUT File dans Effects Browser ajoute une correction native ; les commandes LUT de Color Grading et les nœuds LUT des graphes la placent à différents niveaux. Un conteneur utilise les mêmes nœuds.

```steps
{"items": [{"title": "Étape 1", "body": "Importez la LUT et choisissez la disposition image correspondante si c’est un atlas/table. Un mauvais agencement déforme les couleurs."}, {"title": "Étape 2", "body": "Identifiez ses espaces couleur d’entrée/sortie. Une LUT créative SDR n’est pas automatiquement une conversion de log caméra."}, {"title": "Étape 3", "body": "Placez l’équilibrage avant la LUT, réglez sa force puis comparez au bypass. Vérifiez hautes lumières, saturations et gris neutres."}, {"title": "Étape 4", "body": "Enregistrez : les données LUT sont embarquées. Vérifiez la réouverture sans le fichier LUT d’origine pour confirmer l’autonomie de la correction."}]}
```

[Espaces de travail et précision 16 bits](doc:precision)

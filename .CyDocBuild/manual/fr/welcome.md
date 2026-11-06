---
id: welcome
title: À propos du manuel
description: Apprenez le montage, la composition par nœuds, l’animation et l’audio dans ShaderixFrame. Ce manuel fonctionne entièrement hors ligne.
section: Guide utilisateur
order: 0
sourceRevision: 3
---

Apprenez le montage, la composition par nœuds, l’animation et l’audio dans ShaderixFrame. Ce manuel fonctionne entièrement hors ligne.

## Utilisation {#usage}

Ouvrez **Help > User Manual** ou **Ctrl+F1** dans l’éditeur. CyDoc ouvre le fichier installé `Documentation/Documentation.html` par URL locale, sans serveur web, CDN ni accès Internet.

Choisissez English ou Français en haut. Les identifiants de pages, sections et médias sont communs. Le champ de recherche en haut et **Ctrl+K** recherchent dans le manuel ; **Ctrl+F** du navigateur cherche dans la page. Retour/Suivant du navigateur et sommaire assurent la navigation. Les positions de lecture sont conservées pendant la session.

Les images affichent leurs commandes au survol ou au focus : zoom, déplacement et agrandissement dans cette fenêtre ; Échap réduit. Les commandes restent visibles sur écran tactile. Mon espace garde favoris et notes localement si le stockage du navigateur l’autorise. Imprimer le guide imprime le guide illustré complet ; Ctrl+P imprime la page courante.

```callout
{"type": "note", "title": "Remarque", "body": "Ce guide décrit les fonctions réalisées dans la version alpha 0.14. Les exemples et captures peuvent contenir des noms de clips et titres français : il s’agit du contenu du projet. Les restrictions techniques sont indiquées dans les chapitres concernés."}
```

```diagram
{"type": "flow", "title": "Import / Source → Timeline / clips → Effets / graphes / audio → Export / livraison", "nodes": [{"id": "node-0", "label": "Import / Source", "x": 20, "y": 65, "width": 155}, {"id": "node-1", "label": "Timeline / clips", "x": 240, "y": 65, "width": 160}, {"id": "node-2", "label": "Effets / graphes / audio", "x": 465, "y": 65, "width": 175}, {"id": "node-3", "label": "Export / livraison", "x": 705, "y": 65, "width": 175}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

[Réaliser votre premier montage](doc:quickstart)

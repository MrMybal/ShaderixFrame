---
id: audio-edit
title: Montage audio et nettoyage
description: Rognez le son, préservez la synchro et préparez voix ou musique.
section: Audio et livraison
order: 23
sourceRevision: 1
---

Rognez le son, préservez la synchro et préparez voix ou musique.

## Utilisation {#usage}

Les clips audio vivent sur les pistes et peuvent être liés à l’image. Glissez les poignées de fondu pour éviter les ruptures. La ligne de gain va de -60 à +12 dB ; double-clic revient à 0 dB. Zoomez pour repérer attaques et passages calmes. Gain et panoramique de clip précèdent la chaîne de piste.

```steps
{"items": [{"title": "Étape 1", "body": "Normalize Clip Audio normalise les crêtes (la commande fournie vise -1 dBFS). Cela change le niveau du clip, pas la sonie intégrée."}, {"title": "Étape 2", "body": "Enhance Voice ajoute gate, égaliseur et compresseur. Écoutez syllabes coupées, pompage ou brillance excessive puis ajustez les effets."}, {"title": "Étape 3", "body": "Remove Silence analyse une source audio et referme les intervalles retirés dans la séquence. Commencez prudemment ; liens et verrous sont vérifiés. Contrôlez le rythme de la parole."}, {"title": "Étape 4", "body": "Auto Duck baisse la musique sous la voix par automation. La détection d’attaques crée des repères musicaux ; la synchronisation audio aligne les enregistrements."}, {"title": "Étape 5", "body": "Choisissez l’adaptation temporelle du son étiré et écoutez les artefacts. Inversion et rampes affectent le son comme expliqué au montage."}]}
```

Les analyses indiquent progression et annulation dans la barre d’état, puis appliquent une modification annulable. Ctrl+Z restaure le montage si le nettoyage a retiré un passage utile.

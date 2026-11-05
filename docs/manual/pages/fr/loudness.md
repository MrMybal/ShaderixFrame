---
id: loudness
title: Sonie et vérifications audio finales
description: Mesurez le mixage complet sans confondre crête et volume perçu.
section: Audio et livraison
order: 31
sourceRevision: 1
---

Mesurez le mixage complet sans confondre crête et volume perçu.

## Utilisation {#usage}

LUFS décrit la sonie du programme ; dBTP décrit la crête vraie. Une attaque brève peut atteindre le plafond pendant que la voix reste faible. Normaliser les crêtes d’un clip est différent de mesurer et normaliser le programme complet. Utilisez l’intervalle réel de livraison pour une mesure intégrée pertinente.

```steps
{"items": [{"title": "Étape 1", "body": "Utilisez Sequence > Normalize Loudness et la cible broadcast, podcast ou web correspondant à la livraison."}, {"title": "Étape 2", "body": "Laissez finir la mesure en arrière-plan. Le limiteur master optionnel à -1 dBTP contrôle les crêtes ; observez sa réduction et écoutez les passages exigeants."}, {"title": "Étape 3", "body": "Remesurez après changement du mixage, limiteur ou intervalle. Exportez un JSON de sonie si un rapport est utile."}, {"title": "Étape 4", "body": "Écoutez à niveau modéré la vidéo exportée en plus du montage. Vérifiez intelligibilité, équilibre stéréo, clics aux coupes, écrêtage et fins de réverbération."}]}
```

```callout
{"type": "note", "title": "Remarque", "body": "Les cibles proposées sont des presets, pas une garantie de conformité à tous les diffuseurs. Utilisez les exigences du destinataire et vérifiez le fichier réellement exporté."}
```

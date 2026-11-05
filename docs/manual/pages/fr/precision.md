---
id: precision
title: Espaces couleur et haute précision
description: Distinguez précision de traitement, profondeur des sources et des sorties.
section: Montage et graphisme
order: 22
sourceRevision: 1
---

Distinguez précision de traitement, profondeur des sources et des sorties.

## Utilisation {#usage}

Composition, effets ordinaires et conteneurs utilisent des surfaces RGBA16F : quatre canaux flottants demi-précision préservent les valeurs hors plage d’affichage pendant le traitement. Les espaces proposés comprennent sRGB, Rec.709 linéaire et Rec.2020 linéaire. Gardez une configuration cohérente ; décoder une source et afficher le résultat sont des opérations distinctes.

| Chemin | Précision / périmètre actuel |
| --- | --- |
| Composition intermédiaire | RGBA16F, compositions imbriquées comprises |
| Image/séquence PNG16 | Export entier 16 bits avant quantification d’affichage |
| Image/séquence EXR half | Export demi-flottant, valeurs HDR comprises |
| Décodage/encodage vidéo | Chemins actuels en 8 bits |
| Affichage / scopes | Affichage SDR/tone mapping, pas une chaîne écran HDR |

```callout
{"type": "note", "title": "Remarque", "body": "RGBA16F ne signifie pas décodage vidéo 16 bits, H264/HEVC 10 bits, gestion complète OCIO/ACES ni mastering sur écran HDR. Un effet tiers peut aussi imposer sa précision. Utilisez PrecisionStudy.sframe et un export EXR half pour inspecter les valeurs étendues."}
```

---
id: plugins
title: Plugins natifs, VST3, OFX et shaders
description: Installez des effets compatibles et comprenez les contrats pris en charge.
section: Guide utilisateur
parent: library
order: 27
sourceRevision: 3
---

Installez des effets compatibles et comprenez les contrats pris en charge.

## Utilisation {#usage}

Plugins & MCP expose catalogue et intégration. Les plugins shaders déclarent plugin.json avec chemins relatifs HLSL/graphes dans leur dossier. Recharger le catalogue découvre les changements ; les sources déjà appliquées restent dans les projets. Gardez versions de manifeste et ABI SDK cohérentes lors des mises à jour.

| Format | Hébergement et limites |
| --- | --- |
| Audio natif | DLL Windows x64 ; ABI v1 ShaderixFramePlugin.h ; worker isolé ; jusqu’à 64 paramètres |
| Audio VST3 | Effets x64 via CyDAWPluginWorker.exe ; jusqu’à 512 contrôles ; interface, état opaque et .sfvstpreset |
| Image OFX | Filtres x64 RGBA float pris en charge dans un worker isolé ; HDR et alpha préservés |
| HLSL / graphe | Effets, générateurs et transitions par manifeste ; copiés au projet à l’utilisation |

```steps
{"items": [{"title": "Étape 1", "body": "Pour l’audio, choisissez Native / VST3 dans une chaîne compatible puis le plugin installé. Ouvrez son éditeur et testez un réglage simple avant un preset complexe."}, {"title": "Étape 2", "body": "Pour OFX, utilisez Properties > + Plugin OFX sur un clip vidéo. Choisissez un filtre pris en charge et testez une courte portion, alpha et valeurs étendues compris au besoin."}, {"title": "Étape 3", "body": "Enregistrez, fermez et rouvrez pour vérifier paramètres/état. Le projet stocke l’état, pas le binaire : le même plugin compatible doit exister sur l’autre poste."}, {"title": "Étape 4", "body": "Si un worker signale crash ou timeout, retirez/désactivez l’instance puis testez sa version séparément. Toutes les suites OFX, tous les contextes, API GPU ou architectures ne sont pas hébergés."}]}
```

```callout
{"type": "note", "title": "Remarque", "body": "VST3 exige le worker CyDAW fourni séparément. OFX couvre un sous-ensemble de filtres, pas une compatibilité universelle. Le paquet source inclut en-têtes SDK et informations de licence ; les licences des plugins restent celles de leurs auteurs."}
```

## Manifeste de plugin shader {#shader-plugin-manifest}

Pour l’exemple de teinte, placez plugin.json et tint.hlsl dans plugins/studio-tint/. Donnez un ID unique à l’effet puis rechargez les plugins dans Plugins & MCP. Le shader doit rester dans le dossier du paquet. L’API 1 décrit les effets ordinaires ; les transitions utilisent API 2 et kind: "transition".

```json
{
  "apiVersion": 1,
  "id": "studio.tint-package",
  "name": "Teinte Studio",
  "version": "1.0.0",
  "effects": [
    {"id":"studio.tint","name":"Teinte","category":"Couleur","shader":"tint.hlsl"}
  ]
}
```

```callout
{"type": "note", "title": "Remarque", "body": "OFX exige contexte Filter/General, clips Source/Output et profondeur float. Entrées supplémentaires obligatoires, accès temporel, overlays, effets GPU seuls, générateurs et transitions ne sont pas pris en charge. Les contrôles numériques sont p0, p1, etc. dans les unités du plugin ; chaînes/custom restent aux valeurs initiales. Instruments/MIDI VST3 et sidechains sont hors de ce chemin d’hébergement."}
```

---
id: mixer
title: Mixeur, bus et envois
description: Routez le son entre pistes, bus d’effets partagés et master.
section: Guide utilisateur
parent: audio-edit
order: 24
sourceRevision: 3
---

Routez le son entre pistes, bus d’effets partagés et master.

## Utilisation {#usage}

```image
{"id": "mixer-image-1", "src": "assets/audio-mixer.png", "alt": "Tranches de piste, bus, auxiliaire et master.", "controls": "minimal"}
```
Tranches de piste, bus, auxiliaire et master.

```diagram
{"type": "flow", "title": "Gain / pan du clip → DSP / fader piste → Bus / master → Sortie audio → Envoi / réverbération", "nodes": [{"id": "node-0", "label": "Gain / pan du clip", "x": 15, "y": 20, "width": 160}, {"id": "node-1", "label": "DSP / fader piste", "x": 245, "y": 20, "width": 170}, {"id": "node-2", "label": "Bus / master", "x": 490, "y": 20, "width": 160}, {"id": "node-3", "label": "Sortie audio", "x": 715, "y": 20, "width": 170}, {"id": "node-4", "label": "Envoi / réverbération", "x": 245, "y": 145, "width": 170}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}, {"from": "node-0", "to": "node-4"}, {"from": "node-4", "to": "node-2"}]}
```

```steps
{"items": [{"title": "Étape 1", "body": "Ouvrez Mixer & Audio Graph en Audio (F5). Choisissez piste, bus ou master puis réglez gain, pan, muet/solo et destination."}, {"title": "Étape 2", "body": "Créez un bus pour sommer des pistes liées et routez-les vers lui. Un envoi partage réverbération ou délai sans multiplier les inserts."}, {"title": "Étape 3", "body": "Choisissez envoi pré/post-fader et son niveau. Le pré-fader est indépendant du fader ; le post-fader le suit. Réglez le dry/wet d’une réverbération parallèle de façon cohérente."}, {"title": "Étape 4", "body": "Les vues graphe et signal montrent connexions, forme d’onde, spectre et niveaux. Les cycles sont refusés même via les envois désactivés. Animez gain/pan ou niveaux d’envoi au besoin."}]}
```

Le moteur compense les latences fixes déclarées. Le pan de piste se combine au pan de clip avant les DSP de piste ; celui du bus suit ses DSP. Les effets continuent sur le silence jusqu’à la fin de séquence : laissez assez de durée pour la dernière résonance. Sequence > Sound Output choisit le périphérique.

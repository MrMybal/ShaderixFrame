---
id: devices
title: Track Effects et traitements audio
description: Utilisez des chaînes d’inserts CyDAW avec commandes éditables et animables.
section: Guide utilisateur
parent: mixer
order: 25
sourceRevision: 3
---

Utilisez des chaînes d’inserts CyDAW avec commandes éditables et animables.

## Utilisation {#usage}

```steps
{"items": [{"title": "Étape 1", "body": "Sélectionnez un clip audio ou FX de sa piste pour afficher Track Effects. Choisissez volontairement piste, bus ou master."}, {"title": "Étape 2", "body": "+ Add device cherche par famille ou nom. Power désactive ; glisser réordonne et retirer supprime. L’ordre compte : un EQ avant compresseur change son signal détecté."}, {"title": "Étape 3", "body": "Réglez en écoutant. Un geste de bouton constitue une annulation. Un contrôle animé écrit une clé à la tête ; utilisez l’automation au lieu de changer sans cesse la valeur statique."}, {"title": "Étape 4", "body": "Les commandes natives/VST3 ouvrent la fenêtre du plugin ou ses presets pris en charge. Revérifiez bypass, niveaux et routage après ajout."}]}
```

Le catalogue interne propose 50 inserts stéréo : dynamique, EQ/filtres, réverbérations/convolution, délais, saturation, simulation ampli/cabinet/micro, channel strip/mastering, hauteur, modulation, résonateurs, utilitaires et mesures. Instruments, effets MIDI, modulateurs et conteneurs CyDAW ne sont pas proposés comme inserts ici.

[Construire un graphe DSP parallèle](doc:dsp)

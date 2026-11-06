---
id: troubleshooting
title: Dépannage et limites actuelles
description: Vérifiez les causes usuelles avant de refaire un montage ou d’abandonner votre travail.
section: Guide utilisateur
parent: welcome
order: 36
sourceRevision: 3
---

Vérifiez les causes usuelles avant de refaire un montage ou d’abandonner votre travail.

## Utilisation {#usage}

| Symptôme | Contrôles |
| --- | --- |
| Aperçu noir/transparent | Dépasser un fondu ; vérifier durée, visibilité, opacité, sortie de graphe, médias et application du conteneur |
| Pas de son | Sequence > Sound Output ; muet, solo, gain, routage, flux sonore et worker ; la barre indique les pannes de périphérique |
| Médias absents | File > Relink Missing Media ; nouveau dossier ; attention aux noms identiques |
| Aperçu lent | Proxies, cadrage de la vue, réduction des effets lourds ou Entrée pour le cache |
| Graphe sans effet | Vérifier cible/instance, sortie, bypass et Compile and Apply pour le DSP |
| Raccourcis inattendus | Activer le bon panneau ; champs texte, Source, courbes et graphes ont leur contexte |
| Plugin en échec | Vérifier x64, worker requis, version/chemin et contrat hébergé |
| Export différent du montage récent | L’export utilise l’état au lancement ; relancer après modification |
| Manuel/image manquant | Restaurer le dossier Documentation complet, avec Documentation.html et DocData |

View > Reset This Workspace restaure les panneaux si la disposition devient confuse. Récupération et versions restaurent les montages. Gardez le projet source avant un contournement. --warp choisit D3D11 logiciel pour diagnostiquer le GPU ; il est plus lent qu’un chemin matériel fonctionnel.

```callout
{"type": "note", "title": "Remarque", "body": "Périmètre actuel : Windows x64, son stéréo, décodage/encodage vidéo 8 bits, composition interne RGBA16F, scopes limités d’affichage SDR et sous-ensemble OFX. Pas de chaîne complète OCIO/ACES/écran HDR. Timeline avancée et éditeur de courbes gardent leurs interactions propres, les contrôles communs utilisant CyUI."}
```

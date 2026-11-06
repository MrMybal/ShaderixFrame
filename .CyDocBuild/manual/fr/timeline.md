---
id: timeline
title: Timeline, pistes et navigation
description: Lisez l’arrangement et naviguez de la séquence jusqu’aux images et échantillons.
section: Guide utilisateur
parent: welcome
order: 6
sourceRevision: 3
---

Lisez l’arrangement et naviguez de la séquence jusqu’aux images et échantillons.

## Utilisation {#usage}

```image
{"id": "timeline-image-1", "src": "assets/editing.png", "alt": "La pellicule du rendu final se trouve au-dessus des pistes image et audio.", "controls": "minimal", "annotations": [{"id": "navigator", "type": "rect", "x": 0.31, "y": 0.54, "toX": 0.77, "toY": 0.59, "label": "Bande de navigation"}]}
```
La pellicule du rendu final se trouve au-dessus des pistes image et audio.

- Glissez la bande au-dessus de la règle à gauche/droite pour naviguer, en bas/haut pour zoomer/dézoomer. Double-clic pour cadrer la séquence. Ctrl+molette zoome le temps, Maj+molette le parcourt, la molette défile les pistes et Alt+molette règle leur hauteur.
- Cliquez la règle pour déplacer la tête. Glissez sa partie haute, ou Maj-glissez, pour définir la zone de travail ; ses bords et son corps permettent de la régler. I/O fixent les limites ; L la boucle.
- La rangée FRAME/IMAGE montre la composition finale ; les vignettes montrent le contenu des clips, y compris HLSL et conteneurs. Le zoom révèle les images individuelles. La forme d’onde passe des enveloppes min/max aux échantillons.
- Z cadre la sélection. Le glisser du milieu ou l’outil Main déplace la vue. View affiche/masque vignettes, pellicule, formes d’onde, clés et lignes d’automation. Redimensionnez les en-têtes et les pistes au besoin.

Les en-têtes proposent visibilité ou muet, solo, verrouillage, animation, couleur et renommage. Glissez-les pour réordonner les pistes. Ctrl+G groupe les pistes des clips sélectionnés ; le groupe peut se replier et offre muet/solo. Les verrous protègent aussi le montage lié : un membre verrouillé peut empêcher une coupe ou un déplacement.

[Couper, rogner et changer la vitesse](doc:editing)

[Voir l’annotation du navigateur](doc:timeline?media=timeline-image-1&note=navigator)

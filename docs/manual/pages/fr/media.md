---
id: media
title: Médiathèque, import et proxies
description: Ajoutez vidéos, images et sons au montage et gérez leurs aperçus.
section: Bien commencer
order: 4
sourceRevision: 1
---

Ajoutez vidéos, images et sons au montage et gérez leurs aperçus.

## Utilisation {#usage}

File > Import Media (Ctrl+I) ou le dépôt de fichiers permet l’import. Une vidéo sonore crée des clips image et audio liés sur des pistes séparées. L’onglet Media de Media & Effects contient la médiathèque ; l’ajout au chutier peut être séparé du placement dans la timeline. Double-cliquez une carte pour ouvrir Source ; survolez une vidéo pour la parcourir et lire durée et format.

Windows Media Foundation décode vidéo/audio et WIC les images usuelles ; les codecs disponibles dépendent de la machine. Importez un vrai fichier pour vérifier ses flux. L’extension seule ne garantit pas le décodage. Une image fixe se rogne comme un clip ; générateurs et titres n’exigent pas d’image externe.

```steps
{"items": [{"title": "Étape 1", "body": "Activez Preview Proxies pour les médias lourds. Les proxies automatiques peuvent préparer un aperçu H264 720p après l’import ; la progression indique le travail en arrière-plan."}, {"title": "Étape 2", "body": "Utilisez Prepare Proxy for Selected Clip au besoin. Le proxy allège le montage ; conservez l’original car il sert à l’export final."}, {"title": "Étape 3", "body": "Remove Unused retire du chutier les entrées inutilisées, sans supprimer les sources sur disque. Reliez les fichiers absents plutôt que remplacer manuellement chaque coupe."}]}
```

[Choisir une portion avant insertion](doc:source)

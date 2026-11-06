---
id: shaders
title: Shader Graph et HLSL
description: Créez des effets d’image avec les nœuds Shaderixel ou du code shader.
section: Guide utilisateur
parent: composition
order: 12
sourceRevision: 3
---

Créez des effets d’image avec les nœuds Shaderixel ou du code shader.

## Utilisation {#usage}

```image
{"id": "shaders-image-1", "src": "assets/shader-graph.png", "alt": "Entrée image, désaturation, paramètre animé et sortie.", "controls": "minimal"}
```
Entrée image, désaturation, paramètre animé et sortie.

```steps
{"items": [{"title": "Étape 1", "body": "Sélectionnez le clip vidéo ou nœud à éditer puis ouvrez Shader Graph. Ajoutez des nœuds du catalogue ou importez un JSON Shaderixel compatible."}, {"title": "Étape 2", "body": "Reliez les ports typés depuis l’entrée image jusqu’à la sortie en passant par les traitements. Sélectionnez un nœud pour ses paramètres ; Fit All, molette et glisser du milieu assurent la navigation."}, {"title": "Étape 3", "body": "Les nœuds paramètres exposent les valeurs à animer. Compilez/appliquez et contrôlez la composition. Les erreurs de validation et de compilation signalent ports incompatibles ou code invalide."}, {"title": "Étape 4", "body": "HLSL permet l’édition textuelle. Partez d’un effet de bibliothèque fonctionnel, changez une petite partie, compilez et comparez. Code et réglages sont copiés dans le projet."}]}
```

Le chemin HLSL natif prend en charge jusqu’à quatre entrées, plusieurs passes et l’historique d’images pour les effets ordinaires. Uniformes, déclarations et métadonnées suivent le contrat d’effet : reprenez les déclarations des shaders fournis. Les transitions utilisent un contrat distinct à deux images/progression et restent monopasse. Un shader valide ailleurs peut demander une adaptation.

[Contrats d’effets et de plugins](doc:plugins)

## Un shader de teinte minimal {#a-minimal-tint-shader}

Utilisez ce code pour remplacer un effet vidéo ordinaire dans HLSL, puis compilez. Les métadonnées créent un contrôle couleur et un mélange 0–1. InputUV(0, ctx.uv) lit l’entrée originale de cet effet ; l’alpha est conservé.

```hlsl
//@param tint color 1 0.7 0.3 1 "Teinte"
//@param mixAmount float 0.5 0 1 "Melange"
float4 Effect(EffectContext ctx) {
    float4 c = InputUV(0, ctx.uv);
    c.rgb = lerp(c.rgb, c.rgb * tint.rgb, mixAmount);
    return c;
}
```

EffectContext fournit px, uv, size, texel, pass, time, localTime, deltaTime, frame et sourceSize. time est le temps de séquence et localTime le temps source mappé. InputUV utilise des coordonnées normalisées, Input des pixels. L’entrée 1 peut lire les calques inférieurs. //@passes N déclare 1–8 passes ; Prev/PrevUV lit la passe précédente. History/HistoryUV et HasHistory() accèdent à l’image précédente après la pile du clip. Une recherche temporelle ou modification réinitialise l’historique.

Les métadonnées suivent //@param nom type defauts min max "Libelle". color prend quatre valeurs et un libellé ; bool une valeur et un libellé. Les composantes vectorielles se nomment nom.0 à nom.3 pour animation/MCP. Les noms doivent être des identifiants HLSL valides ; l’hôte accepte jusqu’à 64 paramètres par effet shader.

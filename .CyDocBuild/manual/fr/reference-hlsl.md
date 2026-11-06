---
id: reference-hlsl
title: Interface des effets HLSL
description: Utilisez les entrées, les temps, les passes et l’historique publics des shaders.
section: Référence développeur
parent: api-links
order: 102
sourceRevision: 1
---

## Point d’entrée d’effet {#api}

Les effets d’image ordinaires utilisent `float4 Effect(EffectContext ctx)`. L’hôte fournit `ctx.px`, `ctx.uv`, `ctx.size`, `ctx.texel`, `ctx.pass`, `ctx.time`, `ctx.localTime`, `ctx.deltaTime`, `ctx.frame` et `ctx.sourceSize`. Le temps de séquence diffère du temps mappé dans la source du clip. Retournez la valeur RGBA de sortie en conservant l’alpha si l’effet ne doit pas le modifier.

| Fonction publique | Coordonnées et comportement |
| --- | --- |
| `Input(index, px)` / `InputUV(index, uv)` | Coordonnées en pixels / normalisées d’une image d’entrée, jusqu’à quatre entrées |
| `Prev(px)` / `PrevUV(uv)` | Passe précédente d’un effet multipasse |
| `History(px)` / `HistoryUV(uv)` | Image précédente après la pile d’effets du clip |
| `HasHistory()` | Présence d’un historique d’image valide |
| `HasInput(index)` | Présence de cette image d’entrée |
| `Premultiply(c)` / `Unpremultiply(c)` | Convertir une valeur RGBA vers ou depuis la forme prémultipliée par l’alpha |

`//@passes N` demande de 1 à 8 passes. Un déplacement de lecture ou une modification réinitialise l’historique. L’entrée 1 peut lire les calques inférieurs. Ces fonctions sont fournies par l’hôte au shader ; ce ne sont pas des inclusions du SDK C++. Les transitions utilisent leur propre contrat à deux images et progression, en une seule passe. `ctx.transitionProgress` contient leur progression courante.

## Exemple court {#example}

```hlsl
//@param amount float 0.5 0 1 "Intensité"
float4 Effect(EffectContext ctx) {
    float4 c = InputUV(0, ctx.uv);
    c.rgb = lerp(c.rgb, c.rgb * float3(1.0, 0.8, 0.6), amount);
    return c;
}
```

Consultez [Shader Graph et HLSL](doc:shaders#a-minimal-tint-shader) pour les couleurs et composantes de vecteur, puis les [manifestes de plugins](doc:plugins#shader-plugin-manifest) pour organiser le shader. Shader Graph construit le traitement d’image ; Layer Graph mélange les timelines ; DSP Graph traite l’audio stéréo.

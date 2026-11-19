#pragma once

namespace samples {
inline constexpr const char *document = R"md(# Le laboratoire Markdown

Une page pour tester **le gras**, *l’italique*, ~~le barré~~, <u>le souligné</u>,
==le surlignage== et du `code en ligne`. Accents UTF-8 : été, déjà, cœur.

> Écrivez directement dans cette page, puis sélectionnez du texte pour le formater.
> Les modes Source, Côte à côte et Lecture restent disponibles.

## Liste de vérification

- [x] Ouvrir l’application
- [ ] Modifier une valeur dans le tableau
- [ ] Cocher cette tâche dans l’aperçu
- [ ] Copier un extrait dans le tchat

1. Un premier élément
2. Un deuxième élément
   - Une liste imbriquée
   - Avec du **texte formaté**

## Tableau

| Élément | Valeur | État |
|:--------|-------:|:-----|
| Intensité | 2.56 | **Actif** |
| Limite | 10 | Disponible |
| Texte | UTF-8 | *Éditable* |

## Code C++

```cpp
float value = 2.56f;
float result = std::clamp(value * 100.f, 0.f, 10.f);
// Résultat : 10
```

## Spoiler et image

Cliquez pour révéler : ||Le Markdown fonctionne aussi dans les messages.||

![Aperçu de la palette du thème|360x150](sample://palette)

---

[Revenir au tableau](#tableau) · [Projet CyUI](https://github.com/MrMybal/CyUI)
)md";

} // namespace samples

---
id: reference-sdk
title: SDK audio natif
description: Déclarez un effet audio stéréo isolé avec l’ABI C v1.
section: Référence développeur
parent: api-links
order: 101
sourceRevision: 1
---

## Inclusion et point d’entrée {#L1}

Incluez le fichier distribué `sdk/ShaderixFramePlugin.h`. Compilez une DLL Windows x64 et exportez le symbole C `shaderixframe_plugin_v1` avec `SF_PLUGIN_EXPORT` et `SF_PLUGIN_CALL`. L’en-tête dépend de `<stdint.h>` ; les clients peuvent utiliser C ou C++. L’hôte le charge par `ShaderixFramePluginWorker.exe`.

```cpp
#include "ShaderixFramePlugin.h"
extern "C" SF_PLUGIN_EXPORT const SfPluginV1 *SF_PLUGIN_CALL shaderixframe_plugin_v1(void);
```

## Table ABI {#usage}

Retournez une table `SfPluginV1` avec `size = sizeof(SfPluginV1)`, `version = 1`, un `name` valide, toutes les fonctions de rappel, `parameter_count` jusqu’à 64 et `latency_samples` jusqu’à 480000. Les valeurs de paramètres sont normalisées entre 0 et 1. Déclarez la latence fixe pour permettre sa compensation dans le mixeur.

| Membre | Contrat public |
| --- | --- |
| `parameter_name`, `parameter_default` | Noms et valeurs initiales normalisées par index à partir de zéro |
| `create`, `destroy` | Créer et libérer une instance de traitement |
| `set_parameter` | Modifier un paramètre de cette instance |
| `process` | Lire les plans flottants gauche/droite et écrire des sorties gauche/droite distinctes ; retourner 1 en cas de réussite |

L’hôte fournit actuellement de la stéréo à 48 kHz par blocs de 2 048 échantillons au maximum. Tous les appels ont lieu dans le processus isolé. Ne transmettez jamais de pointeur d’éditeur, ne conservez pas les pointeurs des buffers audio et ne créez pas de thread d’interface depuis une fonction de rappel. Les défaillances du processus sont signalées ; un export échoué préserve le fichier de destination existant.

## Créer et utiliser un effet {#example}

Partez du petit exemple SDK `examples/Gain.cpp` dans la distribution des sources. Compilez votre DLL, ouvrez une chaîne d’effets audio et choisissez **Native / VST3** pour charger l’effet natif. Vérifiez ses commandes, enregistrez le projet puis contrôlez sa restauration. Le projet conserve les réglages, pas votre DLL.

Les pages de fonctions ci-dessous donnent signatures exactes, arguments, retours et relations. VST3 suit un autre contrat et nécessite le processus CyDAW fourni. La page [Compatibilité des plugins](doc:plugins) couvre aussi le traitement des filtres d’image OFX acceptés.

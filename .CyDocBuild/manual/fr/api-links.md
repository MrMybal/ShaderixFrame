---
id: api-links
title: Référence MCP et SDK
description: Automatisez le montage ou créez des effets avec les interfaces publiques.
section: Référence développeur
parent: welcome
order: 100
sourceRevision: 1
---

Choisissez [MCP](doc:mcp) pour automatiser l’éditeur, [le SDK natif](doc:reference-sdk) pour créer des effets audio stéréo ou [HLSL](doc:reference-hlsl) pour traiter l’image. Chaque interface possède ses types de données et son contexte d’exécution.

## Règles des appels {#calling}

Le catalogue MCP expose 163 opérations. Connectez-vous par stdio, appelez `tools/list` pour obtenir les schémas d’entrée actuels, puis `tools/call` avec `name` et `arguments`. Les identifiants de clips et pistes sont stables ; les positions d’effets sont des index à partir de zéro. Les temps sont en secondes sauf indication contraire. Consultez `status` et `list_effects` avant de réutiliser des identifiants.

En cas de réussite, la réponse définit `isError: false`, retourne l’objet de l’opération dans `structuredContent` et inclut le même JSON dans `content[0].text`. Un outil refusé définit `isError: true` et explique le problème dans `content[0].text`. Les requêtes JSON-RPC invalides utilisent plutôt `error`. Un argument facultatif omis est interprété par l’opération ; il n’existe pas de valeur par défaut universelle.

## Requête en lecture seule {#example}

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"status","arguments":{}}}
```

Pour un rendu long, utilisez `start_export`, interrogez `export_status` avec l’identifiant retourné et annulez avec `cancel_export` si nécessaire. Enregistrez explicitement les modifications avec `save_project`. Les outils et la fenêtre du processus travaillent sur la même séquence.

## Trouver une opération {#tools}

Développez les groupes de tâches sous cette page dans la barre latérale, ou saisissez le nom d’une opération dans la recherche globale. Chaque page donne son schéma d’entrée exact, ses arguments obligatoires ou facultatifs, les explications des paramètres et les conventions de résultat. Les identifiants d’API restent identiques dans les deux langues.

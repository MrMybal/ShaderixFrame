---
id: mcp
title: Automatisation MCP intégrée
description: Connectez un client MCP à la session du logiciel par stdio.
section: Guide utilisateur
parent: api-links
order: 33
sourceRevision: 3
---

Connectez un client MCP à la session du logiciel par stdio.

## Utilisation {#usage}

Lancez ShaderixFrame.exe avec --mcp pour exposer JSON-RPC 2.0 par entrée/sortie standard, un message JSON par ligne. --hidden permet l’automatisation sans fenêtre. Le client et l’interface de ce processus partagent une séquence ; cela ne rejoint pas une autre instance déjà ouverte. Le catalogue 0.14 expose 163 outils ; tools/list fournit les schémas de référence.

```json
{
  "mcpServers": {
    "shaderixframe": {
      "command": "chemin/vers/ShaderixFrame.exe",
      "args": ["--mcp"]
    }
  }
}
```

```steps
{"items": [{"title": "Étape 1", "body": "Remplacez command par le chemin de l’exécutable dans la configuration locale du client MCP. Utilisez son lancement de processus normal et des dossiers projet/médias accessibles en écriture."}, {"title": "Étape 2", "body": "Initialisez la connexion, listez les outils puis appelez status pour connaître séquence et sélection. Consultez les catalogues avant de choisir identifiants d’effets ou paramètres."}, {"title": "Étape 3", "body": "Utilisez les IDs stables de clips/pistes retournés par import ou status. Les temps sont en secondes ; les index d’effets commencent à zéro. L’effet omis ou -1 vise généralement le clip si le contrat le permet."}, {"title": "Étape 4", "body": "Vérifiez les réponses : isError signale un refus. Enregistrez explicitement avec save_project. Pour un travail long, utilisez start_export, export_status et cancel_export."}]}
```

| Domaine | Exemples d’outils |
| --- | --- |
| Projets / montage | status, open_project, save_project, select_clips, move_clips, split_clips |
| Graphisme | get_graph, set_graph, set_shader, add_effect, set_keyframe |
| Mixage | set_mix_channel, set_send, add_mix_effect, set_mix_keyframe |
| Livraison | render_frame, start_export, export_status, cancel_export |
| Historique | undo, redo |

```callout
{"type": "note", "title": "Remarque", "body": "Pour les arguments complets de chaque outil, utilisez tools/list dans le client connecté. La référence MCP et SDK de ce lecteur décrit chaque outil et fonction de rappel publics. MCP est un transport de processus local, pas un serveur réseau ni une connexion à un autre éditeur ouvert."}
```

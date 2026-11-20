---
id: mcp
title: Automatisation MCP intégrée
description: Utilisez l’assistant intégré, le terminal et MCP avec les mêmes opérations de l’application.
section: Guide utilisateur
parent: api-links
order: 33
sourceRevision: 8
---

Utilisez l’assistant intégré, le terminal et MCP avec les mêmes opérations de l’application.

## Utilisation {#usage}

Lancez ShaderixFrame.exe avec --mcp pour exposer JSON-RPC 2.0 par entrée/sortie standard, un message JSON par ligne. --hidden permet l’automatisation sans fenêtre. Le client et l’interface de ce processus partagent une séquence ; cela ne rejoint pas une autre instance déjà ouverte. L’éditeur 0.15 expose 163 opérations métier et 23 outils d’interface ; tools/list fournit les schémas de référence.

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
{"type": "note", "title": "Alpha 0.15", "body": "L’assistant et le compagnon console sont fournis avec cette alpha. Les anciens paquets 0.14 ne les contiennent pas. Les fournisseurs, modèles et identifiants se configurent séparément."}
```

## Assistant intégré {#assistant}

Ouvrez **View > Assistant**, ou démarrez avec `--assistant`. Le panneau utilise ChatUI de CyMCPSDK et le rendu Markdown natif CyUI. Dans **Paramètres de l’assistant**, détectez les fournisseurs installés, choisissez une connexion détectée ou renseignez son identifiant et ses paramètres, puis utilisez **Connecter / Nouveau chat**. La détection s’exécute sur un worker et n’envoie aucun message. Une erreur ne provoque jamais de changement silencieux de fournisseur.

Utilisez un service Ollama, LM Studio ou llama.cpp existant pour les modèles locaux, une API compatible ou un CLI natif installé comme Codex. Choisissez un modèle dans le catalogue réellement retourné ; une API distante attend un choix explicite. La clé API vient de la variable d’environnement indiquée et reste hors des fichiers de conversation. Les arguments de processus sont un tableau JSON, lancé sans shell. Modèles, moteurs, CLI et identifiants ne sont pas fournis ni installés automatiquement.

**L’IA peut utiliser les outils de l’application** joint le registre partagé à la prochaine connexion. Les transports capables d’appeler ces outils comprennent les outils dynamiques Codex et les appels de fonctions des API compatibles. Les autres agents natifs nécessitent leur propre connexion MCP configurée ; joindre les outils à un transport incompatible produit une erreur. Les droits de l’agent natif restent distincts des droits de l’application.

Les réponses arrivent progressivement dans la transcription. Annuler interrompt le tour actif tout en conservant sa réponse partielle. Les erreurs restent visibles. Les pièces jointes choisies dans le compositeur sont chargées sur son worker et transmises seulement lors de l’envoi. Les types acceptés dépendent du fournisseur et du modèle. Activez **Enregistrer l’historique de conversation** pour des points de reprise privés et atomiques dans le dossier des conversations. La restauration utilise l’identifiant choisi et la session native exacte enregistrée ; aucun tour n’est automatiquement renvoyé. Reconnecter conserve cette session.

## Ligne de commande {#cli}

`ShaderixFrameCLI.exe` est le compagnon console. `ShaderixFrame.exe --cli` accepte les mêmes commandes et évite la création d’une fenêtre. Les options de lancement existantes restent disponibles hors du mode CLI.

```powershell
ShaderixFrameCLI.exe help
ShaderixFrameCLI.exe list
ShaderixFrameCLI.exe describe set_sequence
ShaderixFrameCLI.exe invoke status --project=examples/CutStudy.sframe --warp
ShaderixFrameCLI.exe invoke set_sequence --json='{"name":"Scripted edit"}' --allow-changes --save=edited.sframe --warp
```

Utilisez `--args=arguments.json` à la place du JSON en ligne si les guillemets du shell posent problème. La sortie est un JSON avec `ok` et `result`, `operations` ou `operation`. Les erreurs produisent du JSON sur stdout et un diagnostic sur stderr. Codes de sortie : 0 succès, 2 arguments/JSON/schéma invalides, 3 droit ou confirmation manquant, 4 authentification, 1 autre erreur. Un appel autonome possède sa séquence ; utilisez `--save` pour conserver les modifications. Les exports autonomes gardent le processus actif jusqu’à la fin de ses travaux d’export. Aucun rattachement implicite à un éditeur ouvert n’a lieu.

## Rejoindre l’éditeur ouvert {#instances}

Dans les paramètres de l’assistant, **Démarrer le MCP local** démarre un serveur HTTP sur `127.0.0.1`. Free est le profil par défaut : utilisez **Copier l’adresse MCP** et connectez-vous avec cette adresse seule. L’adresse finit par `/mcp` ; les appels JSON SDK directs utilisent `/v1`. Aucune interface réseau publique n’est ouverte. Les contrôles Host et Origin restent actifs. Le serveur démarre uniquement sur demande.

```powershell
ShaderixFrame.exe --mcp-http --mcp-info=private/instance.json --assistant
ShaderixFrameCLI.exe invoke status --connect=http://127.0.0.1:PORT/mcp
ShaderixFrameCLI.exe invoke set_sequence --json='{"name":"Address-only edit"}' --connect=http://127.0.0.1:PORT/mcp
```

Remplacez `PORT` par le port affiché dans les paramètres de l’assistant. `--connect=private/instance.json` reste disponible. Les descripteurs sont retirés à l’arrêt normal ; les profils avec jeton enregistrent un identifiant privé dans ce fichier. Gardez ces fichiers hors de Git et des partages. `--mcp` utilise stdio ; `--mcp-compact` expose le portail compact. `ShaderixFrameCLI.exe mcp` et `serve --info=private/instance.json` proposent des sessions autonomes.

## Droits et état de l’éditeur {#permissions}

| Profil | Jeton HTTP | Opérations |
| --- | --- | --- |
| Free (`free`, par défaut) | Aucun | Toutes les opérations enregistrées, sans confirmation supplémentaire |
| Local contrôlé (`local_controlled`) | Aucun | Droit d’écriture de l’instance et confirmation du travail non enregistré |
| Jeton seul (`token_only`) | Requis | Toutes les opérations enregistrées, sans confirmation supplémentaire |
| Protégé (`protected`) | Requis | Droit d’écriture de l’instance et confirmation du travail non enregistré |

Définissez `"access_profile": "protected"` dans la configuration d’automatisation, ou utilisez `--mcp-profile=protected` avec l’un des deux exécutables. Les identifiants sont `free`, `local_controlled`, `token_only` et `protected` (`protected_access` est aussi accepté). Un choix explicite est conservé. Les fichiers existants sans ce champ utilisent Free ; une valeur invalide produit une erreur. Le profil de la ligne de commande remplace celui du fichier pour ce processus.

Les paramètres de l’assistant permettent aussi d’enregistrer un profil en conservant les autres champs. Redémarrez l’application pour l’appliquer ; le serveur ouvert conserve son profil actuel. Sans `--automation-config`, le choix est enregistré dans `%LOCALAPPDATA%/ShaderixFrame/Assistant/automation.json`. Une configuration personnalisée reste le fichier sélectionné.

Assistant, CLI et MCP partagent le même registre d’opérations. Free et Jeton seul utilisent `Policy::allow_all()` et autorisent modifications, Undo, sauvegarde et remplacement des projets non enregistrés sans case d’écriture ni confirmation de remplacement. Les 163 opérations métier et 23 opérations d’interface conservent schémas d’arguments, validation du projet, retour arrière et exécution sur le thread propriétaire. Free n’installe aucun shell et n’ajoute aucun outil externe non sélectionné.

Dans les profils contrôlés, **Autoriser les modifications de l’application** ou `--allow-changes` active l’écriture ; un CLI connecté ne peut pas l’accorder. Remplacer le travail non enregistré nécessite aussi **Autoriser un remplacement** ou `--confirm-discard` en mode autonome. La saisie simulée ne peut accorder ces droits ni changer le profil enregistré. Aucun appel n’est rejoué après expiration, annulation ou référence expirée. L’authentification du fournisseur et les droits des agents natifs restent séparés.

## Pilotage de l’interface {#interface}

L’assistant, le CLI connecté et MCP peuvent inspecter et piloter l’interface réelle, en complément des 163 opérations métier. `get_ui_state` indique panneaux ouverts, mode, géométrie des écrans, thème/langue, navigation et éditeur imbriqué actif. `list_ui_commands` fournit les identifiants stables des commandes et leur disponibilité. Les libellés suivent la langue de l’interface ; les identifiants restent stables.

| Besoin | Opérations |
| --- | --- |
| Palette de commandes | `list_ui_commands`, `ui_command` |
| Modes et fenêtres | `ui_workspace`, `ui_panel` (ouvrir/fermer/focaliser, redimensionner, détacher, ancrer avec un autre panneau) |
| Navigation des vues | `ui_view` (timeline, composition, graphes shader/calques/pistes/DSP, motion) |
| Thème, langue et raccourcis | `ui_preferences`, `list_ui_shortcuts`, `ui_shortcut` |
| Sélection d’effet/canal/source | `ui_focus`, `ui_open_source` |
| Montage de clips imbriqués | `ui_clip_editor`, `get_ui_clip_project`, `ui_clip_invoke` |
| Contrôles visibles | `get_ui_items`, `ui_input`, `get_ui_input`, `ui_cancel_input` |
| Contrôles natifs des plugins | `get_ui_native_windows`, `get_ui_native_controls`, `ui_native_control` |
| Choix des fichiers et aide | `ui_file_list`, `ui_show_help` |

Free et Jeton seul autorisent les opérations d’interface sans confirmation supplémentaire. Dans les profils contrôlés, les appels modifiants exigent **Autoriser les modifications de l’application**, clics et clavier compris ; la saisie synthétique ne peut accorder ce droit ni approuver la confirmation des projets non enregistrés. Validation métier et Undo restent actifs dans chaque profil. Les commandes de fichiers et dossiers acceptent `file_path` ; son absence renvoie `PathRequired` sans ouvrir de sélecteur natif bloquant. Le sélecteur de pièces jointes accepte ce même chemin pendant une saisie automatisée. La recherche de fichiers ne liste que le dossier explicitement fourni, sans récursion ni lecture du contenu.

Privilégiez commandes et paramètres explicites. Pour un autre contrôle visible, recherchez son identifiant et son rectangle avec `get_ui_items`, puis demandez clic, remplacement de texte, touche, molette ou glisser avec `ui_input`. La saisie s’étale sur plusieurs images, entièrement dans le contexte ImGui de l’application. Interrogez `get_ui_input` avec le job retourné avant le geste suivant, puis contrôlez l’état obtenu. La fin du job confirme la livraison des événements, pas la réussite d’une modification métier. Les contrôles absents/désactivés échouent ; annulation, révocation des droits et fermeture relâchent les touches/boutons synthétiques. Aucun geste n’est rejoué. Seul le dernier job est conservé, privé à son appelant. `include_active_text` permet de lire explicitement le champ texte standard focalisé ; les champs de mot de passe sont exclus.

Utilisez `workspace: [identifiantClipConteneur, ...]` pour un éditeur de clip imbriqué déjà ouvert. `ui_clip_invoke` accepte le nom d’une opération métier et ses arguments JSON habituels. Les modifications restent dans le véritable brouillon local jusqu’à `ui_clip_editor` avec `action: "apply"` ; les profils contrôlés refusent de recharger les brouillons non enregistrés, tandis que Free et Jeton seul l’autorisent. Panneaux, commandes et vues acceptent le même chemin de workspace.

La découverte native se limite aux véritables processus VST3 ouverts de ce workspace. L’accessibilité Windows fournit les actions d’activation, valeur, plage, case, sélection et dépliage lorsque le plugin les expose. Les canaux verrouillés ne sont pas modifiables par ce chemin. Les interfaces personnalisées sans contrôles accessibles restent pilotables par les paramètres et états exposés du plugin. Les autres applications, fenêtres de navigateur/compte et identifiants de connexion restent hors de ce pont de fenêtres.

```powershell
ShaderixFrameCLI.exe invoke ui_workspace --json='{"mode":"audio"}' --connect=private/instance.json
ShaderixFrameCLI.exe invoke ui_panel --json='{"panel":"DSP Graph","visible":true}' --connect=private/instance.json
```

## Options indépendantes {#optimizations}

Passez `--automation-config=FICHIER.json` à l’application ou au CLI. Partez de `docs/assistant-config.example.json`. Chaque option est indépendante : `access_profile`, `compact`, `disk_spooling`, `notifications`, `performance`, `inline_bytes`, `chunk_bytes`, `memory_bytes`, `disk_bytes`, `retention_seconds` et `cache_seconds`. Le dossier privé par défaut est `%LOCALAPPDATA%/ShaderixFrame/Assistant` ; `private_directory` le remplace.

La découverte compacte du chat expose cinq outils : recherche, description, appel, lecture de ressource et grep littéral natif. Recherchez une commande, chargez son schéma complet puis appelez-la. La découverte MCP complète reste disponible. `shaderixframe://project` est un instantané JSON immuable de la séquence courante. Les modifications d’interface l’actualisent sous environ une demi-seconde ; les modifications d’automatisation l’actualisent immédiatement. Manifestes et morceaux `firstUri`/`nextUri` conservent tout le contenu pendant la rétention. Les références sont privées au principal appelant ; leur expiration produit un refus sans rejouer l’action. Le stockage disque est désactivé par défaut ; son quota par défaut est de 512 Mio lorsqu’il est activé. Aucun processus ripgrep ni arbre de fichiers arbitraire n’est exposé.

Les changements du registre et des ressources alimentent le journal SDK lorsque les notifications sont activées. Les abonnements dépendent du transport négocié ; les clients doivent conserver les comportements de repli par TTL et actualisation explicite. Le catalogue utilise le TTL configuré ; les mutations ne sont jamais mises en cache. L’option `performance` écrit un JSONL privé et asynchrone contenant noms d’opérations, durées, volumes, état du cache et statut. Messages, arguments, résultats, identifiants appelants et secrets en sont exclus.

**Budget de contexte (Kio, 0 = désactivé)** limite l’historique projeté des fournisseurs API à la prochaine connexion. L’historique complet reste dans l’application ; instructions système, tour actif et échanges d’outils complets restent groupés. Aucun résumeur automatique n’est installé. Les agents CLI natifs conservent leur gestion du contexte.

## Outils sélectionnés d’autres applications {#remote}

Configurez `remote_servers` dans le fichier d’automatisation choisi par l’hôte, puis cliquez sur **Connecter les outils distants autorisés**. Chaque entrée exige `id` et une liste explicite `allow_tools`, ainsi que `url`/`token_env` pour HTTP ou `executable`/`arguments` pour stdio. Une liste vide n’expose rien. Le SDK suit la pagination des outils et conserve leurs schémas. Les outils sélectionnés apparaissent sous `remote.ID.NOM` dans le registre partagé et le chat. Les échecs métier distants restent des erreurs : un échange HTTP réussi ne prouve pas que l’action a réussi. Les droits de mutation s’appliquent aussi à ces outils. Leurs identifiants viennent de l’environnement et ne sont pas enregistrés avec l’historique du chat.

Une liste explicite d’URI `allow_resources` ajoute aussi `remote.ID.read_resource`. Seules ces ressources et leurs liens de morceaux retournés sont lisibles. Les pairs compatibles utilisent les abonnements SDK pour invalider les catalogues et lectures de ressources avant le TTL ; les autres conservent TTL et reconnexion/actualisation explicite. Les mutations vident le cache de lecture distant. Le profil de schéma SDK est volontairement borné : les mots-clés de validation non pris en charge, dont `$ref`, `pattern` et `format`, sont refusés et non supposés fonctionnels.


```image
{"src":"assets/assistant-fr.png","alt":"ShaderixFrame avec le panneau Assistant natif à côté de la composition et de la timeline.","caption":"Profil Free, composition ouverte et paramètres de l’assistant natif dans l’alpha 0.15.0-alpha.2."}
```

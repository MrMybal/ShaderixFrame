# ShaderixFrame 0.15.0-alpha.2

[English](RELEASE_0.15-alpha.2.md) · [Version et téléchargements](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.15.0-alpha.2)

**Alpha destinée aux essais et aux retours.** Cette mise à jour ajoute les profils
d’accès MCP local et utilise Free par défaut. Assistant, CLI et éditeur ouvert
conservent le même registre d’opérations.

## Accès MCP local

| Profil | Jeton HTTP | Droits des opérations |
| --- | --- | --- |
| Free (`free`, par défaut) | Aucun | Toutes les opérations enregistrées, sans confirmation supplémentaire |
| Local contrôlé (`local_controlled`) | Aucun | Droit d’écriture de l’instance et confirmation du projet non enregistré |
| Jeton seul (`token_only`) | Requis | Toutes les opérations enregistrées, sans confirmation supplémentaire |
| Protégé (`protected`) | Requis | Droit d’écriture de l’instance et confirmation du projet non enregistré |

Démarrez le MCP local dans les paramètres de l’assistant, puis utilisez **Copier
l’adresse MCP**. En Free, cette adresse suffit au client. Le serveur écoute sur
`127.0.0.1` et démarre uniquement sur demande. Les schémas d’arguments et la validation
des projets restent actifs avec chaque profil. Free et Jeton seul utilisent
`Policy::allow_all()` pour les opérations enregistrées.

Les configurations sans choix explicite `access_profile` utilisent désormais Free.
Les choix explicites sont conservés. `--mcp-profile=free|local_controlled|token_only|protected`
sélectionne le profil d’un processus. Un choix enregistré dans les paramètres de
l’assistant s’applique au redémarrage. Les profils contrôlés conservent le droit
d’écriture et la confirmation de remplacement du travail non enregistré.

## CLI et interface

`ShaderixFrameCLI.exe --connect` accepte une adresse loopback ou un descripteur
d’instance. Les descripteurs sans jeton ne contiennent pas d’identifiant d’accès.
Les profils avec jeton conservent leur identifiant privé et refusent les connexions
sans jeton valide. `ShaderixFrame.exe --cli` suit le même fonctionnement. Les
modifications et sauvegardes autonomes du CLI suivent également le profil choisi.

Le panneau Assistant natif expose le sélecteur de profil et l’adresse MCP active.
Le pilotage d’interface en Free peut enregistrer un choix explicite ; les profils
contrôlés ne peuvent pas s’accorder des droits supplémentaires par saisie simulée.
Les brouillons de clips imbriqués héritent du profil de l’éditeur parent.

Les sources CyMCPSDK 0.7.1 sont verrouillées au commit
`34d4bcae126bc83d1a2925b5047dac36e3d09d92`. Leur mise à jour explicite conserve
la copie précédente. Le lecteur CyDoc hors ligne anglais/français documente les
quatre profils, les 163 opérations métier et les 23 outils d’interface existants.

## Distribution et limites

La release fournit un ZIP portable Windows x64, un installateur par utilisateur,
les sources correspondantes avec la dépendance JUCE exacte et les sommes SHA-256.
Le processus VST3 compatible IPC-4 conserve sa version et son empreinte enregistrées.

Cette alpha Windows reste non signée. Le décodage et l’encodage vidéo restent en
8 bits ; composition RGBA16F et exports PNG16/EXR half conservent leur périmètre.
L’authentification des fournisseurs et les droits des agents natifs restent séparés
des profils MCP. Les plugins tiers conservent les [limites de l’alpha 1](RELEASE_0.15-alpha.1.fr.md#limites-connues).

Cette version ne publie pas de liste de bugs par ticket. Signalez les problèmes
reproductibles dans le [suivi des problèmes](https://github.com/MrMybal/ShaderixFrame/issues).
Consultez les [licences et sources correspondantes](LICENSING.md) pour les conditions des composants.

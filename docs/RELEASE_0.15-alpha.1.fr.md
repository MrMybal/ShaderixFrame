# ShaderixFrame 0.15.0-alpha.1

[English](RELEASE_0.15-alpha.1.md) · [Release et téléchargements](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.15.0-alpha.1)

**Version alpha destinée aux essais et aux retours.** Cette version ajoute un
assistant agissant sur l’état réel de l’éditeur, un compagnon en ligne de commande
et un accès MCP étendu. Fonctions, formats de projet et compatibilité des plugins
peuvent encore évoluer avant une version stable.

## Assistant et automatisation

- ChatUI natif CyMCPSDK dans **Affichage > Assistant**, avec l’interface CyUI/ImGui de l’application.
- Détection des fournisseurs, sélection du modèle, réponses progressives, annulation,
  erreurs, pièces jointes et historique/restauration de conversation facultatifs.
- Services Ollama, LM Studio et llama.cpp existants, API compatibles et fournisseurs
  CLI installés pris en charge. Moteurs, modèles et identifiants se configurent séparément.
- Le registre partagé conserve les 163 opérations métier et ajoute 23 outils d’éditeur :
  modes, panneaux/ancrage, navigation, préférences, raccourcis, sélection de source/effet,
  brouillons de clips imbriqués, contrôles visibles et contrôles natifs de plugins accessibles.
- `ShaderixFrameCLI.exe` et `ShaderixFrame.exe --cli` proposent aide, découverte,
  schémas et appels JSON. `--connect` cible une instance précise de l’éditeur.
- Le MCP stdio est conservé. Le HTTP loopback authentifié utilise un descripteur
  privé et un jeton Bearer ; le CLI connecté partage les droits de l’éditeur.
- Découverte compacte, ressources/morceaux bornés, stockage disque facultatif,
  notifications de cache, métriques et fenêtre de contexte API sont réglables séparément.
- Les outils et ressources MCP externes explicitement sélectionnés rejoignent ce même registre.

Les modifications nécessitent le droit d’écriture de l’application. Les gestes
automatisés ne peuvent ni l’accorder ni approuver la confirmation de remplacement
d’un projet non enregistré. Les montages imbriqués restent des brouillons locaux
jusqu’à application explicite. Les jobs de saisie indiquent la livraison des
événements, sont annulables et ne sont pas rejoués après un échec.

## Documentation et distribution

Le lecteur CyDoc anglais/français complet est fourni pour une ouverture locale
`file://`. Il comprend le guide de l’assistant et les contrats bilingues de 186
opérations, avec schémas d’entrée/sortie et groupe des outils d’interface. CLI,
licences SDK et notices accompagnent le portable et l’installateur par utilisateur.

Les sources de développement CyMCPSDK 0.7.0 sont verrouillées à
`791d4d7d7680213b282ec8529d248a10f8494d77` ; CyUI 1.2.0 est verrouillé à
`6ef01cfcdb197f3a89d64618ce5ef8818c704728`. Tous deux sont compilés depuis
les sources fournies. Le processus VST3 IPC-4 compatible existant est conservé
avec son empreinte enregistrée.

## Limites connues

- La cible native reste Windows 10/11 x64. Application et installateur ne sont pas signés.
- Décodage/encodage vidéo en 8 bits ; composition RGBA16F et export PNG16/EXR half
  ne constituent pas une chaîne OCIO/ACES ou de monitoring HDR complète.
- L’hébergement OFX conserve le sous-ensemble CPU RGBA filter/general pris en charge.
  Les plugins tiers sont fournis séparément ; leur pilotage natif dépend de leur accessibilité.
- Authentification et droits des CLI natifs restent propres aux fournisseurs.
  Les appels d’outils dépendent du transport choisi ; les intégrations non prises en charge signalent une erreur.
- Les vérifications utilisent des serveurs API/MCP locaux de test et les véritables
  instances de l’application. Elles ne valident pas l’authentification réelle ou la qualité de tous les modèles cloud.

## Téléchargements et signalements

Utilisez le ZIP portable ou l’installateur par utilisateur, les sources correspondantes
et les sommes SHA-256 de la [release](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.15.0-alpha.1).
Pour un problème reproductible, indiquez version, étapes, résultats attendus/obtenus
et projet minimal dans le [suivi des problèmes](https://github.com/MrMybal/ShaderixFrame/issues).
Cette version ne publie pas de liste de corrections bug par bug. Consultez les
[licences et sources](LICENSING.md) et le manuel embarqué pour les procédures détaillées.

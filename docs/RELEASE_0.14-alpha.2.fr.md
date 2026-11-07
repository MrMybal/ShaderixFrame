# ShaderixFrame 0.14.0-alpha.2

[English](RELEASE_0.14-alpha.2.md) ·
[Alpha publiée et téléchargements](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.14.0-alpha.2)

**Version alpha destinée aux essais et aux retours.** Certaines fonctions peuvent
être incomplètes et les contrats des projets/plugins peuvent changer avant une
version stable.

## Documentation intégrée

Help > User Manual et Ctrl+F1 ouvrent le lecteur CyDoc fourni à l’emplacement
`Documentation/Documentation.html`. L’aide contextuelle choisit le panneau actif,
y compris les panneaux détachés et les éditeurs de clips imbriqués. F1 conserve
l’espace Edit. Les anciens noms de pages et adresses du manuel gardent leurs
redirections de compatibilité.

Le manuel comprend 38 chapitres en anglais/français, neuf captures du logiciel,
cinq schémas SVG et une galerie de six vues des espaces de travail. La navigation
est organisée par tâches et sous-pages. Recherche globale en haut, Ctrl+K,
infobulles descriptives, thèmes, favoris, notes et positions de lecture utilisent
le lecteur CyDoc mis à jour. Les médias s’agrandissent dans la fenêtre et Échap
les réduit. Imprimer le guide ouvre le guide utilisateur complet.

La référence de développement décrit les 163 opérations MCP dans les deux langues
et les signatures publiques du SDK audio natif C/C++, avec paramètres, conventions
de résultat et courts exemples JSON/HLSL. Elle indique les limites de séquence
validées : 64–8192 pixels et 1–120 images/s. Les rapports internes, notes de
maintenance et listes d’implémentation sont exclus du lecteur destiné aux utilisateurs.

Le lecteur complet et ses ressources sont fournis localement dans Documentation.
La lecture ne nécessite ni serveur, CDN, environnement Node ni connexion Internet.
Seuls les modules image et galerie sont sélectionnés. DocAI.md est un fichier
séparé pour les assistants. GitHub Pages génère le même export via le workflow
de documentation.

## Plateforme et compatibilité

Cette préversion cible Windows 10/11 x64. Les projets et procédures de montage
conservent le format 0.14. Le décodage et l’encodage vidéo restent en 8 bits,
avec composition interne RGBA16F et export PNG16/EXR half. OFX conserve le
sous-ensemble CPU RGBA filtre/général pris en charge ; VST3 nécessite le processus
compatible fourni. Les binaires des plugins tiers ne sont pas inclus.
L’application et l’installateur par utilisateur ne sont pas signés.

## Limites connues

- Le moteur natif cible Windows 10/11 x64 ; aucune version Linux ou macOS n’est fournie.
- Le décodage et l’encodage vidéo restent en 8 bits. La composition RGBA16F et
  l’export PNG16/EXR half ne constituent pas une chaîne complète OCIO/ACES ou
  de contrôle sur moniteur HDR.
- L’hébergement OFX couvre le sous-ensemble CPU RGBA filtre/général pris en charge,
  pas toutes les suites OFX ni tous les moteurs GPU. Les plugins VST3 doivent être
  Windows x64 et utiliser le processus compatible.
- Les codecs d’import/export disponibles dépendent de Windows Media Foundation.
  Les plugins audio natifs doivent respecter l’ABI documentée. Les contrats des
  projets et des API peuvent évoluer.

## Signalement et téléchargements

Ces notes de version ne publient pas de liste de bugs détaillée. Consultez le
[suivi des problèmes](https://github.com/MrMybal/ShaderixFrame/issues) pour les
signalements, ou fournissez des étapes reproductibles, la version de l’application,
le comportement attendu/observé et un projet minimal si possible. Les limites
ci-dessus décrivent le périmètre actuel ; elles ne constituent pas une liste de
tickets de bugs confirmés.

Téléchargez le ZIP portable ou Setup.exe, avec l’archive source correspondante
et les sommes de contrôle. Utilisez les [fichiers publiés de l’alpha 2](https://github.com/MrMybal/ShaderixFrame/releases/tag/v0.14.0-alpha.2).
Consultez les [licences](LICENSING.md) et la [présentation de l’alpha](RELEASE_ALPHA.md),
en anglais, ainsi que le [manuel français](https://mrmybal.github.io/ShaderixFrame/Documentation.html#/fr/welcome).

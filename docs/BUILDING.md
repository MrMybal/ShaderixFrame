# Construire ShaderixFrame

Installer Visual Studio 2022 avec les outils C++ x64, CMake, Ninja et Python 3.
Les sources partagées Shaderixel, CyDAW v2, ImGui et OpenFX sont conservées dans
`vendor` avec leurs licences et empreintes. Aucun des projets d'origine n'est modifié.

Le dépôt contient les sources et les petits exemples de projet. Les exécutables,
DLL/VST3, médias générés, archives, sorties de build et caches restent séparés.
Git LFS n'est pas nécessaire.

## Composants binaires optionnels

Le moteur audio interne, le DSP et les plugins natifs se compilent depuis les sources.
L'hébergement VST3 nécessite également `CyDAWPluginWorker.exe`, fourni dans le paquet
Windows ShaderixFrame correspondant à la version verrouillée. Après avoir téléchargé
ce paquet lorsqu'il est disponible dans Releases, restaurer son worker avec :

```powershell
python tools/prepare_dependencies.py --bundle chemin/ShaderixFrame-windows-x64.zip
```

Un dossier de paquet déjà extrait convient aussi. Le script vérifie l'empreinte
SHA-256 avant de copier le composant. En développement, il peut le récupérer depuis
un checkout CyDAW v2 indiqué par `SF_CYDAW_ROOT`. `--tests` restaure aussi le VST3 de
test depuis ce checkout ; il n'est pas fourni dans le paquet utilisateur.
Si le worker n'est pas installé, le logiciel se compile avec ses traitements internes
et ses plugins natifs ; l'ouverture d'un VST3 requiert son installation séparée.
La production d'un paquet Windows complet nécessite le worker.
Les suites signalent explicitement les essais VST3 omis lorsque leur fixture externe
n’est pas installée ; les essais vidéo, DSP, plugins natifs et MCP restent exécutés.

La bande sonore originale des exemples peut être générée sans téléchargement :

```powershell
python tools/prepare_dependencies.py --audio-only
build.bat Release
```

`--audio-only` conserve les exemples existants et génère uniquement leur son manquant.
`make_demo.py` régénère également les projets qu'il définit ; conserver les éditions
personnelles de ces projets sous d'autres noms avant de l'utiliser.

## Sources de développement et mises à jour

Les chemins locaux restent configurables et ne sont pas enregistrés dans les empreintes :

```powershell
$env:SF_SHADERIXEL_ROOT = 'chemin/Shaderixel'
$env:SF_CYDAW_ROOT = 'chemin/CyDAW_Project'
$env:SF_JUCE_ROOT = 'chemin/juce-src'
python tools/sync_upstream.py --check
python tools/import_shaderixel.py --check
python tools/extract_composition.py --check
python tools/verify_snapshots.py
```

Sans ces variables, les scripts recherchent les dossiers Shaderixel et CyDAW_Project
à côté du checkout ShaderixFrame. Les fichiers de build de ces projets sont nécessaires
uniquement à la restauration des binaires optionnels et à leur mise à jour explicite.

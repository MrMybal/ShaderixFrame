# Origine des composants

- `vendor/shaderixel/` : copies autorisées par le propriétaire du projet depuis
  Shaderixel. Thème Dear ImGui, types communs, JSON, compilateur EffectGraph, Graph de Document, NodeEditor, shaders de fusion et 317 shaders d’effets/générateurs.
  `tools/import_shaderixel.py` dérive le catalogue HLSL dans `plugins/shaderixel-library`,
  avec helpers/paramètres d’origine et adaptations Slang/HLSL. Les copies originales restent intactes.
  Des extractions déterministes dans `src/adapters` adaptent les frontières vidéo/D3D11.
- `vendor/cydaw2/` : copies autorisées depuis CyDAW v2. Modèle de graphe
  partagé par CyDAW 2, AudioGraph/AudioRenderPlan, buffers, analyse et catalogue/processeurs DSP.
  Widgets/Theme, cartes GraphEditor, bandeaux MixerView et fonds de clips ArrangementView
  alimentent les extractions portables, sans modifier les sources copiées.
  Les libellés DSP reprennent les traductions françaises `resources/i18n/fr.json`. Les fichiers
  `src/v2/ArrangementView.*` et `src/v2/Theme.h` sont conservés comme
  références d'adaptation ; la coque JUCE et l'ancienne interface ne sont pas compilées.
  Le worker VST3 précompilé, son source, l'en-tête IPC et le VST3 de test sont aussi
  copiés avec empreintes. Le worker distribué utilise JUCE ; la licence originale
  figure dans `vendor/juce-license/LICENSE.md` et `JUCE-LICENSE.md` dans le paquet.
- `vendor/imgui/` : Dear ImGui, copie locale de la branche docking utilisée par Shaderixel,
  licence MIT conservée dans `vendor/imgui/LICENSE.txt` et dans la distribution.
- `vendor/openfx/` : en-têtes et exemples officiels ASWF OpenFX, commit
  `e40728885390ec16276d11e00025de9b4282060c`, empreintes dans `openfx.lock.json`.
  Licence conservée dans `vendor/openfx/LICENSE.md`, distribuée comme
  `OPENFX-LICENSE.txt`. L'exemple Basic non modifié sert aux tests, le worker hôte
  et l'exemple OfxGain sont locaux. Les références GraphEditor/GraphLayout/Widgets
  et AudioTrackController de CyDAW v2 ont également été copiées avec empreintes.
- Les logos présents à la racine ont été fournis dans le dossier de travail.
- La lumière procédurale, les shaders inclus et la bande sonore de démonstration sont
  créés pour ShaderixFrame. Les polices Segoe UI et Consolas sont lues depuis Windows,
  avec la police ImGui comme repli ; elles ne sont pas redistribuées.

Ce document indique la provenance technique. Aucune nouvelle licence n'est attribuée
aux projets Shaderixel ou CyDAW ; leurs droits existants restent applicables.

Le SDK ABI 1, son worker natif, les exemples Gain/Latency et les adaptateurs
vidéo/DSP/cache/export sont propres à ShaderixFrame. Le plugin VST3 de test reste
dans les snapshots de développement ; il n'est pas inclus dans le ZIP utilisateur.

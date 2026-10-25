# Après la 0.13

Les modes de travail et la timeline inspirés de CyDAW v2, l'éditeur de courbes, le zoom de la
vue finale et les 406 shaders Shaderixel sont intégrés : [RELEASE_0.13.md](RELEASE_0.13.md).
La 0.12 avait apporté les graphes DSP/calques, fenêtres détachables, conteneurs de clip,
widgets motion, LUT images et transitions : [RELEASE_0.12.md](RELEASE_0.12.md).
Shaderixel et CyDAW restent en lecture seule. Le schéma 10 charge 1–10.

Les extensions suivantes restent distinctes de cette livraison :

- Resynchronisation de `Document`/`NodeEditor` Shaderixel (nœuds « fonction » des graphes de
  calques) avec adaptation de `tools/extract_composition.py`, d'ImGui et des sources CyDAW.
- Éditeur de courbe par effet pour les paramètres `curve` de Shaderixel (Photo RAW).

- Codecs vidéo HDR 10/12 bits, PQ/HLG, écran HDR, OCIO/chaîne ACES complète,
  EXR compressés/tuilés/multipart ; projets image/3D Shaderixel et polices embarquées.
- Tracking rotation/déformation/occlusion, rotoscopie avec courbes de points animées,
  écoute comparative du stretch sur voix/transitoires réels et ratios extrêmes.
- LRA et suite officielle de conformité LUFS/true-peak.
- Graphes contrôles/MIDI/instruments, sidechains VST3 et automation par sample.
- OFX GPU/overlays/accès temporel/générateurs/transitions/paramètres custom et
  catalogue de plugins commerciaux testé.
- Formats d'échange de projets, ressources MCP et connexion à une instance existante.
- Essais multi-écrans physiques à DPI différents, autres versions/machines Windows et codecs/plugins,
  installateur signé avec désinstallation intégrée aux paramètres Windows.

Une mise à jour upstream garde ses anciennes copies et exige recompilation,
fixtures de migration/graphes et tests de traitement. Les empreintes ne suffisent
pas, à elles seules, à garantir la compatibilité d'une nouvelle version des sources.

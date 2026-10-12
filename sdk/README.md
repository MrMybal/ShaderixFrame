# SDK natif ShaderixFrame v1

Inclure `ShaderixFramePlugin.h` et exporter `shaderixframe_plugin_v1` avec liaison C.
Retourner une table `SfPluginV1` de taille exacte et `version=1`. Tous les callbacks
sont obligatoires. `create` prépare un processeur stéréo à 48 kHz, blocs de 2048 frames
maximum ; `process` reçoit quatre plans float indépendants et retourne 1 si réussi.
Les valeurs de paramètres sont normalisées entre 0 et 1, avec au maximum 64 paramètres.
Déclarer la latence fixe en samples dans `latency_samples` (maximum 480000).

Une instance s'exécute dans `ShaderixFramePluginWorker.exe`. Un crash ou un blocage
est signalé au moteur appelant ; un export échoué garde la destination précédente.
Les callbacks ne doivent pas utiliser de pointeurs vers l'éditeur, créer leur propre
thread d'interface ou conserver les pointeurs de buffers après `process`.

`examples/Gain.cpp` fournit un effet neutre à p0=0.5. `examples/Latency.cpp` sert de
fixture de compensation. Les cibles CMake `native_gain` et `native_latency` les
compilent. Les DLL SDK, profils de paramètres et headers ne dépendent pas des projets
Shaderixel ou CyDAW. Les shaders HLSL conservent leur manifeste séparé API 1/2.

Le VST3 utilise le worker de CyDAW v2 copié, avec son protocole IPC 4. Ce worker JUCE
est accompagné de sa licence et de sa provenance ; le nouveau SDK natif ne dépend
pas de JUCE. Les états opaques et fenêtres VST3 sont disponibles dans l'intégration
0.9, tout en restant distincts de l'ABI native v1.

`examples/OfxGain.cpp` exporte l'ABI OpenFX officielle, sans utiliser le SDK audio
natif. Compiler avec les en-têtes ASWF (`vendor/openfx/include` dans le dépôt,
`sdk/openfx` dans le ZIP), liaison DLL et suffixe `.ofx` ; cible CMake `ofx_gain`.
Le gain traite Source/Output RGBA float dans Filter. L'hôte OFX de ShaderixFrame
utilise son propre worker et conserve l'alpha et les valeurs HDR.

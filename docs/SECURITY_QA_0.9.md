# Incident de validation antivirus — 4 octobre 2026

L'utilisateur a communiqué une alerte Kaspersky : `build/Debug/audio_quality_tests.exe`
a été supprimé par l'antivirus fichiers, catégorie Cheval de Troie, lors de la
production par `link.exe`. Aucun identifiant de détection plus précis n'a été fourni.
Le fichier supprimé n'a pas été restauré ni recréé. Aucune exclusion, désactivation,
élévation, modification de l'antivirus ou envoi à un service externe n'a été effectué.

Le `link.exe` MSVC 14.40.33807 de Visual Studio 2022 a une signature Authenticode
valide. Le source local `tests/AudioQualityTests.cpp` génère un WAV synthétique dans
`build/qa/audio-quality`, appelle le moteur audio et compare les résultats. Ce constat
et la signature du compilateur ne suffisent pas à conclure à un faux positif.

Les 36 assertions audio de Release ont réussi avant l'alerte. Elles n'ont pas été
relancées après celle-ci ; Debug audio reste non validé. `build.bat Release --safe-compile`
compile les autres cibles explicitement sans reconstruire ce test. Les sélections
ordinaires CMake omettent aussi la cible (option `SF_BUILD_AUDIO_QUALITY_TESTS`
désactivée par défaut), pour éviter sa recréation à la compilation suivante. Les sélections
CTest utilisées après l'incident excluent `audio_quality`, sous les mêmes protections
antivirus. Le binaire Debug `frame_tests.exe` a également été constaté absent lors
de CTest après sa compilation ; il n'a pas été recréé. La cause de cette deuxième
absence n'est pas confirmée par un détail de détection. La matrice Debug reste
incomplète, avec deux délais dépassés supplémentaires. La livraison s'appuie sur
les suites Release réussies matériel/WARP. Les tests ne font pas partie du ZIP
ni de l'installateur utilisateur.

Les workers de plugins sont lancés directement par `CreateProcessW` avec chemin
explicite et arguments cités. Leur DLL est chargée dans leur propre processus.
Ils ne lancent pas PowerShell, n'ont pas d'élévation et utilisent `CREATE_NO_WINDOW`
pour leur exécution de fond ; les fenêtres d'édition VST3 apparaissent sur demande.
L'isolation de processus protège la stabilité, pas une sandbox de sécurité des DLL.
Charger un plugin externe suppose donc une source de confiance.

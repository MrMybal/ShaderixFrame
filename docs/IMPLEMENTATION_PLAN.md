# Programme d'évolution demandé

Demande : réaliser les six chantiers décrits après la 0.7. Les projets Shaderixel et
CyDAW restent en lecture seule ; les composants copiés conservent leur provenance.
Les six chantiers ci-dessous sont intégrés dans la 0.8.0 et vérifiés. Le périmètre
et les limites des SDK/machines sont décrits dans [RELEASE_0.8.md](RELEASE_0.8.md)
et les résultats dans.

- [x] Fluidité : import/décodage en arrière-plan, cache audio disque, vignettes et proxies.
- [x] Montage : trims ripple/roll, fermeture avec découpe, chevauchement automatique des transitions.
- [x] VFX : deux entrées, 1–8 passes, historique de frames, masques Bézier, suivi de translation, titres contours/ombres.
- [x] Audio : graphes DSP éditables/importables en JSON, envois animés, latence, WSOLA, LUFS.
- [x] Plugins : SDK natif ABI 1, isolation, VST3, fixtures de compatibilité/upstream.
- [x] Sorties : file de rendus, presets, AVI/PNG/rapports supplémentaires, installateur et matrice Release/Debug × GPU/WARP.

La branche OFX conditionnée à un SDK exploitable reste différée : aucun n'est disponible
dans les sources locales. Aucun résultat n'est annoncé pour Windows 11 ou d'autres
machines. L'éditeur graphique DSP, les états/GUI VST3, HDR et les mesures true-peak
restent des extensions explicites, séparées des fonctions livrées.

## Validation

Chaque ajout doit être vérifié dans le cœur, via MCP et dans l'éditeur lorsqu'il a
une interface. Les exports sont redécodés ; caches/proxies et plugins utilisent des
fixtures reproductibles. Une archive portable est vérifiée dans un dossier séparé.
Les configurations réellement testées seront nommées ; une machine Windows ou un
plugin non disponible ne seront pas présentés comme testés.

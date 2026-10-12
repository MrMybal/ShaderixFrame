# Évolution demandée — 0.9

Sources Shaderixel et CyDAW en lecture seule. Reprise des interactions du graphe
ImGui CyDAW v2 et de ses commandes IPC VST3 ; adaptation du moteur granular audio.
OFX et true-peak/LUFS ne sont pas présents dans les sources CyDAW examinées :
ajouts locaux avec API OpenFX officielle et références audio documentées.

- [x] Graphe DSP visuel : cartes/ports/câbles, paramètres, recherche, zoom/pan,
  sélection, déplacement, organisation par signal et historique.
- [x] VST3 : fenêtres isolées, états/presets embarqués, paramètres nommés, sauvegarde
  et restitution au rendu ; OFX isolé avec validation des capacités.
- [x] Composition : RGBA16F, couleurs linéaires/gestion des transferts, alpha correct,
  aperçu SDR séparé et sorties haute précision.
- [x] Audio : modes CyDAW tonal/rythmes/texture, essais de qualité reproductibles,
  true-peak et maintien des crêtes, mesure LUFS complétée et vérifiée.
- [x] Tests, migrations, documentation, ZIP et installateur actualisés ;
  Release matériel/WARP validés, validation Debug partielle décrite dans le rapport.

La validation Debug du test audio optionnel reste suspendue après sa suppression
par Kaspersky lors de la liaison. Aucun contournement ; huit autres suites restent
actives. [Incident et exécutions](SECURITY_QA_0.9.md).

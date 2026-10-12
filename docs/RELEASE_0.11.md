# ShaderixFrame 0.11.0 · Graphes de composition et DSP partagés

Deux niveaux vidéo indépendants reprennent les composants de Shaderixel :

- **Graphe des calques** : chaque timeline vidéo fournit une image transparente.
  Relier ces sources aux fusions, corrections, effets et à la sortie. Une source
  peut alimenter plusieurs branches ; une correction peut précéder ou suivre une fusion.
- **Graphe VFX** : construit le shader d'un effet. Sélectionner un effet dans le
  graphe des calques puis **Ouvrir le graphe shader** pour éditer ce second niveau.
  Le bouton **Retour aux effets du clip** rétablit l'édition des effets du clip.

Le graphe des calques utilise les types, ports et détection de cycles du `Graph`
de Shaderixel. Ses cartes, pins, couleurs et câbles proviennent de son NodeEditor.
Les 27 modes et le calcul de composition proviennent des shaders Shaderixel,
avec adaptation des ressources à D3D11/HLSL. Alpha, masque sur la troisième
entrée, écrêtage sur l'alpha du fond, canaux conservés et Blend If gris/RGB sont disponibles.
La composition et les sorties PNG16/EXR conservent les intermédiaires RGBA16F.

**K** anime l'opacité des fusions et les paramètres des shaders/corrections au
temps de séquence. **Courbe** permet de modifier ou retirer les points et de
choisir l'interpolation linéaire, douce, palier ou courbée. Les connexions restent
fixes pendant la lecture. Molette : zoom ; bouton central : déplacement ; tirer
une sortie vers une entrée : connexion ; clic droit sur une entrée : déconnexion.
**Tout voir / F** cadre les nœuds. Les vignettes montrent leurs sorties réelles.

Sans graphe explicite, les pistes génèrent automatiquement un graphe compatible
avec les anciens projets. La première modification le rend explicite et
enregistrable. Ajouter ensuite une piste exige de la relier dans ce graphe.
**Reconstruire depuis les pistes** remplace le graphe explicite ; cette opération
est annulable. Les fusions entre clips d'une même timeline restent dans la timeline.

## Audio CyDAW v2

Les graphes DSP compilent et s'exécutent maintenant dans le véritable
`AudioRenderPlanBuilder` / `AudioRenderPlan` de CyDAW v2. Les processeurs du
catalogue et la validation `GraphDefinition` sont copiés sans modification.
Leurs paramètres peuvent recevoir des clés de séquence, évaluées par blocs de
64 samples. Les cartes utilisent le dessin de GraphEditor et les véritables
boutons rotatifs de Widgets. **Bandeaux CyDAW** ajoute les faders, pans, mute/solo
et vu-mètres de ces composants. Un DSP déposé sur un clip audio est ajouté à
la chaîne de sa piste. Les anciens effets de clip restent chargés et exécutés.

Les pistes audio servent de canaux à l'arrangement vidéo, puis rejoignent leurs
bus/envois et le master. Les fenêtres et états VST3 isolés restent accessibles
dans les chaînes de canaux.

## Compatibilité et limites

Projet schema **9**, lecture des schemas 1..9 ; MCP **94 outils** avec
`get_layer_graph`, `set_layer_graph`, `set_layer_parameter`, `set_layer_keyframe`.
`examples/LayerGraphStudy.sframe` démontre les deux niveaux de graphes,
les branches de couleur, une fusion animée et une timeline de titre.

Les snapshots source restent intacts. `tools/extract_composition.py` régénère
les composants portables depuis leurs empreintes ; `--check` vérifie la dérivation.
`tools/sync_upstream.py` permet une mise à jour explicite et conserve l'historique.
Toute mise à jour exige régénération, compilation et validation des contrats.

L'interface adapte ces composants aux timelines vidéo ; elle n'embarque pas toute
la coque JUCE Studio de CyDAW. Son JSON DSP est une frontière stéréo versionnée :
les graphes Studio arbitraires, sous-graphes, ports MIDI/contrôle, instruments et
sidechains ne sont pas encore importables. Le contrôleur d'arrangement entier
et toutes les interactions du NodeEditor Shaderixel ne sont pas portés.

Les effets HLSL, EffectGraph et corrections fonctionnent dans le graphe des
calques ; OFX reste dans les piles des clips. History est disponible dans les
effets de clip, pas encore comme historique des nœuds de composition.
L'inspecteur des corrections du graphe offre paramètres animables, presets et
LUT ; l'édition visuelle des courbes couleur, pipettes et suivi reste dans
l'onglet Étalonnage des clips. Les réglages complets sont incorporables au JSON
du nœud. Le décodage et les codecs vidéo restent 8 bits, malgré la composition
16 bits flottants. Voir aussi les limites des [corrections](RELEASE_0.10.md).

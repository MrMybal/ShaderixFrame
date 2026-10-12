# Plugins HLSL, API 1 et 2

Créer `plugins/mon-plugin/plugin.json` et un fichier HLSL dans le même dossier :

```json
{
  "apiVersion": 1,
  "id": "studio.mon-plugin",
  "name": "Mon plugin",
  "version": "1.0.0",
  "effects": [
    {"id":"studio.tint","name":"Teinte","category":"Couleur","shader":"tint.hlsl"}
  ]
}
```

```hlsl
//@param tint color 1 0.7 0.3 1 "Teinte"
//@param mixAmount float 0.5 0 1 "Mélange"
float4 Effect(EffectContext ctx) {
    float4 c = InputUV(0, ctx.uv);
    c.rgb = lerp(c.rgb, c.rgb * tint.rgb, mixAmount);
    return c;
}
```

Les champs disponibles sont `ctx.px`, `uv`, `size`, `texel`, `pass`, `time`, `localTime`,
`deltaTime`, `frame` et `sourceSize`. `time` est le temps de séquence, `localTime` est le
temps source du clip et `frame` suit le fps de la séquence. `InputUV(0,uv)` et `Input(0,px)`
lisent l'entrée originale du clip pour cet effet. `Input(1)` lit les calques inférieurs.
`//@passes N` accepte 1–8 passes ; `Prev/PrevUV` lisent la passe précédente.
`History/HistoryUV` et `HasHistory()` donnent la frame précédente après la pile du clip.
Une recherche/édition réinitialise cet historique ; répéter une frame ne réaccumule pas.

Métadonnées compatibles : `//@param nom type valeurs min max "Libellé"`. `color` fournit
quatre valeurs puis le libellé, `bool` une valeur puis le libellé. Un paramètre vectoriel
expose `nom.0` à `nom.3` pour les keyframes/MCP. Les ID sont uniques dans toute la
bibliothèque ; les noms de paramètres doivent être des identifiants HLSL valides.
64 paramètres maximum par effet. La bibliothèque refuse les versions API inconnues,
les shaders introuvables et les chemins sortant du dossier du paquet.

**Plugins & MCP > Recharger les plugins** rescane les paquets. Le projet embarque le
code à l'ajout : mettre à jour un plugin ne réécrit pas silencieusement les montages.
Pour adopter une nouvelle version, retirer/réajouter l'effet ou appliquer son nouveau
code dans HLSL. Le plugin `film-grain` livré sert d'exemple complet.

Cette API concerne les effets GPU. Les DSP CyDAW sont compilés dans l'application.
Les DLL natives, VST3 et OFX utilisent des workers séparés, décrits ci-dessous.

## Transitions, API 2

Les paquets API 1 restent pris en charge pour les effets ordinaires. Un paquet qui
déclare une transition doit utiliser `apiVersion: 2` et `kind: "transition"` sur son
entrée. Le champ `kind` omis vaut `effect`. Un ancien hôte refusant API 2 indique
explicitement l'incompatibilité au lieu d'utiliser le shader comme un effet ordinaire.

```json
{
  "apiVersion": 2,
  "name": "Mes transitions",
  "effects": [
    {"id":"studio.reveal","name":"Révélation","kind":"transition","shader":"reveal.hlsl"}
  ]
}
```

```hlsl
//@param transition_curve float 1 0.1 4 "Courbure"
float4 Effect(EffectContext ctx) {
    float progress = pow(saturate(ctx.transitionProgress), transition_curve);
    return lerp(InputUV(1, ctx.uv), InputUV(0, ctx.uv), progress);
}
```

L'entrée 0 est la composition incluant le clip entrant, déjà transformé/masqué ; l'entrée
1 est la composition des calques dessous. Ces deux frames ont la résolution de séquence.
`HasInput(0/1)` et `Input(0/1, px)` sont disponibles. Dans ce passage, `ctx.localTime`
est le temps de séquence depuis le début du clip ; les effets ordinaires gardent le
temps source. `ctx.transitionProgress` suit la durée choisie ou l'animation de progression.
Les paramètres déclarés commencent par `transition_` ; `transition_progress` est réservé
et ne doit pas être déclaré. Garantir que 0 retourne l'entrée 1 et 1 retourne l'entrée 0.
Le shader reste appliqué pendant toute la durée du clip, même après la transition automatique.
Les masques/opacités sont déjà inclus dans la frame 0 ; ne pas les appliquer une seconde fois.
Les transitions restent à une passe ; les effets ordinaires acceptent deux entrées et
plusieurs passes, comme décrit dans [RELEASE_0.8.md](RELEASE_0.8.md).

Les transitions livrées sont décrites dans `shaders/transitions.json` ; les effets
historiques restent dans `shaders/effects.json`. Le code de transition est copié dans
le projet à l'application et ne change pas lors d'un rechargement des plugins.

## Plugins audio natifs et VST3

Le [SDK natif](../sdk/README.md) fournit `ShaderixFramePlugin.h`, ABI C versionnée 1,
et un exemple `native_gain.dll`. `ShaderixFramePluginWorker.exe` héberge chaque DLL
dans un processus séparé. `CyDAWPluginWorker.exe`, copié de CyDAW v2, héberge les VST3
avec IPC version 4 ; sa licence JUCE accompagne la distribution.
Les piles de clip, piste, bus et master proposent **Plugin natif/VST3**.
Les paramètres normalisés sont enregistrés et animables (`p0`, `p1`, etc.) :
64 dans le SDK natif v1, jusqu'à 512 pour VST3. Au-delà des 64 contrôles du bloc IPC,
les changements VST3 passent par les commandes individuelles du worker.
Le moteur compense les latences fixes déclarées, dans la limite cumulée de 10 secondes.
Un crash/timeout du plugin produit une erreur de traitement sans charger sa DLL dans
l'éditeur. La fenêtre VST3 s'ouvre depuis sa pile d'effets. L'état opaque base64
(4 Mio maximum), les noms de contrôles et les valeurs sont embarqués dans le projet.
Les presets `.sfvstpreset` restaurent l'état sans supprimer l'automation. Une capture
automatique avant sauvegarde/export/récupération synchronise les changements de GUI.
MIDI/instruments et sidechains restent à ajouter. Les chemins relatifs des plugins
sont résolus depuis le projet ; le binaire du plugin lui-même n'est pas embarqué.

## OpenFX

**+ Plugin OFX** dans les propriétés vidéo et les outils MCP `inspect_ofx` /
`add_ofx_effect` chargent un `.ofx` Windows x64 ou un bundle `Contents/Win64`.
Le worker `ShaderixFrameOfxWorker.exe` publie les suites CPU Property/ImageEffect/
Parameter/Memory/MultiThread/Message/Progress/TimeLine du SDK officiel ASWF.
Le temps passe en images OFX, les pixels en RGBA 32 bits float prémultipliés et
l'origine est adaptée à OFX. Les valeurs numériques deviennent les contrôles
`p0`, `p1`, etc., en unités du plugin et avec leurs limites réelles (pas normalisées).

Contextes Filter/General, Source/Output et profondeur float requis. Les entrées
supplémentaires obligatoires, effets GPU seuls, accès temporel, overlays, générateurs
et transitions ne sont pas pris en charge. Strings/custom restent à leur défaut.
Un chemin explicitement choisi et un identifiant OFX peuvent cibler un plugin d'un
module multiple. Les paramètres sont animables ; le worker est conservé pour les
images suivantes et un crash est remonté sans tuer l'éditeur. Le passage OFX utilise
une lecture CPU puis un upload GPU, avec une limite de transport de 512 Mio.

Le source `sdk/examples/OfxGain.cpp` et les en-têtes SDK officiels accompagnent le
paquet. Leur version et licence sont figées par `openfx.lock.json` et
`OPENFX-LICENSE.txt`. Voir [RELEASE_0.9.md](RELEASE_0.9.md) pour les limites validées.

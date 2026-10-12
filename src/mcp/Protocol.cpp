#include "mcp/Protocol.h"
#include "adapters/DspGraph.h"
#include "adapters/LayerGraph.h"
#include "core/AtomicFile.h"
#include "core/Motion.h"
#include "core/Version.h"
#include "render/Grading.h"
#include <fstream>
namespace sf {
static Json schema(std::initializer_list<std::pair<const char *, const char *>> properties,
                   std::initializer_list<const char *> required = {}) {
    Json s = Json::Object();
    s["type"] = "object";
    s["properties"] = Json::Object();
    s["required"] = Json::Array();
    for (auto &[n, t] : properties) {
        s["properties"][n] = Json::Object();
        s["properties"][n]["type"] = t;
        if (std::string(t) == "array" && std::string(n) == "clips") {
            s["properties"][n]["items"] = Json::Object();
            s["properties"][n]["items"]["type"] = "integer";
            s["properties"][n]["items"]["minimum"] = 1;
            s["properties"][n]["items"]["maximum"] = 2147483647;
            s["properties"][n]["maxItems"] = 4096;
        }
        if (std::string(n) == "points") {
            auto &points = s["properties"][n];
            points["minItems"] = 3;
            points["maxItems"] = 16;
            points["items"] = Json::Object();
            auto &point = points["items"];
            point["type"] = "array";
            point["minItems"] = 6;
            point["maxItems"] = 6;
            point["items"] = Json::Object();
            point["items"]["type"] = "number";
            point["items"]["minimum"] = -2;
            point["items"]["maximum"] = 2;
        }
    }
    for (auto n : required)
        s["required"].Push(n);
    return s;
}
Json Protocol::tools() const {
    Json list = Json::Array();
    auto add = [&](const char *n, const char *d, Json s) {
        Json t = Json::Object();
        t["name"] = n;
        t["description"] = d;
        t["inputSchema"] = s;
        list.Push(t);
    };
    add("set_motion_widget",
        "Configure la hiérarchie/ancrages d'un widget vidéo ; widget_x/y/width/height sont animables",
        schema({{"clip", "integer"}, {"layout", "object"}, {"enabled", "boolean"}}, {"clip"}));
    add("get_motion_widgets", "Hiérarchie motion et rectangles évalués au temps demandé",
        schema({{"time", "number"}}));
    add("make_clip_container", "Convertit un clip vidéo en composition imbriquée, sans perdre son contenu",
        schema({{"clip", "integer"}}, {"clip"}));
    add("get_clip_composition", "Timeline et graphe local du conteneur",
        schema({{"clip", "integer"}}, {"clip"}));
    add("set_clip_composition", "Remplace la composition locale d'un conteneur après validation",
        schema({{"clip", "integer"}, {"project", "object"}}, {"clip", "project"}));
    add("get_layer_graph", "Graphe de composition Shaderixel : timelines, fusions, effets et sorties",
        schema({}));
    add("set_layer_graph", "Remplace le graphe de composition ; validation Shaderixel, undo et rendu HDR",
        schema({{"graph", "object"}}, {"graph"}));
    add("set_layer_parameter", "Valeur de base d'un paramètre du nœud de composition",
        schema({{"node", "integer"}, {"key", "string"}, {"value", "number"}}, {"node", "key", "value"}));
    add("set_layer_keyframe", "Automation du graphe de mélange en temps de séquence",
        schema({{"node", "integer"},
                {"key", "string"},
                {"time", "number"},
                {"value", "number"},
                {"curve", "integer"},
                {"shape", "number"}},
               {"node", "key", "time", "value"}));
    add("status", "État de la séquence, transport et contenu du projet", schema({}));
    add("add_grade",
        "Ajoute une correction HDR au clip vidéo, ordre de pile et animation via "
        "set_effect/set_parameter/set_keyframe",
        schema({{"clip", "integer"}, {"name", "string"}}, {"clip"}));
    add("set_grade",
        "Règle les données de correction schema 1 (name, curves, maskPoints, lutInput/lutOutput) ; "
        "paramètres animables via set_parameter",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"settings", "object"}},
               {"clip", "effect", "settings"}));
    add("set_grade_curve",
        "Courbe master/r/g/b/hue_hue/hue_sat/hue_luma/luma_sat ; 2..32 points [x,y], extrémités 0 et 1",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"curve", "string"}, {"values", "array"}},
               {"clip", "effect", "curve", "values"}));
    add("import_grade_lut",
        "Importe et incorpore une LUT image 2D ou .cube 1D/3D, domaine et espaces explicites ; aucune "
        "référence "
        "externe",
        schema({{"clip", "integer"},
                {"effect", "integer"},
                {"path", "string"},
                {"layout", "string"},
                {"input", "string"},
                {"output", "string"}},
               {"clip", "effect", "path"}));
    add("neutral_grade",
        "Balance neutre depuis un échantillon RGB linéaire Rec.709 avant correction ; remplace les gains WB "
        "et leurs clés",
        schema(
            {{"clip", "integer"}, {"effect", "integer"}, {"r", "number"}, {"g", "number"}, {"b", "number"}},
            {"clip", "effect", "r", "g", "b"}));
    add("list_grade_presets", "Préréglages d'étalonnage natifs", schema({}));
    add("apply_grade_preset", "Remplace une correction par un préréglage natif",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"name", "string"}}, {"clip", "effect", "name"}));
    add("save_grade_preset", "Sauve une correction portable, LUT et animations comprises",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"path", "string"}}, {"clip", "effect", "path"}));
    add("load_grade_preset", "Remplace une correction depuis un fichier .sgrade validé",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"path", "string"}}, {"clip", "effect", "path"}));
    add("video_scopes",
        "Waveform 256x128, parade 3x128x128, vectorscope 256x256, histogramme 3x256 ; analyse 640x360 max",
        schema({{"time", "number"}, {"bins", "boolean"}}));
    add("set_color_management",
        "Composition RGBA16F, couleur de travail, affichage et séquences haute précision",
        schema({{"space", "string"},
                {"tone", "string"},
                {"exposure", "number"},
                {"backgroundAlpha", "number"},
                {"imageFormat", "string"}}));
    add("set_source_color", "Interprétation couleur et alpha d'un média vidéo",
        schema({{"clip", "integer"}, {"space", "string"}, {"premultiplied", "boolean"}}, {"clip"}));
    add("new_project", "Crée une séquence vide",
        schema({{"name", "string"},
                {"width", "integer"},
                {"height", "integer"},
                {"fps", "integer"},
                {"duration", "number"}}));
    add("open_project", "Ouvre un projet .sframe", schema({{"path", "string"}}, {"path"}));
    add("save_project", "Sauve un projet .sframe", schema({{"path", "string"}}));
    add("import_media", "Importe vidéo, image ou audio et crée les clips (son séparé)",
        schema({{"path", "string"}, {"start", "number"}}, {"path"}));
    add("add_track", "Ajoute une piste video ou audio",
        schema({{"kind", "string"}, {"name", "string"}}, {"kind"}));
    add("add_bus", "Ajoute un bus de mixage stéréo", schema({{"name", "string"}}));
    add("remove_bus", "Supprime un bus, reconnecte ses sources au master et retire ses envois",
        schema({{"bus", "integer"}}, {"bus"}));
    add("set_mix_channel", "Règle un canal track, bus ou master ; output 0 désigne le master",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"gain", "number"},
                {"pan", "number"},
                {"output", "integer"},
                {"muted", "boolean"},
                {"solo", "boolean"},
                {"locked", "boolean"},
                {"name", "string"}},
               {"scope"}));
    add("set_send", "Crée ou règle un envoi vers un bus, avant ou après fader",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"bus", "integer"},
                {"gain", "number"},
                {"pre_fader", "boolean"},
                {"enabled", "boolean"}},
               {"scope", "target", "bus"}));
    add("remove_send", "Retire un envoi vers un bus",
        schema({{"scope", "string"}, {"target", "integer"}, {"bus", "integer"}}, {"scope", "target", "bus"}));
    add("add_mix_effect", "Ajoute un DSP CyDAW 2 au canal track, bus ou master",
        schema({{"scope", "string"}, {"target", "integer"}, {"id", "string"}}, {"scope", "id"}));
    add("set_mix_effect", "Active ou déplace un DSP de canal",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"enabled", "boolean"},
                {"position", "integer"}},
               {"scope", "effect"}));
    add("remove_mix_effect", "Retire un DSP de canal et ses animations",
        schema({{"scope", "string"}, {"target", "integer"}, {"effect", "integer"}}, {"scope", "effect"}));
    add("set_mix_parameter", "Règle gain/pan de canal ou un paramètre DSP",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"value", "number"}},
               {"scope", "key", "value"}));
    add("set_mix_keyframe", "Anime un canal ou un DSP au temps de séquence",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"value", "number"},
                {"time", "number"},
                {"curve", "integer"},
                {"shape", "number"},
                {"previous_time", "number"}},
               {"scope", "key", "value", "time"}));
    add("remove_mix_keyframe", "Retire un point ; sans time, rétablit le réglage de base du canal",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"time", "number"}},
               {"scope", "key"}));
    add("get_audio_graph", "Lit le graphe stéréo de routage et DSP, validé par CyDAW 2", schema({}));
    add("set_dsp_graph",
        "Compile et embarque un graphe DSP stéréo v2 ; scope clip/track/bus/master, effect optionnel pour "
        "remplacer",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"graph", "object"}},
               {"scope", "graph"}));
    add("get_dsp_graph", "Lit un graphe DSP embarqué",
        schema({{"scope", "string"}, {"target", "integer"}, {"clip", "integer"}, {"effect", "integer"}},
               {"scope", "effect"}));
    add("add_external_audio",
        "Charge un effet natif SDK v1 ou VST3 dans un processus isolé ; scope clip/track/bus/master",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"kind", "string"},
                {"path", "string"}},
               {"scope", "kind", "path"}));
    add("show_plugin_editor", "Ouvre/masque la fenêtre du VST3 dans son worker isolé",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"visible", "boolean"}},
               {"scope", "effect"}));
    add("capture_plugin_state", "Embarque l'état complet du VST3",
        schema({{"scope", "string"}, {"target", "integer"}, {"clip", "integer"}, {"effect", "integer"}},
               {"scope", "effect"}));
    add("get_plugin_state", "Lit l'état VST3 et les noms des contrôles",
        schema({{"scope", "string"}, {"target", "integer"}, {"clip", "integer"}, {"effect", "integer"}},
               {"scope", "effect"}));
    add("set_plugin_state", "Restaure un état VST3 base64 de 4 Mio maximum",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"state", "string"}},
               {"scope", "effect", "state"}));
    add("save_plugin_preset", "Enregistre un preset complet VST3",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"path", "string"}},
               {"scope", "effect", "path"}));
    add("load_plugin_preset", "Charge un preset VST3, en conservant les courbes d'automation",
        schema({{"scope", "string"},
                {"target", "integer"},
                {"clip", "integer"},
                {"effect", "integer"},
                {"path", "string"}},
               {"scope", "effect", "path"}));
    add("add_generator", "Ajoute un générateur vidéo HLSL animé",
        schema({{"id", "string"}, {"start", "number"}, {"duration", "number"}}));
    add("add_title", "Ajoute un calque texte transparent avec effets et animations",
        schema({{"text", "string"}, {"start", "number"}, {"duration", "number"}}));
    add("set_title", "Règle le texte, la police, l'alignement (0 gauche, 1 centre, 2 droite) et le style",
        schema({{"clip", "integer"},
                {"text", "string"},
                {"font", "string"},
                {"alignment", "integer"},
                {"bold", "boolean"},
                {"italic", "boolean"}},
               {"clip"}));
    add("set_mask", "Règle la forme du masque (none, rectangle, ellipse) et son inversion",
        schema({{"clip", "integer"}, {"shape", "string"}, {"inverted", "boolean"}}, {"clip"}));
    add("set_bezier_mask",
        "Définit un masque fermé ; points [x,y,inX,inY,outX,outY], coordonnées centrées normalisées",
        schema({{"clip", "integer"}, {"points", "array"}}, {"clip", "points"}));
    add("start_mask_tracking",
        "Suit la translation du masque par corrélation en arrière-plan (1800 frames max)",
        schema({{"clip", "integer"},
                {"begin", "number"},
                {"end", "number"},
                {"radius", "integer"},
                {"effect", "integer"}},
               {"clip", "begin", "end"}));
    add("mask_tracking_status", "État du dernier suivi de masque", schema({}));
    add("cancel_mask_tracking", "Annule le suivi en cours", schema({}));
    add("apply_mask_tracking", "Applique les courbes mask_x/y si le clip n'a pas changé",
        schema({{"job", "integer"}}, {"job"}));
    add("set_transition",
        "Transition d'entrée entre le clip vidéo et les calques dessous ; reset_progress rétablit le temps "
        "automatique",
        schema({{"clip", "integer"},
                {"id", "string"},
                {"duration", "number"},
                {"enabled", "boolean"},
                {"reset_progress", "boolean"}},
               {"clip"}));
    add("remove_transition", "Retire la transition et ses paramètres/animations transition_",
        schema({{"clip", "integer"}}, {"clip"}));
    add("set_transition_shader",
        "Compile un shader de transition à deux entrées et l'enregistre dans le clip",
        schema({{"clip", "integer"}, {"code", "string"}}, {"clip", "code"}));
    add("set_clip", "Modifie les temps d'un clip",
        schema({{"clip", "integer"},
                {"start", "number"},
                {"duration", "number"},
                {"in", "number"},
                {"speed", "number"},
                {"fade_in", "number"},
                {"fade_out", "number"},
                {"name", "string"}},
               {"clip"}));
    add("ripple_trim",
        "Raccourcit/allonge de delta secondes et déplace les clips suivants ; left règle l'entrée source",
        schema({{"clip", "integer"}, {"delta", "number"}, {"left", "boolean"}}, {"clip", "delta"}));
    add("roll_trim", "Déplace la coupe entre deux clips adjacents sans changer la durée totale",
        schema({{"clip", "integer"}, {"next", "integer"}, {"delta", "number"}}, {"clip", "next", "delta"}));
    add("overlap_transition", "Crée le chevauchement et le fondu audio avec les poignées source disponibles",
        schema({{"clip", "integer"}, {"next", "integer"}, {"duration", "number"}},
               {"clip", "next", "duration"}));
    add("remove_range", "Découpe puis retire un intervalle sur toutes les pistes en refermant l'espace",
        schema({{"begin", "number"}, {"end", "number"}}, {"begin", "end"}));
    add("split_clip", "Coupe le clip au temps indiqué",
        schema({{"clip", "integer"}, {"time", "number"}}, {"clip", "time"}));
    add("delete_clip", "Supprime un clip et son groupe ; ripple ferme ses intervalles sur toutes les pistes",
        schema({{"clip", "integer"}, {"ripple", "boolean"}}, {"clip"}));
    add("select_clips",
        "Sélectionne les clips et leurs groupes dans l'interface ; liste vide pour tout désélectionner",
        schema({{"clips", "array"}}, {"clips"}));
    add("move_clips", "Déplace une sélection et ses animations de delta secondes, liaisons comprises",
        schema({{"clips", "array"}, {"delta", "number"}}, {"clips", "delta"}));
    add("delete_clips",
        "Supprime une sélection ; ripple ferme l'union de ses intervalles sur toutes les pistes, sans couper "
        "de clips non sélectionnés",
        schema({{"clips", "array"}, {"ripple", "boolean"}}, {"clips"}));
    add("duplicate_clips", "Duplique une sélection en bloc après sa fin, avec liaisons et animations",
        schema({{"clips", "array"}}, {"clips"}));
    add("split_clips", "Découpe les clips sélectionnés qui traversent le temps indiqué, liaisons comprises",
        schema({{"clips", "array"}, {"time", "number"}}, {"clips", "time"}));
    add("duplicate_clip", "Duplique le clip et son groupe lié, animations comprises",
        schema({{"clip", "integer"}}, {"clip"}));
    add("link_clips", "Lie deux clips ou fusionne leurs groupes pour les opérations de montage",
        schema({{"clip", "integer"}, {"other", "integer"}}, {"clip", "other"}));
    add("unlink_clip", "Délie tous les clips du groupe", schema({{"clip", "integer"}}, {"clip"}));
    add("set_parameter", "Règle un paramètre de clip (opacity/x/y/scale/rotation/gain/pan) ou d'effet",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"key", "string"}, {"value", "number"}},
               {"clip", "key", "value"}));
    add("set_keyframe", "Point d'animation, interpolation 0 linéaire, 1 douce, 2 palier, 3 courbe CyDAW",
        schema({{"clip", "integer"},
                {"effect", "integer"},
                {"key", "string"},
                {"value", "number"},
                {"time", "number"},
                {"curve", "integer"},
                {"shape", "number"}},
               {"clip", "key", "value", "time"}));
    add("add_effect", "Ajoute un effet HLSL de la bibliothèque",
        schema({{"clip", "integer"}, {"id", "string"}}, {"clip", "id"}));
    add("add_audio_effect", "Ajoute un effet DSP natif CyDAW 2",
        schema({{"clip", "integer"}, {"id", "string"}}, {"clip", "id"}));
    add("set_effect", "Active un effet et/ou le déplace dans la pile vidéo ou audio (index à partir de 0)",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"enabled", "boolean"}, {"position", "integer"}},
               {"clip", "effect"}));
    add("remove_effect", "Retire un effet vidéo ou audio avec ses animations",
        schema({{"clip", "integer"}, {"effect", "integer"}}, {"clip", "effect"}));
    add("list_effects", "Liste les effets et plugins disponibles", schema({}));
    add("inspect_ofx", "Décrit un OFX RGBA float filtre/general dans un processus isolé",
        schema({{"path", "string"}, {"identifier", "string"}}, {"path"}));
    add("add_ofx_effect", "Ajoute un OFX au clip vidéo, avec paramètres animables",
        schema({{"clip", "integer"}, {"path", "string"}, {"identifier", "string"}}, {"clip", "path"}));
    add("set_shader", "Compile et remplace un shader d'effet",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"code", "string"}}, {"clip", "effect", "code"}));
    add("get_graph", "Lit un graphe Shaderixel embarqué",
        schema({{"clip", "integer"}, {"effect", "integer"}}, {"clip", "effect"}));
    add("set_graph", "Compile un graphe Shaderixel (une entrée), objet JSON natif",
        schema({{"clip", "integer"}, {"effect", "integer"}, {"graph", "object"}},
               {"clip", "effect", "graph"}));
    add("transport", "Lecture/pause et recherche en secondes",
        schema({{"time", "number"}, {"playing", "boolean"}}));
    add("export_video", "Rend la séquence en MP4 H264/AAC", schema({{"path", "string"}}, {"path"}));
    add("export_audio", "Rend le mix en WAV stéréo 48 kHz", schema({{"path", "string"}}, {"path"}));
    add("start_export",
        "Lance un export en arrière-plan depuis un instantané ; kind video (MP4) ou audio (WAV)",
        schema({{"path", "string"}, {"kind", "string"}, {"preset", "string"}}, {"path", "kind"}));
    add("list_export_presets", "Presets de résolution et cadence pour la file de rendus", schema({}));
    add("prepare_proxy",
        "Prépare une copie H264 720p pour la prévisualisation ; l'export utilise la source originale",
        schema({{"clip", "integer"}, {"path", "string"}}, {"clip"}));
    add("set_preview_proxies", "Active/désactive les proxies prêts dans la prévisualisation",
        schema({{"enabled", "boolean"}}, {"enabled"}));
    add("export_status", "État d'un export, de la file et des huit derniers rendus terminés",
        schema({{"job", "integer"}}));
    add("cancel_export", "Demande l'annulation avant le remplacement final du fichier ; retourne accepted",
        schema({{"job", "integer"}}, {"job"}));
    add("render_frame", "Rend PNG8, PNG16 ou EXR half",
        schema({{"path", "string"}, {"time", "number"}, {"format", "string"}}, {"path"}));
    add("undo", "Annule la dernière modification", schema({}));
    add("redo", "Rétablit la modification", schema({}));
    return list;
}
Json Protocol::execute(const std::string &name, const Json &a) {
    if (!a.IsNull() && !a.IsObject())
        throw std::runtime_error("Les arguments doivent être un objet JSON");
    auto catalogue = tools();
    bool found = false;
    for (auto &tool : catalogue.Elements())
        if (tool["name"].S() == name) {
            found = true;
            auto &s = tool["inputSchema"];
            for (auto &n : s["required"].Elements())
                if (!a.Has(n.S()))
                    throw std::runtime_error("Argument requis: " + n.S());
            for (auto &[n, type] : s["properties"].Items())
                if (a.Has(n)) {
                    auto &v = a[n];
                    auto t = type["type"].S();
                    bool valid = t == "string"    ? v.GetType() == Json::Type::String
                                 : t == "boolean" ? v.GetType() == Json::Type::Bool
                                 : t == "object"  ? v.IsObject()
                                 : t == "array"
                                     ? v.IsArray()
                                     : v.GetType() == Json::Type::Number && std::isfinite(v.Num()) &&
                                           (t != "integer" || std::floor(v.Num()) == v.Num());
                    if (!valid)
                        throw std::runtime_error("Type invalide: " + n);
                    if (t == "integer" && (v.Num() < -1 || v.Num() > 2147483647))
                        throw std::runtime_error("Entier hors plage: " + n);
                    if (t == "array" && n == "clips") {
                        if (v.Size() > 4096)
                            throw std::runtime_error("4096 clips maximum par sélection");
                        for (auto &item : v.Elements())
                            if (item.GetType() != Json::Type::Number || !std::isfinite(item.Num()) ||
                                item.Num() < 1 || item.Num() > 2147483647 ||
                                std::floor(item.Num()) != item.Num())
                                throw std::runtime_error("Identifiant invalide dans la sélection");
                    }
                }
            break;
        }
    if (!found)
        throw std::runtime_error("Outil inconnu: " + name);
    auto &s = session;
    s.syncPluginEditors(name == "save_project" || name == "start_export" || name == "export_video" ||
                        name == "export_audio" || name == "open_project" || name == "new_project" ||
                        name == "get_plugin_state");
    auto &p = s.project;
    Project before = p;
    auto previousPath = s.projectPath;
    double previousTime = s.time;
    bool previousPlaying = s.playing, previousDirty = s.dirty;
    Id previousSelected = s.selected;
    auto previousSelection = s.selection;
    bool changed = false;
    auto clip = [&]() -> Clip & {
        auto *c = p.findClip(a["clip"].U());
        if (!c)
            throw std::runtime_error("Clip introuvable");
        auto *t = p.owner(c->id);
        if (t->locked && name != "get_graph")
            throw std::runtime_error("Piste verrouillée");
        return *c;
    };
    auto effect = [&]() -> Effect & {
        auto &c = clip();
        int i = a["effect"].I(-1);
        if (i < 0 || i >= int(c.effects.size()))
            throw std::runtime_error("Effet introuvable");
        return c.effects[i];
    };
    Track *mixTrack = nullptr;
    AudioBus *mixBus = nullptr;
    auto mixChannel = [&]() -> MixChannel & {
        auto scope = a["scope"].S();
        if (scope == "master") {
            if (a["target"].U() != 0)
                throw std::runtime_error("Le master utilise target 0");
            return p.masterMix;
        }
        bool locked = false;
        MixChannel *channel = nullptr;
        if (scope == "track") {
            mixTrack = p.findTrack(a["target"].U());
            if (!mixTrack || mixTrack->kind != TrackKind::Audio)
                throw std::runtime_error("Piste audio introuvable");
            locked = mixTrack->locked;
            channel = &mixTrack->mix;
        } else if (scope == "bus") {
            mixBus = p.findBus(a["target"].U());
            if (!mixBus)
                throw std::runtime_error("Bus audio introuvable");
            locked = mixBus->locked;
            channel = &mixBus->mix;
        } else
            throw std::runtime_error("scope attendu : track, bus ou master");
        if (locked && !(name == "set_mix_channel" && a.Has("locked") && a.Size() == 3))
            throw std::runtime_error("Canal de mixage verrouillé");
        return *channel;
    };
    Json result = Json::Object();
    result["ok"] = true;
    auto clipIds = [&]() {
        std::vector<Id> ids;
        for (auto &item : a["clips"].Elements())
            ids.push_back(item.U());
        return ids;
    };
    auto resultClips = [&](const std::vector<Id> &ids) {
        result["clips"] = Json::Array();
        for (auto id : ids)
            result["clips"].Push(id);
    };
    try {
        if (name == "export_video" || name == "export_audio")
            for (auto &job : s.exports.list())
                if (job.active())
                    throw std::runtime_error("Un export est déjà en cours ; attendre ou l'annuler");
        if (name == "status") {
            s.tidySelection();
            result["project"] = p.json();
            result["time"] = s.time;
            result["playing"] = s.playing;
            result["dirty"] = s.dirty;
            result["version"] = applicationVersion;
            result["exports"] = Json::Array();
            for (auto &job : s.exports.list())
                result["exports"].Push(job.json());
            result["selected"] = s.selected;
            result["selection"] = Json::Array();
            for (auto id : s.selection)
                result["selection"].Push(id);
            return result;
        } else if (name == "set_motion_widget") {
            if (!a["enabled"].B(true))
                clip().widget = Json();
            else {
                if (!a["layout"].IsObject())
                    throw std::runtime_error("Layout motion requis");
                clip().widget = a["layout"];
            }
            changed = true;
        } else if (name == "get_motion_widgets") {
            result["widgets"] = Json::Array();
            double time = a["time"].Num(s.time);
            if (!std::isfinite(time) || time < 0 || time > p.duration)
                throw std::runtime_error("Temps motion hors séquence");
            for (auto &t : p.tracks)
                for (auto &c : t.clips)
                    if (!c.widget.IsNull()) {
                        auto rect = widgetRect(p, c, time);
                        Json item = Json::Object();
                        item["clip"] = c.id;
                        item["layout"] = c.widget;
                        item["x"] = rect.x;
                        item["y"] = rect.y;
                        item["width"] = rect.width;
                        item["height"] = rect.height;
                        item["visible"] = rect.visible;
                        result["widgets"].Push(item);
                    }
            return result;
        } else if (name == "make_clip_container") {
            p.makeContainer(a["clip"].U());
            changed = true;
        } else if (name == "get_clip_composition") {
            result["project"] = clip().composition;
            return result;
        } else if (name == "set_clip_composition") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video || c.composition.IsNull())
                throw std::runtime_error("Conteneur vidéo requis");
            auto child = Project::fromJson(a["project"]);
            c.composition = child.json();
            changed = true;
        } else if (name == "get_layer_graph") {
            result["graph"] = p.layerGraph.IsNull() ? defaultLayerGraph(p) : p.layerGraph;
            result["explicit"] = !p.layerGraph.IsNull();
            return result;
        } else if (name == "set_layer_graph") {
            decodeLayerGraph(a["graph"], p);
            p.layerGraph = a["graph"];
            changed = true;
        } else if (name == "set_layer_parameter" || name == "set_layer_keyframe") {
            if (p.layerGraph.IsNull())
                p.layerGraph = defaultLayerGraph(p);
            if (name == "set_layer_keyframe") {
                if (a["time"].Num() < 0 || a["time"].Num() > p.duration || a["curve"].I() < 0 ||
                    a["curve"].I() > 3)
                    throw std::runtime_error("Clé de composition hors limites");
                setLayerGraphKey(p.layerGraph, a["node"].U(), a["key"].S(),
                                 {a["time"].Num(), a["value"].F(), Curve(a["curve"].I()), a["shape"].F()});
            } else {
                auto nodes = Json::Array();
                bool found = false;
                for (auto item : p.layerGraph["nodes"].Elements()) {
                    if (item["id"].U() == a["node"].U()) {
                        const auto &read = item;
                        auto param = read["params"].Has(a["key"].S())
                                         ? graphParameter(read["params"][a["key"].S()])
                                         : Parameter{};
                        param.value = a["value"].F();
                        item["params"][a["key"].S()] = parameterJson(param);
                        found = true;
                    }
                    nodes.Push(item);
                }
                if (!found)
                    throw std::runtime_error("Nœud introuvable");
                p.layerGraph["nodes"] = nodes;
            }
            decodeLayerGraph(p.layerGraph, p);
            changed = true;
        } else if (name == "add_grade") {
            if (p.owner(clip().id)->kind != TrackKind::Video || clip().effects.size() >= 64)
                throw std::runtime_error("Étalonnage : clip vidéo, 64 étapes maximum");
            clip().effects.push_back(makeGrade(a["name"].S("Correction")));
            result["effect"] = int(clip().effects.size() - 1);
            changed = true;
        } else if (name == "video_scopes") {
            double time = a["time"].Num(s.time);
            if (time < 0 || time > p.duration)
                throw std::runtime_error("Temps scopes hors séquence");
            Renderer analysis;
            analysis.initialize(s.renderer.gpu(), s.renderer.gpuContext());
            analysis.library = s.renderer.library;
            float scale = std::min(1.f, std::min(640.f / p.width, 360.f / p.height));
            analysis.render(p, s.media, time, std::max(1u, unsigned(p.width * scale)),
                            std::max(1u, unsigned(p.height * scale)));
            result["scopes"] =
                videoScopes(analysis.readbackFloat(), p.toneMap, p.exposure).json(a["bins"].B());
            return result;
        } else if (name == "list_grade_presets") {
            result["presets"] = Json::Array();
            for (auto title : {"Neutre", "Chaud", "Froid", "Noir et blanc", "Ombres cyan / hautes chaudes"})
                result["presets"].Push(title);
            return result;
        } else if (name == "set_grade" || name == "set_grade_curve" || name == "import_grade_lut" ||
                   name == "neutral_grade" || name == "apply_grade_preset" || name == "save_grade_preset" ||
                   name == "load_grade_preset") {
            auto &e = effect();
            if (e.id != "grading")
                throw std::runtime_error("Correction d'étalonnage requise");
            if (name == "set_grade")
                for (auto &[key, value] : a["settings"].Items()) {
                    if (key != "name" && key != "curves" && key != "maskPoints" && key != "lutInput" &&
                        key != "lutOutput" && key != "lut")
                        throw std::runtime_error("Donnée d'étalonnage inconnue : " + key);
                    e.grading[key] = value;
                }
            else if (name == "set_grade_curve")
                e.grading["curves"][a["curve"].S()] = a["values"];
            else if (name == "neutral_grade")
                neutralGrade(e, {a["r"].F(), a["g"].F(), a["b"].F()});
            else if (name == "apply_grade_preset")
                e = gradePreset(a["name"].S());
            else if (name == "import_grade_lut") {
                e.grading["lut"] = loadLut(sx::Utf8Path(a["path"].S()), a["layout"].S("auto"));
                if (a.Has("input"))
                    e.grading["lutInput"] = a["input"];
                if (a.Has("output"))
                    e.grading["lutOutput"] = a["output"];
            } else if (name == "save_grade_preset") {
                Json document = Json::Object();
                document["format"] = "shaderixframe-grade";
                document["schema"] = 1;
                document["grading"] = e.grading;
                document["enabled"] = e.enabled;
                document["params"] = Json::Object();
                for (auto &[key, parameter] : e.params) {
                    auto &j = document["params"][key];
                    j = Json::Object();
                    j["value"] = parameter.value;
                    j["keys"] = Json::Array();
                    for (auto &k : parameter.keys) {
                        Json item = Json::Array();
                        item.Push(k.time);
                        item.Push(k.value);
                        item.Push(int(k.curve));
                        item.Push(k.shape);
                        j["keys"].Push(item);
                    }
                }
                validateGrade(e);
                AtomicFile file(a["path"].S());
                std::ofstream out(file.path(), std::ios::binary);
                out << document.Dump();
                out.close();
                if (!out)
                    throw std::runtime_error("Écriture .sgrade impossible");
                file.commit();
                return result;
            } else if (name == "load_grade_preset") {
                auto path = sx::Utf8Path(a["path"].S());
                if (std::filesystem::file_size(path) > 48 * 1024 * 1024)
                    throw std::runtime_error("Préréglage trop volumineux");
                std::ifstream input(path, std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(input)), {});
                Json doc;
                if (!Json::Parse(text, doc) || doc["format"].S() != "shaderixframe-grade" ||
                    doc["schema"].I() != 1)
                    throw std::runtime_error("Fichier .sgrade invalide");
                auto replacement = makeGrade();
                replacement.grading = doc["grading"];
                replacement.enabled = doc["enabled"].B(true);
                for (auto &[key, item] : doc["params"].Items()) {
                    Parameter pp{item["value"].F()};
                    for (auto &k : item["keys"].Elements()) {
                        if (k.Size() != 4 || k.At(2).I() < 0 || k.At(2).I() > 3)
                            throw std::runtime_error("Clé .sgrade invalide");
                        pp.setKey({k.At(0).Num(), k.At(1).F(), Curve(k.At(2).I()), k.At(3).F()});
                    }
                    replacement.params[key] = pp;
                }
                validateGrade(replacement);
                e = std::move(replacement);
            }
            validateGrade(e);
            s.renderer.validateShader(gradeShader(e, p.workingSpace));
            changed = true;
        } else if (name == "set_color_management") {
            if (a.Has("space"))
                p.workingSpace = a["space"].S();
            if (a.Has("tone"))
                p.toneMap = a["tone"].S();
            if (a.Has("exposure"))
                p.exposure = a["exposure"].F();
            if (a.Has("backgroundAlpha"))
                p.backgroundAlpha = a["backgroundAlpha"].F();
            if (a.Has("imageFormat"))
                p.imageFormat = a["imageFormat"].S();
            changed = true;
        } else if (name == "set_source_color") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video)
                throw std::runtime_error("Clip vidéo requis");
            if (a.Has("space"))
                c.sourceColor = a["space"].S();
            if (a.Has("premultiplied"))
                c.sourcePremultiplied = a["premultiplied"].B();
            changed = true;
        } else if (name == "new_project") {
            Project created;
            created.name = a["name"].S("Sans titre");
            created.width = a["width"].I(1280);
            created.height = a["height"].I(720);
            created.fps = a["fps"].I(30);
            created.duration = a["duration"].Num(12);
            created.workingSpace = "linear709";
            created.validate();
            if (s.dirty)
                s.recovery.preserve(p, s.projectPath, s.time);
            p = std::move(created);
            s.projectPath.clear();
            s.time = 0;
            s.playing = false;
            s.selectClip(0);
            changed = true;
        } else if (name == "open_project") {
            auto opened = Project::load(a["path"].S());
            if (s.dirty)
                s.recovery.preserve(p, s.projectPath, s.time);
            p = std::move(opened);
            s.projectPath = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            s.time = 0;
            s.playing = false;
            s.selectClip(0);
            for (auto &track : p.tracks)
                if (!track.clips.empty()) {
                    s.selectClip(track.clips.front().id);
                    break;
                }
            changed = true;
        } else if (name == "save_project") {
            std::string path = a["path"].S(s.projectPath);
            if (path.empty())
                throw std::runtime_error("Chemin de projet requis");
            p.save(path);
            s.projectPath = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(path)).wstring());
            s.dirty = false;
            s.recovery.clear();
        } else if (name == "add_track") {
            auto kind = a["kind"].S();
            if (kind != "video" && kind != "audio")
                throw std::runtime_error("Type attendu: video ou audio");
            auto &t = p.addTrack(kind == "audio" ? TrackKind::Audio : TrackKind::Video);
            t.name = a["name"].S(t.name);
            if (t.kind == TrackKind::Video)
                appendLayerSurface(p, t.id, t.name);
            result["track"] = t.id;
            changed = true;
        } else if (name == "add_bus") {
            auto &bus = p.addBus();
            bus.name = a["name"].S(bus.name);
            result["bus"] = bus.id;
            changed = true;
        } else if (name == "remove_bus") {
            p.removeBus(a["bus"].U());
            changed = true;
        } else if (name == "set_mix_channel") {
            auto &channel = mixChannel();
            if (mixTrack || mixBus) {
                auto &gain = mixTrack ? mixTrack->gain : mixBus->gain;
                auto &pan = mixTrack ? mixTrack->pan : mixBus->pan;
                auto &muted = mixTrack ? mixTrack->muted : mixBus->muted;
                auto &locked = mixTrack ? mixTrack->locked : mixBus->locked;
                auto &label = mixTrack ? mixTrack->name : mixBus->name;
                if (a.Has("gain"))
                    gain = a["gain"].F();
                if (a.Has("pan"))
                    pan = a["pan"].F();
                if (a.Has("muted"))
                    muted = a["muted"].B();
                if (a.Has("locked"))
                    locked = a["locked"].B();
                if (a.Has("name"))
                    label = a["name"].S();
                if (a.Has("output"))
                    channel.output = a["output"].U();
                if (a.Has("solo")) {
                    if (!mixTrack)
                        throw std::runtime_error("Solo réservé aux pistes sources");
                    mixTrack->solo = a["solo"].B();
                }
            } else {
                for (auto key : {"output", "muted", "locked", "name", "solo"})
                    if (a.Has(key))
                        throw std::runtime_error("Réglage non disponible sur le master");
                if (a.Has("gain"))
                    p.master = a["gain"].F();
                if (a.Has("pan"))
                    channel.params["pan"].value = a["pan"].F();
            }
            changed = true;
        } else if (name == "set_send" || name == "remove_send") {
            auto &channel = mixChannel();
            auto destination = a["bus"].U();
            if (a["scope"].S() == "master" || !destination || !p.findBus(destination))
                throw std::runtime_error("Envoi réservé aux pistes/bus vers un bus existant");
            auto it = std::find_if(channel.sends.begin(), channel.sends.end(),
                                   [&](const AudioSend &send) { return send.bus == destination; });
            if (name == "remove_send") {
                if (it == channel.sends.end())
                    throw std::runtime_error("Envoi introuvable");
                channel.sends.erase(it);
                channel.params.erase("send." + std::to_string(destination));
            } else {
                if (it == channel.sends.end()) {
                    channel.sends.push_back({destination});
                    it = channel.sends.end() - 1;
                }
                if (a.Has("gain"))
                    it->gain = a["gain"].F();
                if (a.Has("pre_fader"))
                    it->preFader = a["pre_fader"].B();
                if (a.Has("enabled"))
                    it->enabled = a["enabled"].B();
            }
            changed = true;
        } else if (name == "set_dsp_graph" || name == "get_dsp_graph") {
            std::vector<Effect> *chain = nullptr;
            if (a["scope"].S() == "clip") {
                auto &selected = clip();
                if (p.owner(selected.id)->kind != TrackKind::Audio)
                    throw std::runtime_error("Clip audio requis");
                chain = &selected.effects;
            } else
                chain = &mixChannel().effects;
            int index = a["effect"].I(-1);
            if (index >= int(chain->size()) || (a.Has("effect") && index < 0))
                throw std::runtime_error("Graphe DSP introuvable");
            if (name == "get_dsp_graph") {
                if (index < 0 || (*chain)[index].id != "cydaw.graph")
                    throw std::runtime_error("Graphe DSP requis");
                result["graph"] = (*chain)[index].graph;
            } else {
                decodeDspGraph(a["graph"]);
                Effect item;
                item.id = "cydaw.graph";
                item.graph = a["graph"];
                if (index < 0) {
                    chain->push_back(item);
                    index = int(chain->size() - 1);
                } else {
                    if ((*chain)[index].id != "cydaw.graph")
                        throw std::runtime_error("Un DSP standard ne peut pas être remplacé par un graphe");
                    (*chain)[index] = item;
                }
                result["effect"] = index;
                changed = true;
            }
        } else if (name == "add_external_audio") {
            auto kind = a["kind"].S();
            auto path = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            IsolatedAudio plugin(kind, path);
            Effect item;
            item.id = kind;
            item.code = path;
            auto values = plugin.parameters();
            for (auto &[key, value] : values)
                if (item.params.size() < 512)
                    item.params[key] = {value};
            item.parameterNames = plugin.parameterNames();
            item.pluginState = plugin.state();
            std::vector<Effect> *chain = nullptr;
            if (a["scope"].S() == "clip") {
                auto &selected = clip();
                if (p.owner(selected.id)->kind != TrackKind::Audio)
                    throw std::runtime_error("Clip audio requis");
                chain = &selected.effects;
            } else
                chain = &mixChannel().effects;
            chain->push_back(std::move(item));
            result["effect"] = int(chain->size() - 1);
            changed = true;
        } else if (name == "show_plugin_editor") {
            s.pluginEditors.show(p, a, s.time, a["visible"].B(true));
            result["visible"] = a["visible"].B(true);
        } else if (name == "get_plugin_state") {
            const auto &item = pluginEffect(p, a, false);
            result["state"] = item.pluginState;
            result["parameters"] = Json::Object();
            for (auto &[key, value] : item.parameterNames)
                result["parameters"][key] = value;
        } else if (name == "capture_plugin_state" || name == "save_plugin_preset") {
            auto state = s.pluginEditors.snapshot(p, a, s.time);
            result["state"] = state;
            changed = p.json().Dump(0) != before.json().Dump(0);
            if (name == "save_plugin_preset") {
                Json preset = Json::Object();
                preset["format"] = "shaderixframe.vst3-preset";
                preset["schema"] = 1;
                preset["plugin"] = sx::Narrow(sx::Utf8Path(pluginEffect(p, a).code).filename().wstring());
                preset["state"] = state;
                AtomicFile output(a["path"].S());
                {
                    std::ofstream file(output.path(), std::ios::binary);
                    file << preset.Dump(2);
                    file.flush();
                    if (!file)
                        throw std::runtime_error("Preset illisible/non inscriptible");
                }
                output.commit();
            }
        } else if (name == "set_plugin_state" || name == "load_plugin_preset") {
            std::string state = a["state"].S();
            if (name == "load_plugin_preset") {
                Json preset;
                std::string text;
                if (!sx::ReadTextFile(a["path"].S(), text) || !Json::Parse(text, preset) ||
                    preset["format"].S() != "shaderixframe.vst3-preset" || preset["schema"].I() != 1 ||
                    preset["plugin"].S() !=
                        sx::Narrow(sx::Utf8Path(pluginEffect(p, a).code).filename().wstring()))
                    throw std::runtime_error("Preset incompatible avec ce VST3");
                state = preset["state"].S();
            }
            s.pluginEditors.restore(p, a, state, s.time);
            changed = true;
        } else if (name == "add_mix_effect" || name == "set_mix_effect" || name == "remove_mix_effect") {
            auto &chain = mixChannel().effects;
            if (name == "add_mix_effect") {
                chain.push_back(instantiateAudioEffect(a["id"].S()));
                result["effect"] = int(chain.size() - 1);
            } else {
                int index = a["effect"].I(-1);
                if (index < 0 || index >= int(chain.size()))
                    throw std::runtime_error("DSP introuvable");
                if (name == "remove_mix_effect")
                    chain.erase(chain.begin() + index);
                else {
                    int to = a["position"].I(index);
                    if (to < 0 || to >= int(chain.size()))
                        throw std::runtime_error("Position DSP invalide");
                    if (a.Has("enabled"))
                        chain[index].enabled = a["enabled"].B();
                    auto moved = std::move(chain[index]);
                    chain.erase(chain.begin() + index);
                    chain.insert(chain.begin() + to, std::move(moved));
                }
            }
            changed = true;
        } else if (name == "set_mix_parameter" || name == "set_mix_keyframe" ||
                   name == "remove_mix_keyframe") {
            auto &channel = mixChannel();
            auto *params = &channel.params;
            auto key = a["key"].S();
            float fallback = key == "gain" ? (mixTrack ? mixTrack->gain
                                              : mixBus ? mixBus->gain
                                                       : p.master)
                                           : (mixTrack ? mixTrack->pan
                                              : mixBus ? mixBus->pan
                                                       : 0);
            int index = a["effect"].I(-1);
            if (index >= 0) {
                if (index >= int(channel.effects.size()))
                    throw std::runtime_error("DSP introuvable");
                auto &effect = channel.effects[index];
                params = &effect.params;
                bool known = false;
                if (externalAudio(effect.id) && externalParameter(key) && effect.params.contains(key)) {
                    fallback = effect.params.at(key).value;
                    known = true;
                }
                for (auto &parameter : audioEffect(effect.id)->parameters)
                    if (key == parameter.key) {
                        fallback = parameter.defaultValue;
                        known = true;
                    }
                if (!known)
                    throw std::runtime_error("Paramètre DSP inconnu");
            } else if (key != "gain" && key != "pan") {
                bool found = false;
                for (auto &send : channel.sends)
                    if (key == "send." + std::to_string(send.bus)) {
                        fallback = send.gain;
                        found = true;
                        break;
                    }
                if (!found)
                    throw std::runtime_error("Paramètre de canal inconnu");
            }
            if (name == "remove_mix_keyframe") {
                auto it = params->find(key);
                if (it != params->end()) {
                    if (!a.Has("time"))
                        params->erase(it);
                    else
                        std::erase_if(it->second.keys, [&](const Keyframe &k) {
                            return std::abs(k.time - a["time"].Num()) < 1e-7;
                        });
                }
            } else {
                auto &parameter = params->try_emplace(key, Parameter{fallback}).first->second;
                if (name == "set_mix_parameter")
                    parameter.value = a["value"].F();
                else {
                    int curve = a["curve"].I();
                    if (a["time"].Num() < 0 || curve < 0 || curve > 3 || std::abs(a["shape"].F()) > 1)
                        throw std::runtime_error("Point de mixage invalide");
                    if (a.Has("previous_time"))
                        std::erase_if(parameter.keys, [&](const Keyframe &k) {
                            return std::abs(k.time - a["previous_time"].Num()) < 1e-7;
                        });
                    parameter.setKey({a["time"].Num(), a["value"].F(), Curve(curve), a["shape"].F()});
                }
            }
            changed = true;
        } else if (name == "ripple_trim") {
            p.rippleTrim(a["clip"].U(), a["delta"].Num(), a["left"].B());
            changed = true;
        } else if (name == "roll_trim") {
            p.rollTrim(a["clip"].U(), a["next"].U(), a["delta"].Num());
            changed = true;
        } else if (name == "overlap_transition") {
            p.overlapTransition(a["clip"].U(), a["next"].U(), a["duration"].Num());
            changed = true;
        } else if (name == "remove_range") {
            p.removeRange(a["begin"].Num(), a["end"].Num());
            changed = true;
        } else if (name == "get_audio_graph") {
            auto graph = audioGraph(p);
            auto &j = result["graph"];
            j = Json::Object();
            j["id"] = graph.reference.id;
            j["version"] = graph.reference.version.toString();
            j["schema"] = graph.schemaVersion;
            auto ports = [](const auto &source) {
                auto array = Json::Array();
                for (auto &port : source) {
                    auto item = Json::Object();
                    item["id"] = double(port.id);
                    item["key"] = port.key;
                    item["direction"] =
                        port.direction == cydaw::graph::PortDirection::input ? "input" : "output";
                    item["type"] = "audio";
                    item["layout"] = "stereo";
                    item["channels"] = 2;
                    item["multiple"] = port.acceptsMultipleConnections;
                    array.Push(item);
                }
                return array;
            };
            j["ports"] = ports(graph.ports);
            j["nodes"] = Json::Array();
            for (auto &node : graph.nodes) {
                Json item = Json::Object();
                item["id"] = double(node.id);
                item["name"] = node.displayName;
                item["processor"] = node.processorKey;
                item["ports"] = ports(node.ports);
                j["nodes"].Push(item);
            }
            j["connections"] = Json::Array();
            for (auto &wire : graph.connections) {
                Json item = Json::Object();
                item["source"] = double(wire.source.nodeId);
                item["sourcePort"] = double(wire.source.portId);
                item["destination"] = double(wire.destination.nodeId);
                item["destinationPort"] = double(wire.destination.portId);
                item["channels"] = 2;
                j["connections"].Push(item);
            }
        } else if (name == "add_generator") {
            auto &t = p.addTrack(TrackKind::Video);
            t.name = "Lumière HLSL";
            Clip c;
            c.id = p.allocate();
            c.generator = true;
            c.name = "Aurora · HLSL";
            if (a.Has("id")) {
                auto *d = s.renderer.library.find(a["id"].S());
                if (!d || !d->generator)
                    throw std::runtime_error("Générateur HLSL de catalogue requis");
                c.effects = {s.renderer.library.instantiate(d->id)};
                c.name = t.name = d->name;
            }
            c.start = a["start"].Num(s.time);
            c.duration = a["duration"].Num(6);
            t.clips.push_back(c);
            appendLayerSurface(p, t.id, t.name);
            std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            p.duration = std::max(p.duration, c.start + c.duration);
            s.selectClip(c.id);
            result["clip"] = c.id;
            changed = true;
        } else if (name == "add_title") {
            auto &track = p.addTrack(TrackKind::Video);
            track.name = "Titre";
            Clip c;
            c.id = p.allocate();
            c.title.enabled = true;
            c.title.text = a["text"].S("Votre titre");
            c.name = "Titre";
            c.start = a["start"].Num(s.time);
            c.duration = a["duration"].Num(5);
            c.params["font_size"] = {48};
            for (auto key : {"text_r", "text_g", "text_b", "text_a"})
                c.params[key] = {1};
            track.clips.push_back(c);
            appendLayerSurface(p, track.id, track.name);
            std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            p.duration = std::max(p.duration, c.start + c.duration);
            s.selectClip(c.id);
            result["clip"] = c.id;
            changed = true;
        } else if (name == "set_title") {
            auto &title = clip().title;
            if (!title.enabled)
                throw std::runtime_error("Clip titre requis");
            if (a.Has("text"))
                title.text = a["text"].S();
            if (a.Has("font"))
                title.font = a["font"].S();
            if (a.Has("alignment"))
                title.alignment = a["alignment"].I();
            if (a.Has("bold"))
                title.bold = a["bold"].B();
            if (a.Has("italic"))
                title.italic = a["italic"].B();
            changed = true;
        } else if (name == "set_transition" || name == "remove_transition" ||
                   name == "set_transition_shader") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video)
                throw std::runtime_error("Piste vidéo requise pour une transition");
            if (name == "remove_transition") {
                c.transition = {};
                std::erase_if(c.params,
                              [](const auto &entry) { return entry.first.starts_with("transition_"); });
            } else {
                if (name == "set_transition_shader" || a.Has("id")) {
                    std::string code, id;
                    if (name == "set_transition_shader") {
                        code = a["code"].S();
                        id = "transition.custom";
                    } else {
                        auto *definition = s.renderer.library.find(a["id"].S());
                        if (!definition || definition->kind != "transition")
                            throw std::runtime_error("Shader de transition introuvable");
                        code = definition->code;
                        id = definition->id;
                    }
                    validateTransitionParameters(code);
                    s.renderer.validateShader(code);
                    c.transition.id = id;
                    c.transition.code = code;
                    c.transition.duration = std::min(c.transition.duration, c.duration);
                    c.transition.enabled = true;
                    for (auto &parameter : effectParameters(code)) {
                        int count = parameter.type == "float2"                                ? 2
                                    : parameter.type == "float3"                              ? 3
                                    : parameter.type == "float4" || parameter.type == "color" ? 4
                                                                                              : 1;
                        for (int index = 0; index < count; ++index)
                            c.params.try_emplace(parameter.key +
                                                     (count > 1 ? "." + std::to_string(index) : ""),
                                                 Parameter{parameter.value[index]});
                    }
                }
                if (a.Has("duration"))
                    c.transition.duration = a["duration"].Num();
                if (a.Has("enabled")) {
                    c.transition.enabled = a["enabled"].B();
                    if (c.transition.enabled && !a.Has("duration"))
                        c.transition.duration = std::min(c.transition.duration, c.duration);
                }
                if (a["reset_progress"].B())
                    c.params.erase("transition_progress");
                if (c.transition.enabled) {
                    auto code = s.renderer.library.transitionCode(c.transition);
                    s.renderer.validateShader(code);
                    c.transition.code = std::move(code);
                }
            }
            changed = true;
        } else if (name == "start_mask_tracking") {
            result["job"] = s.tracking.start(p, a["clip"].U(), a["begin"].Num(), a["end"].Num(),
                                             a["radius"].I(24), a["effect"].I(-1));
        } else if (name == "mask_tracking_status") {
            result["tracking"] = s.tracking.status();
        } else if (name == "cancel_mask_tracking") {
            result["accepted"] = s.tracking.cancel();
        } else if (name == "apply_mask_tracking") {
            s.tracking.apply(p, a["job"].U());
            changed = true;
        } else if (name == "set_bezier_mask") {
            auto &selected = clip();
            if (p.owner(selected.id)->kind != TrackKind::Video)
                throw std::runtime_error("Clip vidéo requis");
            selected.mask.points.clear();
            for (auto &point : a["points"].Elements()) {
                if (!point.IsArray() || point.Size() != 6)
                    throw std::runtime_error("Point Bézier : six coordonnées requises");
                for (auto &value : point.Elements())
                    if (value.GetType() != Json::Type::Number)
                        throw std::runtime_error("Coordonnée Bézier numérique requise");
                selected.mask.points.push_back({point.At(0).F(), point.At(1).F(), point.At(2).F(),
                                                point.At(3).F(), point.At(4).F(), point.At(5).F()});
            }
            selected.mask.shape = MaskShape::Bezier;
            changed = true;
        } else if (name == "set_mask") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Video)
                throw std::runtime_error("Piste vidéo requise");
            if (a.Has("shape")) {
                auto shape = a["shape"].S();
                if (shape != "none" && shape != "rectangle" && shape != "ellipse")
                    throw std::runtime_error("Forme attendue : none, rectangle, ellipse");
                c.mask.shape = shape == "none"      ? MaskShape::None
                               : shape == "ellipse" ? MaskShape::Ellipse
                                                    : MaskShape::Rectangle;
            }
            if (a.Has("inverted"))
                c.mask.inverted = a["inverted"].B();
            changed = true;
        } else if (name == "import_media") {
            auto path = sx::Narrow(std::filesystem::absolute(sx::Utf8Path(a["path"].S())).wstring());
            auto &asset = s.media.get(path);
            double start = a["start"].Num(s.time);
            double duration = asset.image ? 6 : asset.duration;
            if (duration <= 0)
                throw std::runtime_error("Durée du média inconnue");
            result["clips"] = Json::Array();
            auto add = [&](TrackKind k) {
                auto &t = p.addTrack(k);
                Clip c;
                c.id = p.allocate();
                c.name = sx::Narrow(sx::Utf8Path(path).filename().wstring());
                c.source = path;
                c.start = start;
                c.duration = duration;
                t.name = c.name;
                t.clips.push_back(c);
                result["clips"].Push(c.id);
                if (k == TrackKind::Video)
                    std::rotate(p.tracks.begin(), p.tracks.end() - 1, p.tracks.end());
            };
            if (asset.video)
                add(TrackKind::Video);
            if (asset.hasAudio)
                add(TrackKind::Audio);
            if (result["clips"].Size())
                s.selectClip(result["clips"].At(0).U());
            if (result["clips"].Size() == 2)
                p.linkClips(result["clips"].At(0).U(), result["clips"].At(1).U());
            p.duration = std::max(p.duration, start + duration);
            changed = true;
        } else if (name == "set_clip") {
            auto &c = clip();
            p.setTiming(c.id, a["start"].Num(c.start), a["duration"].Num(c.duration), a["in"].Num(c.in),
                        a["speed"].Num(c.speed));
            if (a.Has("fade_in"))
                c.fadeIn = a["fade_in"].Num();
            if (a.Has("fade_out"))
                c.fadeOut = a["fade_out"].Num();
            if (a.Has("name"))
                c.name = a["name"].S();
            changed = true;
        } else if (name == "duplicate_clip") {
            auto id = p.duplicate(clip().id);
            result["clip"] = id;
            s.selectClip(id);
            changed = true;
        } else if (name == "select_clips") {
            s.setSelection(clipIds());
            resultClips(s.selection);
        } else if (name == "move_clips") {
            auto ids = clipIds();
            p.moveClips(ids, a["delta"].Num());
            s.setSelection(ids);
            resultClips(s.selection);
            changed = true;
        } else if (name == "duplicate_clips") {
            auto copies = p.duplicateClips(clipIds());
            s.setSelection(copies);
            resultClips(copies);
            changed = true;
        } else if (name == "split_clips") {
            auto ids = clipIds();
            p.splitClips(ids, a["time"].Num());
            s.setSelection(ids);
            changed = true;
        } else if (name == "delete_clips") {
            p.removeClips(clipIds(), a["ripple"].B());
            s.time = std::min(s.time, p.duration);
            changed = true;
        } else if (name == "link_clips") {
            p.linkClips(clip().id, a["other"].U());
            changed = true;
        } else if (name == "unlink_clip") {
            p.unlink(clip().id);
            changed = true;
        } else if (name == "split_clip") {
            p.split(clip().id, a["time"].Num());
            changed = true;
        } else if (name == "delete_clip") {
            p.removeClips({clip().id}, a["ripple"].B());
            s.time = std::min(s.time, p.duration);
            changed = true;
        } else if (name == "set_parameter" || name == "set_keyframe") {
            auto &ps = a["effect"].I(-1) >= 0 ? effect().params : clip().params;
            auto key = a["key"].S();
            if (key.empty())
                throw std::runtime_error("Clé de paramètre requise");
            if (name == "set_parameter")
                ps[key].value = a["value"].F();
            else {
                if (a["time"].Num() < 0)
                    throw std::runtime_error("Temps d'animation négatif");
                int curve = a["curve"].I();
                if (curve < 0 || curve > 3)
                    throw std::runtime_error("Interpolation invalide");
                ps[key].setKey({a["time"].Num(), a["value"].F(), Curve(curve), a["shape"].F()});
            }
            changed = true;
        } else if (name == "add_audio_effect") {
            auto &c = clip();
            if (p.owner(c.id)->kind != TrackKind::Audio)
                throw std::runtime_error("Piste audio requise");
            c.effects.push_back(instantiateAudioEffect(a["id"].S()));
            changed = true;
        } else if (name == "set_effect") {
            auto &e = effect();
            auto &effects = clip().effects;
            int from = a["effect"].I(), to = a["position"].I(from);
            if (to < 0 || to >= int(effects.size()))
                throw std::runtime_error("Position d'effet hors pile");
            if (a.Has("enabled"))
                e.enabled = a["enabled"].B();
            Effect moved = std::move(e);
            effects.erase(effects.begin() + from);
            effects.insert(effects.begin() + to, std::move(moved));
            changed = true;
        } else if (name == "remove_effect") {
            effect(); // Validate the index and lock before mutation.
            auto &effects = clip().effects;
            effects.erase(effects.begin() + a["effect"].I());
            changed = true;
        } else if (name == "add_effect") {
            if (p.owner(clip().id)->kind != TrackKind::Video)
                throw std::runtime_error("Piste vidéo requise");
            clip().effects.push_back(s.renderer.library.instantiate(a["id"].S()));
            changed = true;
        } else if (name == "list_effects") {
            result["effects"] = Json::Array();
            for (auto &d : s.renderer.library.definitions) {
                Json e = Json::Object();
                e["id"] = d.id;
                e["name"] = d.name;
                e["origin"] = d.origin;
                e["kind"] = d.kind;
                e["generator"] = d.generator;
                e["inputs"] = d.inputs;
                result["effects"].Push(e);
            }
            for (auto &d : cydaw::engine::effectCatalogue)
                if (!d.convertsChannels()) {
                    Json e = Json::Object();
                    e["id"] = "cydaw." + std::string(d.typeKey);
                    e["name"] = std::string(d.displayName);
                    e["origin"] = "CyDAW 2 DSP";
                    e["kind"] = "audio";
                    result["effects"].Push(e);
                }
            return result;
        } else if (name == "set_shader") {
            if (p.owner(clip().id)->kind != TrackKind::Video)
                throw std::runtime_error("Piste vidéo requise");
            s.renderer.validateShader(a["code"].S());
            auto &e = effect();
            if (e.id == "ofx" || e.id == "grading")
                throw std::runtime_error("OFX ne contient pas de shader HLSL éditable");
            e.code = a["code"].S();
            e.graph = {};
            changed = true;
        } else if (name == "get_graph") {
            result["graph"] = effect().graph;
            return result;
        } else if (name == "set_graph") {
            if (p.owner(clip().id)->kind != TrackKind::Video)
                throw std::runtime_error("Piste vidéo requise");
            Effect e = effect();
            if (e.id == "ofx" || e.id == "grading")
                throw std::runtime_error("OFX ne contient pas de graphe Shaderixel");
            e.graph = a["graph"];
            e.code = effectCode(e);
            s.renderer.validateShader(e.code);
            effect() = e;
            changed = true;
        } else if (name == "transport") {
            if (a.Has("time")) {
                double t = a["time"].Num();
                if (t < 0 || t > p.duration)
                    throw std::runtime_error("Temps hors séquence");
                s.time = t;
            }
            if (a.Has("playing"))
                s.playing = a["playing"].B();
        } else if (name == "export_video") {
            exportVideo(p, s.media, s.renderer, a["path"].S());
        } else if (name == "export_audio") {
            exportWav(p, s.media, a["path"].S());
        } else if (name == "set_preview_proxies") {
            s.media.proxiesEnabled = a["enabled"].B();
        } else if (name == "prepare_proxy") {
            auto &selected = clip();
            auto &asset = s.media.get(selected.source);
            if (asset.loading || !asset.video || asset.image || asset.duration <= 0)
                throw std::runtime_error("Une vidéo chargée est requise pour le proxy");
            auto path =
                a.Has("path") ? a["path"].S() : sx::Narrow(s.media.proxyPath(selected.source).wstring());
            Project proxy;
            proxy.name = "Proxy · " + selected.name;
            proxy.duration = asset.duration;
            float scale = std::min(1.f, 720.f / asset.frame.height);
            proxy.width = std::max(2, int(asset.frame.width * scale) / 2 * 2);
            proxy.height = std::max(2, int(asset.frame.height * scale) / 2 * 2);
            proxy.fps = p.fps;
            Clip source;
            source.id = proxy.allocate();
            source.source = selected.source;
            source.duration = asset.duration;
            proxy.addTrack(TrackKind::Video).clips.push_back(source);
            proxy.validate();
            auto job = s.exports.start(proxy, s.renderer.library, ExportKind::Video, path);
            s.proxyJobs.push_back({job, selected.source, path});
            result["job"] = job;
        } else if (name == "list_export_presets") {
            result["presets"] = Json::Array();
            for (auto &preset : exportPresets()) {
                Json item = Json::Object();
                item["id"] = preset.id;
                item["name"] = preset.name;
                item["width"] = preset.width;
                item["height"] = preset.height;
                item["fps"] = preset.fps;
                result["presets"].Push(item);
            }
        } else if (name == "start_export") {
            auto kind = a["kind"].S();
            if (kind != "video" && kind != "audio" && kind != "images" && kind != "analysis")
                throw std::runtime_error("Format d'export inconnu : video ou audio requis");
            auto id = s.exports.start(p, s.renderer.library,
                                      kind == "video"    ? ExportKind::Video
                                      : kind == "audio"  ? ExportKind::Audio
                                      : kind == "images" ? ExportKind::Images
                                                         : ExportKind::Analysis,
                                      a["path"].S(), a["preset"].S("project"));
            result["job"] = id;
        } else if (name == "export_status") {
            if (a.Has("job"))
                result["export"] = s.exports.status(a["job"].U()).json();
            else {
                result["exports"] = Json::Array();
                for (auto &job : s.exports.list())
                    result["exports"].Push(job.json());
            }
        } else if (name == "cancel_export") {
            result["accepted"] = s.exports.cancel(a["job"].U());
            result["export"] = s.exports.status(a["job"].U()).json();
        } else if (name == "inspect_ofx" || name == "add_ofx_effect") {
            auto path = ofxBinary(a["path"].S());
            IsolatedOfx worker(path, a["identifier"].S(), 64, 64);
            auto metadata = worker.metadata();
            result["plugin"] = metadata;
            if (name == "add_ofx_effect") {
                auto &c = clip();
                if (p.owner(c.id)->kind != TrackKind::Video)
                    throw std::runtime_error("Clip vidéo requis");
                Effect added;
                added.id = "ofx";
                added.code = path;
                added.graph = metadata;
                for (auto &parameter : metadata["parameters"].Elements()) {
                    added.params[parameter["key"].S()] = {parameter["default"].F()};
                    added.parameterNames[parameter["key"].S()] = parameter["name"].S();
                }
                validateOfx(added);
                c.effects.push_back(added);
                result["effect"] = int(c.effects.size() - 1);
                changed = true;
            }
        } else if (name == "render_frame") {
            double t = a["time"].Num(s.time);
            if (t < 0 || t > p.duration)
                throw std::runtime_error("Temps hors séquence");
            s.renderer.render(p, s.media, t, p.width, p.height);
            auto format =
                a["format"].S(sx::Utf8Path(a["path"].S()).extension() == L".exr" ? "exr16" : "png8");
            if (format == "exr16")
                writeExr(s.renderer.readbackFloat(), a["path"].S());
            else if (format == "png16")
                writePng16(s.renderer.readbackFloat(true), a["path"].S());
            else if (format == "png8")
                writePng(s.renderer.readback(), a["path"].S());
            else
                throw std::runtime_error("Format image inconnu");
        } else if (name == "undo") {
            if (s.history.undo(p)) {
                s.dirty = true;
                s.time = std::min(s.time, p.duration);
            }
        } else if (name == "redo") {
            if (s.history.redo(p)) {
                s.dirty = true;
                s.time = std::min(s.time, p.duration);
            }
        }
        if (changed) {
            p.validate();
            if (name == "new_project" || name == "open_project")
                s.history.clear();
            else
                s.history.checkpoint(before);
            s.dirty = name != "open_project";
        }
        if (changed || name == "undo" || name == "redo")
            s.media.resetAudio();
        if (changed || name == "undo" || name == "redo" || name == "open_project" || name == "new_project")
            s.renderer.clearHistory();
        if (name == "open_project" || name == "new_project")
            s.media.clear();
        if (name == "open_project" || name == "new_project" || name == "undo" || name == "redo" ||
            name == "set_effect" || name == "remove_effect" || name == "set_mix_effect" ||
            name == "remove_mix_effect")
            s.pluginEditors.clear();
        s.lastCommand = name;
        s.tidySelection();
    } catch (...) {
        p = before;
        s.projectPath = previousPath;
        s.time = previousTime;
        s.playing = previousPlaying;
        s.dirty = previousDirty;
        s.selected = previousSelected;
        s.selection = previousSelection;
        throw;
    }
    return result;
}
Json Protocol::respond(const Json &request) {
    Json response = Json::Object();
    response["jsonrpc"] = "2.0";
    response["id"] = request["id"];
    auto error = [&](int code, const std::string &message) {
        response["error"] = Json::Object();
        response["error"]["code"] = code;
        response["error"]["message"] = message;
        return response;
    };
    if (!request.IsObject() || request["jsonrpc"].S() != "2.0" ||
        request["method"].GetType() != Json::Type::String)
        return error(-32600, "Invalid Request");
    if (!request.Has("id"))
        return {};
    auto method = request["method"].S();
    Json result = Json::Object();
    if (method == "initialize") {
        auto v = request["params"]["protocolVersion"].S();
        result["protocolVersion"] =
            (v == "2025-06-18" || v == "2025-03-26" || v == "2024-11-05") ? v : "2025-06-18";
        result["serverInfo"] = Json::Object();
        result["serverInfo"]["name"] = "shaderixframe";
        result["serverInfo"]["version"] = applicationVersion;
        result["capabilities"] = Json::Object();
        result["capabilities"]["tools"] = Json::Object();
        result["instructions"] = "Le processus MCP possède une fenêtre ShaderixFrame : les outils et la "
                                 "fenêtre éditent la même séquence.";
    } else if (method == "ping") {
    } else if (method == "tools/list")
        result["tools"] = tools();
    else if (method == "tools/call") {
        Json content = Json::Object();
        content["type"] = "text";
        try {
            auto data = execute(request["params"]["name"].S(), request["params"]["arguments"]);
            content["text"] = data.Dump(0);
            result["isError"] = false;
            result["structuredContent"] = data;
        } catch (const std::exception &e) {
            content["text"] = e.what();
            result["isError"] = true;
        }
        result["content"] = Json::Array();
        result["content"].Push(content);
    } else
        return error(-32601, "Method not found");
    response["result"] = result;
    return response;
}
} // namespace sf

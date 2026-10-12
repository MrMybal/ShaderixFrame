#include "render/Grading.h"
#include <fstream>
#include <iomanip>
#include <sstream>
namespace sf {
const std::vector<GradeControl> &gradeControls() {
    static const std::vector<GradeControl> controls = {
        {"intensity", "Intensité", 1, 0, 1},
        {"temperature", "Température relative (K)", 0, -6000, 6000},
        {"tint", "Teinte vert / magenta", 0, -1, 1},
        {"wb_r", "Balance R", 1, .125f, 8},
        {"wb_g", "Balance V", 1, .125f, 8},
        {"wb_b", "Balance B", 1, .125f, 8},
        {"exposure", "Exposition (EV)", 0, -12, 12},
        {"contrast", "Contraste", 1, 0, 4},
        {"pivot", "Pivot linéaire", .18f, .001f, 1},
        {"blacks", "Noirs", 0, -1, 1},
        {"whites", "Blancs", 0, -1, 2},
        {"saturation", "Saturation", 1, 0, 3},
        {"vibrance", "Vibrance", 0, -1, 1},
        {"shadow_r", "Ombres R", 0, -1, 1},
        {"shadow_g", "Ombres V", 0, -1, 1},
        {"shadow_b", "Ombres B", 0, -1, 1},
        {"mid_r", "Moyens R", 0, -1, 1},
        {"mid_g", "Moyens V", 0, -1, 1},
        {"mid_b", "Moyens B", 0, -1, 1},
        {"high_r", "Hautes R", 0, -1, 1},
        {"high_g", "Hautes V", 0, -1, 1},
        {"high_b", "Hautes B", 0, -1, 1},
        {"offset_r", "Offset R", 0, -1, 1},
        {"offset_g", "Offset V", 0, -1, 1},
        {"offset_b", "Offset B", 0, -1, 1},
        {"qualifier", "Sélection HSL", 0, 0, 1},
        {"hue", "Teinte sélectionnée", .08f, 0, 1},
        {"hue_width", "Largeur teinte", .08f, 0, .5f},
        {"hue_soft", "Douceur teinte", .03f, .001f, .5f},
        {"sat_min", "Saturation min", 0, 0, 1},
        {"sat_max", "Saturation max", 1, 0, 1},
        {"luma_min", "Luminance min", 0, 0, 1},
        {"luma_max", "Luminance max", 1, 0, 1},
        {"qual_soft", "Douceur sélection", .05f, .001f, .5f},
        {"qual_invert", "Inverser sélection", 0, 0, 1},
        {"mask_mode", "Masque", 0, 0, 3},
        {"mask_x", "Centre X", 0, -.5f, .5f},
        {"mask_y", "Centre Y", 0, -.5f, .5f},
        {"mask_width", "Largeur", .5f, .001f, 2},
        {"mask_height", "Hauteur", .5f, .001f, 2},
        {"mask_feather", "Contour progressif", .05f, 0, .5f},
        {"mask_rotation", "Rotation", 0, -360, 360},
        {"mask_invert", "Inverser masque", 0, 0, 1},
        {"lut_intensity", "Intensité LUT", 1, 0, 1},
        {"matte", "Matte", 0, 0, 1}};
    return controls;
}
Effect makeGrade(const std::string &name) {
    Effect e;
    e.id = "grading";
    e.grading = Json::Object();
    e.grading["schema"] = 1;
    e.grading["name"] = name;
    e.grading["curves"] = Json::Object();
    e.grading["lutInput"] = "linear709";
    e.grading["lutOutput"] = "linear709";
    for (auto &p : gradeControls())
        e.params[p.key] = {p.value};
    return e;
}
static float param(const Effect &e, const char *key, double t) {
    auto it = e.params.find(key);
    if (it != e.params.end())
        return it->second.at(t);
    for (auto &p : gradeControls())
        if (std::string(p.key) == key)
            return p.value;
    return 0;
}
static bool space(const std::string &s) {
    return s == "linear709" || s == "linear2020" || s == "srgb" || s == "logc3-awg3-ei800";
}
void validateGrade(const Effect &e) {
    if (e.id != "grading" || e.grading["schema"].I() != 1 || !e.grading.IsObject() ||
        e.grading["name"].S().size() > 256 || !e.code.empty() || !e.graph.IsNull())
        throw std::runtime_error("Correction d'étalonnage invalide");
    for (auto &[key, p] : e.params) {
        auto it = std::find_if(gradeControls().begin(), gradeControls().end(),
                               [&](auto &c) { return key == c.key; });
        if (it == gradeControls().end())
            throw std::runtime_error("Paramètre d'étalonnage inconnu : " + key);
        auto check = [&](float x) {
            if (!std::isfinite(x) || x < it->minimum || x > it->maximum)
                throw std::runtime_error("Paramètre d'étalonnage hors limites : " + key);
        };
        check(p.value);
        if (p.keys.size() > 10000)
            throw std::runtime_error("Trop de clés d'étalonnage");
        double last = -1;
        for (auto &k : p.keys) {
            check(k.value);
            if (!std::isfinite(k.time) || k.time < 0 || k.time <= last || !std::isfinite(k.shape) ||
                k.shape < -1 || k.shape > 1 || int(k.curve) < 0 || int(k.curve) > 3)
                throw std::runtime_error("Clé d'étalonnage invalide");
            last = k.time;
        }
    }
    if (param(e, "sat_min", 0) > param(e, "sat_max", 0) || param(e, "luma_min", 0) > param(e, "luma_max", 0))
        throw std::runtime_error("Bornes de sélection inversées");
    for (auto &[name, curve] : e.grading["curves"].Items()) {
        bool hue = name == "hue_hue", scalar = name == "hue_sat" || name == "hue_luma" || name == "luma_sat";
        if (!hue && !scalar && name != "master" && name != "r" && name != "g" && name != "b")
            throw std::runtime_error("Courbe d'étalonnage inconnue");
        if (!curve.IsArray() || curve.Size() < 2 || curve.Size() > 32)
            throw std::runtime_error("Une courbe exige 2 à 32 points");
        float previous = -1;
        for (auto &pt : curve.Elements()) {
            if (!pt.IsArray() || pt.Size() != 2)
                throw std::runtime_error("Point de courbe invalide");
            if (pt.At(0).GetType() != Json::Type::Number || pt.At(1).GetType() != Json::Type::Number)
                throw std::runtime_error("Coordonnées de courbe numériques requises");
            float x = pt.At(0).F(), y = pt.At(1).F();
            if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || x <= previous ||
                y < (hue ? -.5f : 0) ||
                y > (hue      ? .5f
                     : scalar ? 3.f
                              : 1.f))
                throw std::runtime_error("Points de courbe non ordonnés ou hors limites");
            previous = x;
        }
        if (curve.At(0).At(0).F() != 0 || curve.At(curve.Size() - 1).At(0).F() != 1)
            throw std::runtime_error("Les extrémités de courbe doivent être 0 et 1");
        if (name.starts_with("hue_") && curve.At(0).At(1).F() != curve.At(curve.Size() - 1).At(1).F())
            throw std::runtime_error("Les courbes de teinte doivent boucler sans couture");
    }
    auto &points = e.grading["maskPoints"];
    if (!points.IsNull()) {
        if (!points.IsArray() || points.Size() < 3 || points.Size() > 16)
            throw std::runtime_error("Masque polygonal : 3 à 16 sommets");
        for (auto &p : points.Elements()) {
            if (!p.IsArray() || p.Size() != 2)
                throw std::runtime_error("Sommet de masque invalide");
            for (auto &v : p.Elements())
                if (v.GetType() != Json::Type::Number || !std::isfinite(v.F()) || std::abs(v.F()) > 2)
                    throw std::runtime_error("Sommet hors limites");
        }
    }
    if (param(e, "mask_mode", 0) >= 2.5f && points.IsNull())
        throw std::runtime_error("Le masque polygonal exige des sommets");
    if (!space(e.grading["lutInput"].S("linear709")) || !space(e.grading["lutOutput"].S("linear709")))
        throw std::runtime_error("Espace LUT inconnu");
    auto &lut = e.grading["lut"];
    if (!lut.IsNull()) {
        int n = lut["size"].I(), dim = lut["dimension"].I();
        if ((dim != 1 && dim != 3) || n < 2 || n > (dim == 1 ? 16384 : 65) ||
            lut["data"].Size() != size_t(dim == 1 ? n : n * n * n) * 3)
            throw std::runtime_error("Dimensions LUT invalides (3D max 65, 1D max 16384)");
        if (!lut["data"].IsArray() || lut["size"].Num() != n || lut["dimension"].Num() != dim)
            throw std::runtime_error("Entête LUT invalide");
        for (auto &x : lut["data"].Elements())
            if (x.GetType() != Json::Type::Number || !std::isfinite(x.F()) || std::abs(x.F()) > 65504)
                throw std::runtime_error("Valeur LUT invalide");
        if (lut["min"].Size() != 3 || lut["max"].Size() != 3)
            throw std::runtime_error("Domaine LUT invalide");
        for (int i = 0; i < 3; ++i)
            if (lut["min"].At(i).GetType() != Json::Type::Number ||
                lut["max"].At(i).GetType() != Json::Type::Number || !std::isfinite(lut["min"].At(i).F()) ||
                !std::isfinite(lut["max"].At(i).F()) || lut["max"].At(i).F() <= lut["min"].At(i).F())
                throw std::runtime_error("Domaine LUT vide");
    }
}
Json loadCube(const std::filesystem::path &path) {
    if (std::filesystem::file_size(path) > 32 * 1024 * 1024)
        throw std::runtime_error("LUT trop volumineuse");
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("Ouverture LUT impossible");
    Json lut = Json::Object();
    lut["dimension"] = 0;
    lut["size"] = 0;
    lut["data"] = Json::Array();
    lut["min"] = Json::Array();
    lut["max"] = Json::Array();
    for (int i = 0; i < 3; ++i) {
        lut["min"].Push(0);
        lut["max"].Push(1);
    }
    std::string line;
    bool data = false;
    while (std::getline(in, line)) {
        if (auto hash = line.find('#'); hash != std::string::npos)
            line.resize(hash);
        std::istringstream row(line);
        std::string token;
        if (!(row >> token))
            continue;
        if (token == "TITLE") {
            if (data)
                throw std::runtime_error("Entête LUT après données");
            std::string title;
            std::getline(row, title);
            lut["title"] = title;
            continue;
        }
        if (token == "LUT_1D_SIZE" || token == "LUT_3D_SIZE") {
            int n;
            std::string extra;
            if (data || lut["dimension"].I() || !(row >> n) || (row >> extra))
                throw std::runtime_error("Une LUT .cube doit avoir une seule taille 1D ou 3D");
            int dim = token == "LUT_1D_SIZE" ? 1 : 3;
            if (n < 2 || n > (dim == 1 ? 16384 : 65))
                throw std::runtime_error("Taille LUT non prise en charge");
            lut["dimension"] = dim;
            lut["size"] = n;
            continue;
        }
        if (token == "DOMAIN_MIN" || token == "DOMAIN_MAX") {
            float a, b, c;
            std::string extra;
            if (data || !(row >> a >> b >> c) || (row >> extra))
                throw std::runtime_error("Domaine .cube invalide");
            auto key = token == "DOMAIN_MIN" ? "min" : "max";
            lut[key] = Json::Array();
            for (float f : {a, b, c})
                lut[key].Push(f);
            continue;
        }
        if (!lut["dimension"].I())
            throw std::runtime_error("Entête ou directive .cube non prise en charge : " + token);
        std::istringstream values(line);
        float a, b, c;
        std::string extra;
        if (!(values >> a >> b >> c) || (values >> extra))
            throw std::runtime_error("Ligne LUT invalide");
        data = true;
        for (float f : {a, b, c})
            lut["data"].Push(f);
        if (lut["data"].Size() > 65u * 65 * 65 * 3)
            throw std::runtime_error("Trop de données LUT");
    }
    auto e = makeGrade();
    e.grading["lut"] = lut;
    validateGrade(e);
    return lut;
}
float gradeCurve(const Json &pts, float x, float fallback) {
    if (pts.IsNull() || pts.Size() < 2)
        return fallback;
    size_t i = 1;
    while (i + 1 < pts.Size() && x > pts.At(i).At(0).F())
        ++i;
    float a = pts.At(i - 1).At(0).F(), b = pts.At(i).At(0).F();
    return std::lerp(pts.At(i - 1).At(1).F(), pts.At(i).At(1).F(), (x - a) / (b - a));
}
static ColorRGB hsv(ColorRGB c) {
    float hi = std::max({c.x, c.y, c.z}), lo = std::min({c.x, c.y, c.z}), d = hi - lo, h = 0;
    if (d > 1e-7f)
        h = (hi == c.x ? (c.y - c.z) / d : hi == c.y ? 2 + (c.z - c.x) / d : 4 + (c.x - c.y) / d) / 6;
    h -= std::floor(h);
    return {h, hi > 1e-7f ? d / hi : 0, hi};
}
static ColorRGB rgb(ColorRGB h) {
    ColorRGB out;
    for (int i = 0; i < 3; ++i) {
        float x = h.x + (i == 0 ? 0.f : i == 1 ? 2.f / 3 : 1.f / 3);
        x -= std::floor(x);
        out[i] = h.z * std::lerp(1.f, std::clamp(std::abs(x * 6 - 3) - 1, 0.f, 1.f), h.y);
    }
    return out;
}
static float smooth(float a, float b, float x) {
    float t = std::clamp((x - a) / std::max(1e-6f, b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
float gradeSelection(const Effect &e, ColorRGB c, float u, float v, double time) {
    auto p = [&](const char *k) { return param(e, k, time); };
    float weight = 1;
    if (p("qualifier") > .5f) {
        c = convertColor(c, "linear709", "srgb");
        c = {std::max(0.f, c.x), std::max(0.f, c.y), std::max(0.f, c.z)};
        auto h = hsv(c);
        float d = std::abs(h.x - p("hue"));
        d = std::min(d, 1 - d);
        float l = (std::max({c.x, c.y, c.z}) + std::min({c.x, c.y, c.z})) / 2;
        float s = (l > 0 && l < 1) ? (std::max({c.x, c.y, c.z}) - std::min({c.x, c.y, c.z})) /
                                         std::max(1e-6f, 1 - std::abs(2 * l - 1))
                                   : 0;
        auto band = [&](float x, float lo, float hi) {
            if (lo > hi)
                return 0.f;
            return smooth(lo - p("qual_soft"), lo, x) * (1 - smooth(hi, hi + p("qual_soft"), x));
        };
        weight = (1 - smooth(p("hue_width"), p("hue_width") + p("hue_soft"), d)) *
                 band(s, p("sat_min"), p("sat_max")) * band(l, p("luma_min"), p("luma_max"));
        if (p("qual_invert") > .5f)
            weight = 1 - weight;
    }
    int mode = int(std::round(p("mask_mode")));
    if (mode) {
        float a = p("mask_rotation") * .01745329252f, x = u - .5f - p("mask_x"), y = v - .5f - p("mask_y");
        float tx = std::cos(a) * x + std::sin(a) * y, ty = -std::sin(a) * x + std::cos(a) * y;
        float wx = p("mask_width") * .5f, wy = p("mask_height") * .5f, distance;
        if (mode == 1)
            distance = std::max(std::abs(tx) - wx, std::abs(ty) - wy);
        else if (mode == 2)
            distance = (std::sqrt(tx * tx / (wx * wx) + ty * ty / (wy * wy)) - 1) * std::min(wx, wy);
        else {
            auto &pts = e.grading["maskPoints"];
            bool inside = false;
            distance = 10;
            for (size_t i = 0; i < pts.Size(); ++i) {
                auto &aa = pts.At(i), &bb = pts.At((i + 1) % pts.Size());
                float ax = aa.At(0).F() * wx * 2, ay = aa.At(1).F() * wy * 2, bx = bb.At(0).F() * wx * 2,
                      by = bb.At(1).F() * wy * 2;
                float dx = bx - ax, dy = by - ay,
                      t = std::clamp(((tx - ax) * dx + (ty - ay) * dy) / std::max(1e-9f, dx * dx + dy * dy),
                                     0.f, 1.f);
                distance = std::min(distance, std::hypot(tx - ax - t * dx, ty - ay - t * dy));
                if ((ay > ty) != (by > ty) && tx < (bx - ax) * (ty - ay) / (by - ay) + ax)
                    inside = !inside;
            }
            if (inside)
                distance = -distance;
        }
        float feather = p("mask_feather"), m = feather > 0
                                                   ? 1 - smooth(-feather * .5f, feather * .5f, distance)
                                               : distance <= 0 ? 1.f
                                                               : 0.f;
        if (p("mask_invert") > .5f)
            m = 1 - m;
        weight *= m;
    }
    return weight;
}
static ColorRGB sampleLut(const Json &lut, ColorRGB c) {
    int n = lut["size"].I(), dim = lut["dimension"].I();
    auto &data = lut["data"];
    for (int i = 0; i < 3; ++i)
        c[i] = std::clamp((c[i] - lut["min"].At(i).F()) / (lut["max"].At(i).F() - lut["min"].At(i).F()), 0.f,
                          1.f) *
               (n - 1);
    ColorRGB out{};
    if (dim == 1) {
        for (int i = 0; i < 3; ++i) {
            int a = int(c[i]), b = std::min(n - 1, a + 1);
            out[i] = std::lerp(data.At(a * 3 + i).F(), data.At(b * 3 + i).F(), c[i] - a);
        }
        return out;
    }
    int base[3] = {int(c.x), int(c.y), int(c.z)};
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x) {
                int idx = (std::min(n - 1, base[2] + z) * n * n + std::min(n - 1, base[1] + y) * n +
                           std::min(n - 1, base[0] + x)) *
                          3;
                float w = (x ? c.x - base[0] : 1 - c.x + base[0]) * (y ? c.y - base[1] : 1 - c.y + base[1]) *
                          (z ? c.z - base[2] : 1 - c.z + base[2]);
                for (int i = 0; i < 3; ++i)
                    out[i] += data.At(idx + i).F() * w;
            }
    return out;
}
ColorRGB gradeColor(const Effect &e, ColorRGB original, const std::string &space, double time, float u,
                    float v) {
    if (!e.enabled || param(e, "intensity", time) <= 0)
        return original;
    auto p = [&](const char *k) { return param(e, k, time); };
    auto c = convertColor(original, space, "linear709"), initial = c;
    c.x *= p("wb_r") * std::exp2(p("temperature") / 6000 + p("tint") * .5f);
    c.y *= p("wb_g") * std::exp2(-p("tint") * .5f);
    c.z *= p("wb_b") * std::exp2(-p("temperature") / 6000 + p("tint") * .5f);
    float gain = std::exp2(p("exposure"));
    for (int i = 0; i < 3; ++i) {
        c[i] *= gain;
        c[i] += (c[i] - p("pivot")) * (p("contrast") - 1);
    }
    float l = c.x * .2126f + c.y * .7152f + c.z * .0722f, sh = 1 - smooth(0, .5f, l), hi = smooth(.18f, 1, l),
          mid = std::max(0.f, 1 - sh - hi);
    const char *sk[] = {"shadow_r", "shadow_g", "shadow_b"}, *mk[] = {"mid_r", "mid_g", "mid_b"},
               *hk[] = {"high_r", "high_g", "high_b"}, *ok[] = {"offset_r", "offset_g", "offset_b"};
    for (int i = 0; i < 3; ++i)
        c[i] +=
            p("blacks") * sh + p("whites") * hi + p(sk[i]) * sh + p(mk[i]) * mid + p(hk[i]) * hi + p(ok[i]);
    l = c.x * .2126f + c.y * .7152f + c.z * .0722f;
    auto hc = hsv({std::max(c.x, 0.f), std::max(c.y, 0.f), std::max(c.z, 0.f)});
    float sat = p("saturation") * (1 + p("vibrance") * (1 - hc.y));
    for (int i = 0; i < 3; ++i)
        c[i] = std::lerp(l, c[i], sat);
    auto &curves = e.grading["curves"];
    const char *channels[] = {"r", "g", "b"};
    for (int i = 0; i < 3; ++i) {
        c[i] = gradeCurve(curves["master"], c[i], c[i]);
        c[i] = gradeCurve(curves[channels[i]], c[i], c[i]);
    }
    if (!curves["hue_hue"].IsNull() || !curves["hue_sat"].IsNull() || !curves["hue_luma"].IsNull()) {
        ColorRGB positive = {std::max(c.x, 0.f), std::max(c.y, 0.f), std::max(c.z, 0.f)};
        auto h = hsv(positive);
        float old = h.x;
        h.x += gradeCurve(curves["hue_hue"], old, 0);
        h.y *= gradeCurve(curves["hue_sat"], old, 1);
        h.z *= gradeCurve(curves["hue_luma"], old, 1);
        auto changed = rgb(h);
        for (int i = 0; i < 3; ++i)
            c[i] += changed[i] - positive[i];
    }
    l = c.x * .2126f + c.y * .7152f + c.z * .0722f;
    sat = gradeCurve(curves["luma_sat"], std::clamp(l, 0.f, 1.f), 1);
    for (int i = 0; i < 3; ++i)
        c[i] = std::lerp(l, c[i], sat);
    auto &lut = e.grading["lut"];
    if (!lut.IsNull()) {
        auto output =
            convertColor(sampleLut(lut, convertColor(c, "linear709", e.grading["lutInput"].S("linear709"))),
                         e.grading["lutOutput"].S("linear709"), "linear709");
        for (int i = 0; i < 3; ++i)
            c[i] = std::lerp(c[i], output[i], p("lut_intensity"));
    }
    float w = p("intensity") * gradeSelection(e, initial, u, v, time);
    for (int i = 0; i < 3; ++i)
        c[i] = std::lerp(initial[i], c[i], w);
    return convertColor(c, "linear709", space);
}
void neutralGrade(Effect &e, ColorRGB c) {
    if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) ||
        std::min({c.x, c.y, c.z}) <= 1e-5f)
        throw std::runtime_error("La pipette exige une zone neutre éclairée, sans valeur négative");
    float l = c.x * .2126f + c.y * .7152f + c.z * .0722f;
    e.params["temperature"] = {0};
    e.params["tint"] = {0};
    e.params["wb_r"] = {std::clamp(l / c.x, .125f, 8.f)};
    e.params["wb_g"] = {std::clamp(l / c.y, .125f, 8.f)};
    e.params["wb_b"] = {std::clamp(l / c.z, .125f, 8.f)};
}
Effect gradePreset(const std::string &name) {
    auto e = makeGrade(name);
    if (name == "Neutre")
        return e;
    if (name == "Chaud") {
        e.params["temperature"].value = 900;
        e.params["vibrance"].value = .15f;
    } else if (name == "Froid") {
        e.params["temperature"].value = -900;
        e.params["contrast"].value = 1.08f;
    } else if (name == "Noir et blanc")
        e.params["saturation"].value = 0;
    else if (name == "Ombres cyan / hautes chaudes") {
        e.params["shadow_b"].value = .03f;
        e.params["shadow_g"].value = .02f;
        e.params["high_r"].value = .05f;
    } else
        throw std::runtime_error("Préréglage inconnu");
    return e;
}
VideoScopes videoScopes(const FloatImage &im, const std::string &tone, float exposure) {
    if (im.rgba.size() != size_t(im.width) * im.height * 4 || !im.width || !im.height)
        throw std::runtime_error("Image scopes invalide");
    VideoScopes s;
    s.waveform.resize(256 * 128);
    s.parade.resize(3 * 128 * 128);
    s.vectorscope.resize(256 * 256);
    s.histogram.resize(3 * 256);
    s.minimum = std::numeric_limits<float>::infinity();
    s.maximum = -s.minimum;
    unsigned step = std::max(1u, unsigned(std::ceil(std::sqrt(double(im.width) * im.height / 262144))));
    for (unsigned y = 0; y < im.height; y += step)
        for (unsigned x = 0; x < im.width; x += step) {
            auto i = (size_t(y) * im.width + x) * 4;
            if (im.rgba[i + 3] <= 0)
                continue;
            auto c = convertColor({im.rgba[i], im.rgba[i + 1], im.rgba[i + 2]}, im.colorSpace, "linear709");
            if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z))
                continue;
            float lo = std::min({c.x, c.y, c.z}), hi = std::max({c.x, c.y, c.z});
            s.minimum = std::min(s.minimum, lo);
            s.maximum = std::max(s.maximum, hi);
            s.below += lo < 0;
            s.above += hi * std::exp2(exposure) > 1;
            ++s.samples;
            c = displayColor(c, "linear709", tone, exposure);
            float l = c.x * .2126f + c.y * .7152f + c.z * .0722f;
            auto bin = [](float v, int n) { return unsigned(std::clamp(v, 0.f, 1.f) * (n - 1)); };
            ++s.waveform[(127 - bin(l, 128)) * 256 + std::min(255u, x * 256 / im.width)];
            for (int k = 0; k < 3; ++k) {
                ++s.parade[k * 128 * 128 + (127 - bin(c[k], 128)) * 128 + std::min(127u, x * 128 / im.width)];
                ++s.histogram[k * 256 + bin(c[k], 256)];
            }
            float cb = (c.z - l) / 1.8556f, cr = (c.x - l) / 1.5748f;
            ++s.vectorscope[(255 - bin(cr + .5f, 256)) * 256 + bin(cb + .5f, 256)];
        }
    if (!s.samples)
        s.minimum = s.maximum = 0;
    return s;
}
Json VideoScopes::json(bool bins) const {
    Json j = Json::Object();
    j["samples"] = samples;
    j["belowZero"] = below;
    j["aboveOne"] = above;
    j["minimumLinear709"] = minimum;
    j["maximumLinear709"] = maximum;
    j["domain"] = "SDR display after project tone mapping; range statistics in scene linear709";
    if (bins) {
        const char *names[] = {"waveform", "parade", "vectorscope", "histogram"};
        const std::vector<unsigned> *arrays[] = {&waveform, &parade, &vectorscope, &histogram};
        for (int k = 0; k < 4; ++k) {
            j[names[k]] = Json::Array();
            for (auto v : *arrays[k])
                j[names[k]].Push(v);
        }
    }
    return j;
}
} // namespace sf

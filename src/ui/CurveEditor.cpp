// Animations panel: a curve editor in the spirit of CyDAW v2's curves::Editor. Several parameters of
// the selected clip are drawn together, each on its own value range; keys are picked, framed with a
// rubber band, dragged on video frames, bent with Alt and given CyDAW's interpolations.
#include "ui/Editor.h"
#include "ui/FrameStyle.h"
#include "ui/Theme.h"
#include <cmath>
#include <imgui_internal.h>
#include <set>

namespace sf {
using namespace style;
std::string timecode(double time, int fps);
std::string laneLabel(const std::string &key, const std::string &effect);
namespace {
const char *curveNames[] = {"Linear", "Smooth", "Step", "CyDAW"};
std::string paramId(int effect, const std::string &key) { return std::to_string(effect) + "/" + key; }
} // namespace

std::map<std::string, Parameter> *Editor::curveParams(Clip &clip, int effect) {
    if (effect < 0)
        return &clip.params;
    return effect < int(clip.effects.size()) ? &clip.effects[effect].params : nullptr;
}

std::pair<float, float> Editor::curveRange(const std::string &key, const Parameter &param) const {
    std::pair<float, float> range{0.f, 1.f};
    if (auto known = parameterRanges.find(key); known != parameterRanges.end())
        range = known->second;
    else if (key == "gain")
        range = {-60.f, 12.f};
    else if (key == "pan" || key == "x" || key == "y")
        range = {-1.f, 1.f};
    else if (key == "rotation")
        range = {-180.f, 180.f};
    else if (key == "scale" || key == "speed_ramp")
        range = {key == "scale" ? .05f : 0.f, 4.f};
    range.first = std::min(range.first, param.value);
    range.second = std::max(range.second, param.value);
    for (auto &k : param.keys) {
        range.first = std::min(range.first, k.value);
        range.second = std::max(range.second, k.value);
    }
    if (range.second - range.first < 1e-4f)
        range.second = range.first + 1;
    return range;
}

void Editor::animations() {
    if (!panels["Animation"] || !beginPanel("Animation"))
        return;
    auto *clip = s.project.findClip(s.selected);
    if (!clip) {
        ImGui::TextWrapped("Select a clip: its animatable parameters appear here. "
                           "K adds a keyframe at the playhead.");
        ImGui::End();
        return;
    }
    auto &io = ImGui::GetIO();
    auto &p = s.project;
    bool locked = p.owner(clip->id)->locked;
    double frame = 1. / p.fps;
    if (curveClip != clip->id) {
        curveClip = clip->id;
        curveSelection.clear();
        curveStart = clip->start - clip->duration * .03;
        curveEnd = clip->start + clip->duration * 1.03;
        curveVisible.clear();
    }
    if (animationEffect >= int(clip->effects.size()))
        animationEffect = -1;
    // Parameters: the clip's, then each effect's; animated ones and the active one are shown.
    struct Entry {
        int effect;
        std::string key, label;
        ImU32 colour;
    };
    std::vector<Entry> entries;
    for (auto &[key, param] : clip->params)
        entries.push_back({-1, key, laneLabel(key, {}), 0});
    for (int i = 0; i < int(clip->effects.size()); ++i)
        for (auto &[key, param] : clip->effects[i].params)
            entries.push_back({i, key, clip->effects[i].id + " / " + key, 0});
    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].colour = cyui::categorical(int(i % 8));
        auto *params = curveParams(*clip, entries[i].effect);
        auto &param = params->at(entries[i].key);
        if (curveVisible.empty() && !param.keys.empty())
            curveVisible.insert(paramId(entries[i].effect, entries[i].key));
    }
    curveVisible.insert(paramId(animationEffect, animationKey));
    auto find = [&](int effect, const std::string &key) -> Parameter * {
        auto *params = curveParams(*clip, effect);
        if (!params)
            return nullptr;
        auto it = params->find(key);
        return it == params->end() ? nullptr : &it->second;
    };
    auto *active = find(animationEffect, animationKey);

    // ---- Toolbar.
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(clip->name.c_str());
        ImGui::SameLine();
        ImGui::BeginDisabled(locked || !active);
        if (iconButton(icon::add, "Add Keyframe at Playhead [K]") && active) {
            s.history.checkpoint(p, "Animation curve");
            active->setKey({snapTime(s.time, p.fps, true), active->at(s.time), Curve::Smooth, 0});
            s.dirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 2);
        ImGui::BeginDisabled(locked || curveSelection.empty());
        if (iconButton(icon::trash, "Delete Selected Keys [Delete]"))
            curveDelete = true;
        ImGui::SameLine(0, 6);
        for (int c = 0; c < 4; ++c) {
            if (c)
                ImGui::SameLine(0, 2);
            if (ImGui::SmallButton(curveNames[c]))
                curveInterpolation = c;
            tooltip("Interpolation from Selected Keys to the Next Key");
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 6);
        if (iconButton(icon::fit, "Fit View to Clip Duration [F]")) {
            curveStart = clip->start - clip->duration * .03;
            curveEnd = clip->start + clip->duration * 1.03;
        }
        ImGui::SameLine(0, 2);
        if (iconButton(icon::grid, "Value Table", curveTable))
            curveTable = !curveTable;
        if (locked) {
            ImGui::SameLine();
            ImGui::TextDisabled(cyui::tr("Track is locked").c_str());
        }
    }

    // ---- Parameter list.
    float listWidth = std::min(230.f, ImGui::GetContentRegionAvail().x * .3f);
    ImGui::BeginChild("curveParams", {listWidth, -ImGui::GetFrameHeightWithSpacing() * (curveSelection.size() == 1 ? 1.2f : 0.f)},
                      true);
    for (auto &entry : entries) {
        auto id = paramId(entry.effect, entry.key);
        auto *param = find(entry.effect, entry.key);
        ImGui::PushID(id.c_str());
        bool shown = curveVisible.count(id) > 0;
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGui::ColorConvertU32ToFloat4(entry.colour));
        if (ImGui::Checkbox("##show", &shown)) {
            if (shown)
                curveVisible.insert(id);
            else
                curveVisible.erase(id);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        bool isActive = entry.effect == animationEffect && entry.key == animationKey;
        auto label = entry.label + (param && !param->keys.empty() ? "  ●" : "");
        if (ImGui::Selectable(label.c_str(), isActive)) {
            animationKey = entry.key;
            animationEffect = entry.effect;
            curveVisible.insert(id);
            automationReveal = true;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    if (curveTable) {
        ImGui::BeginChild("curveTable", {0, 0});
        if (!active) {
            ImGui::TextDisabled(cyui::tr("Choose a parameter.").c_str());
        } else if (ImGui::BeginTable("keys", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Time");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Interpolation");
            ImGui::TableSetupColumn("Curvature");
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();
            auto &parameter = *active;
            for (int i = 0; i < int(parameter.keys.size()); ++i) {
                ImGui::PushID(i);
                auto &k = parameter.keys[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto before = p;
                double lo = i ? parameter.keys[i - 1].time + 1e-5 : 0;
                double hi = i + 1 < int(parameter.keys.size()) ? parameter.keys[i + 1].time - 1e-5
                                                               : std::max(p.duration, k.time);
                ImGui::SetNextItemWidth(-1);
                ImGui::BeginDisabled(locked);
                ImGui::DragScalar("##time", ImGuiDataType_Double, &k.time, .01f, &lo, &hi, "%.3f s",
                                  ImGuiSliderFlags_AlwaysClamp);
                editCheckpoint(before);
                ImGui::TableNextColumn();
                before = p;
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##value", &k.value, .01f);
                editCheckpoint(before);
                ImGui::TableNextColumn();
                int curve = int(k.curve);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##curve", &curve, "Linear\0Smooth\0Step\0CyDAW\0")) {
                    s.history.checkpoint(p, "Animation curve");
                    k.curve = Curve(curve);
                    s.dirty = true;
                }
                ImGui::TableNextColumn();
                before = p;
                ImGui::BeginDisabled(k.curve != Curve::Shaped);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##shape", &k.shape, -1, 1, "%.2f");
                editCheckpoint(before);
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::TableNextColumn();
                if (ImGui::SmallButton(cyui::trLabel("Go").c_str()))
                    s.time = k.time;
                ImGui::SameLine();
                ImGui::BeginDisabled(locked);
                bool erase = ImGui::SmallButton(cyui::trLabel("X").c_str());
                ImGui::EndDisabled();
                ImGui::PopID();
                if (erase) {
                    s.history.checkpoint(p, "Animation curve");
                    parameter.keys.erase(parameter.keys.begin() + i);
                    s.dirty = true;
                    break;
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    // ---- Curve canvas.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg0);
    ImGui::BeginChild("curveCanvas", {0, -ImGui::GetFrameHeightWithSpacing() * (curveSelection.size() == 1 ? 1.2f : 0.f)},
                      false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 120.f);
    size.y = std::max(size.y, 80.f);
    const float ruler = 20, padding = 14;
    const float top = origin.y + ruler + padding, bottom = origin.y + size.y - padding;
    if (curveEnd - curveStart < 2 * frame)
        curveEnd = curveStart + 2 * frame;
    double span = curveEnd - curveStart;
    auto x = [&](double t) { return origin.x + float((t - curveStart) / span * size.x); };
    auto timeAt = [&](float px) { return curveStart + (px - origin.x) / size.x * span; };
    auto yOf = [&](float v, std::pair<float, float> r) {
        return bottom - (v - r.first) / (r.second - r.first) * (bottom - top);
    };
    ImGui::InvisibleButton("curveInput", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
    ImVec2 mouse = io.MousePos;

    // View: wheel zooms time around the pointer, Shift pans, middle button drags.
    if (hovered && io.MouseWheel != 0) {
        if (io.KeyShift)
            curveStart -= io.MouseWheel * span * .1, curveEnd -= io.MouseWheel * span * .1;
        else {
            double anchor = timeAt(mouse.x), factor = std::pow(.85, io.MouseWheel);
            curveStart = anchor - (anchor - curveStart) * factor;
            curveEnd = anchor + (curveEnd - anchor) * factor;
        }
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(2, 0)) {
        double shift = io.MouseDelta.x / size.x * span;
        curveStart -= shift;
        curveEnd -= shift;
    }

    // Background: clip span, grid and ruler.
    dl->AddRectFilled(origin, origin + size, bg0);
    dl->AddRectFilled({x(clip->start), origin.y + ruler}, {x(clip->start + clip->duration), origin.y + size.y},
                      bg1);
    double step = 3600;
    for (double candidate : {frame, 2 * frame, 5 * frame, 10 * frame, .5, 1., 2., 5., 10., 30., 60., 300.})
        if (candidate / span * size.x >= 90) {
            step = candidate;
            break;
        }
    dl->AddRectFilled(origin, {origin.x + size.x, origin.y + ruler}, bg1);
    for (double t = std::floor(curveStart / step) * step; t <= curveEnd; t += step) {
        dl->AddLine({x(t), origin.y + ruler}, {x(t), origin.y + size.y}, gridStrong);
        auto code = timecode(t, p.fps);
        smallText(dl, {x(t) + 3, origin.y + 4}, textDim, code.c_str(), .72f);
    }
    for (float fraction : {0.f, .25f, .5f, .75f, 1.f})
        dl->AddLine({origin.x, top + (bottom - top) * fraction}, {origin.x + size.x, top + (bottom - top) * fraction},
                    fraction == 0 || fraction == 1 ? gridStrong : grid);
    if (hovered && mouse.y < origin.y + ruler && ImGui::IsMouseDown(0))
        s.time = std::clamp(snapTime(timeAt(mouse.x), p.fps, true), 0., p.duration);

    // Points of every visible curve; the active one is drawn last, on top.
    struct Point {
        int effect;
        std::string key;
        int index;
        ImVec2 at;
    };
    std::vector<Point> points;
    curvePoints.clear();
    auto selected = [&](int effect, const std::string &key, int index) {
        for (auto &k : curveSelection)
            if (k.effect == effect && k.key == key && k.index == index)
                return true;
        return false;
    };
    std::vector<const Entry *> order;
    for (auto &entry : entries)
        if (curveVisible.count(paramId(entry.effect, entry.key)) &&
            !(entry.effect == animationEffect && entry.key == animationKey))
            order.push_back(&entry);
    for (auto &entry : entries)
        if (entry.effect == animationEffect && entry.key == animationKey)
            order.push_back(&entry);
    dl->PushClipRect({origin.x, origin.y + ruler}, origin + size, true);
    for (auto *entry : order) {
        auto *param = find(entry->effect, entry->key);
        if (!param)
            continue;
        bool isActive = entry->effect == animationEffect && entry->key == animationKey;
        auto range = curveRange(entry->key, *param);
        ImU32 colour = isActive ? entry->colour : alpha(entry->colour, .55f);
        ImVec2 previous{origin.x, yOf(param->at(timeAt(origin.x)), range)};
        for (float px = origin.x + 2; px <= origin.x + size.x; px += 2) {
            ImVec2 point{px, yOf(param->at(timeAt(px)), range)};
            dl->AddLine(previous, point, colour, isActive ? 2.f : 1.2f);
            previous = point;
        }
        if (isActive) {
            char bound[32];
            snprintf(bound, sizeof(bound), "%.3g", range.second);
            smallText(dl, {origin.x + 4, top - 12}, alpha(entry->colour, .8f), bound, .7f);
            snprintf(bound, sizeof(bound), "%.3g", range.first);
            smallText(dl, {origin.x + 4, bottom + 1}, alpha(entry->colour, .8f), bound, .7f);
        }
        for (int i = 0; i < int(param->keys.size()); ++i) {
            ImVec2 at{x(param->keys[i].time), yOf(param->keys[i].value, range)};
            points.push_back({entry->effect, entry->key, i, at});
            curvePoints.push_back({entry->effect, entry->key, i, at});
        }
    }
    // Hovered point (the active curve wins).
    int hot = -1;
    for (int i = 0; i < int(points.size()); ++i)
        if (hovered && std::abs(mouse.x - points[i].at.x) < 7 && std::abs(mouse.y - points[i].at.y) < 7)
            hot = i;
    for (int i = 0; i < int(points.size()); ++i) {
        auto &point = points[i];
        auto *param = find(point.effect, point.key);
        ImU32 colour = cyui::categorical(int(0));
        for (size_t e = 0; e < entries.size(); ++e)
            if (entries[e].effect == point.effect && entries[e].key == point.key)
                colour = entries[e].colour;
        bool chosen = selected(point.effect, point.key, point.index);
        float r = chosen ? 5.f : i == hot ? 4.5f : 3.5f;
        if (param->keys[point.index].curve == Curve::Hold)
            dl->AddRectFilled(point.at - ImVec2(r, r), point.at + ImVec2(r, r), chosen ? text : colour);
        else
            dl->AddCircleFilled(point.at, r, chosen ? text : colour, 14);
        if (chosen)
            dl->AddCircle(point.at, r + 1, colour, 14, 1.5f);
    }
    float play = x(s.time);
    dl->AddLine({play, origin.y}, {play, origin.y + size.y}, s.playing ? style::play : accent, 1.5f);
    dl->PopClipRect();
    if (points.empty() && active)
        smallText(dl, {origin.x + 12, top + 6}, textFaint, "Double-click: add a keyframe to the active parameter", .8f);

    // ---- Gestures.
    auto clampDelta = [&](double dt) {
        // One time offset for the whole selection, bounded by unselected neighbours.
        for (auto &k : curveSelection) {
            auto *param = find(k.effect, k.key);
            if (!param || k.index >= int(curveOriginal.size()))
                continue;
            auto &keys = param->keys;
            double original = curveOriginal[&k - curveSelection.data()].time;
            for (int n = k.index - 1; n >= 0; --n)
                if (!selected(k.effect, k.key, n)) {
                    dt = std::max(dt, keys[n].time + frame - original);
                    break;
                }
            for (int n = k.index + 1; n < int(keys.size()); ++n)
                if (!selected(k.effect, k.key, n)) {
                    dt = std::min(dt, keys[n].time - frame - original);
                    break;
                }
        }
        return dt;
    };
    if (!locked && hovered && ImGui::IsMouseClicked(0) && mouse.y >= origin.y + ruler) {
        bool additive = io.KeyCtrl || io.KeyShift;
        if (hot >= 0) {
            auto &point = points[hot];
            animationKey = point.key;
            animationEffect = point.effect;
            bool chosen = selected(point.effect, point.key, point.index);
            if (io.KeyCtrl && chosen)
                std::erase_if(curveSelection, [&](const CurveKey &k) {
                    return k.effect == point.effect && k.key == point.key && k.index == point.index;
                });
            else if (!chosen) {
                if (!additive)
                    curveSelection.clear();
                curveSelection.push_back({point.effect, point.key, point.index});
            }
            curveGesture = 1;
            curveAnchor = mouse;
            curveBefore = p;
            curveOriginal.clear();
            for (auto &k : curveSelection)
                if (auto *param = find(k.effect, k.key); param && k.index < int(param->keys.size()))
                    curveOriginal.push_back(param->keys[k.index]);
                else
                    curveOriginal.push_back({});
        } else if (io.KeyAlt && active) {
            // Alt + drag on a segment bends it with CyDAW's shaped curve.
            double at = timeAt(mouse.x);
            for (int i = 0; i + 1 < int(active->keys.size()); ++i)
                if (at > active->keys[i].time && at < active->keys[i + 1].time) {
                    curveGesture = 3;
                    curveBend = i;
                    curveAnchor = mouse;
                    curveBefore = p;
                    curveBendShape = active->keys[i].curve == Curve::Shaped ? active->keys[i].shape : 0;
                }
        } else {
            if (!additive)
                curveSelection.clear();
            curveGesture = 2;
            curveAnchor = mouse;
        }
    }
    if (curveGesture == 1 && ImGui::IsMouseDown(0)) {
        double dt = std::round((mouse.x - curveAnchor.x) / size.x * span / frame) * frame;
        if (io.KeyAlt)
            dt = (mouse.x - curveAnchor.x) / size.x * span;
        dt = clampDelta(dt);
        float dy = mouse.y - curveAnchor.y;
        bool moved = std::abs(mouse.x - curveAnchor.x) > 2 || std::abs(dy) > 2;
        for (size_t n = 0; moved && n < curveSelection.size(); ++n) {
            auto &k = curveSelection[n];
            auto *param = find(k.effect, k.key);
            if (!param || k.index >= int(param->keys.size()))
                continue;
            auto range = curveRange(k.key, *param);
            auto &key = param->keys[k.index];
            double time = std::max(0., curveOriginal[n].time + dt);
            float value = curveOriginal[n].value - dy / (bottom - top) * (range.second - range.first);
            if (auto known = parameterRanges.find(k.key); known != parameterRanges.end())
                value = std::clamp(value, known->second.first, known->second.second);
            if (key.time != time || key.value != value) {
                if (curveBefore) {
                    s.history.checkpoint(*curveBefore, "Animation curve");
                    curveBefore.reset();
                }
                key.time = time;
                key.value = value;
                s.dirty = true;
            }
        }
        if (moved && curveSelection.size() == 1)
            if (auto *param = find(curveSelection[0].effect, curveSelection[0].key);
                param && curveSelection[0].index < int(param->keys.size()))
                ImGui::SetTooltip(cyui::tr("%s  ·  %.3f").c_str(), timecode(param->keys[curveSelection[0].index].time, p.fps).c_str(),
                                  param->keys[curveSelection[0].index].value);
    }
    if (curveGesture == 3 && ImGui::IsMouseDown(0) && active && curveBend + 1 < int(active->keys.size())) {
        auto &key = active->keys[curveBend];
        float shape = std::clamp(curveBendShape - (mouse.y - curveAnchor.y) / 80.f, -1.f, 1.f);
        if (key.curve != Curve::Shaped || key.shape != shape) {
            if (curveBefore) {
                s.history.checkpoint(*curveBefore, "Animation curve");
                curveBefore.reset();
            }
            key.curve = Curve::Shaped;
            key.shape = shape;
            s.dirty = true;
        }
        ImGui::SetTooltip(cyui::tr("Curvature %.2f").c_str(), shape);
    }
    if (curveGesture == 2) {
        ImVec2 low{std::min(curveAnchor.x, mouse.x), std::min(curveAnchor.y, mouse.y)};
        ImVec2 high{std::max(curveAnchor.x, mouse.x), std::max(curveAnchor.y, mouse.y)};
        if (std::abs(mouse.x - curveAnchor.x) > 3 || std::abs(mouse.y - curveAnchor.y) > 3) {
            dl->AddRectFilled(low, high, alpha(accent, .08f));
            dl->AddRect(low, high, alpha(accent, .7f));
            if (ImGui::IsMouseReleased(0))
                for (auto &point : points)
                    if (point.at.x >= low.x && point.at.x <= high.x && point.at.y >= low.y && point.at.y <= high.y &&
                        !selected(point.effect, point.key, point.index))
                        curveSelection.push_back({point.effect, point.key, point.index});
        }
    }
    if (!ImGui::IsMouseDown(0)) {
        // Keys moved past each other keep the curve ordered.
        if (curveGesture == 1)
            for (auto &entry : entries)
                if (auto *param = find(entry.effect, entry.key))
                    std::stable_sort(param->keys.begin(), param->keys.end(),
                                     [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
        curveGesture = 0;
        curveBefore.reset();
    }
    // Double click: a key on the active parameter at the pointer.
    if (!locked && hovered && hot < 0 && ImGui::IsMouseDoubleClicked(0) && mouse.y >= origin.y + ruler) {
        if (!active) {
            auto *params = curveParams(*clip, animationEffect);
            if (params)
                active = &params->try_emplace(animationKey, Parameter{animationKey == "opacity" || animationKey == "scale"
                                                                          ? 1.f
                                                                          : 0.f})
                              .first->second;
        }
        if (active) {
            auto range = curveRange(animationKey, *active);
            float value = range.first + (bottom - mouse.y) / (bottom - top) * (range.second - range.first);
            s.history.checkpoint(p, "Animation curve");
            active->setKey({std::max(0., snapTime(timeAt(mouse.x), p.fps, true)), value, Curve::Smooth, 0});
            s.dirty = true;
            curveSelection.clear();
            curveGesture = 0;
        }
    }
    if (hovered && hot >= 0 && ImGui::IsMouseClicked(1)) {
        auto &point = points[hot];
        if (!selected(point.effect, point.key, point.index)) {
            curveSelection.clear();
            curveSelection.push_back({point.effect, point.key, point.index});
        }
        ImGui::OpenPopup("curveContext");
    }
    if (ImGui::BeginPopup("curveContext")) {
        for (int c = 0; c < 4; ++c)
            if (ImGui::MenuItem(curveNames[c]))
                curveInterpolation = c;
        ImGui::Separator();
        if (curveSelection.size() == 1)
            if (auto *param = find(curveSelection[0].effect, curveSelection[0].key);
                param && curveSelection[0].index < int(param->keys.size()) && ImGui::MenuItem(cyui::trLabel("Go to Keyframe").c_str()))
                s.time = param->keys[curveSelection[0].index].time;
        if (ImGui::MenuItem(cyui::trLabel("Copy").c_str(), "Ctrl+C"))
            curveCopy = true;
        if (ImGui::MenuItem(cyui::trLabel("Paste at Playhead").c_str(), "Ctrl+V", false, !locked && !curveClipboard.empty()))
            curvePaste = true;
        if (ImGui::MenuItem(cyui::trLabel("Delete").c_str(), "Delete", false, !locked))
            curveDelete = true;
        ImGui::EndPopup();
    }
    if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
        curveCopy = true;
    if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
        curvePaste = true;
    if (focused && !locked) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))
            curveDelete = !curveSelection.empty();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            curveSelection.clear();
            for (auto &point : points)
                curveSelection.push_back({point.effect, point.key, point.index});
        }
        if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K) && active) {
            s.history.checkpoint(p, "Animation curve");
            active->setKey({snapTime(s.time, p.fps, true), active->at(s.time), Curve::Smooth, 0});
            s.dirty = true;
        }
        if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
            curveStart = clip->start - clip->duration * .03;
            curveEnd = clip->start + clip->duration * 1.03;
        }
    }
    // Deferred edits on the selection: interpolation and deletion.
    if (curveInterpolation >= 0 && !locked && !curveSelection.empty()) {
        s.history.checkpoint(p, "Animation curve");
        for (auto &k : curveSelection)
            if (auto *param = find(k.effect, k.key); param && k.index < int(param->keys.size()))
                param->keys[k.index].curve = Curve(curveInterpolation);
        s.dirty = true;
    }
    curveInterpolation = -1;
    if (curveCopy && !curveSelection.empty()) {
        curveClipboard.clear();
        for (auto &k : curveSelection)
            if (auto *param = find(k.effect, k.key); param && k.index < int(param->keys.size()))
                curveClipboard.push_back({k.effect, k.key, param->keys[k.index]});
        double first = 86400;
        for (auto &copied : curveClipboard)
            first = std::min(first, copied.frame.time);
        for (auto &copied : curveClipboard)
            copied.frame.time -= first;
    }
    curveCopy = false;
    if (curvePaste && !locked && !curveClipboard.empty()) {
        // Parameters the clip lacks are created; effects it lacks are skipped. The pasted keys become the selection.
        s.history.checkpoint(p, "Animation curve");
        double at = snapTime(s.time, p.fps, true);
        curveSelection.clear();
        std::vector<std::pair<Parameter *, const CopiedKey *>> pasted;
        for (auto &copied : curveClipboard)
            if (auto *params = curveParams(*clip, copied.effect)) {
                auto *param = &params->try_emplace(copied.key, Parameter{copied.frame.value}).first->second;
                auto key = copied.frame;
                key.time = snapTime(at + key.time, p.fps, true);
                param->setKey(key);
                pasted.push_back({param, &copied});
                curveVisible.insert(paramId(copied.effect, copied.key));
            }
        for (auto &[param, copied] : pasted) {
            double time = snapTime(at + copied->frame.time, p.fps, true);
            for (int i = 0; i < int(param->keys.size()); ++i)
                if (std::abs(param->keys[i].time - time) < 1e-7)
                    curveSelection.push_back({copied->effect, copied->key, i});
        }
        s.dirty = true;
    }
    curvePaste = false;
    if (curveDelete && !locked && !curveSelection.empty()) {
        s.history.checkpoint(p, "Animation curve");
        // Remove from the end so indices stay valid; an emptied parameter keeps its value at the playhead.
        auto ordered = curveSelection;
        std::sort(ordered.begin(), ordered.end(), [](const CurveKey &a, const CurveKey &b) { return a.index > b.index; });
        for (auto &k : ordered)
            if (auto *param = find(k.effect, k.key); param && k.index < int(param->keys.size())) {
                if (param->keys.size() == 1)
                    param->value = param->keys[0].value;
                param->keys.erase(param->keys.begin() + k.index);
            }
        curveSelection.clear();
        s.dirty = true;
    }
    curveDelete = false;
    ImGui::EndChild();

    // One selected key: CyDAW's one-line inspector.
    if (curveSelection.size() == 1)
        if (auto *param = find(curveSelection[0].effect, curveSelection[0].key);
            param && curveSelection[0].index < int(param->keys.size())) {
            auto &k = param->keys[curveSelection[0].index];
            int index = curveSelection[0].index;
            ImGui::BeginDisabled(locked);
            double lo = index ? param->keys[index - 1].time + frame : 0;
            double hi = index + 1 < int(param->keys.size()) ? param->keys[index + 1].time - frame : 86400.;
            auto before = p;
            ImGui::SetNextItemWidth(110);
            ImGui::DragScalar(cyui::trLabel("Time").c_str(), ImGuiDataType_Double, &k.time, .01f, &lo, &hi, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
            editCheckpoint(before);
            ImGui::SameLine();
            before = p;
            ImGui::SetNextItemWidth(90);
            ImGui::DragFloat(cyui::trLabel("Value").c_str(), &k.value, .01f);
            editCheckpoint(before);
            ImGui::SameLine();
            int curve = int(k.curve);
            ImGui::SetNextItemWidth(100);
            if (ImGui::Combo("##interpolation", &curve, "Linear\0Smooth\0Step\0CyDAW\0")) {
                s.history.checkpoint(p, "Animation curve");
                k.curve = Curve(curve);
                s.dirty = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(k.curve != Curve::Shaped);
            before = p;
            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat(cyui::trLabel("Curvature").c_str(), &k.shape, -1, 1, "%.2f");
            editCheckpoint(before);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
        }
    ImGui::End();
}
} // namespace sf

#include "media/Tracking.h"
#include <mfapi.h>
namespace sf {
static std::string identity(const Project &project, Id id) {
    auto json = project.json();
    for (auto &track : json["tracks"].Elements())
        for (auto &clip : track["clips"].Elements())
            if (clip["id"].U() == id)
                return clip.Dump(0);
    throw std::runtime_error("Tracking clip not found");
}
static std::vector<float> gray(const Image &frame, unsigned &width, unsigned &height) {
    if (frame.pixels.empty())
        throw std::runtime_error("Empty tracking image");
    float scale = std::min(1.f, 320.f / std::max(frame.width, frame.height));
    width = std::max(1u, unsigned(frame.width * scale));
    height = std::max(1u, unsigned(frame.height * scale));
    std::vector<float> result(size_t(width) * height);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            auto pixel =
                frame.pixels.data() +
                (size_t(y) * frame.height / height * frame.width + size_t(x) * frame.width / width) * 4;
            result[y * width + x] = (pixel[0] * .0722f + pixel[1] * .7152f + pixel[2] * .2126f) / 255;
        }
    return result;
}
TrackingJobs::~TrackingJobs() {
    cancellation = true;
    if (worker.joinable())
        worker.join();
}
Id TrackingJobs::start(const Project &project, Id selected, double begin, double end, int radius,
                       int effect) {
    auto *clip = project.findClip(selected);
    Clip tracing;
    if (effect >= 0 && clip) {
        if (effect >= int(clip->effects.size()) || clip->effects[effect].id != "grading")
            throw std::runtime_error("Tracking correction not found");
        tracing = *clip;
        auto mask = clip->effects[effect].params.find("mask_mode");
        tracing.mask.shape = MaskShape(
            mask == clip->effects[effect].params.end() ? 0 : int(std::round(mask->second.at(begin))));
        tracing.params = clip->effects[effect].params;
        tracing.params["tracking_canvas_aspect"] = {float(project.width) / project.height};
        auto &asset = clip->source;
        tracing.source = asset;
    }
    if (!clip || project.owner(selected)->kind != TrackKind::Video || project.owner(selected)->locked ||
        clip->source.empty() || (effect < 0 ? clip->mask.shape : tracing.mask.shape) == MaskShape::None ||
        !std::isfinite(begin) || !std::isfinite(end) || begin < clip->start || end <= begin ||
        end > clip->start + clip->duration || (end - begin) * project.fps > 1800 || radius < 1 || radius > 64)
        throw std::runtime_error(
            "Tracking requires a masked video clip, an interior range (1800 frames maximum) and radius 1..64");
    {
        std::lock_guard lock(mutex);
        if (state == "running")
            throw std::runtime_error("Tracking is already running");
    }
    if (worker.joinable())
        worker.join();
    std::lock_guard lock(mutex);
    cancellation = false;
    state = "running";
    error.clear();
    progress = 0;
    x.clear();
    y.clear();
    clipId = selected;
    gradeIndex = effect;
    identity = sf::identity(project, selected);
    ++id;
    try {
        worker = std::thread([this, copy = effect < 0 ? *clip : tracing, begin, end, fps = project.fps,
                              radius] { run(copy, begin, end, fps, radius); });
    } catch (...) {
        state = "failed";
        throw;
    }
    return id;
}
void TrackingJobs::run(Clip clip, double begin, double end, int fps, int radius) {
    std::vector<Keyframe> xs, ys;
    std::string failure;
    bool cancelled = false;
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED),
            mf = SUCCEEDED(com) ? MFStartup(MF_VERSION) : com;
    {
        try {
            check(mf, "Tracking runtime");
            MediaAsset asset;
            asset.open(clip.source, {}, false);
            unsigned width = 0, height = 0;
            auto first = gray(asset.at(clip.sourceTime(begin)), width, height);
            float ratio =
                clip.param("tracking_canvas_aspect", begin, float(width) / height) / (float(width) / height);
            float fitX = std::max(1.f, ratio), fitY = std::max(1.f, 1 / ratio);
            int centerX = int((clip.param("mask_x", begin, 0) * fitX + .5f) * width),
                centerY = int((clip.param("mask_y", begin, 0) * fitY + .5f) * height);
            int half =
                std::max(3, std::min({20, int(clip.param("mask_width", begin, .1f) * fitX * width * .5f),
                                      int(clip.param("mask_height", begin, .1f) * fitY * height * .5f)}));
            if (centerX - half < 0 || centerY - half < 0 || centerX + half >= int(width) ||
                centerY + half >= int(height))
                throw std::runtime_error("Tracking region must be entirely inside the image");
            std::vector<float> pattern;
            double mean = 0;
            for (int dy = -half; dy <= half; dy += 2)
                for (int dx = -half; dx <= half; dx += 2) {
                    float value = first[(centerY + dy) * width + centerX + dx];
                    pattern.push_back(value);
                    mean += value;
                }
            mean /= pattern.size();
            double energy = 0;
            for (auto &value : pattern) {
                value -= float(mean);
                energy += value * value;
            }
            if (energy < .001)
                throw std::runtime_error("Tracking region lacks contrast");
            int total = int(std::ceil((end - begin) * fps));
            xs.push_back({begin, (float(centerX) / width - .5f) / fitX, Curve::Linear});
            ys.push_back({begin, (float(centerY) / height - .5f) / fitY, Curve::Linear});
            for (int frame = 1; frame < total; ++frame) {
                if (cancellation) {
                    cancelled = true;
                    break;
                }
                double time = begin + double(frame) / fps;
                unsigned w = 0, h = 0;
                auto image = gray(asset.at(clip.sourceTime(time)), w, h);
                if (w != width || h != height)
                    throw std::runtime_error("Dimensions changed during tracking");
                double best = 1e30;
                int bestX = centerX, bestY = centerY;
                for (int cy = std::max(half, centerY - radius);
                     cy <= std::min(int(height) - half - 1, centerY + radius); ++cy)
                    for (int cx = std::max(half, centerX - radius);
                         cx <= std::min(int(width) - half - 1, centerX + radius); ++cx) {
                        double average = 0;
                        for (int dy = -half; dy <= half; dy += 2)
                            for (int dx = -half; dx <= half; dx += 2)
                                average += image[(cy + dy) * width + cx + dx];
                        average /= pattern.size();
                        double score = 0;
                        size_t n = 0;
                        for (int dy = -half; dy <= half; dy += 2)
                            for (int dx = -half; dx <= half; dx += 2) {
                                double d = image[(cy + dy) * width + cx + dx] - average - pattern[n++];
                                score += d * d;
                            }
                        if (score < best) {
                            best = score;
                            bestX = cx;
                            bestY = cy;
                        }
                    }
                if (best / energy > .65)
                    throw std::runtime_error("Tracking lost: adjust the region or shorten the range");
                centerX = bestX;
                centerY = bestY;
                xs.push_back({time, (float(centerX) / width - .5f) / fitX, Curve::Linear});
                ys.push_back({time, (float(centerY) / height - .5f) / fitY, Curve::Linear});
                std::lock_guard lock(mutex);
                progress = float(frame + 1) / total;
            }
        } catch (const std::exception &issue) {
            failure = issue.what();
        }
    }
    if (SUCCEEDED(mf))
        MFShutdown();
    if (SUCCEEDED(com))
        CoUninitialize();
    std::lock_guard lock(mutex);
    state = cancelled || cancellation ? "cancelled" : failure.empty() ? "completed" : "failed";
    error = failure;
    if (state == "completed") {
        x = std::move(xs);
        y = std::move(ys);
        progress = 1;
    }
}
Json TrackingJobs::status() const {
    std::lock_guard lock(mutex);
    Json result = Json::Object();
    result["job"] = id;
    result["clip"] = clipId;
    result["effect"] = gradeIndex;
    result["state"] = state;
    result["progress"] = progress;
    result["error"] = error;
    result["keys"] = unsigned(x.size());
    return result;
}
bool TrackingJobs::cancel() {
    std::lock_guard lock(mutex);
    if (state != "running")
        return false;
    cancellation = true;
    return true;
}
void TrackingJobs::apply(Project &project, Id job) {
    std::lock_guard lock(mutex);
    if (job != id || state != "completed" || sf::identity(project, clipId) != identity)
        throw std::runtime_error("The clip has changed since tracking; run it again");
    auto *track = project.owner(clipId);
    if (track->locked)
        throw std::runtime_error("The tracking track is locked");
    auto candidate = project;
    auto &clip = *candidate.findClip(clipId);
    auto &params = gradeIndex < 0 ? clip.params : clip.effects.at(gradeIndex).params;
    for (auto key : x)
        params["mask_x"].setKey(key);
    for (auto key : y)
        params["mask_y"].setKey(key);
    candidate.validate();
    project = std::move(candidate);
    state = "applied";
}
} // namespace sf

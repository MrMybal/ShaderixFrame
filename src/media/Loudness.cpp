#include "media/Loudness.h"
namespace sf {
static double level(double energy) {
    return energy > 0 ? std::max(-120., -.691 + 10 * std::log10(energy)) : -120.;
}
void Loudness::interpolatePeak(double left, double right) {
    // ITU-R BS.1770 Annex 2, 48-tap / 4-phase interpolation at 48 kHz.
    static constexpr double coefficients[12][4] = {
        {.001708984375, -.0291748046875, -.0189208984375, -.00830078125},
        {.010986328125, .029296875, .0330810546875, .014892578125},
        {-.0196533203125, -.0517578125, -.0582275390625, -.026611328125},
        {.033203125, .089111328125, .1015625, .047607421875},
        {-.0594482421875, -.16650390625, -.2003173828125, -.102294921875},
        {.1373291015625, .465087890625, .77978515625, .97216796875},
        {.97216796875, .77978515625, .465087890625, .1373291015625},
        {-.102294921875, -.2003173828125, -.16650390625, -.0594482421875},
        {.047607421875, .1015625, .089111328125, .033203125},
        {-.026611328125, -.0582275390625, -.0517578125, -.0196533203125},
        {.014892578125, .0330810546875, .029296875, .010986328125},
        {-.00830078125, -.0189208984375, -.0291748046875, .001708984375}};
    peakHistory[0][peakCursor] = left;
    peakHistory[1][peakCursor] = right;
    for (unsigned channel = 0; channel < 2; ++channel)
        for (unsigned phase = 0; phase < 4; ++phase) {
            double value = 0;
            for (unsigned tap = 0; tap < 12; ++tap)
                value += peakHistory[channel][(peakCursor + 12 - tap) % 12] * coefficients[tap][phase];
            truePeakValue = std::max(truePeakValue, std::abs(value));
        }
    peakCursor = (peakCursor + 1) % 12;
}
void Loudness::push(const std::vector<float> &data) {
    for (size_t frame = 0; frame + 1 < data.size(); frame += 2) {
        double square = 0;
        interpolatePeak(std::isfinite(data[frame]) ? data[frame] : 0,
                        std::isfinite(data[frame + 1]) ? data[frame + 1] : 0);
        for (unsigned channel = 0; channel < 2; ++channel) {
            double x = std::isfinite(data[frame + channel]) ? data[frame + channel] : 0;
            peak = std::max(peak, std::abs(x));
            double y = highpass[channel].at(shelf[channel].at(x));
            square += y * y;
        }
        auto index = samples % ring.size();
        energy -= ring[index];
        ring[index] = square;
        energy += square;
        auto shortIndex = samples % shortRing.size();
        shortEnergy -= shortRing[shortIndex];
        shortRing[shortIndex] = square;
        shortEnergy += square;
        ++samples;
        if (samples >= ring.size() && (samples - ring.size()) % 4800 == 0)
            blocks.push_back(std::max(0., energy / ring.size()));
    }
}
double Loudness::momentary() const {
    return samples >= ring.size() ? level(std::max(0., energy / ring.size())) : -120.;
}
double Loudness::integrated() const {
    double sum = 0;
    size_t count = 0;
    for (double value : blocks)
        if (level(value) > -70) {
            sum += value;
            ++count;
        }
    if (!count)
        return -120.;
    double threshold = level(sum / count) - 10;
    sum = 0;
    count = 0;
    for (double value : blocks)
        if (level(value) > std::max(-70., threshold)) {
            sum += value;
            ++count;
        }
    return count ? level(sum / count) : -120.;
}
double Loudness::samplePeak() const {
    return peak > 0 ? 20 * std::log10(peak) : -120.;
}
double Loudness::truePeak(bool flushTail) const {
    double value = std::max(peak, truePeakValue);
    if (flushTail) {
        Loudness tail;
        memcpy(tail.peakHistory, peakHistory, sizeof(peakHistory));
        tail.peakCursor = peakCursor;
        tail.truePeakValue = value;
        for (int i = 0; i < 12; ++i)
            tail.interpolatePeak(0, 0);
        value = tail.truePeakValue;
    }
    return value > 0 ? std::max(-120., 20 * std::log10(value)) : -120.;
}
double Loudness::shortTerm() const {
    return samples >= shortRing.size() ? level(std::max(0., shortEnergy / shortRing.size())) : -120.;
}
Json Loudness::json() const {
    Json result = Json::Object();
    result["integrated_lufs"] = integrated();
    result["momentary_lufs"] = momentary();
    result["sample_peak_dbfs"] = samplePeak();
    result["true_peak_dbtp"] = truePeak(true);
    result["short_term_lufs"] = shortTerm();
    result["true_peak_oversampling"] = 4;
    result["seconds"] = samples / 48000.;
    result["gating_blocks"] = double(blocks.size());
    result["silence"] = integrated() <= -120;
    result["true_peak_measured"] = true;
    return result;
}
} // namespace sf

#pragma once
#include "core/Project.h"
#include <array>
namespace sf {
// 48 kHz stereo BS.1770 K-weighting, 400 ms windows at 75% overlap.
// 4x polyphase FIR true peak, independent of block boundaries; 3 s short-term LUFS.
class Loudness {
    struct Filter {
        double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
        double at(double x) {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    Filter shelf[2] = {
        {1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, .73248077421585},
        {1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, .73248077421585}};
    Filter highpass[2] = {{1, -2, 1, -1.99004745483398, .99007225036621},
                          {1, -2, 1, -1.99004745483398, .99007225036621}};
    std::vector<double> ring = std::vector<double>(19200, 0);
    std::vector<double> shortRing = std::vector<double>(144000, 0);
    double shortEnergy = 0, truePeakValue = 0;
    double peakHistory[2][12]{};
    unsigned peakCursor = 0;
    void interpolatePeak(double left, double right);
    uint64_t samples = 0;
    double energy = 0, peak = 0;
    std::vector<double> blocks;

  public:
    void push(const std::vector<float> &stereo);
    double momentary() const;
    double integrated() const;
    double samplePeak() const;
    double truePeak(bool flushTail = false) const;
    double shortTerm() const;
    Json json() const;
    void reset() { *this = Loudness{}; }
};
} // namespace sf

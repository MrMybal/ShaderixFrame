#include "ShaderixFramePlugin.h"
#include <array>
struct State {
    std::array<float, 64> delay{};
    unsigned cursor = 0;
};
static const char *SF_PLUGIN_CALL name(uint32_t) {
    return "";
}
static float SF_PLUGIN_CALL value(uint32_t) {
    return 0;
}
static void *SF_PLUGIN_CALL create(double, uint32_t) {
    return new State;
}
static void SF_PLUGIN_CALL destroy(void *state) {
    delete static_cast<State *>(state);
}
static void SF_PLUGIN_CALL parameter(void *, uint32_t, float) {}
static int SF_PLUGIN_CALL process(void *state, const float *left, const float *right, float *outLeft,
                                  float *outRight, uint32_t frames) {
    auto &s = *static_cast<State *>(state);
    for (uint32_t i = 0; i < frames; ++i) {
        outLeft[i] = s.delay[s.cursor * 2];
        outRight[i] = s.delay[s.cursor * 2 + 1];
        s.delay[s.cursor * 2] = left[i];
        s.delay[s.cursor * 2 + 1] = right[i];
        s.cursor = (s.cursor + 1) % 32;
    }
    return 1;
}
extern "C" SF_PLUGIN_EXPORT const SfPluginV1 *SF_PLUGIN_CALL shaderixframe_plugin_v1() {
    static const SfPluginV1 api = {
        sizeof(SfPluginV1), 1, "Latency test", 0, 32, name, value, create, destroy, parameter, process};
    return &api;
}

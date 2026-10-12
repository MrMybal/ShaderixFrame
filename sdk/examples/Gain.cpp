#include "ShaderixFramePlugin.h"
static const char *SF_PLUGIN_CALL name(uint32_t) {
    return "Gain";
}
static float SF_PLUGIN_CALL defaultValue(uint32_t) {
    return .5f;
}
static void *SF_PLUGIN_CALL create(double, uint32_t) {
    return new float(.5f);
}
static void SF_PLUGIN_CALL destroy(void *instance) {
    delete static_cast<float *>(instance);
}
static void SF_PLUGIN_CALL parameter(void *instance, uint32_t index, float value) {
    if (index == 0)
        *static_cast<float *>(instance) = value;
}
static int SF_PLUGIN_CALL process(void *instance, const float *left, const float *right, float *outLeft,
                                  float *outRight, uint32_t count) {
    float gain = *static_cast<float *>(instance) * 2;
    for (uint32_t i = 0; i < count; ++i) {
        outLeft[i] = left[i] * gain;
        outRight[i] = right[i] * gain;
    }
    return 1;
}
extern "C" SF_PLUGIN_EXPORT const SfPluginV1 *SF_PLUGIN_CALL shaderixframe_plugin_v1() {
    static const SfPluginV1 api = {
        sizeof(SfPluginV1), 1,      "ShaderixFrame Gain", 1, 0, name, defaultValue, create, destroy,
        parameter,          process};
    return &api;
}

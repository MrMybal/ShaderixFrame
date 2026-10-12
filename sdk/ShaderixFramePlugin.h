#pragma once
#include <stdint.h>
// C ABI v1. Stereo float at 48 kHz; frames <= 2048; normalized parameters 0..1.
// All calls occur in a dedicated helper process. No host/UI pointers cross this ABI.
#if defined(_WIN32)
#define SF_PLUGIN_EXPORT __declspec(dllexport)
#define SF_PLUGIN_CALL __cdecl
#else
#define SF_PLUGIN_EXPORT
#define SF_PLUGIN_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SfPluginV1 {
    uint32_t size, version;
    const char *name;
    uint32_t parameter_count, latency_samples;
    const char *(SF_PLUGIN_CALL *parameter_name)(uint32_t index);
    float(SF_PLUGIN_CALL *parameter_default)(uint32_t index);
    void *(SF_PLUGIN_CALL *create)(double sample_rate, uint32_t maximum_frames);
    void(SF_PLUGIN_CALL *destroy)(void *instance);
    void(SF_PLUGIN_CALL *set_parameter)(void *instance, uint32_t index, float normalized);
    int(SF_PLUGIN_CALL *process)(void *instance, const float *left, const float *right, float *out_left,
                                 float *out_right, uint32_t frames);
} SfPluginV1;
typedef const SfPluginV1 *(SF_PLUGIN_CALL *SfPluginEntry)(void);
SF_PLUGIN_EXPORT const SfPluginV1 *SF_PLUGIN_CALL shaderixframe_plugin_v1(void);
#ifdef __cplusplus
}
#endif

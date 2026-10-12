// Small real OpenFX ABI fixture, using the unmodified ASWF SDK headers.
#include <cstring>
#include <initializer_list>
#include <ofxImageEffect.h>
#include <ofxParam.h>
#include <ofxProperty.h>
namespace {
OfxHost *host = nullptr;
OfxPropertySuiteV1 *props = nullptr;
OfxImageEffectSuiteV1 *images = nullptr;
OfxParameterSuiteV1 *params = nullptr;
void setHost(OfxHost *h) {
    host = h;
}
OfxStatus mainEntry(const char *action, const void *handle, OfxPropertySetHandle in, OfxPropertySetHandle) {
    auto effect = reinterpret_cast<OfxImageEffectHandle>(const_cast<void *>(handle));
    if (!strcmp(action, kOfxActionLoad)) {
        props = (OfxPropertySuiteV1 *)host->fetchSuite(host->host, kOfxPropertySuite, 1);
        images = (OfxImageEffectSuiteV1 *)host->fetchSuite(host->host, kOfxImageEffectSuite, 1);
        params = (OfxParameterSuiteV1 *)host->fetchSuite(host->host, kOfxParameterSuite, 1);
        return props && images && params ? kOfxStatOK : kOfxStatErrMissingHostFeature;
    }
    if (!strcmp(action, kOfxActionDescribe)) {
        OfxPropertySetHandle p;
        images->getPropertySet(effect, &p);
        props->propSetString(p, kOfxPropLabel, 0, "OpenFX Gain / float");
        props->propSetString(p, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
        props->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
        props->propSetInt(p, kOfxImageEffectPropSupportsTiles, 0, 0);
        return kOfxStatOK;
    }
    if (!strcmp(action, kOfxImageEffectActionDescribeInContext)) {
        OfxPropertySetHandle p;
        for (auto name : {"Source", "Output"}) {
            images->clipDefine(effect, name, &p);
            props->propSetString(p, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
        }
        OfxParamSetHandle set;
        images->getParamSet(effect, &set);
        params->paramDefine(set, kOfxParamTypeDouble, "gain", &p);
        props->propSetString(p, kOfxPropLabel, 0, "Gain HDR");
        props->propSetDouble(p, kOfxParamPropDefault, 0, 1);
        props->propSetDouble(p, kOfxParamPropMin, 0, 0);
        props->propSetDouble(p, kOfxParamPropMax, 0, 8);
        return kOfxStatOK;
    }
    if (!strcmp(action, kOfxImageEffectActionRender)) {
        double time;
        props->propGetDouble(in, kOfxPropTime, 0, &time);
        OfxImageClipHandle src, dst;
        images->clipGetHandle(effect, "Source", &src, nullptr);
        images->clipGetHandle(effect, "Output", &dst, nullptr);
        OfxPropertySetHandle source, output;
        if (images->clipGetImage(src, time, nullptr, &source) != kOfxStatOK)
            return kOfxStatFailed;
        if (images->clipGetImage(dst, time, nullptr, &output) != kOfxStatOK) {
            images->clipReleaseImage(source);
            return kOfxStatFailed;
        }
        OfxParamSetHandle set;
        OfxParamHandle gain;
        images->getParamSet(effect, &set);
        params->paramGetHandle(set, "gain", &gain, nullptr);
        double value;
        params->paramGetValueAtTime(gain, time, &value);
        void *a, *b;
        int sa, sb, bounds[4];
        props->propGetPointer(source, kOfxImagePropData, 0, &a);
        props->propGetPointer(output, kOfxImagePropData, 0, &b);
        props->propGetInt(source, kOfxImagePropRowBytes, 0, &sa);
        props->propGetInt(output, kOfxImagePropRowBytes, 0, &sb);
        props->propGetIntN(output, kOfxImagePropBounds, 4, bounds);
        for (int y = 0; y < bounds[3] - bounds[1]; ++y) {
            auto *input = (float *)((char *)a + y * sa), *out = (float *)((char *)b + y * sb);
            for (int x = 0; x < bounds[2] - bounds[0]; ++x)
                for (int k = 0; k < 4; ++k)
                    out[x * 4 + k] = input[x * 4 + k] * float(k < 3 ? value : 1);
        }
        images->clipReleaseImage(source);
        images->clipReleaseImage(output);
        return kOfxStatOK;
    }
    return kOfxStatReplyDefault;
}
OfxPlugin plugin{kOfxImageEffectPluginApi, 1, "org.shaderixframe.test.gain", 1, 0, setHost, mainEntry};
} // namespace
extern "C" __declspec(dllexport) int OfxGetNumberOfPlugins() {
    return 1;
}
extern "C" __declspec(dllexport) OfxPlugin *OfxGetPlugin(int index) {
    return index == 0 ? &plugin : nullptr;
}

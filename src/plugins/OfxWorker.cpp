#include "core/Json.h"
#include "plugins/OfxTransport.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <map>
#include <memory>
#include <mutex>
#include <ofxCore.h>
#include <ofxImageEffect.h>
#include <ofxMemory.h>
#include <ofxMessage.h>
#include <ofxMultiThread.h>
#include <ofxParam.h>
#include <ofxProgress.h>
#include <ofxProperty.h>
#include <ofxTimeLine.h>
#include <set>
#include <stdexcept>
#include <variant>
#include <vector>
#include <windows.h>
namespace {
using Json = sx::Json;
using Value = std::variant<void *, std::string, double, int>;
struct Props {
    std::map<std::string, std::vector<Value>> data;
};
Props &props(OfxPropertySetHandle h) {
    return *reinterpret_cast<Props *>(h);
}
OfxPropertySetHandle handle(Props &p) {
    return reinterpret_cast<OfxPropertySetHandle>(&p);
}
template <class T> OfxStatus set(OfxPropertySetHandle h, const char *name, int index, T value) {
    if (!h || !name)
        return kOfxStatErrBadHandle;
    if (index < 0 || index >= 512)
        return kOfxStatErrBadIndex;
    try {
        auto &v = props(h).data[name];
        if (v.size() <= unsigned(index))
            v.resize(index + 1);
        v[index] = value;
        return kOfxStatOK;
    } catch (...) {
        return kOfxStatErrMemory;
    }
}
template <class T> OfxStatus get(OfxPropertySetHandle h, const char *name, int index, T *out) {
    if (!h || !name || !out)
        return kOfxStatErrBadHandle;
    auto it = props(h).data.find(name);
    if (it == props(h).data.end())
        return kOfxStatErrUnknown;
    if (index < 0 || index >= int(it->second.size()))
        return kOfxStatErrBadIndex;
    auto *v = std::get_if<T>(&it->second[index]);
    if (!v)
        return kOfxStatErrValue;
    *out = *v;
    return kOfxStatOK;
}
void string(Props &p, const char *name, const std::string &value, int index = 0) {
    set(handle(p), name, index, value);
}
void number(Props &p, const char *name, double value, int index = 0) {
    set(handle(p), name, index, value);
}
void integer(Props &p, const char *name, int value, int index = 0) {
    set(handle(p), name, index, value);
}
// Separate getter names avoid overload ambiguity between setting and reading defaults.
std::string text(Props &p, const char *name, std::string fallback = "", int index = 0) {
    std::string v;
    return get(handle(p), name, index, &v) == kOfxStatOK ? v : fallback;
}
double real(Props &p, const char *name, double fallback = 0, int index = 0) {
    double v;
    int i;
    if (get(handle(p), name, index, &v) == kOfxStatOK)
        return v;
    if (get(handle(p), name, index, &i) == kOfxStatOK)
        return i;
    return fallback;
}
struct Param {
    Props props;
    std::string type, name, text;
    std::vector<double> values;
};
struct ParamSet {
    Props props;
    std::map<std::string, Param> params;
};
struct Clip {
    Props props, image;
    std::vector<float> pixels;
};
struct Effect {
    Props props;
    ParamSet params;
    std::map<std::string, Clip> clips;
};
sf::OfxShared *shared = nullptr;
OfxPropertySuiteV1 properties{};
OfxImageEffectSuiteV1 images{};
OfxParameterSuiteV1 parameters{};
OfxMemorySuiteV1 memory{};
OfxMultiThreadSuiteV1 threads{};
OfxMessageSuiteV2 messages{};
OfxProgressSuiteV1 progress{};
OfxTimeLineSuiteV1 timeline{};
bool isInt(const Param &p) {
    return p.type == kOfxParamTypeInteger || p.type == kOfxParamTypeBoolean ||
           p.type == kOfxParamTypeChoice || p.type == kOfxParamTypeInteger2D ||
           p.type == kOfxParamTypeInteger3D;
}
unsigned dimensions(const Param &p) {
    if (p.type == kOfxParamTypeRGB || p.type == kOfxParamTypeDouble3D || p.type == kOfxParamTypeInteger3D)
        return 3;
    if (p.type == kOfxParamTypeRGBA)
        return 4;
    if (p.type == kOfxParamTypeDouble2D || p.type == kOfxParamTypeInteger2D)
        return 2;
    return 1;
}
bool numeric(const Param &p) {
    return isInt(p) || p.type == kOfxParamTypeDouble || p.type == kOfxParamTypeDouble2D ||
           p.type == kOfxParamTypeDouble3D || p.type == kOfxParamTypeRGB || p.type == kOfxParamTypeRGBA;
}
OfxStatus paramRead(OfxParamHandle h, va_list args, bool derivative = false) {
    if (!h)
        return kOfxStatErrBadHandle;
    auto &p = *reinterpret_cast<Param *>(h);
    if (p.type == kOfxParamTypeString || p.type == kOfxParamTypeCustom) {
        *va_arg(args, char **) = p.text.data();
        return kOfxStatOK;
    }
    if (!numeric(p))
        return kOfxStatErrUnsupported;
    for (unsigned i = 0; i < dimensions(p); ++i) {
        double v = derivative ? 0 : p.values[i];
        if (isInt(p))
            *va_arg(args, int *) = int(std::lround(v));
        else
            *va_arg(args, double *) = v;
    }
    return kOfxStatOK;
}
OfxStatus paramWrite(OfxParamHandle h, va_list args) {
    if (!h)
        return kOfxStatErrBadHandle;
    auto &p = *reinterpret_cast<Param *>(h);
    if (p.type == kOfxParamTypeString || p.type == kOfxParamTypeCustom) {
        auto *v = va_arg(args, char *);
        p.text = v ? v : "";
        return kOfxStatOK;
    }
    if (!numeric(p))
        return kOfxStatErrUnsupported;
    for (unsigned i = 0; i < dimensions(p); ++i)
        p.values[i] = isInt(p) ? double(va_arg(args, int)) : va_arg(args, double);
    return kOfxStatOK;
}
OfxStatus paramValue(OfxParamHandle h, ...) {
    va_list a;
    va_start(a, h);
    auto r = paramRead(h, a);
    va_end(a);
    return r;
}
OfxStatus paramTime(OfxParamHandle h, OfxTime t, ...) {
    va_list a;
    va_start(a, t);
    auto r = paramRead(h, a);
    va_end(a);
    return r;
}
OfxStatus paramDerivative(OfxParamHandle h, OfxTime t, ...) {
    va_list a;
    va_start(a, t);
    auto r = paramRead(h, a, true);
    va_end(a);
    return r;
}
OfxStatus paramIntegral(OfxParamHandle h, OfxTime begin, OfxTime end, ...) {
    va_list a;
    va_start(a, end);
    auto &p = *reinterpret_cast<Param *>(h);
    for (unsigned i = 0; i < dimensions(p); ++i)
        *va_arg(a, double *) = p.values[i] * (end - begin);
    va_end(a);
    return kOfxStatOK;
}
OfxStatus paramSet(OfxParamHandle h, ...) {
    va_list a;
    va_start(a, h);
    auto r = paramWrite(h, a);
    va_end(a);
    return r;
}
OfxStatus paramSetTime(OfxParamHandle h, OfxTime t, ...) {
    va_list a;
    va_start(a, t);
    auto r = paramWrite(h, a);
    va_end(a);
    return r;
}
OfxStatus message(void *, const char *type, const char *, const char *format, ...) {
    va_list a;
    va_start(a, format);
    vsnprintf_s(shared->message, sizeof(shared->message), _TRUNCATE, format, a);
    va_end(a);
    return strcmp(type, kOfxMessageQuestion) == 0 ? kOfxStatReplyYes : kOfxStatOK;
}
void suites() {
    properties.propSetPointer = [](OfxPropertySetHandle h, const char *n, int i, void *v) {
        return set(h, n, i, v);
    };
    properties.propSetString = [](OfxPropertySetHandle h, const char *n, int i, const char *v) {
        return set(h, n, i, std::string(v ? v : ""));
    };
    properties.propSetDouble = [](OfxPropertySetHandle h, const char *n, int i, double v) {
        return set(h, n, i, v);
    };
    properties.propSetInt = [](OfxPropertySetHandle h, const char *n, int i, int v) {
        return set(h, n, i, v);
    };
    properties.propGetPointer = [](OfxPropertySetHandle h, const char *n, int i, void **v) {
        return get(h, n, i, v);
    };
    properties.propGetDouble = [](OfxPropertySetHandle h, const char *n, int i, double *v) {
        return get(h, n, i, v);
    };
    properties.propGetInt = [](OfxPropertySetHandle h, const char *n, int i, int *v) {
        return get(h, n, i, v);
    };
    properties.propGetString = [](OfxPropertySetHandle h, const char *n, int i, char **v) {
        if (!h || !v)
            return kOfxStatErrBadHandle;
        auto it = props(h).data.find(n);
        if (it == props(h).data.end())
            return kOfxStatErrUnknown;
        if (i < 0 || i >= int(it->second.size()))
            return kOfxStatErrBadIndex;
        auto *s = std::get_if<std::string>(&it->second[i]);
        if (!s)
            return kOfxStatErrValue;
        *v = s->data();
        return kOfxStatOK;
    };
#define SETN(F, T)                                                                                           \
    properties.F##N = [](OfxPropertySetHandle h, const char *n, int count, T v) {                            \
        if (count < 0 || count > 512)                                                                        \
            return kOfxStatErrBadIndex;                                                                      \
        for (int i = 0; i < count; ++i) {                                                                    \
            auto r = properties.F(h, n, i, v[i]);                                                            \
            if (r != kOfxStatOK)                                                                             \
                return r;                                                                                    \
        }                                                                                                    \
        return kOfxStatOK;                                                                                   \
    };
    SETN(propSetPointer, void *const *)
    SETN(propSetString, const char *const *) SETN(propSetDouble, const double *) SETN(propSetInt, const int *)
#undef SETN
#define GETN(F, T)                                                                                           \
    properties.F##N = [](OfxPropertySetHandle h, const char *n, int count, T v) {                            \
        if (count < 0 || count > 512)                                                                        \
            return kOfxStatErrBadIndex;                                                                      \
        for (int i = 0; i < count; ++i) {                                                                    \
            auto r = properties.F(h, n, i, v + i);                                                           \
            if (r != kOfxStatOK)                                                                             \
                return r;                                                                                    \
        }                                                                                                    \
        return kOfxStatOK;                                                                                   \
    };
        GETN(propGetPointer, void **) GETN(propGetString, char **) GETN(propGetDouble, double *)
            GETN(propGetInt, int *)
#undef GETN
                properties.propReset = [](OfxPropertySetHandle h, const char *n) {
        if (!h)
            return kOfxStatErrBadHandle;
        return props(h).data.erase(n) ? kOfxStatOK : kOfxStatErrUnknown;
    };
    properties.propGetDimension = [](OfxPropertySetHandle h, const char *n, int *v) {
        if (!h || !v)
            return kOfxStatErrBadHandle;
        auto it = props(h).data.find(n);
        if (it == props(h).data.end())
            return kOfxStatErrUnknown;
        *v = int(it->second.size());
        return kOfxStatOK;
    };
    images.getPropertySet = [](OfxImageEffectHandle h, OfxPropertySetHandle *p) {
        if (!h || !p)
            return kOfxStatErrBadHandle;
        *p = handle(reinterpret_cast<Effect *>(h)->props);
        return kOfxStatOK;
    };
    images.getParamSet = [](OfxImageEffectHandle h, OfxParamSetHandle *p) {
        *p = reinterpret_cast<OfxParamSetHandle>(&reinterpret_cast<Effect *>(h)->params);
        return kOfxStatOK;
    };
    images.clipDefine = [](OfxImageEffectHandle h, const char *name, OfxPropertySetHandle *p) {
        auto &clip = reinterpret_cast<Effect *>(h)->clips[name];
        string(clip.props, kOfxPropName, std::string(name));
        string(clip.props, kOfxPropType, std::string(kOfxTypeClip));
        integer(clip.props, kOfxImageClipPropOptional, 0);
        integer(clip.props, kOfxImageEffectPropTemporalClipAccess, 0);
        integer(clip.props, kOfxImageClipPropIsMask, 0);
        *p = handle(clip.props);
        return kOfxStatOK;
    };
    images.clipGetHandle = [](OfxImageEffectHandle h, const char *n, OfxImageClipHandle *c,
                              OfxPropertySetHandle *p) {
        auto &clips = reinterpret_cast<Effect *>(h)->clips;
        auto it = clips.find(n);
        if (it == clips.end())
            return kOfxStatErrUnknown;
        if (c)
            *c = reinterpret_cast<OfxImageClipHandle>(&it->second);
        if (p)
            *p = handle(it->second.props);
        return kOfxStatOK;
    };
    images.clipGetPropertySet = [](OfxImageClipHandle h, OfxPropertySetHandle *p) {
        *p = handle(reinterpret_cast<Clip *>(h)->props);
        return kOfxStatOK;
    };
    images.clipGetImage = [](OfxImageClipHandle h, OfxTime t, const OfxRectD *, OfxPropertySetHandle *p) {
        if (!h || !p)
            return kOfxStatErrBadHandle;
        if (std::abs(t - shared->time) > 1e-5)
            return kOfxStatFailed;
        auto &clip = *reinterpret_cast<Clip *>(h);
        if (clip.pixels.empty())
            return kOfxStatFailed;
        auto *copy = new (std::nothrow) Props(clip.image);
        if (!copy)
            return kOfxStatErrMemory;
        *p = handle(*copy);
        return kOfxStatOK;
    };
    images.clipReleaseImage = [](OfxPropertySetHandle h) {
        delete reinterpret_cast<Props *>(h);
        return kOfxStatOK;
    };
    images.clipGetRegionOfDefinition = [](OfxImageClipHandle, OfxTime, OfxRectD *r) {
        *r = {0, 0, double(shared->width), double(shared->height)};
        return kOfxStatOK;
    };
    images.abort = [](OfxImageEffectHandle) {
        return std::atomic_ref(shared->quit).load(std::memory_order_acquire) ? 1 : 0;
    };
    images.imageMemoryAlloc = [](OfxImageEffectHandle, size_t n, OfxImageMemoryHandle *h) {
        if (n > 512ull * 1024 * 1024)
            return kOfxStatErrMemory;
        void *p = _aligned_malloc(std::max(size_t(1), n), 16);
        *h = reinterpret_cast<OfxImageMemoryHandle>(p);
        return p ? kOfxStatOK : kOfxStatErrMemory;
    };
    images.imageMemoryFree = [](OfxImageMemoryHandle h) {
        _aligned_free(h);
        return kOfxStatOK;
    };
    images.imageMemoryLock = [](OfxImageMemoryHandle h, void **p) {
        *p = h;
        return kOfxStatOK;
    };
    images.imageMemoryUnlock = [](OfxImageMemoryHandle) { return kOfxStatOK; };
    parameters.paramDefine = [](OfxParamSetHandle h, const char *type, const char *name,
                                OfxPropertySetHandle *p) {
        auto &set = *reinterpret_cast<ParamSet *>(h);
        if (set.params.size() >= 512)
            return kOfxStatErrMemory;
        auto &param = set.params[name];
        param.type = type;
        param.name = name;
        string(param.props, kOfxParamPropType, std::string(type));
        string(param.props, kOfxPropName, std::string(name));
        string(param.props, kOfxPropLabel, std::string(name));
        integer(param.props, kOfxParamPropEnabled, 1);
        integer(param.props, kOfxParamPropAnimates, 1);
        integer(param.props, kOfxParamPropSecret, 0);
        param.values.resize(dimensions(param), 0);
        for (unsigned i = 0; i < param.values.size(); ++i) {
            if (isInt(param))
                integer(param.props, kOfxParamPropDefault, 0, i);
            else
                number(param.props, kOfxParamPropDefault, 0, i);
        }
        *p = handle(param.props);
        return kOfxStatOK;
    };
    parameters.paramGetHandle = [](OfxParamSetHandle h, const char *n, OfxParamHandle *p,
                                   OfxPropertySetHandle *v) {
        auto &params = reinterpret_cast<ParamSet *>(h)->params;
        auto it = params.find(n);
        if (it == params.end())
            return kOfxStatErrUnknown;
        if (p)
            *p = reinterpret_cast<OfxParamHandle>(&it->second);
        if (v)
            *v = handle(it->second.props);
        return kOfxStatOK;
    };
    parameters.paramSetGetPropertySet = [](OfxParamSetHandle h, OfxPropertySetHandle *p) {
        *p = handle(reinterpret_cast<ParamSet *>(h)->props);
        return kOfxStatOK;
    };
    parameters.paramGetPropertySet = [](OfxParamHandle h, OfxPropertySetHandle *p) {
        *p = handle(reinterpret_cast<Param *>(h)->props);
        return kOfxStatOK;
    };
    parameters.paramGetValue = paramValue;
    parameters.paramGetValueAtTime = paramTime;
    parameters.paramGetDerivative = paramDerivative;
    parameters.paramGetIntegral = paramIntegral;
    parameters.paramSetValue = paramSet;
    parameters.paramSetValueAtTime = paramSetTime;
    parameters.paramGetNumKeys = [](OfxParamHandle, unsigned *n) {
        *n = 0;
        return kOfxStatOK;
    };
    parameters.paramGetKeyTime = [](OfxParamHandle, unsigned, OfxTime *) { return kOfxStatErrBadIndex; };
    parameters.paramGetKeyIndex = [](OfxParamHandle, OfxTime, int, int *i) {
        *i = -1;
        return kOfxStatFailed;
    };
    parameters.paramDeleteKey = [](OfxParamHandle, OfxTime) { return kOfxStatFailed; };
    parameters.paramDeleteAllKeys = [](OfxParamHandle) { return kOfxStatOK; };
    parameters.paramCopy = [](OfxParamHandle to, OfxParamHandle from, OfxTime, const OfxRangeD *) {
        auto &a = *reinterpret_cast<Param *>(to), &b = *reinterpret_cast<Param *>(from);
        if (a.type != b.type)
            return kOfxStatErrValue;
        a.values = b.values;
        a.text = b.text;
        return kOfxStatOK;
    };
    parameters.paramEditBegin = [](OfxParamSetHandle, const char *) { return kOfxStatOK; };
    parameters.paramEditEnd = [](OfxParamSetHandle) { return kOfxStatOK; };
    memory.memoryAlloc = [](void *, size_t n, void **p) {
        if (n > 512ull * 1024 * 1024)
            return kOfxStatErrMemory;
        *p = malloc(std::max(size_t(1), n));
        return *p ? kOfxStatOK : kOfxStatErrMemory;
    };
    memory.memoryFree = [](void *p) {
        free(p);
        return kOfxStatOK;
    };
    threads.multiThread = [](OfxThreadFunctionV1 f, unsigned, void *p) {
        f(0, 1, p);
        return kOfxStatOK;
    };
    threads.multiThreadNumCPUs = [](unsigned *n) {
        *n = 1;
        return kOfxStatOK;
    };
    threads.multiThreadIndex = [](unsigned *n) {
        *n = 0;
        return kOfxStatOK;
    };
    threads.multiThreadIsSpawnedThread = []() { return 0; };
    threads.mutexCreate = [](OfxMutexHandle *h, int count) {
        auto *m = new std::recursive_mutex;
        while (count-- > 0)
            m->lock();
        *h = reinterpret_cast<OfxMutexHandle>(m);
        return kOfxStatOK;
    };
    threads.mutexDestroy = [](OfxMutexHandle h) {
        delete reinterpret_cast<std::recursive_mutex *>(h);
        return kOfxStatOK;
    };
    threads.mutexLock = [](OfxMutexHandle h) {
        reinterpret_cast<std::recursive_mutex *>(h)->lock();
        return kOfxStatOK;
    };
    threads.mutexUnLock = [](OfxMutexHandle h) {
        reinterpret_cast<std::recursive_mutex *>(h)->unlock();
        return kOfxStatOK;
    };
    threads.mutexTryLock = [](OfxMutexHandle h) {
        return reinterpret_cast<std::recursive_mutex *>(h)->try_lock() ? kOfxStatOK : kOfxStatFailed;
    };
    messages.message = message;
    messages.setPersistentMessage = message;
    messages.clearPersistentMessage = [](void *) {
        shared->message[0] = 0;
        return kOfxStatOK;
    };
    progress.progressStart = [](void *, const char *) { return kOfxStatOK; };
    progress.progressUpdate = [](void *, double) { return shared->quit ? kOfxStatReplyNo : kOfxStatOK; };
    progress.progressEnd = [](void *) { return kOfxStatOK; };
    timeline.getTime = [](void *, double *t) {
        *t = shared->time;
        return kOfxStatOK;
    };
    timeline.gotoTime = [](void *, double) { return kOfxStatErrUnsupported; };
    timeline.getTimeBounds = [](void *, double *a, double *b) {
        *a = 0;
        *b = shared->duration;
        return kOfxStatOK;
    };
}
const void *fetch(OfxPropertySetHandle, const char *name, int version) {
    if (version == 1) {
        if (!strcmp(name, kOfxPropertySuite))
            return &properties;
        if (!strcmp(name, kOfxImageEffectSuite))
            return &images;
        if (!strcmp(name, kOfxParameterSuite))
            return &parameters;
        if (!strcmp(name, kOfxMemorySuite))
            return &memory;
        if (!strcmp(name, kOfxMultiThreadSuite))
            return &threads;
        if (!strcmp(name, kOfxMessageSuite))
            return &messages;
        if (!strcmp(name, kOfxProgressSuite))
            return &progress;
        if (!strcmp(name, kOfxTimeLineSuite))
            return &timeline;
    }
    if (version == 2 && !strcmp(name, kOfxMessageSuite))
        return &messages;
    return nullptr;
}
void action(OfxPlugin *plugin, const char *name, Effect *effect = nullptr, Props *in = nullptr,
            Props *out = nullptr) {
    int status = plugin->mainEntry(name, effect, in ? handle(*in) : nullptr, out ? handle(*out) : nullptr);
    bool mandatory = !strcmp(name, kOfxActionDescribe) ||
                     !strcmp(name, kOfxImageEffectActionDescribeInContext) ||
                     !strcmp(name, kOfxImageEffectActionRender);
    if (status != kOfxStatOK && (mandatory || status != kOfxStatReplyDefault))
        throw std::runtime_error(std::string(name) + " (statut " + std::to_string(status) + ")");
}
bool supports(Props &p, const char *name, const char *value) {
    auto it = p.data.find(name);
    if (it == p.data.end())
        return false;
    for (auto &v : it->second)
        if (auto s = std::get_if<std::string>(&v); s && *s == value)
            return true;
    return false;
}
void clipProperties(Clip &clip, unsigned w, unsigned h) {
    string(clip.props, kOfxImageEffectPropPixelDepth, std::string(kOfxBitDepthFloat));
    string(clip.props, kOfxImageEffectPropComponents, std::string(kOfxImageComponentRGBA));
    string(clip.props, kOfxImageClipPropUnmappedPixelDepth, std::string(kOfxBitDepthFloat));
    string(clip.props, kOfxImageClipPropUnmappedComponents, std::string(kOfxImageComponentRGBA));
    string(clip.props, kOfxImageEffectPropPreMultiplication, std::string(kOfxImagePreMultiplied));
    string(clip.props, kOfxImageClipPropFieldOrder, std::string(kOfxImageFieldNone));
    integer(clip.props, kOfxImageClipPropConnected, 1);
    integer(clip.props, kOfxImageClipPropContinuousSamples, 0);
    number(clip.props, kOfxImagePropPixelAspectRatio, 1);
    number(clip.props, kOfxImageEffectPropFrameRate, shared->fps);
    number(clip.props, kOfxImageEffectPropFrameRange, 0, 0);
    number(clip.props, kOfxImageEffectPropFrameRange, shared->duration, 1);
    clip.pixels.resize(size_t(w) * h * 4);
    clip.image = clip.props;
    string(clip.image, kOfxPropType, std::string(kOfxTypeImage));
    set(handle(clip.image), kOfxImagePropData, 0, static_cast<void *>(clip.pixels.data()));
    integer(clip.image, kOfxImagePropRowBytes, int(w * 16));
    for (int i = 0; i < 4; ++i) {
        int value = i < 2 ? 0 : i == 2 ? int(w) : int(h);
        integer(clip.image, kOfxImagePropBounds, value, i);
        integer(clip.image, kOfxImagePropRegionOfDefinition, value, i);
    }
    number(clip.image, kOfxImageEffectPropRenderScale, 1, 0);
    number(clip.image, kOfxImageEffectPropRenderScale, 1, 1);
    string(clip.image, kOfxImagePropField, std::string(kOfxImageFieldNone));
    string(clip.image, kOfxImagePropUniqueIdentifier, std::to_string(shared->time));
}
void run(const wchar_t *binary) {
    suites();
    Props hostProps;
    string(hostProps, kOfxPropType, std::string(kOfxTypeImageEffectHost));
    string(hostProps, kOfxPropName, std::string("org.shaderixframe.host"));
    string(hostProps, kOfxPropLabel, std::string("ShaderixFrame"));
    integer(hostProps, kOfxPropVersion, 0, 0);
    integer(hostProps, kOfxPropVersion, 9, 1);
    integer(hostProps, kOfxPropVersion, 0, 2);
    string(hostProps, kOfxPropVersionLabel, std::string("0.9.0"));
    for (auto p : {kOfxImageEffectHostPropIsBackground, kOfxImageEffectPropSupportsOverlays,
                   kOfxImageEffectPropSupportsTiles, kOfxImageEffectPropTemporalClipAccess,
                   kOfxImageEffectPropSupportsMultiResolution, kOfxImageEffectPropSupportsMultipleClipDepths,
                   kOfxImageEffectPropSupportsMultipleClipPARs, kOfxParamHostPropSupportsStringAnimation,
                   kOfxParamHostPropSupportsCustomInteract, kOfxParamHostPropSupportsCustomAnimation})
        integer(hostProps, p, 0);
    integer(hostProps, kOfxParamHostPropSupportsBooleanAnimation, 1);
    integer(hostProps, kOfxParamHostPropSupportsChoiceAnimation, 1);
    integer(hostProps, kOfxParamHostPropMaxParameters, 512);
    integer(hostProps, kOfxParamHostPropMaxPages, 0);
    string(hostProps, kOfxImageEffectPropSupportedComponents, std::string(kOfxImageComponentRGBA));
    string(hostProps, kOfxImageEffectPropSupportedPixelDepths, std::string(kOfxBitDepthFloat));
    string(hostProps, kOfxImageEffectPropSupportedContexts, std::string(kOfxImageEffectContextFilter), 0);
    string(hostProps, kOfxImageEffectPropSupportedContexts, std::string(kOfxImageEffectContextGeneral), 1);
    OfxHost host{handle(hostProps), fetch};
    integer(hostProps, kOfxPropAPIVersion, 1, 0);
    integer(hostProps, kOfxPropAPIVersion, 4, 1);
    string(hostProps, kOfxImageEffectHostPropNativeOrigin, std::string(kOfxHostNativeOriginBottomLeft));
    integer(hostProps, kOfxParamHostPropPageRowColumnCount, 0, 0);
    integer(hostProps, kOfxParamHostPropPageRowColumnCount, 0, 1);
    HMODULE module =
        LoadLibraryExW(binary, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module)
        throw std::runtime_error("Chargement DLL OFX échoué");
    auto count = reinterpret_cast<int (*)()>(GetProcAddress(module, "OfxGetNumberOfPlugins"));
    auto at = reinterpret_cast<OfxPlugin *(*)(int)>(GetProcAddress(module, "OfxGetPlugin"));
    if (!count || !at)
        throw std::runtime_error("Entrées OFX manquantes");
    int n = count();
    if (n < 1 || n > 256)
        throw std::runtime_error("Catalogue OFX invalide");
    OfxPlugin *plugin = nullptr;
    for (int i = 0; i < n; ++i) {
        auto *p = at(i);
        if (p && p->apiVersion == 1 && !strcmp(p->pluginApi, kOfxImageEffectPluginApi) &&
            (!shared->identifier[0] || !strcmp(p->pluginIdentifier, shared->identifier))) {
            plugin = p;
            break;
        }
    }
    if (!plugin)
        throw std::runtime_error("Identifiant OFX ImageEffect v1 introuvable");
    plugin->setHost(&host);
    action(plugin, kOfxActionLoad);
    Effect descriptor;
    action(plugin, kOfxActionDescribe, &descriptor);
    std::string context =
        supports(descriptor.props, kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextFilter)
            ? kOfxImageEffectContextFilter
            : kOfxImageEffectContextGeneral;
    if (!supports(descriptor.props, kOfxImageEffectPropSupportedContexts, context.c_str()) ||
        !supports(descriptor.props, kOfxImageEffectPropSupportedPixelDepths, kOfxBitDepthFloat))
        throw std::runtime_error("OFX requiert filtre/general RGBA float CPU");
    Props args;
    string(args, kOfxImageEffectPropContext, context);
    action(plugin, kOfxImageEffectActionDescribeInContext, &descriptor, &args);
    if (!descriptor.clips.contains("Source") || !descriptor.clips.contains("Output"))
        throw std::runtime_error("OFX : clips Source et Output requis");
    for (auto &[name, clip] : descriptor.clips) {
        int optional = 0;
        get(handle(clip.props), kOfxImageClipPropOptional, 0, &optional);
        if (name != "Source" && name != "Output" && !optional)
            throw std::runtime_error("OFX : entrées supplémentaires obligatoires non prises en charge");
        if (!supports(clip.props, kOfxImageEffectPropSupportedComponents, kOfxImageComponentRGBA))
            throw std::runtime_error("OFX : clip RGBA requis");
    }
    Effect instance = descriptor;
    string(instance.props, kOfxPropType, std::string(kOfxTypeImageEffectInstance));
    string(instance.props, kOfxImageEffectPropContext, context);
    set(handle(instance.props), kOfxPropInstanceData, 0, static_cast<void *>(nullptr));
    number(instance.props, kOfxImageEffectPropProjectSize, shared->width, 0);
    number(instance.props, kOfxImageEffectPropProjectSize, shared->height, 1);
    number(instance.props, kOfxImageEffectPropProjectExtent, shared->width, 0);
    number(instance.props, kOfxImageEffectPropProjectExtent, shared->height, 1);
    number(instance.props, kOfxImageEffectPropProjectOffset, 0, 0);
    number(instance.props, kOfxImageEffectPropProjectOffset, 0, 1);
    number(instance.props, kOfxImageEffectPropProjectPixelAspectRatio, 1);
    number(instance.props, kOfxImageEffectPropFrameRate, shared->fps);
    number(instance.props, kOfxImageEffectInstancePropEffectDuration, shared->duration);
    Json metadata = Json::Object();
    metadata["identifier"] = plugin->pluginIdentifier;
    metadata["name"] = text(descriptor.props, kOfxPropLabel, plugin->pluginIdentifier);
    metadata["context"] = context;
    metadata["depth"] = "float32";
    metadata["parameters"] = Json::Array();
    unsigned index = 0;
    std::vector<std::pair<Param *, unsigned>> bindings;
    for (auto &[name, param] : instance.params.params) {
        if (param.type == kOfxParamTypeString || param.type == kOfxParamTypeCustom) {
            param.text = text(param.props, kOfxParamPropDefault);
            continue;
        }
        if (!numeric(param))
            continue;
        for (unsigned component = 0; component < dimensions(param); ++component) {
            if (index >= 512)
                throw std::runtime_error("512 contrôles OFX maximum");
            param.values[component] = real(param.props, kOfxParamPropDefault, 0, component);
            Json p = Json::Object();
            p["key"] = "p" + std::to_string(index++);
            p["name"] = text(param.props, kOfxPropLabel, name) +
                        (dimensions(param) > 1 ? "." + std::to_string(component) : "");
            double low =
                       real(param.props, kOfxParamPropMin,
                            (param.type == kOfxParamTypeRGB || param.type == kOfxParamTypeRGBA) ? 0 : -100000,
                            component),
                   high =
                       real(param.props, kOfxParamPropMax,
                            (param.type == kOfxParamTypeRGB || param.type == kOfxParamTypeRGBA) ? 1 : 100000,
                            component);
            if (param.type == kOfxParamTypeBoolean) {
                low = 0;
                high = 1;
            }
            if (param.type == kOfxParamTypeChoice) {
                low = 0;
                auto options = param.props.data.find(kOfxParamPropChoiceOption);
                high = options == param.props.data.end() ? 0 : double(options->second.size() - 1);
            }
            if (!std::isfinite(low) || !std::isfinite(high) || low > high)
                throw std::runtime_error("Plage OFX invalide");
            p["minimum"] = low;
            p["maximum"] = high;
            p["default"] = std::clamp(param.values[component], low, high);
            p["integer"] = isInt(param);
            metadata["parameters"].Push(p);
            bindings.emplace_back(&param, component);
        }
    }
    clipProperties(instance.clips["Source"], shared->width, shared->height);
    clipProperties(instance.clips["Output"], shared->width, shared->height);
    action(plugin, kOfxActionCreateInstance, &instance);
    Props preferences;
    action(plugin, kOfxImageEffectActionGetClipPreferences, &instance, nullptr, &preferences);
    std::string premult = text(preferences, kOfxImageEffectPropPreMultiplication, kOfxImagePreMultiplied);
    if (premult != kOfxImagePreMultiplied && premult != kOfxImageUnPreMultiplied &&
        premult != kOfxImageOpaque)
        throw std::runtime_error("Préférence alpha OFX inconnue");
    string(instance.clips["Output"].props, kOfxImageEffectPropPreMultiplication, premult);
    string(instance.clips["Output"].image, kOfxImageEffectPropPreMultiplication, premult);
    auto textMeta = metadata.Dump(0);
    if (textMeta.size() >= sizeof(shared->metadata))
        throw std::runtime_error("Métadonnées OFX trop grandes");
    memcpy(shared->metadata, textMeta.c_str(), textMeta.size() + 1);
    std::atomic_ref(shared->ready).store(1, std::memory_order_release);
    uint64_t last = 0;
    while (!std::atomic_ref(shared->quit).load(std::memory_order_acquire)) {
        auto request = std::atomic_ref(shared->request).load(std::memory_order_acquire);
        if (request == last) {
            Sleep(1);
            continue;
        }
        shared->result = 0;
        try {
            Json values;
            if (!Json::Parse(shared->parameters, values))
                throw std::runtime_error("Paramètres OFX invalides");
            std::set<std::string> changed;
            for (unsigned i = 0; i < bindings.size(); ++i) {
                auto &[p, c] = bindings[i];
                auto key = "p" + std::to_string(i);
                double v = values[key].Num(p->values[c]);
                auto &m = metadata["parameters"].At(i);
                if (!std::isfinite(v) || v < m["minimum"].Num() || v > m["maximum"].Num())
                    throw std::runtime_error("Valeur OFX hors plage");
                v = isInt(*p) ? std::round(v) : v;
                if (p->values[c] != v)
                    changed.insert(p->name);
                p->values[c] = v;
            }
            if (!changed.empty()) {
                Props event;
                string(event, kOfxPropChangeReason, std::string(kOfxChangeUserEdited));
                number(event, kOfxPropTime, shared->time);
                number(event, kOfxImageEffectPropRenderScale, 1, 0);
                number(event, kOfxImageEffectPropRenderScale, 1, 1);
                action(plugin, kOfxActionBeginInstanceChanged, &instance, &event);
                for (auto &name : changed) {
                    string(event, kOfxPropName, name);
                    string(event, kOfxPropType, std::string(kOfxTypeParameter));
                    action(plugin, kOfxActionInstanceChanged, &instance, &event);
                }
                action(plugin, kOfxActionEndInstanceChanged, &instance, &event);
            }
            auto *input = reinterpret_cast<float *>(shared + 1);
            auto &source = instance.clips["Source"], &output = instance.clips["Output"];
            unsigned w = shared->width, h = shared->height;
            for (unsigned y = 0; y < h; ++y)
                for (unsigned x = 0; x < w; ++x) {
                    size_t from = (size_t(y) * w + x) * 4, to = (size_t(h - 1 - y) * w + x) * 4;
                    float alpha = input[from + 3];
                    for (unsigned k = 0; k < 4; ++k)
                        source.pixels[to + k] = input[from + k] * (k < 3 ? alpha : 1);
                }
            std::fill(output.pixels.begin(), output.pixels.end(), 0);
            for (auto *clip : {&source, &output}) {
                number(clip->props, kOfxImageEffectPropFrameRate, shared->fps);
                number(clip->props, kOfxImageEffectPropFrameRange, shared->duration, 1);
                string(clip->image, kOfxImagePropUniqueIdentifier, std::to_string(request));
            }
            number(instance.props, kOfxImageEffectPropFrameRate, shared->fps);
            number(instance.props, kOfxImageEffectInstancePropEffectDuration, shared->duration);
            Props in;
            number(in, kOfxPropTime, shared->time);
            number(in, kOfxImageEffectPropRenderScale, 1, 0);
            number(in, kOfxImageEffectPropRenderScale, 1, 1);
            string(in, kOfxImageEffectPropFieldToRender, std::string(kOfxImageFieldNone));
            integer(in, kOfxImageEffectPropSequentialRenderStatus, 0);
            integer(in, kOfxImageEffectPropInteractiveRenderStatus, 0);
            for (int i = 0; i < 4; ++i)
                integer(in, kOfxImageEffectPropRenderWindow, i < 2 ? 0 : i == 2 ? int(w) : int(h), i);
            number(in, kOfxImageEffectPropFrameRange, shared->time, 0);
            number(in, kOfxImageEffectPropFrameRange, shared->time, 1);
            number(in, kOfxImageEffectPropFrameStep, 1);
            action(plugin, kOfxImageEffectActionBeginSequenceRender, &instance, &in);
            try {
                action(plugin, kOfxImageEffectActionRender, &instance, &in);
            } catch (...) {
                action(plugin, kOfxImageEffectActionEndSequenceRender, &instance, &in);
                throw;
            }
            action(plugin, kOfxImageEffectActionEndSequenceRender, &instance, &in);
            auto *result = input + size_t(w) * h * 4;
            for (unsigned y = 0; y < h; ++y)
                for (unsigned x = 0; x < w; ++x) {
                    size_t to = (size_t(y) * w + x) * 4, from = (size_t(h - 1 - y) * w + x) * 4;
                    float alpha = premult == kOfxImageOpaque ? 1 : output.pixels[from + 3];
                    result[to + 3] = alpha;
                    for (unsigned k = 0; k < 3; ++k)
                        result[to + k] = premult == kOfxImagePreMultiplied
                                             ? (alpha > 0 ? output.pixels[from + k] / alpha : 0)
                                             : output.pixels[from + k];
                }
        } catch (const std::exception &e) {
            shared->result = 1;
            strncpy_s(shared->message, e.what(), _TRUNCATE);
        }
        last = request;
        std::atomic_ref(shared->response).store(last, std::memory_order_release);
    }
    action(plugin, kOfxActionDestroyInstance, &instance);
    action(plugin, kOfxActionUnload);
    FreeLibrary(module);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    if (argc != 3)
        return 2;
    HANDLE file = CreateFileW(argv[1], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return 3;
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!mapping) {
        CloseHandle(file);
        return 4;
    }
    shared = static_cast<sf::OfxShared *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!shared) {
        CloseHandle(mapping);
        CloseHandle(file);
        return 5;
    }
    int result = 0;
    try {
        LARGE_INTEGER size{};
        GetFileSizeEx(file, &size);
        if (shared->magic != 0x53464f58 || shared->version != 1 || !shared->width || !shared->height ||
            sf::ofxBytes(shared->width, shared->height) > uint64_t(size.QuadPart))
            throw std::runtime_error("Protocole OFX invalide");
        run(argv[2]);
    } catch (const std::exception &e) {
        shared->result = 1;
        strncpy_s(shared->message, e.what(), _TRUNCATE);
        std::atomic_ref(shared->ready).store(2, std::memory_order_release);
        result = 1;
    }
    UnmapViewOfFile(shared);
    CloseHandle(mapping);
    CloseHandle(file);
    return result;
}

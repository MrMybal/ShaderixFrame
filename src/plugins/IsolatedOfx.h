#pragma once
#include "media/Media.h"
#include "plugins/OfxTransport.h"
namespace sf {
class IsolatedOfx {
    HANDLE file = INVALID_HANDLE_VALUE, mapping = nullptr, process = nullptr;
    std::filesystem::path ipc;
    OfxShared *shared = nullptr;
    uint64_t sequence = 0;
    void cleanup();
    void await(uint64_t value, unsigned timeout = 15000);

  public:
    IsolatedOfx(const std::string &path, const std::string &identifier, unsigned width, unsigned height);
    ~IsolatedOfx() { cleanup(); }
    IsolatedOfx(const IsolatedOfx &) = delete;
    Json metadata() const;
    FloatImage render(const FloatImage &image, const Effect &effect, double time, double fps,
                      double duration);
    void terminateForTest();
};
std::string ofxBinary(const std::string &path);
void validateOfx(const Effect &effect);
} // namespace sf

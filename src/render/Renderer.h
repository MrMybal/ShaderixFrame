#pragma once
#include "media/Media.h"
#include "plugins/Effects.h"
#include "plugins/IsolatedOfx.h"
#include "render/Titles.h"
#include <d3d11.h>
namespace sf {
class Renderer {
    struct Target {
        Com<ID3D11Texture2D> texture;
        Com<ID3D11RenderTargetView> rtv;
        Com<ID3D11ShaderResourceView> srv;
    };
    struct Shader {
        Com<ID3D11PixelShader> shader;
        std::vector<EffectParameter> params;
    };
    Com<ID3D11Device> device;
    Com<ID3D11DeviceContext> context;
    Com<ID3D11VertexShader> vertex;
    Com<ID3D11Buffer> constants, parameters, maskConstants;
    Com<ID3D11SamplerState> sampler;
    Com<ID3D11BlendState> alpha;
    Com<ID3D11RasterizerState> raster;
    std::map<std::string, Shader> shaders;
    std::map<uint64_t, std::pair<Com<ID3D11ShaderResourceView>, Com<ID3D11ShaderResourceView>>> gradeLuts;
    Com<ID3D11ShaderResourceView> gradeLut, gradeLut1;
    void bindGradeLut(const Effect &effect);
    struct OfxEntry {
        std::string identity;
        unsigned width = 0, height = 0;
        std::unique_ptr<IsolatedOfx> process;
    };
    std::map<std::pair<Id, unsigned>, OfxEntry> ofx;
    FloatImage readTarget(const Target &target);
    void writeTarget(Target &target, const FloatImage &image);
    struct Nested {
        std::string identity;
        Project project;
        std::unique_ptr<Renderer> renderer;
    };
    std::map<Id, Nested> nested;
    TitleRasterizer titles;
    Target composite, alternateComposite, transitionOutput, originalInput, display, working[2];
    std::map<Id, Target> compositionNodes;
    std::string currentSpace = "srgb", currentTone = "clip";
    float currentExposure = 0;
    struct HistoryFrame {
        Target previous, current;
        bool valid = false;
    };
    std::map<Id, HistoryFrame> history;
    double previousTime = -1, shaderTimeOrigin = 0;
    unsigned width = 0, height = 0;
    unsigned fps = 30;
    void createTarget(Target &t, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    Shader &compile(const std::string &body);
    void draw(Target &target, const std::string &code, ID3D11ShaderResourceView *input,
              const std::map<std::string, Parameter> &params, double time, double local, unsigned sourceWidth,
              unsigned sourceHeight, const Clip *clip = nullptr,
              ID3D11ShaderResourceView *background = nullptr, ID3D11ShaderResourceView *secondInput = nullptr,
              float transitionProgress = 0, unsigned pass = 0,
              ID3D11ShaderResourceView *previousPass = nullptr,
              ID3D11ShaderResourceView *previousFrame = nullptr, ID3D11ShaderResourceView *input2 = nullptr,
              ID3D11ShaderResourceView *input3 = nullptr);

  public:
    EffectLibrary library;
    Id captureGradeClip = 0;
    int captureGradeEffect = -1;
    FloatImage capturedGradeInput;
    void initialize(ID3D11Device *device = nullptr, ID3D11DeviceContext *context = nullptr);
    void validateShader(const std::string &body) { compile(body); }
    void clearHistory() {
        history.clear();
        previousTime = -1;
        for (auto &[id, entry] : nested)
            if (entry.renderer)
                entry.renderer->clearHistory();
    }
    void render(const Project &p, MediaCache &media, double time, unsigned width, unsigned height,
                bool bypassGrading = false, Id matteClip = 0, int matteEffect = -1);
    ID3D11ShaderResourceView *layerNodeView(Id id) const {
        auto it = compositionNodes.find(id);
        return it == compositionNodes.end() ? nullptr : it->second.srv.Get();
    }
    Image readback();
    FloatImage readbackFloat(bool forDisplay = false);
    ID3D11ShaderResourceView *view() const { return display.srv.Get(); }
    ID3D11Device *gpu() const { return device.Get(); }
    ID3D11DeviceContext *gpuContext() const { return context.Get(); }
};
} // namespace sf

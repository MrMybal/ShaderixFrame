#include "render/Renderer.h"
#include "adapters/LayerGraph.h"
#include "adapters/ShaderixelBlend.h"
#include "core/Motion.h"
#include "render/Color.h"
#include "render/Grading.h"
#include <DirectXPackedVector.h>
#include <bit>
#include <d3dcompiler.h>
#include <functional>
#include <mutex>
#include <sstream>
namespace sf {
static const char *vertexSource = R"(
struct VSOut {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};
VSOut main(uint id:SV_VertexID){VSOut v;v.uv=float2((id<<1)&2,id&2);v.pos=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;}
)";
static const char *prelude = R"(
#define pass sfPass
cbuffer Frame:register(b0){float4 dimensions;float4 clock;float4 transform;float4 options;float4 execution;};
cbuffer BezierMask:register(b2){float4 sfMaskInfo;float4 sfMaskPoints[32];};
Texture2D<float4> gIn0:register(t0);SamplerState sLinearClamp:register(s0);
Texture2D<float4> sfBackground:register(t1);
Texture2D<float4> gIn1:register(t2);
Texture2D<float4> gIn2:register(t7);Texture2D<float4> gIn3:register(t8);
Texture2D<float4> sfPreviousPass:register(t3);
Texture2D<float4> sfPreviousFrame:register(t4);
struct EffectContext {int2 px;float2 uv;float2 size;float2 texel;uint pass;float time;float localTime;float deltaTime;uint frame;float2 sourceSize;float transitionProgress;};
float4 sfDecode(float4 c){
#if SF_PREMULTIPLIED
return c;
#else
return float4(c.a>0?c.rgb/c.a:float3(0,0,0),c.a);
#endif
}
float4 InputUV(uint i,float2 uv){return sfDecode(i==0?gIn0.SampleLevel(sLinearClamp,uv,0):i==1?gIn1.SampleLevel(sLinearClamp,uv,0):i==2?gIn2.SampleLevel(sLinearClamp,uv,0):i==3?gIn3.SampleLevel(sLinearClamp,uv,0):float4(0,0,0,0));}
float4 Input(uint i,int2 px){return InputUV(i,(float2(px)+0.5)/dimensions.xy);}
float4 PrevUV(float2 uv){return sfDecode(sfPreviousPass.SampleLevel(sLinearClamp,uv,0));}
float4 Prev(int2 px){return PrevUV((float2(px)+0.5)/dimensions.xy);}
float4 HistoryUV(float2 uv){return sfDecode(sfPreviousFrame.SampleLevel(sLinearClamp,uv,0));}
float4 History(int2 px){return HistoryUV((float2(px)+0.5)/dimensions.xy);}
bool HasHistory(){return execution.y!=0;}
bool HasInput(uint i){return i<4&&(((uint)options.w&(1u<<i))!=0);}
float4 Premultiply(float4 c){return float4(c.rgb*c.a,c.a);}
float4 Unpremultiply(float4 c){return float4(c.a>0?c.rgb/c.a:float3(0,0,0),c.a);}
)";
static const char *suffix = R"(
float4 main(float4 pos:SV_POSITION,float2 uv:TEXCOORD0):SV_TARGET {
    EffectContext ctx;ctx.px=int2(pos.xy);ctx.uv=uv;ctx.size=dimensions.xy;ctx.texel=1/dimensions.xy;
    ctx.pass=(uint)execution.x;ctx.time=clock.x;ctx.localTime=clock.y;ctx.deltaTime=clock.z;ctx.frame=(uint)clock.w;ctx.sourceSize=dimensions.zw;
    ctx.transitionProgress=options.z;
    float4 c=Effect(ctx);
#if SF_PREMULTIPLIED || SF_DISPLAY
    return c;
#else
    return Premultiply(c);
#endif
})";
static const char *generator = R"(
float4 Effect(EffectContext ctx){
    float2 p=(ctx.uv-0.5)*float2(ctx.size.x/ctx.size.y,1);float t=ctx.localTime;
    float glow=0;float3 c=float3(0.015,0.02,0.045);
    for(int i=0;i<6;++i){float fi=i;float y=p.y+0.2*sin(p.x*2.4+t*0.35+fi*0.7)+0.13*cos(p.x*5-t*0.2+fi);
      float band=exp(-abs(y+0.13-fi*0.04)*(18+fi*5));float3 tone=lerp(float3(0.95,0.19,0.045),float3(0.16,0.42,0.72),fi/5);c+=tone*band*0.34;}
    float moon=1-smoothstep(0.135,0.137,length(p-float2(0.48,-0.19)));c=lerp(c,float3(1,0.72,0.42),moon);
    c*=1-0.6*length(ctx.uv-0.5);return float4(saturate(c),1);
})";
static const char *copy = R"(
float4 Effect(EffectContext ctx){float ratio=(ctx.size.x/ctx.size.y)/(ctx.sourceSize.x/ctx.sourceSize.y);float2 uv=(ctx.uv-0.5)*float2(max(1,ratio),max(1,1/ratio))+0.5;if(any(uv<0)||any(uv>1))return 0;return InputUV(0,uv);}
)";
static const char *compose = R"(
//@param widget_enabled bool 0 "Motion"
//@param widget_rect float4 0 0 1 1 -1024 1024 "Motion rectangle"
//@param widget_clip float4 0 0 1 1 -1024 1024 "Motion clip"
//@param mask_mode int 0 0 3 "Mask shape"
//@param mask_invert bool 0 "Invert"
//@param mask_x float 0 -2 2 "Center X"
//@param mask_y float 0 -2 2 "Center Y"
//@param mask_width float 1 0.001 4 "Width"
//@param mask_height float 1 0.001 4 "Height"
//@param mask_rotation float 0 -36000 36000 "Rotation"
//@param mask_feather float 0 0 1024 "Outline"
//@param mask_pixel_scale float 0.001388889 0 1 "Pixel scale"
float4 Effect(EffectContext ctx){
    float4 bg=Unpremultiply(sfBackground.SampleLevel(sLinearClamp,ctx.uv,0));
    float2 p=(ctx.uv-0.5-transform.xy)*float2(ctx.size.x/ctx.size.y,1);float a=transform.w;
    float2 q=float2(cos(a)*p.x+sin(a)*p.y,-sin(a)*p.x+cos(a)*p.y)/max(transform.z,0.001);
    float2 uv=q/float2(ctx.size.x/ctx.size.y,1)+0.5;
    if(any(uv<0)||any(uv>1))return bg;
    if(widget_enabled){
        if(any(ctx.uv<widget_clip.xy)||any(ctx.uv>widget_clip.zw))return bg;
        uv=(uv-widget_rect.xy)/widget_rect.zw;
        if(any(uv<0)||any(uv>1))return bg;
    }
    float4 c=InputUV(0,uv);c.a=saturate(c.a*options.x);
    if(mask_mode!=0){
        float aspect=ctx.size.x/ctx.size.y;
        float2 mp=(uv-0.5-float2(mask_x,mask_y))*float2(aspect,1);
        float angle=mask_rotation*0.01745329252;
        mp=float2(cos(angle)*mp.x+sin(angle)*mp.y,-sin(angle)*mp.x+cos(angle)*mp.y);
        float2 radius=max(float2(mask_width*aspect,mask_height)*0.5,0.00001);
        float2 box=abs(mp)-radius;
        float distance=mask_mode==1 ? length(max(box,0))+min(max(box.x,box.y),0) :
                                     (length(mp/radius)-1)*min(radius.x,radius.y);
        if(mask_mode==3){
            float2 p=mp/(radius*2);float nearest=100;bool inside=false;
            [loop]for(int anchor=0;anchor<(int)sfMaskInfo.x;++anchor){
                int next=(anchor+1)%(int)sfMaskInfo.x;
                float2 a=sfMaskPoints[anchor*2].xy,b=sfMaskPoints[anchor*2+1].xy;
                float2 c=sfMaskPoints[next*2].zw,d=sfMaskPoints[next*2].xy;
                float2 previous=a;
                [loop]for(int step=1;step<=16;++step){float t=step/16.0,u=1-t;float2 q=u*u*u*a+3*u*u*t*b+3*u*t*t*c+t*t*t*d;
                    float2 edge=q-previous;float along=saturate(dot(p-previous,edge)/max(dot(edge,edge),.0000001));
                    nearest=min(nearest,length(p-previous-edge*along));
                    if((previous.y>p.y)!=(q.y>p.y)){float cross=previous.x+(p.y-previous.y)*(q.x-previous.x)/(q.y-previous.y);if(p.x<cross)inside=!inside;}
                    previous=q;
                }
            }
            distance=nearest*(inside?-1:1)*min(radius.x,radius.y)*2;
        }
        float feather=max(mask_feather*mask_pixel_scale,0.5/ctx.size.y);
        float coverage=1-smoothstep(-feather*0.5,feather*0.5,distance);
        c.a*=mask_invert ? 1-coverage : coverage;
    }
    float3 mixed=c.rgb;
    if(options.y==1)mixed=bg.rgb+c.rgb;
    if(options.y==2)mixed=bg.rgb*c.rgb;
    if(options.y==3)mixed=1-(1-bg.rgb)*(1-c.rgb);
    // Straight textures, premultiplied Porter-Duff accumulation, then straight output.
    float outAlpha=c.a+bg.a*(1-c.a);
    float3 sourceBlend=lerp(c.rgb,mixed,bg.a);
    return float4(outAlpha>0?(sourceBlend*c.a+bg.rgb*bg.a*(1-c.a))/outAlpha:float3(0,0,0),outAlpha);
}
)";
static const char *displayCode = R"(
//@param space int 0 0 2 "Space"
//@param tone int 0 0 2 "Tone mapping"
//@param exposure float 0 -20 20 "Exposure"
float3 Decode(float3 c){return float3(c.x<=.04045?c.x/12.92:pow((c.x+.055)/1.055,2.4),c.y<=.04045?c.y/12.92:pow((c.y+.055)/1.055,2.4),c.z<=.04045?c.z/12.92:pow((c.z+.055)/1.055,2.4));}
float3 Encode(float3 c){return float3(c.x<=.0031308?c.x*12.92:1.055*pow(c.x,1/2.4)-.055,c.y<=.0031308?c.y*12.92:1.055*pow(c.y,1/2.4)-.055,c.z<=.0031308?c.z*12.92:1.055*pow(c.z,1/2.4)-.055);}
float4 Effect(EffectContext ctx){float4 c=Input(0,ctx.px);if(space==0 && tone==0 && exposure==0)return c;
if(space==0)c.rgb=Decode(c.rgb);if(space==2)c.rgb=mul(float3x3(1.660491,-.587641,-.072850,-.124550,1.132900,-.008349,-.018151,-.100579,1.118730),c.rgb);
c.rgb=max(0,c.rgb*exp2(exposure));if(tone==1)c.rgb=c.rgb/(1+c.rgb);if(tone==2)c.rgb=saturate((c.rgb*(2.51*c.rgb+.03))/(c.rgb*(2.43*c.rgb+.59)+.14));c.rgb=Encode(c.rgb);return c;}
)";
static Com<ID3DBlob> blob(const std::string &text, const char *profile) {
    // Bytecode is device-independent; preview, exports and tracking share compilations.
    static std::mutex mutex;
    static std::map<std::string, Com<ID3DBlob>> cache;
    auto key = std::string(profile) + text;
    {
        std::lock_guard lock(mutex);
        if (auto found = cache.find(key); found != cache.end())
            return found->second;
    }
    Com<ID3DBlob> b, e;
    HRESULT hr = D3DCompile(text.data(), text.size(), "ShaderixFrame", nullptr, nullptr, "main", profile,
                            D3DCOMPILE_ENABLE_STRICTNESS, 0, &b, &e);
    if (FAILED(hr))
        throw std::runtime_error(
            e ? std::string(static_cast<const char *>(e->GetBufferPointer()), e->GetBufferSize())
              : "HLSL compilation failed");
    {
        std::lock_guard lock(mutex);
        if (cache.size() >= 256)
            cache.erase(cache.begin());
        cache.emplace(std::move(key), b);
    }
    return b;
}
void Renderer::initialize(ID3D11Device *d, ID3D11DeviceContext *c) {
    if (d) {
        device = d;
        context = c;
    } else {
        D3D_FEATURE_LEVEL level;
        HRESULT hr = D3D11CreateDevice(nullptr,
                                       GetEnvironmentVariableW(L"SF_FORCE_WARP", nullptr, 0)
                                           ? D3D_DRIVER_TYPE_WARP
                                           : D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context);
        if (FAILED(hr))
            check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                    &device, &level, &context),
                  "Direct3D initialization");
    }
    auto v = blob(vertexSource, "vs_5_0");
    check(device->CreateVertexShader(v->GetBufferPointer(), v->GetBufferSize(), nullptr, &vertex),
          "Vertex shader");
    D3D11_BUFFER_DESC b{};
    b.ByteWidth = 80;
    b.Usage = D3D11_USAGE_DEFAULT;
    b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(device->CreateBuffer(&b, nullptr, &constants), "Video constants");
    b.ByteWidth = 64 * 16;
    check(device->CreateBuffer(&b, nullptr, &parameters), "Video parameters");
    b.ByteWidth = 33 * 16;
    check(device->CreateBuffer(&b, nullptr, &maskConstants), "Bezier mask");
    D3D11_SAMPLER_DESC s{};
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&s, &sampler);
    D3D11_BLEND_DESC blend{};
    auto &a = blend.RenderTarget[0];
    a.BlendEnable = TRUE;
    a.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    a.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    a.BlendOp = D3D11_BLEND_OP_ADD;
    a.SrcBlendAlpha = D3D11_BLEND_ONE;
    a.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    a.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    a.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend, &alpha);
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &raster);
}
void Renderer::createTarget(Target &t, DXGI_FORMAT format) {
    t = {};
    D3D11_TEXTURE2D_DESC d{};
    d.Width = width;
    d.Height = height;
    d.MipLevels = d.ArraySize = 1;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    check(device->CreateTexture2D(&d, nullptr, &t.texture), "Video texture");
    check(device->CreateRenderTargetView(t.texture.Get(), nullptr, &t.rtv), "Video render target");
    check(device->CreateShaderResourceView(t.texture.Get(), nullptr, &t.srv), "Video view");
}
Renderer::Shader &Renderer::compile(const std::string &code) {
    auto i = shaders.find(code);
    if (i != shaders.end())
        return i->second;
    Shader s;
    s.params = effectParameters(code);
    std::string text = std::string("#define SF_PREMULTIPLIED ") +
                       (code.find("//@premultiplied") != std::string::npos ? "1" : "0") +
                       "\n#define SF_DISPLAY " + (code == displayCode ? "1" : "0") + "\n" + prelude;
    text += "\ncbuffer Params:register(b1){float4 sfParams[64];};\n";
    for (size_t n = 0; n < s.params.size(); ++n) {
        auto &p = s.params[n];
        std::string swizzle = p.type == "float2"                          ? "xy"
                              : p.type == "float3"                        ? "xyz"
                              : (p.type == "float4" || p.type == "color") ? "xyzw"
                                                                          : "x";
        text += "#define " + p.key + " (" +
                (p.type == "int"    ? "(int)"
                 : p.type == "bool" ? "(bool)"
                                    : "") +
                "sfParams[" + std::to_string(n) + "]." + swizzle + ")\n";
    }
    text += code;
    text += suffix;
    auto b = blob(text, "ps_5_0");
    check(device->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &s.shader),
          "Pixel shader");
    if (shaders.size() >= 256)
        shaders.clear();
    return shaders.emplace(code, std::move(s)).first->second;
}
void Renderer::draw(Target &target, const std::string &code, ID3D11ShaderResourceView *input,
                    const std::map<std::string, Parameter> &ps, double time, double local, unsigned sw,
                    unsigned sh, const Clip *clip, ID3D11ShaderResourceView *background,
                    ID3D11ShaderResourceView *secondInput, float transitionProgress, unsigned passIndex,
                    ID3D11ShaderResourceView *previousPass, ID3D11ShaderResourceView *previousFrame,
                    ID3D11ShaderResourceView *input2, ID3D11ShaderResourceView *input3) {
    auto &shader = compile(code);
    float frame[20] = {float(width),
                       float(height),
                       float(sw),
                       float(sh),
                       float(time + shaderTimeOrigin),
                       float(local),
                       1.f / fps,
                       float(std::floor((time + shaderTimeOrigin) * fps)),
                       0,
                       0,
                       1,
                       0,
                       1,
                       0,
                       transitionProgress,
                       float((input ? 1 : 0) | (secondInput ? 2 : 0) | (input2 ? 4 : 0) | (input3 ? 8 : 0)),
                       float(passIndex),
                       previousFrame ? 1.f : 0.f,
                       0,
                       0};
    if (clip) {
        frame[8] = clip->param("x", time, 0);
        frame[9] = clip->param("y", time, 0);
        frame[10] = clip->param("scale", time, 1);
        frame[11] = clip->param("rotation", time, 0) * 3.14159265f / 180;
        frame[12] = clip->param("opacity", time, 1) * clip->envelope(time);
        frame[13] = std::clamp(std::round(clip->param("blend", time, 0)), 0.f, 3.f);
    }
    std::array<sx::float4, 64> params{};
    for (size_t n = 0; n < shader.params.size(); ++n) {
        auto &p = shader.params[n];
        params[n] = p.value;
        int count = p.type == "float2"                          ? 2
                    : p.type == "float3"                        ? 3
                    : (p.type == "float4" || p.type == "color") ? 4
                                                                : 1;
        for (int k = 0; k < count; ++k) {
            auto it = ps.find(p.key + (count > 1 ? "." + std::to_string(k) : ""));
            if (it != ps.end())
                params[n][k] = it->second.at(time);
        }
    }
    context->UpdateSubresource(constants.Get(), 0, nullptr, frame, 0, 0);
    context->UpdateSubresource(parameters.Get(), 0, nullptr, params.data(), 0, 0);
    std::array<sx::float4, 33> path{};
    if (clip) {
        path[0].x = float(clip->mask.points.size());
        for (size_t i = 0; i < clip->mask.points.size(); ++i) {
            auto &point = clip->mask.points[i];
            path[1 + i * 2] = {point.x, point.y, point.inX, point.inY};
            path[2 + i * 2] = {point.outX, point.outY, 0, 0};
        }
    }
    context->UpdateSubresource(maskConstants.Get(), 0, nullptr, path.data(), 0, 0);
    auto *rt = target.rtv.Get();
    context->OMSetRenderTargets(1, &rt, nullptr);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    D3D11_VIEWPORT vp{0, 0, float(width), float(height), 0, 1};
    context->RSSetViewports(1, &vp);
    context->RSSetState(raster.Get());
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(), nullptr, 0);
    context->PSSetShader(shader.shader.Get(), nullptr, 0);
    ID3D11Buffer *cb[] = {constants.Get(), parameters.Get(), maskConstants.Get()};
    context->PSSetConstantBuffers(0, 3, cb);
    auto *sp = sampler.Get();
    context->PSSetSamplers(0, 1, &sp);
    ID3D11ShaderResourceView *sources[] = {input, background, secondInput,
                                           previousPass ? previousPass : input, previousFrame};
    context->PSSetShaderResources(0, 5, sources);
    ID3D11ShaderResourceView *extras[] = {input2, input3};
    context->PSSetShaderResources(7, 2, extras);
    context->Draw(3, 0);
    extras[0] = extras[1] = nullptr;
    context->PSSetShaderResources(7, 2, extras);
    sources[0] = sources[1] = sources[2] = sources[3] = sources[4] = nullptr;
    context->PSSetShaderResources(0, 5, sources);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}
void Renderer::bindGradeLut(const Effect &e) {
    const auto &lut = e.grading["lut"];
    gradeLut.Reset();
    gradeLut1.Reset();
    if (lut.IsNull())
        return;
    int n = lut["size"].I(), dim = lut["dimension"].I();
    uint64_t hash = uint64_t(n) * 11 + dim;
    for (auto &v : lut["data"].Elements())
        hash = (hash ^ std::bit_cast<uint32_t>(v.F())) * 1099511628211ull;
    if (auto it = gradeLuts.find(hash); it != gradeLuts.end()) {
        gradeLut = it->second.first;
        gradeLut1 = it->second.second;
        return;
    }
    std::vector<float> data;
    data.reserve(lut["data"].Size() * 4 / 3);
    for (size_t i = 0; i < lut["data"].Size(); i += 3) {
        for (int k = 0; k < 3; ++k)
            data.push_back(lut["data"].At(i + k).F());
        data.push_back(1);
    }
    D3D11_SUBRESOURCE_DATA upload{data.data(), unsigned(n * 16),
                                  unsigned(n * 16) * (dim == 3 ? unsigned(n) : 1u)};
    if (dim == 3) {
        D3D11_TEXTURE3D_DESC d{};
        d.Width = d.Height = d.Depth = n;
        d.MipLevels = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.Usage = D3D11_USAGE_IMMUTABLE;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Com<ID3D11Texture3D> t;
        check(device->CreateTexture3D(&d, &upload, &t), "Texture LUT 3D");
        check(device->CreateShaderResourceView(t.Get(), nullptr, &gradeLut), "3D LUT view");
    } else {
        D3D11_TEXTURE1D_DESC d{};
        d.Width = n;
        d.ArraySize = d.MipLevels = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.Usage = D3D11_USAGE_IMMUTABLE;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Com<ID3D11Texture1D> t;
        check(device->CreateTexture1D(&d, &upload, &t), "Texture LUT 1D");
        check(device->CreateShaderResourceView(t.Get(), nullptr, &gradeLut1), "1D LUT view");
    }
    if (gradeLuts.size() >= 8)
        gradeLuts.clear();
    gradeLuts[hash] = {gradeLut, gradeLut1};
}
ID3D11ShaderResourceView *Renderer::evaluateComposition(LayerComposition &composition, std::map<Id, Target> &nodes,
                                                        const std::map<Id, Target> &sources, Target &empty,
                                                        const Project &p, double time, bool bypassGrading, Id id) {
    if (!id)
        return empty.srv.Get();
    if (auto it = nodes.find(id); it != nodes.end())
        return it->second.srv.Get();
    auto &n = *composition.graph.Find(id);
    ID3D11ShaderResourceView *inputs[4] = {empty.srv.Get(), empty.srv.Get(), empty.srv.Get(),
                                           empty.srv.Get()};
    std::vector<Target> channels(n.inputs.size());
    for (size_t k = 0; k < n.inputs.size(); ++k) {
        inputs[k] = evaluateComposition(composition, nodes, sources, empty, p, time, bypassGrading, n.inputs[k]);
        auto ch = sx::InputChannel(n, k);
        if (ch) {
            createTarget(channels[k]);
            draw(channels[k],
                 "//@param channel int 1 1 4\nfloat4 Effect(EffectContext ctx){float "
                 "v=Input(0,ctx.px)[(int)channel-1];return float4(v,v,v,1);}",
                 inputs[k], {{"channel", {float(ch)}}}, time, time, width, height);
            inputs[k] = channels[k].srv.Get();
        }
    }
    auto &target = nodes[id];
    createTarget(target);
    if (n.kind == sx::NodeKind::Surface) {
        auto it = sources.find(n.surface);
        draw(target, copy, it == sources.end() ? empty.srv.Get() : it->second.srv.Get(), {},
             time, time, width, height);
    } else if (n.kind == sx::NodeKind::Output || n.kind == sx::NodeKind::Split || n.bypass) {
        draw(target, copy, inputs[0], {}, time, time, width, height);
    } else if (n.kind == sx::NodeKind::Blend) {
        std::string shader = shaderixelComposite;
        auto params = composition.parameters[n.id];
        params["gOpacity"] = params.contains("opacity") ? params.at("opacity") : Parameter{n.opacity};
        params["gMode"] = {float(n.blend)};
        params["gFlags"] = {float(12u | (n.inputs[2] ? 1u : 0u) | (n.clip ? 16u : 0u) |
                                  (n.blendIf.Active() ? 32u : 0u) | (unsigned(n.channelOff) << 8))};
        params["sfSrgb"] = {p.workingSpace == "srgb" ? 1.f : 0.f};
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k) {
                params["sfSelf" + std::to_string(c) + std::to_string(k)] = {n.blendIf.self[c][k] /
                                                                            255.f};
                params["sfUnder" + std::to_string(c) + std::to_string(k)] = {n.blendIf.under[c][k] /
                                                                             255.f};
            }
        draw(target, shader, inputs[0], params, time, time, width, height, nullptr, nullptr, inputs[1], 0, 0,
             nullptr, nullptr, inputs[2]);
    } else {
        auto &e = composition.effects.at(n.id);
        if (!e.enabled || (bypassGrading && e.id == "grading"))
            draw(target, copy, inputs[0], {}, time, time, width, height);
        else if (e.id == "grading") {
            bindGradeLut(e);
            ID3D11ShaderResourceView *luts[] = {gradeLut.Get(), gradeLut1.Get()};
            context->PSSetShaderResources(5, 2, luts);
            draw(target, gradeShader(e, p.workingSpace), inputs[0], e.params, time, time, width, height);
            luts[0] = luts[1] = nullptr;
            context->PSSetShaderResources(5, 2, luts);
        } else {
            auto code = effectCode(e);
            context->CopyResource(originalInput.texture.Get(), target.texture.Get());
            // An unconnected side input is absent (HasInput false), so an optional mask reads white.
            auto linked = [&](size_t k) { return k < n.inputs.size() && n.inputs[k] ? inputs[k] : nullptr; };
            for (int pass = 0; pass < effectPasses(code); ++pass) {
                draw(working[0], code, inputs[0], e.params, time, time, width, height, nullptr, nullptr,
                     linked(1), e.params.contains("progress") ? e.params.at("progress").at(time) : 0,
                     unsigned(pass), pass ? target.srv.Get() : inputs[0], nullptr, linked(2),
                     linked(3));
                context->CopyResource(target.texture.Get(), working[0].texture.Get());
            }
        }
    }
    return target.srv.Get();
}
void Renderer::render(const Project &p, MediaCache &m, double time, unsigned w, unsigned h,
                      bool bypassGrading, Id matteClip, int matteEffect) {
    capturedGradeInput = {};
    auto frame = int64_t(std::floor(time * p.fps + 1e-5)),
         previousFrame = int64_t(std::floor(previousTime * p.fps + 1e-5));
    bool sequential = previousTime >= 0 && frame == previousFrame + 1;
    if (previousTime < 0 || (frame != previousFrame && !sequential))
        history.clear();
    if (sequential)
        for (auto &[id, saved] : history) {
            std::swap(saved.previous, saved.current);
            saved.valid = bool(saved.previous.texture);
        }
    fps = p.fps;
    if (currentSpace != p.workingSpace)
        history.clear();
    shaderTimeOrigin = p.timeOrigin;
    currentSpace = p.workingSpace;
    currentTone = p.toneMap;
    currentExposure = p.exposure;
    titles.retain(p);
    if (w != width || h != height) {
        compositionNodes.clear();
        width = w;
        height = h;
        createTarget(composite);
        createTarget(alternateComposite);
        createTarget(transitionOutput);
        createTarget(working[0]);
        createTarget(working[1]);
        createTarget(originalInput);
        createTarget(display, DXGI_FORMAT_B8G8R8A8_UNORM);
        history.clear();
    }
    const float black[] = {0, 0, 0, p.backgroundAlpha};
    context->ClearRenderTargetView(composite.rtv.Get(), black);
    const bool nodal = true;
    auto compositionDocument = p.layerGraph.IsNull() ? defaultLayerGraph(p) : p.layerGraph;
    compositionNodes.clear();
    trackNodes.clear();
    std::map<Id, Target> timelineSources;
    Target graphBackground, automaticBelow;
    if (nodal) {
        auto decoded = decodeLayerGraph(compositionDocument, p);
        size_t trackGraphNodes = 0;
        for (auto &t : p.tracks)
            if (!t.graph.IsNull())
                trackGraphNodes += t.graph["nodes"].Size();
        if (uint64_t(w) * h * 8 * (decoded.graph.nodes.size() + trackGraphNodes + p.tracks.size() + 4) >
            512ull * 1024 * 1024)
            throw std::runtime_error("Video graph: 512 MiB GPU budget exceeded");
        createTarget(graphBackground);
        context->CopyResource(graphBackground.texture.Get(), composite.texture.Get());
        createTarget(automaticBelow);
        context->CopyResource(automaticBelow.texture.Get(), composite.texture.Get());
    }
    for (auto ti = p.tracks.rbegin(); ti != p.tracks.rend(); ++ti) {
        if (ti->kind != TrackKind::Video || !p.audible(*ti))
            continue;
        if (nodal) {
            const float clear[] = {0, 0, 0, 0};
            context->ClearRenderTargetView(composite.rtv.Get(), clear);
        }
        std::vector<const Clip *> ordered;
        for (auto &clip : ti->clips)
            ordered.push_back(&clip);
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const Clip *a, const Clip *b) { return a->start < b->start; });
        for (auto *item : ordered) {
            auto &c = *item;
            if (!c.live(time))
                continue;
            WidgetRect layout;
            bool widget = !c.widget.IsNull();
            if (widget) {
                layout = widgetRect(p, c, time);
                if (!layout.visible)
                    continue;
            }
            Com<ID3D11ShaderResourceView> source;
            unsigned sw = w, sh = h;
            if (c.adjustment) {
                // Adjustment layer: the picture of the tracks below (automatic layer graph) runs through its effects.
                if (!p.layerGraph.IsNull())
                    continue;
                source = automaticBelow.srv;
            } else if (!c.composition.IsNull()) {
                auto &entry = nested[c.id];
                auto identity = c.composition.Dump(0);
                if (!entry.renderer) {
                    entry.renderer = std::make_unique<Renderer>();
                    entry.renderer->initialize(device.Get(), context.Get());
                }
                if (entry.identity != identity) {
                    entry.project = Project::fromJson(c.composition);
                    entry.identity = identity;
                    entry.renderer->clearHistory();
                }
                entry.project.workingSpace = p.workingSpace;
                entry.renderer->library = library;
                sw = std::max(1u, unsigned(w));
                sh = std::max(1u, unsigned(double(sw) * entry.project.height / entry.project.width));
                entry.renderer->render(entry.project, m, c.sourceTime(time), sw, sh, bypassGrading);
                source = entry.renderer->composite.srv;
            } else if (!c.generator) {
                auto &image =
                    c.title.enabled
                        ? titles.at(c, time,
                                    widget ? std::clamp(unsigned(layout.width * w / p.width), 1u, 8192u) : w,
                                    widget ? std::clamp(unsigned(layout.height * h / p.height), 1u, 8192u)
                                           : h,
                                    widget ? std::max(1, int(layout.height)) : p.height)
                        : m.frame(c.source, c.sourceTime(time));
                if (image.pixels.empty())
                    continue;
                sw = image.width;
                sh = image.height;
                D3D11_TEXTURE2D_DESC d{};
                d.Width = sw;
                d.Height = sh;
                d.MipLevels = d.ArraySize = 1;
                d.SampleDesc.Count = 1;
                d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                d.Usage = D3D11_USAGE_IMMUTABLE;
                d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                std::vector<float> pixels(size_t(sw) * sh * 4);
                for (size_t n = 0; n < pixels.size(); n += 4) {
                    float a = image.rgba.empty() ? image.pixels[n + 3] / 255.f : image.rgba[n + 3];
                    ColorRGB rgb = image.rgba.empty()
                                       ? ColorRGB{image.pixels[n + 2] / 255.f, image.pixels[n + 1] / 255.f,
                                                  image.pixels[n] / 255.f}
                                       : ColorRGB{image.rgba[n], image.rgba[n + 1], image.rgba[n + 2]};
                    if (c.sourcePremultiplied && !c.title.enabled)
                        rgb = a > 0 ? rgb / a : ColorRGB{};
                    rgb = convertColor(
                        rgb, c.title.enabled || c.sourceColor == "auto" ? image.colorSpace : c.sourceColor,
                        p.workingSpace);
                    pixels[n] = rgb.x * a;
                    pixels[n + 1] = rgb.y * a;
                    pixels[n + 2] = rgb.z * a;
                    pixels[n + 3] = a;
                }
                D3D11_SUBRESOURCE_DATA data{pixels.data(), sw * 16, 0};
                Com<ID3D11Texture2D> texture;
                check(device->CreateTexture2D(&d, &data, &texture), "Video upload");
                device->CreateShaderResourceView(texture.Get(), nullptr, &source);
            }
            std::string sourceCopy = copy;
            if (widget) {
                auto ratio = (layout.width / layout.height) / (float(sw) / sh);
                sourceCopy = "float4 Effect(EffectContext ctx){float2 uv=(ctx.uv-.5)*float2(" +
                             std::to_string(std::max(1.f, ratio)) + "," +
                             std::to_string(std::max(1.f, 1 / ratio)) +
                             ")+.5;if(any(uv<0)||any(uv>1))return 0;return InputUV(0,uv);}";
            }
            draw(working[0], c.generator && c.composition.IsNull() ? generator : sourceCopy, source.Get(), {},
                 time, c.sourceTime(time), sw, sh);
            int current = 0;
            unsigned effectIndex = 0;
            for (auto &e : c.effects) {
                unsigned index = effectIndex++;
                if (c.id == matteClip && matteEffect >= 0 && int(index) > matteEffect)
                    continue;
                if (!e.enabled)
                    continue;
                if (e.id == "grading") {
                    if (c.id == captureGradeClip && int(index) == captureGradeEffect)
                        capturedGradeInput = readTarget(working[current]);
                    if (bypassGrading)
                        continue;
                    bindGradeLut(e);
                    ID3D11ShaderResourceView *luts[] = {gradeLut.Get(), gradeLut1.Get()};
                    context->PSSetShaderResources(5, 2, luts);
                    auto values = e.params;
                    values["matte"] = {c.id == matteClip && int(index) == matteEffect ? 1.f : 0.f};
                    draw(working[1 - current], gradeShader(e, p.workingSpace), working[current].srv.Get(),
                         values, time, c.sourceTime(time), w, h);
                    current = 1 - current;
                    luts[0] = luts[1] = nullptr;
                    context->PSSetShaderResources(5, 2, luts);
                    continue;
                }
                if (e.id == "ofx") {
                    auto identity = e.code + "|" + e.graph["identifier"].S();
                    auto key = std::make_pair(c.id, index);
                    if (!ofx.contains(key) && ofx.size() >= 16)
                        throw std::runtime_error("16 OFX instances maximum per render");
                    auto &entry = ofx[key];
                    if (entry.identity != identity || entry.width != w || entry.height != h) {
                        entry.process =
                            std::make_unique<IsolatedOfx>(e.code, e.graph["identifier"].S(), w, h);
                        entry.identity = identity;
                        entry.width = w;
                        entry.height = h;
                    }
                    auto image = readTarget(working[current]);
                    image.colorSpace = p.workingSpace;
                    auto output = entry.process->render(image, e, time, p.fps, p.duration);
                    writeTarget(working[current], output);
                    continue;
                }
                std::string code = effectCode(e);
                if (code.empty()) {
                    auto *d = library.find(e.id);
                    if (!d)
                        throw std::runtime_error("Missing effect: " + e.id);
                    code = d->code;
                }
                context->CopyResource(originalInput.texture.Get(), working[current].texture.Get());
                auto old = history.find(c.id);
                // A blend shader reads the picture below as its background (input 0) and the clip as
                // its layer (input 1); other effects get the clip first and the picture below second.
                auto *below = p.layerGraph.IsNull() ? automaticBelow.srv.Get() : composite.srv.Get();
                bool blendShader = code.find("//@blend") != std::string::npos;
                for (int passIndex = 0; passIndex < effectPasses(code); ++passIndex) {
                    draw(working[1 - current], code, blendShader ? below : originalInput.srv.Get(), e.params, time,
                         c.sourceTime(time), w, h, nullptr, nullptr,
                         blendShader ? originalInput.srv.Get() : below, 0,
                         unsigned(passIndex), working[current].srv.Get(),
                         old == history.end() || !old->second.valid ? nullptr
                                                                    : old->second.previous.srv.Get());
                    current = 1 - current;
                }
            }
            // Retain only clips explicitly requesting temporal history, bounded to 16 full-size textures.
            bool temporal = false;
            for (auto &effect : c.effects)
                if (effect.enabled && effect.id != "ofx" && effect.id != "grading" &&
                    effectCode(effect).find("History") != std::string::npos)
                    temporal = true;
            if (temporal) {
                if (!history.contains(c.id) &&
                    (history.size() >= 16 || uint64_t(w) * h * 16 * (history.size() + 1) > 256 * 1024 * 1024))
                    throw std::runtime_error("Video history: 16 layers or 256 MiB maximum");
                auto &saved = history[c.id];
                if (!saved.current.texture)
                    createTarget(saved.current);
                context->CopyResource(saved.current.texture.Get(), working[current].texture.Get());
            }
            auto compositionParams = c.params;
            if (widget) {
                compositionParams["widget_enabled"] = {1};
                float rect[] = {layout.x / p.width, layout.y / p.height, layout.width / p.width,
                                layout.height / p.height};
                float bounds[] = {layout.left / p.width, layout.top / p.height, layout.right / p.width,
                                  layout.bottom / p.height};
                for (int k = 0; k < 4; ++k) {
                    compositionParams["widget_rect." + std::to_string(k)] = {rect[k]};
                    compositionParams["widget_clip." + std::to_string(k)] = {bounds[k]};
                }
            }
            if (p.layerGraph.IsNull())
                compositionParams["blend"] = {0};
            compositionParams["mask_mode"] = {float(c.mask.shape)};
            compositionParams["mask_invert"] = {c.mask.inverted ? 1.f : 0.f};
            compositionParams["mask_pixel_scale"] = {1.f / p.height};
            draw(alternateComposite, compose, working[current].srv.Get(), compositionParams, time,
                 c.sourceTime(time), w, h, &c, composite.srv.Get());
            if (c.transition.enabled) {
                draw(transitionOutput, library.transitionCode(c.transition) + "\n//@premultiplied\n",
                     alternateComposite.srv.Get(), c.params, time, time - c.start, w, h, nullptr, nullptr,
                     composite.srv.Get(), c.transitionProgress(time));
                std::swap(composite, transitionOutput);
            } else
                std::swap(composite, alternateComposite);
        }
        if (nodal) {
            auto &source = timelineSources[ti->id];
            createTarget(source);
            context->CopyResource(source.texture.Get(), composite.texture.Get());
            // The track's own effect graph: its picture (and tracks below, already rendered) in, the
            // processed picture out, before it is blended with the tracks below.
            if (!ti->graph.IsNull()) {
                auto trackGraph = decodeTrackGraph(ti->graph, p, ti->id);
                Target none;
                createTarget(none);
                const float clearNone[] = {0, 0, 0, 0};
                context->ClearRenderTargetView(none.rtv.Get(), clearNone);
                auto &nodes = trackNodes[ti->id];
                auto *processed = evaluateComposition(trackGraph, nodes, timelineSources, none, p, time,
                                                      bypassGrading, trackGraph.graph.output);
                Target result;
                createTarget(result);
                draw(result, copy, processed, {}, time, time, w, h);
                context->CopyResource(source.texture.Get(), result.texture.Get());
            }
            if (p.layerGraph.IsNull()) {
                Clip mix;
                for (auto &clip : ti->clips)
                    if (clip.live(time))
                        mix.params["blend"] = {clip.param("blend", time, 0)};
                draw(alternateComposite, compose, source.srv.Get(), {}, time, time, w, h, &mix,
                     automaticBelow.srv.Get());
                context->CopyResource(automaticBelow.texture.Get(), alternateComposite.texture.Get());
            }
        }
    }
    if (nodal) {
        auto composition = decodeLayerGraph(compositionDocument, p);
        if (p.layerGraph.IsNull())
            for (auto &node : composition.graph.nodes)
                if (node.kind == sx::NodeKind::Blend) {
                    auto *source = composition.graph.Find(node.inputs[1]);
                    auto track = std::find_if(p.tracks.begin(), p.tracks.end(),
                                              [&](auto &t) { return t.id == source->surface; });
                    // A track's own blend mode wins over the clips' quick blend.
                    if (track != p.tracks.end() && track->blend == 0)
                        for (auto &clip : track->clips)
                            if (clip.live(time)) {
                                int mode =
                                    int(std::clamp(std::round(clip.param("blend", time, 0)), 0.f, 3.f));
                                node.blend = sx::BlendMode(mode == 1 ? 4 : mode == 2 ? 1 : mode == 3 ? 2 : 0);
                            }
                }
        Target empty;
        createTarget(empty);
        const float clear[] = {0, 0, 0, 0};
        context->ClearRenderTargetView(empty.rtv.Get(), clear);
        if (p.layerGraph.IsNull())
            context->CopyResource(empty.texture.Get(), graphBackground.texture.Get());
        auto output = evaluateComposition(composition, compositionNodes, timelineSources, empty, p, time,
                                          bypassGrading, composition.graph.output);
        draw(composite, compose, output, {}, time, time, w, h, nullptr, graphBackground.srv.Get());
    }

    std::erase_if(history, [&](const auto &entry) {
        auto *clip = p.findClip(entry.first);
        return !clip || !clip->live(time);
    });
    std::erase_if(nested, [&](const auto &entry) {
        auto *clip = p.findClip(entry.first);
        return !clip || !clip->live(time) || clip->composition.IsNull();
    });
    previousTime = time;
    std::erase_if(ofx, [&](const auto &item) {
        auto *c = p.findClip(item.first.first);
        return !c || !c->live(time) || item.first.second >= c->effects.size() ||
               c->effects[item.first.second].id != "ofx" || !c->effects[item.first.second].enabled;
    });
    draw(display, displayCode, composite.srv.Get(),
         {{"space",
           {float(p.workingSpace == "srgb"        ? 0
                  : p.workingSpace == "linear709" ? 1
                                                  : 2)}},
          {"tone",
           {float(p.toneMap == "clip"       ? 0
                  : p.toneMap == "reinhard" ? 1
                                            : 2)}},
          {"exposure", {p.exposure}}},
         time, 0, w, h);
}
Image Renderer::readback() {
    Image i;
    i.width = width;
    i.height = height;
    i.pixels.resize(size_t(width) * height * 4);
    D3D11_TEXTURE2D_DESC d;
    display.texture->GetDesc(&d);
    d.BindFlags = 0;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Com<ID3D11Texture2D> stage;
    check(device->CreateTexture2D(&d, nullptr, &stage), "GPU readback");
    context->CopyResource(stage.Get(), display.texture.Get());
    D3D11_MAPPED_SUBRESOURCE data;
    check(context->Map(stage.Get(), 0, D3D11_MAP_READ, 0, &data), "Mapping GPU");
    for (unsigned y = 0; y < height; ++y)
        memcpy(i.pixels.data() + size_t(y) * width * 4,
               static_cast<BYTE *>(data.pData) + size_t(y) * data.RowPitch, width * 4);
    context->Unmap(stage.Get(), 0);
    return i;
}
FloatImage Renderer::readTarget(const Target &target) {
    FloatImage image{width, height, std::vector<float>(size_t(width) * height * 4), currentSpace};
    D3D11_TEXTURE2D_DESC d{};
    target.texture->GetDesc(&d);
    d.BindFlags = 0;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Com<ID3D11Texture2D> stage;
    check(device->CreateTexture2D(&d, nullptr, &stage), "GPU16F readback");
    context->CopyResource(stage.Get(), target.texture.Get());
    D3D11_MAPPED_SUBRESOURCE data{};
    check(context->Map(stage.Get(), 0, D3D11_MAP_READ, 0, &data), "Mapping GPU16F");
    for (unsigned y = 0; y < height; ++y) {
        auto *row = reinterpret_cast<const uint16_t *>(static_cast<const BYTE *>(data.pData) +
                                                       size_t(y) * data.RowPitch);
        for (unsigned x = 0; x < width * 4; ++x)
            image.rgba[size_t(y) * width * 4 + x] = DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
    }
    context->Unmap(stage.Get(), 0);
    for (size_t n = 0; n < image.rgba.size(); n += 4) {
        float a = image.rgba[n + 3];
        for (int k = 0; k < 3; ++k)
            image.rgba[n + k] = a > 0 ? image.rgba[n + k] / a : 0;
    }
    return image;
}
void Renderer::writeTarget(Target &target, const FloatImage &image) {
    std::vector<uint16_t> pixels(image.rgba.size());
    for (size_t n = 0; n < pixels.size(); ++n)
        pixels[n] = DirectX::PackedVector::XMConvertFloatToHalf(image.rgba[n] *
                                                                (n % 4 == 3 ? 1 : image.rgba[n - n % 4 + 3]));
    context->UpdateSubresource(target.texture.Get(), 0, nullptr, pixels.data(), image.width * 8, 0);
}
FloatImage Renderer::readbackFloat(bool forDisplay) {
    auto image = readTarget(composite);
    if (forDisplay) {
        for (size_t n = 0; n < image.rgba.size(); n += 4) {
            auto rgb = displayColor({image.rgba[n], image.rgba[n + 1], image.rgba[n + 2]}, currentSpace,
                                    currentTone, currentExposure);
            image.rgba[n] = rgb.x;
            image.rgba[n + 1] = rgb.y;
            image.rgba[n + 2] = rgb.z;
        }
        image.colorSpace = "srgb";
    }
    return image;
}
} // namespace sf

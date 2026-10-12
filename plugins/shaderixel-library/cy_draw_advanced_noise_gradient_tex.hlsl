#define select(c,a,b) ((c)?(a):(b))
float Luma(float3 c){return dot(c,float3(.2126,.7152,.0722));}
float SrgbToLinear1(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float LinearToSrgb1(float c) { c = max(c, 0.0); return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
float3 SrgbToLinear(float3 c) { return float3(SrgbToLinear1(c.r), SrgbToLinear1(c.g), SrgbToLinear1(c.b)); }
float3 LinearToSrgb(float3 c) { return float3(LinearToSrgb1(c.r), LinearToSrgb1(c.g), LinearToSrgb1(c.b)); }

float3 RGBToHSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 HSVToRGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}
//@name Advanced Noise Gradient Tex
//@category Dessin
//@generator
//@halo 0
//@param sx_Scale float 100.0 0.0 200.0 "Scale"
//@param sx_Offset float 1.0 0.0 2.0 "Offset"
//@param sx_Mix float 1.0 0.0 2.0 "Mix"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedNoiseGradientTex.M_CyDraw_AdvancedNoiseGradientTex: Native CyTexture advanced Draw preset with live GPU parameters.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }  // unconnected = white, as Unreal
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
#define Texture2DSampleLevel(T, S, UVS, LOD) CyTex(T, UVS)
static EffectContext ctx;  // for helper functions (noise) reading the document size
// Unreal view values and helpers used by Custom nodes (a document pixel = a screen pixel).
struct SXView { float4 ViewSizeAndInvSize; float4 BufferSizeAndInvSize; float4 TemporalAAParams; };
static SXView View;
static SXView ResolvedView;
float2 ViewportUVToBufferUV(float2 uv) { return uv; }
float2 BufferUVToViewportUV(float2 uv) { return uv; }
float4 SXRet(float v) { return float4(v, v, v, 1.0); }
float4 SXRet(float2 v) { return float4(v, 0.0, 1.0); }
float4 SXRet(float3 v) { return float4(v, 1.0); }
float4 SXRet(float4 v) { return v; }

float Noise_3_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_3_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_3_h(i), n100 = Noise_3_h(i + float3(1, 0, 0)), n010 = Noise_3_h(i + float3(0, 1, 0)), n110 = Noise_3_h(i + float3(1, 1, 0));
    float n001 = Noise_3_h(i + float3(0, 0, 1)), n101 = Noise_3_h(i + float3(1, 0, 1)), n011 = Noise_3_h(i + float3(0, 1, 1)), n111 = Noise_3_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_3(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_3_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_4_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_4_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_4_h(i), n100 = Noise_4_h(i + float3(1, 0, 0)), n010 = Noise_4_h(i + float3(0, 1, 0)), n110 = Noise_4_h(i + float3(1, 1, 0));
    float n001 = Noise_4_h(i + float3(0, 0, 1)), n101 = Noise_4_h(i + float3(1, 0, 1)), n011 = Noise_4_h(i + float3(0, 1, 1)), n111 = Noise_4_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_4(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 10; ++l)
    {
        float v = Noise_4_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_n4_o = (UV * ((float2)(sx_Scale)));
    float v_f2_n2_o = (float4(v_n4_o, 0.0, 1.0)).x;
    float2 v_f1_n5_o = float2(v_f2_n2_o, v_f2_n2_o);
    float3 v_f1_n4_o = float3(v_f1_n5_o, sx_Offset);
    float v_n1_o = lerp(-1.0, 1.0, saturate(Noise_3(v_f1_n4_o * 0.1) * 0.5 + 0.5));
    float v_n9_o = lerp(-1.0, 1.0, saturate(Noise_4(v_f1_n4_o * 0.2) * 0.5 + 0.5));
    float v_n0_o = lerp(v_n1_o, v_n9_o, sx_Mix);
    float v_n13_o = (((float4)(v_n0_o))).x;
    float v_n14_o = (((float4)(v_n0_o))).y;
    float v_n12_o = max(v_n13_o, v_n14_o);
    float v_n15_o = (((float4)(v_n0_o))).z;
    float v_n11_o = max(v_n12_o, v_n15_o);
    return float4(((float3)(v_n0_o)), v_n11_o);
}

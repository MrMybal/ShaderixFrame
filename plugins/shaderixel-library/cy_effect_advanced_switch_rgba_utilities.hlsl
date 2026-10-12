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
//@name Advanced Switch RGBA Utilities
//@category Traitements
//@input Image
//@halo 0
//@param sx_R___R float 1.0 0.0 2.0 "R - R"
//@param sx_R___G float 0.0 0.0 1.0 "R - G"
//@param sx_R___B float 0.0 0.0 1.0 "R - B"
//@param sx_R___A float 0.0 0.0 1.0 "R - A"
//@param sx_R___Mul float 1.0 0.0 2.0 "R - Mul"
//@param sx_G___R float 0.0 0.0 1.0 "G - R"
//@param sx_G___G float 1.0 0.0 2.0 "G - G"
//@param sx_G___B float 0.0 0.0 1.0 "G - B"
//@param sx_G___A float 0.0 0.0 1.0 "G - A"
//@param sx_G___Mul float 1.0 0.0 2.0 "G - Mul"
//@param sx_B___R float 0.0 0.0 1.0 "B - R"
//@param sx_B___G float 0.0 0.0 1.0 "B - G"
//@param sx_B___B float 1.0 0.0 2.0 "B - B"
//@param sx_B___A float 0.0 0.0 1.0 "B - A"
//@param sx_B___Mul float 1.0 0.0 2.0 "B - Mul"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSwitchRGBAUtilities.M_CyEffect_AdvancedSwitchRGBAUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float4 b_n6 = CyTex(0, UV);
    float v_f2_n2_o = (b_n6).x;
    float v_n4_o = (v_f2_n2_o * sx_R___R);
    float v_f3_n4_o = (b_n6).y;
    float v_n8_o = (v_f3_n4_o * sx_R___G);
    float v_n3_o = (v_n4_o + v_n8_o);
    float v_f4_n6_o = (b_n6).z;
    float v_n11_o = (v_f4_n6_o * sx_R___B);
    float v_f5_n8_o = (b_n6).w;
    float v_n13_o = (v_f5_n8_o * sx_R___A);
    float v_n10_o = (v_n11_o + v_n13_o);
    float v_n2_o = (v_n3_o + v_n10_o);
    float v_n1_o = (v_n2_o * sx_R___Mul);
    float v_f6_n2_o = (b_n6).x;
    float v_n19_o = (v_f6_n2_o * sx_G___R);
    float v_f7_n4_o = (b_n6).y;
    float v_n22_o = (v_f7_n4_o * sx_G___G);
    float v_n18_o = (v_n19_o + v_n22_o);
    float v_f8_n6_o = (b_n6).z;
    float v_n25_o = (v_f8_n6_o * sx_G___B);
    float v_f9_n8_o = (b_n6).w;
    float v_n27_o = (v_f9_n8_o * sx_G___A);
    float v_n24_o = (v_n25_o + v_n27_o);
    float v_n17_o = (v_n18_o + v_n24_o);
    float v_n16_o = (v_n17_o * sx_G___Mul);
    float2 v_f1_n5_o = float2(v_n1_o, v_n16_o);
    float v_f10_n2_o = (b_n6).x;
    float v_n33_o = (v_f10_n2_o * sx_B___R);
    float v_f11_n4_o = (b_n6).y;
    float v_n36_o = (v_f11_n4_o * sx_B___G);
    float v_n32_o = (v_n33_o + v_n36_o);
    float v_f12_n6_o = (b_n6).z;
    float v_n39_o = (v_f12_n6_o * sx_B___B);
    float v_f13_n8_o = (b_n6).w;
    float v_n41_o = (v_f13_n8_o * sx_B___A);
    float v_n38_o = (v_n39_o + v_n41_o);
    float v_n31_o = (v_n32_o + v_n38_o);
    float v_n30_o = (v_n31_o * sx_B___Mul);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_n30_o);
    float4 b_n45 = CyTex(0, UV);
    float v_n44_o = (b_n45).w;
    return float4(v_f1_n4_o, v_n44_o);
}

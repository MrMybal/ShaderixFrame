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
    float3 p3 = frac(((float3)(p.xyx)) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash22(float2 p)
{
    float3 p3 = frac(((float3)(p.xyx)) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}
//@name Advanced Gradient Color
//@category Dessin
//@generator
//@halo 0
//@param sx_A color 1.0 0.0 0.0 0.0 "A"
//@param sx_A_Mul float 1.0 0.0 2.0 "A Mul"
//@param sx_B color 0.0 1.0 0.0 0.0 "B"
//@param sx_B_Mul float 1.0 0.0 2.0 "B Mul"
//@param sx_C color 0.0 0.0 1.0 0.0 "C"
//@param sx_C_Mul float 1.0 0.0 2.0 "C Mul"
//@param sx_Power float 1.0 0.0 2.0 "Power"
//@param sx_Mul float 1.0 0.0 2.0 "Mul"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedGradientColor.M_CyDraw_AdvancedGradientColor: Native CyTexture advanced Draw preset with live GPU parameters.
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
    float3 v_n4_o = (sx_A).xyz;
    float3 v_n3_o = (v_n4_o * ((float3)(sx_A_Mul)));
    float3 v_n7_o = (sx_B).xyz;
    float3 v_n6_o = (v_n7_o * ((float3)(sx_B_Mul)));
    float v_f2_n2_o = (float4(UV, 0.0, 1.0)).x;
    float v_f1_n10_o = (v_f2_n2_o * 2.0);
    float v_f1_n9_o = clamp(v_f1_n10_o, 0.0, 1.0);
    float3 v_f1_n8_o = lerp(v_n3_o, v_n6_o, v_f1_n9_o);
    float3 v_n10_o = (sx_C).xyz;
    float3 v_n9_o = (v_n10_o * ((float3)(sx_C_Mul)));
    float v_f1_n12_o = (v_f1_n10_o - 1.0);
    float v_f1_n11_o = clamp(v_f1_n12_o, 0.0, 1.0);
    float3 v_f1_n7_o = lerp(v_f1_n8_o, v_n9_o, v_f1_n11_o);
    float3 v_n1_o = pow(max(v_f1_n7_o, 0.0), ((float3)(sx_Power)));
    float3 v_n0_o = (v_n1_o * ((float3)(sx_Mul)));
    float v_n18_o = (float4(v_n0_o, 1.0)).x;
    float v_n19_o = (float4(v_n0_o, 1.0)).y;
    float v_n17_o = max(v_n18_o, v_n19_o);
    float v_n20_o = (float4(v_n0_o, 1.0)).z;
    float v_n16_o = max(v_n17_o, v_n20_o);
    return float4(v_n0_o, v_n16_o);
}

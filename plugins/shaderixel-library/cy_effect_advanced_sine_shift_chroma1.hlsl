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
//@name Advanced Sine Shift Chroma1
//@category Traitements
//@input Image
//@halo full
//@param sx_Sin float 1.0 0.0 2.0 "Sin"
//@param sx_SinPower float 1.0 0.0 2.0 "Sin Power"
//@param sx_Radius float 0.5 0.0 1.0 "Radius"
//@param sx_Density float 1.0 0.0 2.0 "Density"
//@param sx_Power float 1.0 0.0 2.0 "Power"
//@param sx_Lerp float 0.5 0.0 1.0 "Lerp"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSineShiftChroma1.M_CyEffect_AdvancedSineShiftChroma1: Native CyTexture advanced Effect preset with live GPU parameters.
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

float3 RotateAboutAxis_4(float3 axis, float angle, float3 pivot, float3 pos)
{
    float3 c = pivot + axis * dot(axis, pos - pivot);
    float3 u = pos - c, v = cross(axis, u);
    float s, co;
    sincos(angle * 6.283185, s, co);
    return c + u * co + v * s - pos;
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float3 v_f1_n7_o = float3(1.0, 1.0, 1.0);
    float3 v_f1_n6_o = normalize(v_f1_n7_o);
    float2 v_f3_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f3_n3_o = (v_f3_n4_o - ((float2)(1.0)));
    float2 v_f3_n19_o = (v_f3_n3_o * v_f3_n3_o);
    float v_f3_n18_o = (float4(v_f3_n19_o, 0.0, 1.0)).x;
    float v_f3_n20_o = (float4(v_f3_n19_o, 0.0, 1.0)).y;
    float v_f3_n17_o = (v_f3_n18_o + v_f3_n20_o);
    float v_f3_n16_o = sqrt(v_f3_n17_o);
    float v_f2_n2_o = (((float4)(v_f3_n16_o))).x;
    float v_n7_o = (v_f2_n2_o * sx_Sin);
    float v_n6_o = (1.0 - v_n7_o);
    float v_n5_o = pow(max(v_n6_o, 0.0), sx_SinPower);
    float3 v_f1_n1_o = float3(0.0, 1.0, 0.0);
    float3 v_f1_n5_o = RotateAboutAxis_4(v_f1_n6_o, v_n5_o, ((float3)(0.0)), v_f1_n1_o);
    float3 v_f1_n4_o = (v_f1_n5_o + v_f1_n1_o);
    float2 v_f5_n0_o = float2(0.5, 0.5);
    float v_f5_n12_o = distance(UV, v_f5_n0_o);
    float v_f5_n11_o = (v_f5_n12_o * 1.0);
    float v_f5_n10_o = (v_f5_n11_o / sx_Radius);
    float v_f5_f6_n13_o = (v_f5_n10_o * sx_Density);
    float v_f5_f6_n12_o = (v_f5_f6_n13_o * v_f5_f6_n13_o);
    float v_f5_f6_n11_o = ((1.0 > 0.5) ? v_f5_f6_n12_o : v_f5_f6_n13_o);
    float v_f5_f6_n9_o = pow(max(2.718282, 0.0), v_f5_f6_n11_o);
    float v_f5_f6_n7_o = (1.0 / v_f5_f6_n9_o);
    float v_f5_f6_n5_o = ((v_f5_n10_o == 0.0) ? 1.0 : ((v_f5_n10_o > 0.0) ? v_f5_f6_n7_o : 1.0));
    float v_f5_n15_o = (1.0 - v_f5_n10_o);
    float v_f5_f7_n13_o = (v_f5_n15_o * sx_Density);
    float v_f5_f7_n12_o = (v_f5_f7_n13_o * v_f5_f7_n13_o);
    float v_f5_f7_n11_o = ((1.0 > 0.5) ? v_f5_f7_n12_o : v_f5_f7_n13_o);
    float v_f5_f7_n9_o = pow(max(2.718282, 0.0), v_f5_f7_n11_o);
    float v_f5_f7_n7_o = (1.0 / v_f5_f7_n9_o);
    float v_f5_f7_n5_o = ((v_f5_n15_o == 0.0) ? 1.0 : ((v_f5_n15_o > 0.0) ? v_f5_f7_n7_o : 1.0));
    float v_f5_n13_o = (1.0 - v_f5_f7_n5_o);
    float v_f5_n8_o = ((0.0 > 0.5) ? v_f5_f6_n5_o : v_f5_n13_o);
    float v_n12_o = pow(max(v_f5_n8_o, 0.0), sx_Power);
    float3 v_n3_o = lerp(((float3)(1.0)), v_f1_n4_o, v_n12_o);
    float3 v_n1_o = lerp(((float3)(1.0)), v_n3_o, sx_Lerp);
    float4 b_n18 = CyTex(0, UV);
    float3 v_n18_RGB = (b_n18).xyz;
    float3 v_n0_o = (v_n1_o * v_n18_RGB);
    float4 b_n21 = CyTex(0, UV);
    float v_n20_o = (b_n21).w;
    return float4(v_n0_o, v_n20_o);
}

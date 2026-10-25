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
//@name Advanced Rad Grad Distortion2
//@category Traitements
//@input Image
//@halo full
//@param sx_Intensity float 0.0 0.0 1.0 "Intensity"
//@param sx_Radius float 0.0 0.0 1.0 "Radius"
//@param sx_Density float 0.0 0.0 1.0 "Density"
//@param sx_Invert float 0.0 0.0 1.0 "Invert"
//@param sx_Power float 0.0 0.0 1.0 "Power"
//@param sx_Curve float 0.0 0.0 1.0 "Curve"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRadGradDistortion2.M_CyEffect_AdvancedRadGradDistortion2: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float2 v_n2_o = (float4(UV, 0.0, 1.0)).xy;
    float v_n8_o = -0.5;
    float2 v_n7_o = (UV + ((float2)(v_n8_o)));
    float v_f2_n12_o = distance(v_n7_o, ((float2)(0.0)));
    float v_f2_n11_o = (v_f2_n12_o * 1.0);
    float v_f2_n10_o = (v_f2_n11_o / sx_Radius);
    float v_f2_f3_n13_o = (v_f2_n10_o * sx_Density);
    float v_f2_f3_n12_o = (v_f2_f3_n13_o * v_f2_f3_n13_o);
    float v_f2_f3_n11_o = ((1.0 > 0.5) ? v_f2_f3_n12_o : v_f2_f3_n13_o);
    float v_f2_f3_n9_o = pow(max(2.718282, 0.0), v_f2_f3_n11_o);
    float v_f2_f3_n7_o = (1.0 / v_f2_f3_n9_o);
    float v_f2_f3_n5_o = ((v_f2_n10_o == 0.0) ? 1.0 : ((v_f2_n10_o > 0.0) ? v_f2_f3_n7_o : 1.0));
    float v_f2_n15_o = (1.0 - v_f2_n10_o);
    float v_f2_f4_n13_o = (v_f2_n15_o * sx_Density);
    float v_f2_f4_n12_o = (v_f2_f4_n13_o * v_f2_f4_n13_o);
    float v_f2_f4_n11_o = ((1.0 > 0.5) ? v_f2_f4_n12_o : v_f2_f4_n13_o);
    float v_f2_f4_n9_o = pow(max(2.718282, 0.0), v_f2_f4_n11_o);
    float v_f2_f4_n7_o = (1.0 / v_f2_f4_n9_o);
    float v_f2_f4_n5_o = ((v_f2_n15_o == 0.0) ? 1.0 : ((v_f2_n15_o > 0.0) ? v_f2_f4_n7_o : 1.0));
    float v_f2_n13_o = (1.0 - v_f2_f4_n5_o);
    float v_f2_n8_o = ((0.0 > 0.5) ? v_f2_f3_n5_o : v_f2_n13_o);
    float v_n16_o = (1.0 - v_f2_n8_o);
    float v_n11_o = lerp(v_f2_n8_o, v_n16_o, sx_Invert);
    float v_n10_o = pow(max(v_n11_o, 0.0), sx_Power);
    float v_f1_n9_o = (2.0 * v_n10_o);
    float v_f1_n8_o = clamp(v_f1_n9_o, 0.0, 1.0);
    float v_f1_n7_o = pow(max(v_f1_n8_o, 0.0), sx_Curve);
    float v_f1_n6_o = (v_f1_n7_o * 0.5);
    float v_f1_n21_o = -0.5;
    float v_f1_n20_o = (v_n10_o + v_f1_n21_o);
    float v_f1_n19_o = (v_f1_n20_o * 2.0);
    float v_f1_n18_o = clamp(v_f1_n19_o, 0.0, 1.0);
    float v_f1_n17_o = (1.0 - v_f1_n18_o);
    float v_f1_n16_o = pow(max(v_f1_n17_o, 0.0), sx_Curve);
    float v_f1_n15_o = (1.0 - v_f1_n16_o);
    float v_f1_n14_o = (v_f1_n15_o * 0.5);
    float v_f1_n12_o = (0.5 + v_f1_n14_o);
    float v_f1_n25_o = (v_f1_n15_o * 500.0);
    float v_f1_n24_o = clamp(v_f1_n25_o, 0.0, 1.0);
    float v_f1_n5_o = lerp(v_f1_n6_o, v_f1_n12_o, v_f1_n24_o);
    float2 v_n6_o = (v_n7_o * ((float2)(v_f1_n5_o)));
    float2 v_n4_o = (((float2)(sx_Intensity)) * v_n6_o);
    float2 v_n1_o = (v_n2_o - v_n4_o);
    float4 b_n0 = CyTex(0, v_n1_o);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n21 = CyTex(0, UV);
    float v_n20_o = (b_n21).w;
    return float4(v_n0_RGB, v_n20_o);
}

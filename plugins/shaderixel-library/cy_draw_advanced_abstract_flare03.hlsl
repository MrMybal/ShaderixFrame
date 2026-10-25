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
//@name Advanced Abstract Flare03
//@category Dessin
//@generator
//@halo full
//@param sx_Divisor float 4.862034 0.0 9.724068 "Divisor"
//@param sx_Mask float 1.0 0.0 2.0 "Mask"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedAbstractFlare03.M_CyDraw_AdvancedAbstractFlare03: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_f4_n12(float x, float y)
{
return SXRet( (atan2(y,x)));
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_f1_n0_o = float2(0.5, 0.5);
    float v_f1_n12_o = distance(UV, v_f1_n0_o);
    float v_f1_n11_o = (v_f1_n12_o * 1.0);
    float v_f1_n10_o = (v_f1_n11_o / 0.5);
    float2 v_f4_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f4_n3_o = (v_f4_n4_o - ((float2)(1.0)));
    float v_f4_n13_o = (float4(v_f4_n3_o, 0.0, 1.0)).x;
    float v_f4_n14_o = (float4(v_f4_n3_o, 0.0, 1.0)).y;
    float b_f4_n12 = (Custom_f4_n12(v_f4_n13_o, v_f4_n14_o)).x;
    float v_f4_n11_o = (((float4)(b_f4_n12))).x;
    float v_f4_n10_o = (v_f4_n11_o / 6.283185);
    float v_f4_n9_o = frac(v_f4_n10_o);
    float2 v_f4_n19_o = (v_f4_n3_o * v_f4_n3_o);
    float v_f4_n18_o = (float4(v_f4_n19_o, 0.0, 1.0)).x;
    float v_f4_n20_o = (float4(v_f4_n19_o, 0.0, 1.0)).y;
    float v_f4_n17_o = (v_f4_n18_o + v_f4_n20_o);
    float v_f4_n16_o = sqrt(v_f4_n17_o);
    float2 v_f4_n8_o = float2(v_f4_n9_o, v_f4_n16_o);
    float2 v_f4_n21_o = float2(v_f4_n16_o, v_f4_n9_o);
    float2 v_f4_n7_o = ((1.0 > 0.5) ? v_f4_n8_o : v_f4_n21_o);
    float v_n9_o = (1.0 / sx_Divisor);
    float2 v_n7_o = fmod(v_f4_n7_o, ((float2)(v_n9_o)));
    float2 v_n6_o = (v_n7_o / ((float2)(v_n9_o)));
    float2 v_n5_o = sin((v_n6_o * 2.094395));
    float v_f3_n2_o = (float4(v_n5_o, 0.0, 1.0)).x;
    float v_n3_o = sin((v_f3_n2_o * 2.094395));
    float v_f5_n4_o = (float4(v_n5_o, 0.0, 1.0)).y;
    float v_n2_o = (v_n3_o + v_f5_n4_o);
    float v_f1_f2_n13_o = (v_f1_n10_o * v_n2_o);
    float v_f1_f2_n12_o = (v_f1_f2_n13_o * v_f1_f2_n13_o);
    float v_f1_f2_n11_o = ((1.0 > 0.5) ? v_f1_f2_n12_o : v_f1_f2_n13_o);
    float v_f1_f2_n9_o = pow(max(2.718282, 0.0), v_f1_f2_n11_o);
    float v_f1_f2_n7_o = (1.0 / v_f1_f2_n9_o);
    float v_f1_f2_n5_o = ((v_f1_n10_o == 0.0) ? 1.0 : ((v_f1_n10_o > 0.0) ? v_f1_f2_n7_o : 1.0));
    float v_f1_n15_o = (1.0 - v_f1_n10_o);
    float v_f1_f6_n13_o = (v_f1_n15_o * v_n2_o);
    float v_f1_f6_n12_o = (v_f1_f6_n13_o * v_f1_f6_n13_o);
    float v_f1_f6_n11_o = ((1.0 > 0.5) ? v_f1_f6_n12_o : v_f1_f6_n13_o);
    float v_f1_f6_n9_o = pow(max(2.718282, 0.0), v_f1_f6_n11_o);
    float v_f1_f6_n7_o = (1.0 / v_f1_f6_n9_o);
    float v_f1_f6_n5_o = ((v_f1_n15_o == 0.0) ? 1.0 : ((v_f1_n15_o > 0.0) ? v_f1_f6_n7_o : 1.0));
    float v_f1_n13_o = (1.0 - v_f1_f6_n5_o);
    float v_f1_n8_o = ((0.0 > 0.5) ? v_f1_f2_n5_o : v_f1_n13_o);
    float2 v_f7_n0_o = float2(0.5, 0.5);
    float v_f7_n12_o = distance(UV, v_f7_n0_o);
    float v_f7_n11_o = (v_f7_n12_o * 1.0);
    float v_f7_n10_o = (v_f7_n11_o / 0.5);
    float v_f7_f8_n13_o = (v_f7_n10_o * sx_Mask);
    float v_f7_f8_n12_o = (v_f7_f8_n13_o * v_f7_f8_n13_o);
    float v_f7_f8_n11_o = ((1.0 > 0.5) ? v_f7_f8_n12_o : v_f7_f8_n13_o);
    float v_f7_f8_n9_o = pow(max(2.718282, 0.0), v_f7_f8_n11_o);
    float v_f7_f8_n7_o = (1.0 / v_f7_f8_n9_o);
    float v_f7_f8_n5_o = ((v_f7_n10_o == 0.0) ? 1.0 : ((v_f7_n10_o > 0.0) ? v_f7_f8_n7_o : 1.0));
    float v_f7_n15_o = (1.0 - v_f7_n10_o);
    float v_f7_f9_n13_o = (v_f7_n15_o * sx_Mask);
    float v_f7_f9_n12_o = (v_f7_f9_n13_o * v_f7_f9_n13_o);
    float v_f7_f9_n11_o = ((1.0 > 0.5) ? v_f7_f9_n12_o : v_f7_f9_n13_o);
    float v_f7_f9_n9_o = pow(max(2.718282, 0.0), v_f7_f9_n11_o);
    float v_f7_f9_n7_o = (1.0 / v_f7_f9_n9_o);
    float v_f7_f9_n5_o = ((v_f7_n15_o == 0.0) ? 1.0 : ((v_f7_n15_o > 0.0) ? v_f7_f9_n7_o : 1.0));
    float v_f7_n13_o = (1.0 - v_f7_f9_n5_o);
    float v_f7_n8_o = ((0.0 > 0.5) ? v_f7_f8_n5_o : v_f7_n13_o);
    float v_n0_o = (v_f1_n8_o * v_f7_n8_o);
    float v_n15_o = (((float4)(v_n0_o))).x;
    float v_n16_o = (((float4)(v_n0_o))).y;
    float v_n14_o = max(v_n15_o, v_n16_o);
    float v_n17_o = (((float4)(v_n0_o))).z;
    float v_n13_o = max(v_n14_o, v_n17_o);
    return float4(((float3)(v_n0_o)), v_n13_o);
}

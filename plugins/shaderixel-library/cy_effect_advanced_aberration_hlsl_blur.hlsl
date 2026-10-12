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
//@name Advanced Aberration HLSL Blur
//@category Traitements
//@input Image
//@halo full
//@param sx_X float 1.0 0.0 2.0 "X"
//@param sx_Y float 1.0 0.0 2.0 "Y"
//@param sx_Distance float 0.001 0.0 1.0 "Distance"
//@param sx_Step float 16.0 1.0 128.0 "Step"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedAberrationHLSLBlur.M_CyEffect_AdvancedAberrationHLSLBlur: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_f1_n7(uint Tex, float2 UV, float2 Vector, float sx_arg_Distance, float Steps)
{
float3 CurColor = (float3 )0;
float3 newSample = (float3 )0;

int i = (int )0;
Steps = max(Steps, 1);
float StepSize = sx_arg_Distance / (int) Steps;
float2 NewUV = UV - Vector*0.5*sx_arg_Distance;
while ( i < (int) Steps)
{
float3 newSample=Texture2DSample(Tex,TexSampler,NewUV).rgb;
float3 CurrentAbberationValue=saturate(1.0-abs(3*(i/Steps)-float4(0.0,1.0,2.0,3.0))).xyz;
CurColor += newSample*CurrentAbberationValue;
NewUV.x += (StepSize * Vector.x);
NewUV.y += (StepSize * Vector.y);
i++;
}
CurColor = CurColor / i;
return SXRet( CurColor);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_f2_n3_o = float2(sx_X, sx_Y);
    float3 b_f1_n7 = (Custom_f1_n7(0, UV, v_f2_n3_o, sx_Distance, sx_Step)).xyz;
    float3 v_n0_o = (b_f1_n7 * ((float3)(4.0)));
    float4 b_n11 = CyTex(0, UV);
    float v_n10_o = (b_n11).w;
    float v_n14_o = (float4(v_n0_o, 1.0)).x;
    float v_n15_o = (float4(v_n0_o, 1.0)).y;
    float v_n13_o = max(v_n14_o, v_n15_o);
    float v_n16_o = (float4(v_n0_o, 1.0)).z;
    float v_n12_o = max(v_n13_o, v_n16_o);
    float v_n9_o = max(v_n10_o, v_n12_o);
    return float4(v_n0_o, v_n9_o);
}

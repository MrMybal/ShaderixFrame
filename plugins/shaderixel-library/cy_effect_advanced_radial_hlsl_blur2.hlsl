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
//@name Advanced Radial HLSL Blur2
//@category Traitements
//@input Image
//@halo full
//@param sx_RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param sx_Distance float 0.05 0.0 1.0 "Distance"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRadialHLSLBlur2.M_CyEffect_AdvancedRadialHLSLBlur2: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n0(float sx_arg_RadialStep, float2 UV, float BlurDist, uint Tex)
{
//Radial blur , TExture
//By CyberAlien , 2022


//RadialStep , number of Step
//UV , input UV coord
//BlurDist , Blur
//Tex , TextureInput

const float PI = 3.14159265 ;
float RadialStepAngle = ( 2 * PI ) / sx_arg_RadialStep ;
float4 Result = 0 ;
float2 dir = float2(0.5,0.5) - UV;


for(int i = 0; i<sx_arg_RadialStep; i++)
{
    float2 pos = UV + dir * ( i * RadialStepAngle ) * BlurDist;
    pos = clamp(pos, float2(0.0,0.0), float2(1.0, 1.0));
    Result +=Texture2DSample(Tex,TexSampler, pos);
}

return SXRet( Result / sx_arg_RadialStep );
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float3 b_n0 = (Custom_n0(sx_RadialStep, UV, sx_Distance, 0)).xyz;
    float4 b_n7 = CyTex(0, UV);
    float v_n6_o = (b_n7).w;
    float v_n10_o = (float4(b_n0, 1.0)).x;
    float v_n11_o = (float4(b_n0, 1.0)).y;
    float v_n9_o = max(v_n10_o, v_n11_o);
    float v_n12_o = (float4(b_n0, 1.0)).z;
    float v_n8_o = max(v_n9_o, v_n12_o);
    float v_n5_o = max(v_n6_o, v_n8_o);
    return float4(b_n0, v_n5_o);
}

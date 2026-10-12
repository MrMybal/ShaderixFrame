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
//@name Advanced HLSL Sharpen
//@category Traitements
//@input Image
//@halo full
//@param sx_RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param sx_Sharpen float 0.1 0.0 1.0 "Sharpen"
//@param sx_TextureSize float 2048.0 0.0 4096.0 "Texture Size"
//@param sx_p_lerp float 0.0 0.0 1.0 "lerp"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedHLSLSharpen.M_CyEffect_AdvancedHLSLSharpen: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n1(float2 UV, float sx_arg_RadialStep, float sx_arg_Sharpen, uint Tex, float sx_arg_TextureSize, float2 Radius)
{
// CyberAlien PostProcess HLSL
// Sharpen Radial
// v 1.0

// float2 , UV 
// float , RadialStep
// float , Intensity
// float , Shapen
// Tex , TextureInput
// TextureSize

// PI
const float PI = 3.14159265 ;
// RadialStep
float RadialStepAngle = ( 2 * PI ) / sx_arg_RadialStep ;
// Coord
float2 Coord = UV ;
// Store Result
float4 Result = Texture2DSample( Tex,TexSampler, Coord ) ;
float PX_Offset = 1 / sx_arg_TextureSize ;

// Exec

for ( int i=1 ; i<=sx_arg_RadialStep ; ++i )
{
	float Angle = i * RadialStepAngle ;
	float2 OffsetVector = float2(cos(Angle),sin(Angle)) ;
	float2 OffsetPX = ( PX_Offset * OffsetVector ) * Radius ;
	Coord = UV.xy + OffsetPX  ;
	Result += Texture2DSample( Tex,TexSampler, saturate(Coord) ) ;
}
Result = ( Result * ( -1.0 / sx_arg_RadialStep ) ) * sx_arg_Sharpen ;
Result += Texture2DSample( Tex,TexSampler, saturate(Coord) ) * ( sx_arg_Sharpen + 1.0 ); 
return SXRet( Result );
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_n7_o = float2(1.0, 1.0);
    float3 b_n1 = (Custom_n1(UV, sx_RadialStep, sx_Sharpen, 0, sx_TextureSize, v_n7_o)).xyz;
    float4 b_n8 = CyTex(0, UV);
    float3 v_n8_RGB = (b_n8).xyz;
    float3 v_n0_o = lerp(b_n1, v_n8_RGB, sx_p_lerp);
    float4 b_n11 = CyTex(0, UV);
    float v_n10_o = (b_n11).w;
    return float4(v_n0_o, v_n10_o);
}

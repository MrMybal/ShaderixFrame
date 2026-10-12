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
//@name Advanced Rotate Blur2 Variant
//@category Traitements
//@input Image
//@halo full
//@param sx_Step float 16.0 1.0 128.0 "Step"
//@param sx_Blur float -202.6579 -202.6579 1.0 "Blur"
//@param sx_BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRotateBlur2.M_CyEffect_AdvancedRotateBlur2: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float sx_arg_Step, float sx_arg_Blur, float2 Center, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// Rotate blur
// By CyberAlien

// UV
// Step
// Blur
// Center
// ID
// Filtered


float PI = 6.28318548;
float2 BaseUV = ( Center * -1 ) + UV;
float2 Rot = (float2 )0;
float Rotate = (float )0;
float4 Result = (float4 )0;

for( int i = 0 ; i<sx_arg_Step ; ++i )
{
	Rotate = (sx_arg_Blur/100000) * ( ((sx_arg_Step/2)*-1)+i );
	Rot = float2( dot( BaseUV , float2(cos((Rotate*PI)),sin((Rotate*PI))*-1) ) , dot( BaseUV , float2( sin((Rotate*PI)),cos((Rotate*PI))) ) );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, Center + Rot);
}
return SXRet( Result / sx_arg_Step);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float4 b_n1 = CyTex(0, UV);
    float3 v_n1_RGB = (b_n1).xyz;
    float v_n9_o = (0.0 + 0.5);
    float v_n8_o = lerp(1.0, v_n9_o, sx_BlurDitherIntensity);
    float v_n6_o = (sx_Blur * v_n8_o);
    float2 v_n12_o = float2(0.5, 0.5);
    float4 b_n3 = (Custom_n3(UV, sx_Step, v_n6_o, v_n12_o, 14.0, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, 1.0);
    float4 b_n30 = CyTex(0, UV);
    float v_n29_o = (b_n30).w;
    float v_n33_o = (float4(v_n0_o, 1.0)).x;
    float v_n34_o = (float4(v_n0_o, 1.0)).y;
    float v_n32_o = max(v_n33_o, v_n34_o);
    float v_n35_o = (float4(v_n0_o, 1.0)).z;
    float v_n31_o = max(v_n32_o, v_n35_o);
    float v_n28_o = max(v_n29_o, v_n31_o);
    return float4(v_n0_o, v_n28_o);
}

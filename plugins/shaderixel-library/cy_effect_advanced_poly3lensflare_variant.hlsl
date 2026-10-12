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
//@name Advanced Poly3lensflare Variant
//@category Traitements
//@input Image
//@halo full
//@param sx_K1 float 2.757838 0.0 5.515676 "K1"
//@param sx_K2 float 19.01343 0.0 38.02686 "K2"
//@param sx_K3 float -54.3229 -54.3229 1.0 "K3"
//@param sx_size float 1.061383 0.0 2.122766 "size"
//@param sx_RadialStep float 32.0 1.0 128.0 "Radial Step"
//@param sx_Center color 0.5 0.5 0.0 0.0 "Center"
//@param sx_Blur float 0.304042 0.0 1.0 "Blur"
//@param sx_BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param sx_Index float 14.0 1.0 128.0 "Index"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedPoly3lensflare.M_CyEffect_AdvancedPoly3lensflare: Native CyTexture advanced Effect preset with live GPU parameters.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }  // unconnected = white, as Unreal
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
#define Texture2DSampleLevel(T, S, UVS, LOD) CyTex(T, UVS)
static EffectContext ctx;  // for helper functions (noise) reading the document sx_size
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

float4 Custom_n2(float2 UV, float sx_arg_K1, float sx_arg_K2, float sx_arg_K3)
{
// Poly3 distorsion UV 
// Cubic screen
// by CyberAlien 2023

// UV
// K1
// K2
// K3
float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = distance(CubicUV , float2(0.5,0.5) );
float Kcal = (sx_arg_K1*R) + (sx_arg_K2*pow(R,2)) + (sx_arg_K3*pow(R,3));

return SXRet( UV - ( (UV - 0.5 ) * Kcal ) );
}

float4 Custom_n8(float2 UV, float sx_arg_RadialStep, float2 sx_arg_Center, float sx_arg_Blur, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
//Radial blur , Post process
//By CyberAlien , 2023


//RadialStep , number of Step
//UV , input UV coord
//Blur , Blur
//Center
//ID , SceneTextureLookup Index
//Filtered , SceneTextureLookupFiltered

float RadialStepAngle = 6.28318530 / sx_arg_RadialStep ;
float4 Result = 0 ;
float2 Dir = sx_arg_Center - UV;


for(int i = 0; i<sx_arg_RadialStep; i++)
{
    float2 pos = ViewportUVToBufferUV( UV + Dir * ( i * RadialStepAngle ) * sx_arg_Blur );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, pos);
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
    float2 b_n2 = (Custom_n2(UV, sx_K1, sx_K2, sx_K3)).xy;
    float4 b_n1 = CyTex(0, b_n2);
    float3 v_n1_RGB = (b_n1).xyz;
    float2 v_f1_n7_o = (b_n2 / ((float2)(sx_size)));
    float2 v_f1_n6_o = (v_f1_n7_o + ((float2)(0.5)));
    float v_f1_n9_o = (0.5 / sx_size);
    float2 v_f1_n5_o = (v_f1_n6_o - ((float2)(v_f1_n9_o)));
    float v_n13_R = (sx_Center).x;
    float2 v_f2_n3_o = float2(v_n13_R, v_n13_R);
    float v_n17_o = (0.0 + 0.5);
    float v_n16_o = lerp(1.0, v_n17_o, sx_BlurDitherIntensity);
    float v_n14_o = (sx_Blur * v_n16_o);
    float4 b_n8 = (Custom_n8(v_f1_n5_o, sx_RadialStep, v_f2_n3_o, v_n14_o, sx_Index, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n7_o = (b_n8).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n7_o, 1.0);
    float4 b_n29 = CyTex(0, UV);
    float v_n28_o = (b_n29).w;
    float v_n32_o = (float4(v_n0_o, 1.0)).x;
    float v_n33_o = (float4(v_n0_o, 1.0)).y;
    float v_n31_o = max(v_n32_o, v_n33_o);
    float v_n34_o = (float4(v_n0_o, 1.0)).z;
    float v_n30_o = max(v_n31_o, v_n34_o);
    float v_n27_o = max(v_n28_o, v_n30_o);
    return float4(v_n0_o, v_n27_o);
}

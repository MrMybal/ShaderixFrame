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
//@name Advanced Blur Poly3
//@category Traitements
//@input Image
//@halo full
//@param sx_Scale float 1.0 0.0 2.0 "Scale"
//@param sx_K1 float 0.0 0.0 1.0 "K1"
//@param sx_K2 float 0.0 0.0 1.0 "K2"
//@param sx_K3 float 0.0 0.0 1.0 "K3"
//@param sx_Step float 16.0 1.0 128.0 "Step"
//@param sx_Blur float 0.0 0.0 1.0 "Blur"
//@param sx_BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param sx_Type float 0.0 0.0 1.0 "Type"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedBlurPoly3.M_CyEffect_AdvancedBlurPoly3: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float sx_arg_K1, float sx_arg_K2, float sx_arg_K3, float sx_arg_Step, float sx_arg_Blur, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// Poly3 distorsion Blur , BC
// Brown-Conrady model , Cubic screen
// by CyberAlien 2023

// UV
// K1
// K2
// K3
// Step
// Blur
// ID
// Filtered
float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = distance(CubicUV , float2(0.5,0.5) );
float Kcal = (float )0;
float4 Result = (float4 )0;
float2 Duv = (float2 )0;

for( int i = 0 ; i<sx_arg_Step ; ++i )
{
	Kcal = ((lerp( sx_arg_K1 , 0 , (i/sx_arg_Step)*sx_arg_Blur ))*pow(R,2)) + ((lerp( sx_arg_K2 , 0 , (i/sx_arg_Step)*sx_arg_Blur ))*pow(R,4)) + ((lerp( sx_arg_K3 , 0 , (i/sx_arg_Step)*sx_arg_Blur ))*pow(R,6));
	Duv = UV - ( (UV - 0.5 ) * Kcal );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, Duv);
}
return SXRet( Result / ( sx_arg_Step  ));
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
    float2 v_f1_n7_o = (UV / ((float2)(sx_Scale)));
    float2 v_f1_n6_o = (v_f1_n7_o + ((float2)(0.5)));
    float v_f1_n9_o = (0.5 / sx_Scale);
    float2 v_f1_n5_o = (v_f1_n6_o - ((float2)(v_f1_n9_o)));
    float v_n14_o = (0.0 + 0.5);
    float v_n13_o = lerp(1.0, v_n14_o, sx_BlurDitherIntensity);
    float v_n11_o = (sx_Blur * v_n13_o);
    float4 b_n3 = (Custom_n3(v_f1_n5_o, sx_K1, sx_K2, sx_K3, sx_Step, v_n11_o, 14.0, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, sx_Type);
    float4 b_n26 = CyTex(0, UV);
    float v_n25_o = (b_n26).w;
    float v_n29_o = (float4(v_n0_o, 1.0)).x;
    float v_n30_o = (float4(v_n0_o, 1.0)).y;
    float v_n28_o = max(v_n29_o, v_n30_o);
    float v_n31_o = (float4(v_n0_o, 1.0)).z;
    float v_n27_o = max(v_n28_o, v_n31_o);
    float v_n24_o = max(v_n25_o, v_n27_o);
    return float4(v_n0_o, v_n24_o);
}

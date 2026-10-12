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
//@name Advanced Shape Lens
//@category Dessin
//@generator
//@halo full
//@param sx_RadialStep float 5.01 1.0 128.0 "Radial Step"
//@param sx_Lenght float 0.4 0.0 1.0 "Lenght"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedShapeLens.M_CyDraw_AdvancedShapeLens: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_n0(float2 UV, float Step, float Length)
{
// Lens Shape shader
// By CyberAlien

//RadialStep
//Length
//UV

float2 Center = float2(0.5, 0.5);
float2 ShapePoint = (float2 )0;
const float PI = 3.14159265;
float sx_RadialStep = Step ;
float RadialStepAngle = ( 2 * PI ) / sx_RadialStep ;
float Result = 0 ;
float2 Points[12] ;
float2 CurPoint = (float2 )0;
int v = (int )0;
float DotP = (float )0;


for (int i = 0; i < sx_RadialStep; i++)
{
    float Angle = i * RadialStepAngle ;
    ShapePoint = float2(cos(Angle),sin(Angle)) ;
	Points[i] = Center + ShapePoint * Length ;
}
for ( int u = 0; u < sx_RadialStep; u++ )
{
	v = u + 1 ;
	if ( v > sx_RadialStep ) {
		v = 0 ;
	}
	CurPoint = normalize( Points[v] - Points[u] ) ;
	DotP = dot(Points[u]-UV , float2(CurPoint.g,CurPoint.r)*float2(1,-1));
		if ( DotP >= 0 ) {
			Result += 1 ;
		}
		else {
			Result += 0 ;
		}
}
float OutDiv = sx_RadialStep - frac(sx_RadialStep);
Result /= OutDiv;
if ( Result >= 1.01000 )
{
	return SXRet( 1 );
}
return SXRet( 0 );
//return SXRet( Result);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float b_n0 = (Custom_n0(UV, sx_RadialStep, sx_Lenght)).x;
    float v_n6_o = (((float4)(b_n0))).x;
    float v_n7_o = (((float4)(b_n0))).y;
    float v_n5_o = max(v_n6_o, v_n7_o);
    float v_n8_o = (((float4)(b_n0))).z;
    float v_n4_o = max(v_n5_o, v_n8_o);
    return float4(((float3)(b_n0)), v_n4_o);
}

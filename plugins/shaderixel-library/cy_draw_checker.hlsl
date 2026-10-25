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
//@name Checker
//@category Dessin
//@graph {"name":"Checker","category":"Dessin","inputs":0,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Tiling","text":"Tiling","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityA","text":"Opacity A","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityB","text":"Opacity B","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Checker","text":"    float2 cell=floor(UV*max(Tiling,1.0)); float v=fmod(cell.x+cell.y,2.0); return float4(lerp(ColorA,ColorB,v),lerp(OpacityA,OpacityB,v));","pins":[["UV",2],["CyCanvasSize",4],["Tiling",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Tiling float 8 1 256 "Tiling"
//@param sx_OpacityA float 1 0 1 "Opacity A"
//@param sx_OpacityB float 1 0 1 "Opacity B"
//@param sx_ColorA color 0 0 0 1 "Color A"
//@param sx_ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Procedural checker pattern with independent colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Tiling, float sx_arg_OpacityA, float sx_arg_OpacityB, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 cell=floor(UV*max(sx_arg_Tiling,1.0)); float v=fmod(cell.x+cell.y,2.0); return float4(lerp(sx_arg_ColorA,sx_arg_ColorB,v),lerp(sx_arg_OpacityA,sx_arg_OpacityB,v));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Tiling, sx_OpacityA, sx_OpacityB, sx_ColorA.rgb, sx_ColorB.rgb);
}

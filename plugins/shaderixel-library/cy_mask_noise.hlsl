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
//@name Noise
//@category Masques
//@graph {"name":"Noise","category":"Masques","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[256.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":1.0,"max":4096.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":32.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"UseSource","text":"Use Source","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Noise","text":"    float l=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,float3(.2126,.7152,.0722)); float n=frac(sin(dot(floor(UV*max(Scale,1.0))+Seed,float2(12.9898,78.233)))*43758.5453); float v=saturate((n-Threshold)*Contrast)*lerp(1.0,l,saturate(UseSource)); return float4(v.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Threshold",1],["Contrast",1],["UseSource",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param sx_Scale float 256 1 4096 "Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Threshold float 0.35 0 1 "Threshold"
//@param sx_Contrast float 4 0 32 "Contrast"
//@param sx_UseSource float 0 0 1 "Use Source"

// Imported preset (Mask / Mask): Multiplies source luminance by procedural noise.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Seed, float sx_arg_Threshold, float sx_arg_Contrast, float sx_arg_UseSource)
{
    float l=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,float3(.2126,.7152,.0722)); float n=frac(sin(dot(floor(UV*max(sx_arg_Scale,1.0))+sx_arg_Seed,float2(12.9898,78.233)))*43758.5453); float v=saturate((n-sx_arg_Threshold)*sx_arg_Contrast)*lerp(1.0,l,saturate(sx_arg_UseSource)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Seed, sx_Threshold, sx_Contrast, sx_UseSource);
}

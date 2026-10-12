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
//@name Seamless Offset
//@category Traitements
//@graph {"name":"Seamless Offset","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Feather","text":"Feather","index":1,"min":0.00100000005,"max":0.5},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":6,"op":"param2","pos":[40,550],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Offset","text":"Offset","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Seamless Offset","text":"    float2 uv=frac(UV+Offset.xy);float4 base=Texture2DSample(CyInput,CyInputSampler,uv);float2 edge=min(uv,1.0-uv);float wx=1.0-smoothstep(0.0,Feather,edge.x);float wy=1.0-smoothstep(0.0,Feather,edge.y);float4 x=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(.5,0)));float4 y=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(0,.5)));float4 xy=Texture2DSample(CyInput,CyInputSampler,frac(uv+.5));float4 blended=lerp(lerp(base,x,wx),lerp(y,xy,wx),wy);return lerp(base,blended,Strength);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Feather",1],["Strength",1],["Offset",3]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param sx_Feather float 0.08 0.001 0.5 "Feather"
//@param sx_Strength float 1 0 1 "Strength"
//@param sx_Offset float2 0.5 0.5 0 1 "Offset"

// Imported preset (Effect / PBR): Wraps and crossfades an offset texture to reduce visible borders during seamless texture authoring.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Feather, float sx_arg_Strength, float3 sx_arg_Offset)
{
    float2 uv=frac(UV+sx_arg_Offset.xy);float4 base=Texture2DSample(CyInput,CyInputSampler,uv);float2 edge=min(uv,1.0-uv);float wx=1.0-smoothstep(0.0,sx_arg_Feather,edge.x);float wy=1.0-smoothstep(0.0,sx_arg_Feather,edge.y);float4 x=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(.5,0)));float4 y=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(0,.5)));float4 xy=Texture2DSample(CyInput,CyInputSampler,frac(uv+.5));float4 blended=lerp(lerp(base,x,wx),lerp(y,xy,wx),wy);return lerp(base,blended,sx_arg_Strength);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Feather, sx_Strength, float3(sx_Offset, 0.0));
}

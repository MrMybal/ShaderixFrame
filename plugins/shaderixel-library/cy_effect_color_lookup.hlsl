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
//@name Color Lookup
//@category Traitements
//@graph {"name":"Color Lookup","category":"Traitements","inputs":2,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"LookupTexture"},{"id":3,"op":"uv","pos":[40,260],"in":[],"value":[0,0,0,0]},{"id":4,"op":"canvassize","pos":[40,350],"in":[],"value":[0,0,0,0]},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"LUTSize","text":"LUT Size","index":1,"min":2.0,"max":64.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Color Lookup","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(LUTSize,2.0); float blue=saturate(s.b)*(n-1.0); float slice0=floor(blue); float slice1=min(slice0+1.0,n-1.0); float grid=ceil(sqrt(n)); float2 cell0=float2(fmod(slice0,grid),floor(slice0/grid)); float2 cell1=float2(fmod(slice1,grid),floor(slice1/grid)); float2 local=(s.rg*(n-1.0)+0.5)/n; float2 uv0=(cell0+local)/grid; float2 uv1=(cell1+local)/grid; float3 lut=lerp(Texture2DSample(LookupTexture,LookupTextureSampler,uv0).rgb,Texture2DSample(LookupTexture,LookupTextureSampler,uv1).rgb,frac(blue)); return float4(lerp(s.rgb,lut,saturate(Strength)),s.a);","pins":[["CyInput",5],["LookupTexture",5],["UV",2],["CyCanvasSize",4],["LUTSize",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@input Lookup
//@halo full
//@param sx_LUTSize float 16 2 64 "LUT Size"
//@param sx_Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Applies a standard square 2D color lookup table with configurable grid size.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
static const uint LookupTexture = 1;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_LUTSize, float sx_arg_Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(sx_arg_LUTSize,2.0); float blue=saturate(s.b)*(n-1.0); float slice0=floor(blue); float slice1=min(slice0+1.0,n-1.0); float grid=ceil(sqrt(n)); float2 cell0=float2(fmod(slice0,grid),floor(slice0/grid)); float2 cell1=float2(fmod(slice1,grid),floor(slice1/grid)); float2 local=(s.rg*(n-1.0)+0.5)/n; float2 uv0=(cell0+local)/grid; float2 uv1=(cell1+local)/grid; float3 lut=lerp(Texture2DSample(LookupTexture,LookupTextureSampler,uv0).rgb,Texture2DSample(LookupTexture,LookupTextureSampler,uv1).rgb,frac(blue)); return float4(lerp(s.rgb,lut,saturate(sx_arg_Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_LUTSize, sx_Strength);
}

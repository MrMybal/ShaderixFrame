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
//@name Mosaic Glass
//@category Traitements
//@graph {"name":"Mosaic Glass","category":"Traitements","inputs":1,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Cells","text":"Cells","index":1,"min":2.0,"max":512.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.550000012,0.0,0.0,0.0],"name":"CellJitter","text":"Cell Jitter","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0599999987,0.0,0.0,0.0],"name":"GroutWidth","text":"Grout Width","index":1,"min":0.00100000005,"max":0.449999988},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0149999997,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.699999988,0.0,0.0,0.0],"name":"BevelPower","text":"Bevel Power","index":1,"min":0.0500000007,"max":8.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"BevelAmount","text":"Bevel Amount","index":1,"min":0.0,"max":2.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"GroutOpacity","text":"Grout Opacity","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.0149999997,0.0149999997,0.0179999992,1.0],"name":"GroutColor","text":"Grout Color","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Mosaic Glass","text":"    float2 q=UV*max(Cells,1.0);float2 id=floor(q);float2 f=frac(q)-.5;float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float2 jitter=float2(h-.5,frac(h*19.17)-.5)*CellJitter;float2 sampleUV=(id+.5+jitter)/max(Cells,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,sampleUV);float edge=max(abs(f.x),abs(f.y));float grout=smoothstep(.5-GroutWidth,.5-GroutWidth+Softness,edge);float bevel=pow(saturate(1.0-edge*2.0),BevelPower);float3 glass=s.rgb*lerp(1.0,bevel,BevelAmount);return float4(lerp(glass,GroutColor.rgb,grout),lerp(s.a,GroutOpacity,grout));","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Cells",1],["Seed",1],["CellJitter",1],["GroutWidth",1],["Softness",1],["BevelPower",1],["BevelAmount",1],["GroutOpacity",1],["GroutColor",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param sx_Cells float 24 2 512 "Cells"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_CellJitter float 0.55 0 1 "Cell Jitter"
//@param sx_GroutWidth float 0.06 0.001 0.45 "Grout Width"
//@param sx_Softness float 0.015 0.0001 0.2 "Softness"
//@param sx_BevelPower float 0.7 0.05 8 "Bevel Power"
//@param sx_BevelAmount float 0.25 0 2 "Bevel Amount"
//@param sx_GroutOpacity float 1 0 1 "Grout Opacity"
//@param sx_GroutColor color 0.015 0.015 0.018 1 "Grout Color"

// Imported preset (Effect / Stylize): Cellular stained-glass mosaic with seeded sampling, bevel shading, and grout color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Cells, float sx_arg_Seed, float sx_arg_CellJitter, float sx_arg_GroutWidth, float sx_arg_Softness, float sx_arg_BevelPower, float sx_arg_BevelAmount, float sx_arg_GroutOpacity, float3 sx_arg_GroutColor)
{
    float2 q=UV*max(sx_arg_Cells,1.0);float2 id=floor(q);float2 f=frac(q)-.5;float h=frac(sin(dot(id+sx_arg_Seed,float2(127.1,311.7)))*43758.5453);float2 jitter=float2(h-.5,frac(h*19.17)-.5)*sx_arg_CellJitter;float2 sampleUV=(id+.5+jitter)/max(sx_arg_Cells,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,sampleUV);float edge=max(abs(f.x),abs(f.y));float grout=smoothstep(.5-sx_arg_GroutWidth,.5-sx_arg_GroutWidth+sx_arg_Softness,edge);float bevel=pow(saturate(1.0-edge*2.0),sx_arg_BevelPower);float3 glass=s.rgb*lerp(1.0,bevel,sx_arg_BevelAmount);return float4(lerp(glass,sx_arg_GroutColor.rgb,grout),lerp(s.a,sx_arg_GroutOpacity,grout));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Cells, sx_Seed, sx_CellJitter, sx_GroutWidth, sx_Softness, sx_BevelPower, sx_BevelAmount, sx_GroutOpacity, sx_GroutColor.rgb);
}

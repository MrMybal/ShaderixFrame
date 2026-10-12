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
//@name Starfield
//@category Dessin
//@graph {"name":"Starfield","category":"Dessin","inputs":0,"premultiplied":false,"nextId":22,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[48.0,0.0,0.0,0.0],"name":"CellCount","text":"Cell Count","index":1,"min":4.0,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.180000007,0.0,0.0,0.0],"name":"Density","text":"Density","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.800000012,0.0,0.0,0.0],"name":"Jitter","text":"Jitter","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"MinRadius","text":"Min Radius","index":1,"min":0.00100000005,"max":0.5},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.129999995,0.0,0.0,0.0],"name":"MaxRadius","text":"Max Radius","index":1,"min":0.00100000005,"max":0.699999988},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Falloff","text":"Falloff","index":1,"min":0.0500000007,"max":16.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.150000006,0.0,0.0,0.0],"name":"SpikeWidth","text":"Spike Width","index":1,"min":0.00100000005,"max":1.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"SpikeLength","text":"Spike Length","index":1,"min":1.0,"max":16.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[5.0,0.0,0.0,0.0],"name":"SpikeSharpness","text":"Spike Sharpness","index":1,"min":0.100000001,"max":32.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[0.300000012,0.0,0.0,0.0],"name":"SpikeIntensity","text":"Spike Intensity","index":1,"min":0.0,"max":8.0},{"id":14,"op":"param","pos":[40,1430],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":64.0},{"id":15,"op":"param","pos":[40,1540],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":16,"op":"param","pos":[40,1650],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BackgroundOpacity","text":"Background Opacity","index":1},{"id":17,"op":"paramcolor","pos":[40,1760],"in":[],"value":[0.400000006,0.699999988,2.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":18,"op":"paramcolor","pos":[40,1870],"in":[],"value":[2.0,0.800000012,0.349999994,1.0],"name":"ColorB","text":"Color B","index":1},{"id":19,"op":"paramcolor","pos":[40,1980],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundColor","text":"Background Color","index":1},{"id":20,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0],[17,0],[18,0],[19,0]],"value":[0,0,0,0],"name":"Starfield","text":"    float2 q=UV*max(CellCount,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); float h2=frac(sin(dot(id+Seed+17.3,float2(269.5,183.3)))*43758.5453); float2 o=float2(h-0.5,h2-0.5)*Jitter; float2 d=p-o; float active=step(1.0-saturate(Density),h); float radius=lerp(MinRadius,MaxRadius,h2); float core=pow(saturate(1.0-length(d)/max(radius,0.0001)),max(Falloff,0.05)); float spikes=pow(saturate(1.0-min(abs(d.x),abs(d.y))/max(radius*SpikeWidth,0.0001)),SpikeSharpness)*pow(saturate(1.0-length(d)/(radius*SpikeLength)),2.0); float v=active*(core+spikes*SpikeIntensity); float3 color=lerp(ColorA.rgb,ColorB.rgb,h2); return float4(BackgroundColor.rgb+color*v*Intensity,saturate(BackgroundOpacity+v*Opacity));","pins":[["UV",2],["CyCanvasSize",4],["CellCount",1],["Seed",1],["Density",1],["Jitter",1],["MinRadius",1],["MaxRadius",1],["Falloff",1],["SpikeWidth",1],["SpikeLength",1],["SpikeSharpness",1],["SpikeIntensity",1],["Intensity",1],["Opacity",1],["BackgroundOpacity",1],["ColorA",3],["ColorB",3],["BackgroundColor",3]],"outs":[["Resultat",4]]},{"id":21,"op":"output","pos":[780,40],"in":[[20,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_CellCount float 48 4 512 "Cell Count"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Density float 0.18 0 1 "Density"
//@param sx_Jitter float 0.8 0 1 "Jitter"
//@param sx_MinRadius float 0.025 0.001 0.5 "Min Radius"
//@param sx_MaxRadius float 0.13 0.001 0.7 "Max Radius"
//@param sx_Falloff float 2 0.05 16 "Falloff"
//@param sx_SpikeWidth float 0.15 0.001 1 "Spike Width"
//@param sx_SpikeLength float 4 1 16 "Spike Length"
//@param sx_SpikeSharpness float 5 0.1 32 "Spike Sharpness"
//@param sx_SpikeIntensity float 0.3 0 8 "Spike Intensity"
//@param sx_Intensity float 4 0 64 "Intensity"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_BackgroundOpacity float 1 0 1 "Background Opacity"
//@param sx_ColorA color 0.4 0.7 2 1 "Color A"
//@param sx_ColorB color 2 0.8 0.35 1 "Color B"
//@param sx_BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Light & Space): Seeded HDR star field with variable size, density, diffraction spikes, and color variation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_CellCount, float sx_arg_Seed, float sx_arg_Density, float sx_arg_Jitter, float sx_arg_MinRadius, float sx_arg_MaxRadius, float sx_arg_Falloff, float sx_arg_SpikeWidth, float sx_arg_SpikeLength, float sx_arg_SpikeSharpness, float sx_arg_SpikeIntensity, float sx_arg_Intensity, float sx_arg_Opacity, float sx_arg_BackgroundOpacity, float3 sx_arg_ColorA, float3 sx_arg_ColorB, float3 sx_arg_BackgroundColor)
{
    float2 q=UV*max(sx_arg_CellCount,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float h=frac(sin(dot(id+sx_arg_Seed,float2(127.1,311.7)))*43758.5453); float h2=frac(sin(dot(id+sx_arg_Seed+17.3,float2(269.5,183.3)))*43758.5453); float2 o=float2(h-0.5,h2-0.5)*sx_arg_Jitter; float2 d=p-o; float active=step(1.0-saturate(sx_arg_Density),h); float radius=lerp(sx_arg_MinRadius,sx_arg_MaxRadius,h2); float core=pow(saturate(1.0-length(d)/max(radius,0.0001)),max(sx_arg_Falloff,0.05)); float spikes=pow(saturate(1.0-min(abs(d.x),abs(d.y))/max(radius*sx_arg_SpikeWidth,0.0001)),sx_arg_SpikeSharpness)*pow(saturate(1.0-length(d)/(radius*sx_arg_SpikeLength)),2.0); float v=active*(core+spikes*sx_arg_SpikeIntensity); float3 color=lerp(sx_arg_ColorA.rgb,sx_arg_ColorB.rgb,h2); return float4(sx_arg_BackgroundColor.rgb+color*v*sx_arg_Intensity,saturate(sx_arg_BackgroundOpacity+v*sx_arg_Opacity));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_CellCount, sx_Seed, sx_Density, sx_Jitter, sx_MinRadius, sx_MaxRadius, sx_Falloff, sx_SpikeWidth, sx_SpikeLength, sx_SpikeSharpness, sx_SpikeIntensity, sx_Intensity, sx_Opacity, sx_BackgroundOpacity, sx_ColorA.rgb, sx_ColorB.rgb, sx_BackgroundColor.rgb);
}

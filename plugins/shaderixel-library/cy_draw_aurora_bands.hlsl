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
//@name Aurora Bands
//@category Dessin
//@graph {"name":"Aurora Bands","category":"Dessin","inputs":0,"premultiplied":false,"nextId":18,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Frequency","text":"Frequency","index":1,"min":0.100000001,"max":64.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-20.0,"max":20.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Amplitude","text":"Amplitude","index":1,"min":0.0,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Width","text":"Width","index":1,"min":0.00100000005,"max":0.5},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.39999998,0.0,0.0,0.0],"name":"Sharpness","text":"Sharpness","index":1,"min":0.0500000007,"max":8.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[18.0,0.0,0.0,0.0],"name":"Striation","text":"Striation","index":1,"min":1.0,"max":256.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.550000012,0.0,0.0,0.0],"name":"StriationAmount","text":"Striation Amount","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.519999981,0.0,0.0,0.0],"name":"Horizon","text":"Horizon","index":1},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.649999976,0.0,0.0,0.0],"name":"HorizonFalloff","text":"Horizon Falloff","index":1,"min":0.00100000005,"max":2.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":64.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.0500000007,2.0,0.449999988,1.0],"name":"ColorA","text":"Color A","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.400000006,0.0799999982,2.5,1.0],"name":"ColorB","text":"Color B","index":1},{"id":16,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0]],"value":[0,0,0,0],"name":"Aurora Bands","text":"    float2 p=UV; float wave=0.5+Amplitude*(sin(p.x*Frequency+Phase)+0.45*sin(p.x*Frequency*2.17-Phase*1.3)); float d=abs(p.y-wave); float ribbon=exp(-d/max(Width,0.0001)); float striation=0.55+0.45*sin((p.x*Striation+p.y*7.0+Phase)*6.283); float horizon=pow(saturate(1.0-abs(p.y-Horizon)/max(HorizonFalloff,0.0001)),2.0); float v=pow(saturate(ribbon*lerp(1.0,striation,StriationAmount)*horizon),max(Sharpness,0.05)); float3 rgb=lerp(ColorA.rgb,ColorB.rgb,saturate(p.x+v*0.25))*v*Intensity; return float4(rgb,saturate(v*Opacity));","pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Phase",1],["Amplitude",1],["Width",1],["Sharpness",1],["Striation",1],["StriationAmount",1],["Horizon",1],["HorizonFalloff",1],["Intensity",1],["Opacity",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":17,"op":"output","pos":[780,40],"in":[[16,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Frequency float 8 0.1 64 "Frequency"
//@param sx_Phase float 0 -20 20 "Phase"
//@param sx_Amplitude float 0.08 0 0.5 "Amplitude"
//@param sx_Width float 0.08 0.001 0.5 "Width"
//@param sx_Sharpness float 1.4 0.05 8 "Sharpness"
//@param sx_Striation float 18 1 256 "Striation"
//@param sx_StriationAmount float 0.55 0 1 "Striation Amount"
//@param sx_Horizon float 0.52 0 1 "Horizon"
//@param sx_HorizonFalloff float 0.65 0.001 2 "Horizon Falloff"
//@param sx_Intensity float 3 0 64 "Intensity"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_ColorA color 0.05 2 0.45 1 "Color A"
//@param sx_ColorB color 0.4 0.08 2.5 1 "Color B"

// Imported preset (Draw / Light & Space): Layered HDR aurora ribbons with turbulence, glow, and horizon falloff.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Frequency, float sx_arg_Phase, float sx_arg_Amplitude, float sx_arg_Width, float sx_arg_Sharpness, float sx_arg_Striation, float sx_arg_StriationAmount, float sx_arg_Horizon, float sx_arg_HorizonFalloff, float sx_arg_Intensity, float sx_arg_Opacity, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 p=UV; float wave=0.5+sx_arg_Amplitude*(sin(p.x*sx_arg_Frequency+sx_arg_Phase)+0.45*sin(p.x*sx_arg_Frequency*2.17-sx_arg_Phase*1.3)); float d=abs(p.y-wave); float ribbon=exp(-d/max(sx_arg_Width,0.0001)); float striation=0.55+0.45*sin((p.x*sx_arg_Striation+p.y*7.0+sx_arg_Phase)*6.283); float horizon=pow(saturate(1.0-abs(p.y-sx_arg_Horizon)/max(sx_arg_HorizonFalloff,0.0001)),2.0); float v=pow(saturate(ribbon*lerp(1.0,striation,sx_arg_StriationAmount)*horizon),max(sx_arg_Sharpness,0.05)); float3 rgb=lerp(sx_arg_ColorA.rgb,sx_arg_ColorB.rgb,saturate(p.x+v*0.25))*v*sx_arg_Intensity; return float4(rgb,saturate(v*sx_arg_Opacity));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Frequency, sx_Phase, sx_Amplitude, sx_Width, sx_Sharpness, sx_Striation, sx_StriationAmount, sx_Horizon, sx_HorizonFalloff, sx_Intensity, sx_Opacity, sx_ColorA.rgb, sx_ColorB.rgb);
}

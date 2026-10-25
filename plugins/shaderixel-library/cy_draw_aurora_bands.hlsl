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
//@name Aurora Bands
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Aurora Bands","nextId":18,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":64,"min":0.100000001,"name":"Frequency","op":"param","pos":[40, 220],"text":"Frequency","value":[8, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":20,"min":-20,"name":"Phase","op":"param","pos":[40, 330],"text":"Phase","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":0,"name":"Amplitude","op":"param","pos":[40, 440],"text":"Amplitude","value":[0.0799999982, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"Width","op":"param","pos":[40, 550],"text":"Width","value":[0.0799999982, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"Sharpness","op":"param","pos":[40, 660],"text":"Sharpness","value":[1.39999998, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":256,"min":1,"name":"Striation","op":"param","pos":[40, 770],"text":"Striation","value":[18, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"StriationAmount","op":"param","pos":[40, 880],"text":"Striation Amount","value":[0.550000012, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Horizon","op":"param","pos":[40, 990],"text":"Horizon","value":[0.519999981, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"HorizonFalloff","op":"param","pos":[40, 1100],"text":"Horizon Falloff","value":[0.649999976, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":64,"min":0,"name":"Intensity","op":"param","pos":[40, 1210],"text":"Intensity","value":[3, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1320],"text":"Opacity","value":[1, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 1430],"text":"Color A","value":[0.0500000007, 2, 0.449999988, 1]},{"id":15,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 1540],"text":"Color B","value":[0.400000006, 0.0799999982, 2.5, 1]},{"id":16,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0]],"name":"Aurora Bands","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Phase",1],["Amplitude",1],["Width",1],["Sharpness",1],["Striation",1],["StriationAmount",1],["Horizon",1],["HorizonFalloff",1],["Intensity",1],["Opacity",1],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV;\n    float wave = 0.5 + Amplitude * (sin(p.x * Frequency + Phase) + 0.45 * sin(p.x * Frequency * 2.17 - Phase * 1.3));\n    float d = abs(p.y - wave);\n    float ribbon = exp(-d / max(Width, 0.0001));\n    float striation = 0.55 + 0.45 * sin((p.x * Striation + p.y * 7.0 + Phase) * 6.283);\n    float horizon = pow(saturate(1.0 - abs(p.y - Horizon) / max(HorizonFalloff, 0.0001)), 2.0);\n    float v = pow(saturate(ribbon * lerp(1.0, striation, StriationAmount) * horizon), max(Sharpness, 0.05));\n    float3 rgb = lerp(ColorA.rgb, ColorB.rgb, saturate(p.x + v * 0.25)) * v * Intensity;\n    return float4(rgb, saturate(v * Opacity));","value":[0, 0, 0, 0]},{"id":17,"in":[[16, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
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
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Frequency, float sx_arg_Phase, float sx_arg_Amplitude, float sx_arg_Width, float sx_arg_Sharpness, float sx_arg_Striation, float sx_arg_StriationAmount, float sx_arg_Horizon, float sx_arg_HorizonFalloff, float sx_arg_Intensity, float sx_arg_Opacity, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 p = UV;
    float wave = 0.5 + sx_arg_Amplitude * (sin(p.x * sx_arg_Frequency + sx_arg_Phase) + 0.45 * sin(p.x * sx_arg_Frequency * 2.17 - sx_arg_Phase * 1.3));
    float d = abs(p.y - wave);
    float ribbon = exp(-d / max(sx_arg_Width, 0.0001));
    float striation = 0.55 + 0.45 * sin((p.x * sx_arg_Striation + p.y * 7.0 + sx_arg_Phase) * 6.283);
    float horizon = pow(saturate(1.0 - abs(p.y - sx_arg_Horizon) / max(sx_arg_HorizonFalloff, 0.0001)), 2.0);
    float v = pow(saturate(ribbon * lerp(1.0, striation, sx_arg_StriationAmount) * horizon), max(sx_arg_Sharpness, 0.05));
    float3 rgb = lerp(sx_arg_ColorA.rgb, sx_arg_ColorB.rgb, saturate(p.x + v * 0.25)) * v * sx_arg_Intensity;
    return float4(rgb, saturate(v * sx_arg_Opacity));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Frequency, sx_Phase, sx_Amplitude, sx_Width, sx_Sharpness, sx_Striation, sx_StriationAmount, sx_Horizon, sx_HorizonFalloff, sx_Intensity, sx_Opacity, sx_ColorA.rgb, sx_ColorB.rgb);
}

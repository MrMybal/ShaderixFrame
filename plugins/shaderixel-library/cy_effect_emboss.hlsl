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
//@name Emboss
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Emboss","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 330],"text":"Angle","value":[0.785000026, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0.25,"name":"Radius","op":"param","pos":[40, 440],"text":"Radius","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":16,"min":0,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[2, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2,"min":-2,"name":"Bias","op":"param","pos":[40, 660],"text":"Bias","value":[0.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Blend","op":"param","pos":[40, 770],"text":"Blend","value":[1, 0, 0, 0]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Emboss","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Angle",1],["Radius",1],["Strength",1],["Bias",1],["Blend",1]],"pos":[420, 40],"text":"    float2 t = float2(cos(Angle) / max(CyCanvasSize.x, 1.0), sin(Angle) / max(CyCanvasSize.y, 1.0)) * Radius;\n    float4 a = Texture2DSample(CyInput, CyInputSampler, UV - t);\n    float4 b = Texture2DSample(CyInput, CyInputSampler, UV + t);\n    float3 e = (a.rgb - b.rgb) * Strength + Bias;\n    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    return float4(lerp(s.rgb, e, saturate(Blend)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Angle float 0.785 -6.283 6.283 "Angle"
//@param sx_Radius float 1 0.25 32 "Radius"
//@param sx_Strength float 2 0 16 "Strength"
//@param sx_Bias float 0.5 -2 2 "Bias"
//@param sx_Blend float 1 0 1 "Blend"
// Imported preset (Effect / Detail): Directional emboss filter with adjustable bias.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Angle, float sx_arg_Radius, float sx_arg_Strength, float sx_arg_Bias, float sx_arg_Blend)
{
    float2 t = float2(cos(sx_arg_Angle) / max(CyCanvasSize.x, 1.0), sin(sx_arg_Angle) / max(CyCanvasSize.y, 1.0)) * sx_arg_Radius;
    float4 a = Texture2DSample(CyInput, CyInputSampler, UV - t);
    float4 b = Texture2DSample(CyInput, CyInputSampler, UV + t);
    float3 e = (a.rgb - b.rgb) * sx_arg_Strength + sx_arg_Bias;
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    return float4(lerp(s.rgb, e, saturate(sx_arg_Blend)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Angle, sx_Radius, sx_Strength, sx_Bias, sx_Blend);
}

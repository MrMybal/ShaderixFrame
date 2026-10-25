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
//@name Tilt Shift Blur
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Tilt Shift Blur","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":512,"min":0,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[24, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"FocusAngle","op":"param","pos":[40, 440],"text":"Focus Angle","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"FocusWidth","op":"param","pos":[40, 550],"text":"Focus Width","value":[0.0799999982, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.200000003, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":128,"min":1,"name":"Steps","op":"param","pos":[40, 770],"text":"Steps","value":[20, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"HighQuality","op":"param","pos":[40, 880],"text":"High Quality","value":[0, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Dither","op":"param","pos":[40, 990],"text":"Dither","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0,"name":"DitherAmount","op":"param","pos":[40, 1100],"text":"Dither Amount","value":[0.75, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":1000,"min":-1000,"name":"DitherSeed","op":"param","pos":[40, 1210],"text":"Dither Seed","value":[0, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1320],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Tilt Shift Blur","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["FocusAngle",1],["FocusWidth",1],["Softness",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1],["Center",3]],"pos":[420, 40],"text":"    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);\n    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));\n    float2 FocusNormal = float2(-sin(FocusAngle), cos(FocusAngle));\n    float Distance = abs(dot(UV - Center.xy, FocusNormal));\n    float LocalRadius = Radius * saturate((Distance - FocusWidth) / max(Softness, 0.0001));\n    float4 Sum = 0;\n    [loop] for (int Index = 0; Index < 128; ++Index) {\n        if (Index >= Count) break;\n        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;\n        float F = saturate((Index + 0.5 + Jitter) / Count);\n        float A = Index * 2.39996323;\n        float2 Offset = float2(cos(A), sin(A)) * sqrt(F) * LocalRadius / max(CyCanvasSize.xy, 1.0);\n        Sum += Texture2DSample(CyInput, CyInputSampler, UV + Offset);\n    }\n    return Sum / max((float)Count, 1.0);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Radius float 24 0 512 "Radius"
//@param sx_FocusAngle float 0 -6.283 6.283 "Focus Angle"
//@param sx_FocusWidth float 0.08 0 1 "Focus Width"
//@param sx_Softness float 0.2 0.001 1 "Softness"
//@param sx_Steps float 20 1 128 "Steps"
//@param sx_HighQuality float 0 0 1 "High Quality"
//@param sx_Dither float 1 0 1 "Dither"
//@param sx_DitherAmount float 0.75 0 2 "Dither Amount"
//@param sx_DitherSeed float 0 -1000 1000 "Dither Seed"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Blur & Denoise): Focus-band blur with adjustable angle, transition, radius, and quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Radius, float sx_arg_FocusAngle, float sx_arg_FocusWidth, float sx_arg_Softness, float sx_arg_Steps, float sx_arg_HighQuality, float sx_arg_Dither, float sx_arg_DitherAmount, float sx_arg_DitherSeed, float3 sx_arg_Center)
{
    int Count = sx_arg_HighQuality > 0.5 ? 128 : clamp((int)round(sx_arg_Steps), 1, 128);
    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));
    float2 FocusNormal = float2(-sin(sx_arg_FocusAngle), cos(sx_arg_FocusAngle));
    float Distance = abs(dot(UV - sx_arg_Center.xy, FocusNormal));
    float LocalRadius = sx_arg_Radius * saturate((Distance - sx_arg_FocusWidth) / max(sx_arg_Softness, 0.0001));
    float4 Sum = 0;
    [loop] for (int Index = 0; Index < 128; ++Index) {
        if (Index >= Count) break;
        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + sx_arg_DitherSeed, Index * 7.13 - sx_arg_DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * sx_arg_DitherAmount * sx_arg_Dither;
        float F = saturate((Index + 0.5 + Jitter) / Count);
        float A = Index * 2.39996323;
        float2 Offset = float2(cos(A), sin(A)) * sqrt(F) * LocalRadius / max(CyCanvasSize.xy, 1.0);
        Sum += Texture2DSample(CyInput, CyInputSampler, UV + Offset);
    }
    return Sum / max((float)Count, 1.0);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Radius, sx_FocusAngle, sx_FocusWidth, sx_Softness, sx_Steps, sx_HighQuality, sx_Dither, sx_DitherAmount, sx_DitherSeed, float3(sx_Center, 0.0));
}

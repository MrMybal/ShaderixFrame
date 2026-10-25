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
//@name Heat Haze
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Heat Haze","nextId":12,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":128,"min":0.00999999978,"name":"Scale","op":"param","pos":[40, 330],"text":"Scale","value":[4, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":100,"min":-100,"name":"Phase","op":"param","pos":[40, 440],"text":"Phase","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0,"name":"Distortion","op":"param","pos":[40, 550],"text":"Distortion","value":[0.699999988, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":512,"min":0,"name":"ReferencePixels","op":"param","pos":[40, 660],"text":"Reference Pixels","value":[24, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"UseAlphaMask","op":"param","pos":[40, 770],"text":"Use Alpha Mask","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"MaskPower","op":"param","pos":[40, 880],"text":"Mask Power","value":[1, 0, 0, 0]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"Heat Haze","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Scale",1],["Phase",1],["Distortion",1],["ReferencePixels",1],["UseAlphaMask",1],["MaskPower",1]],"pos":[420, 40],"text":"    float2 p = UV * max(Scale, 0.01);\n    float nx = sin(p.y * 7.13 + Phase) + sin((p.x + p.y) * 13.17 - Phase * .7) + sin(p.y * 29.7 + p.x * 3.1);\n    float ny = cos(p.x * 8.31 - Phase) + cos((p.x - p.y) * 17.71 + Phase * .4) + cos(p.x * 31.3 - p.y * 2.7);\n    float2 warp = float2(nx, ny) / 3.0 * Distortion / max(CyCanvasSize.xy, 1.0) * ReferencePixels;\n    float mask = lerp(1.0, pow(saturate(Texture2DSample(CyInput, CyInputSampler, UV).a), MaskPower), UseAlphaMask);\n    float4 s = Texture2DSample(CyInput, CyInputSampler, UV + warp * mask);\n    return s;","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Scale float 4 0.01 128 "Scale"
//@param sx_Phase float 0 -100 100 "Phase"
//@param sx_Distortion float 0.7 0 8 "Distortion"
//@param sx_ReferencePixels float 24 0 512 "Reference Pixels"
//@param sx_UseAlphaMask float 0 0 1 "Use Alpha Mask"
//@param sx_MaskPower float 1 0.05 8 "Mask Power"
// Imported preset (Effect / Distort): Layered procedural refraction for heat shimmer, underwater distortion, and energy fields.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Phase, float sx_arg_Distortion, float sx_arg_ReferencePixels, float sx_arg_UseAlphaMask, float sx_arg_MaskPower)
{
    float2 p = UV * max(sx_arg_Scale, 0.01);
    float nx = sin(p.y * 7.13 + sx_arg_Phase) + sin((p.x + p.y) * 13.17 - sx_arg_Phase * .7) + sin(p.y * 29.7 + p.x * 3.1);
    float ny = cos(p.x * 8.31 - sx_arg_Phase) + cos((p.x - p.y) * 17.71 + sx_arg_Phase * .4) + cos(p.x * 31.3 - p.y * 2.7);
    float2 warp = float2(nx, ny) / 3.0 * sx_arg_Distortion / max(CyCanvasSize.xy, 1.0) * sx_arg_ReferencePixels;
    float mask = lerp(1.0, pow(saturate(Texture2DSample(CyInput, CyInputSampler, UV).a), sx_arg_MaskPower), sx_arg_UseAlphaMask);
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV + warp * mask);
    return s;
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Phase, sx_Distortion, sx_ReferencePixels, sx_UseAlphaMask, sx_MaskPower);
}

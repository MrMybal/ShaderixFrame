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
//@name Edge
//@category Masques
//@graph {"category":"Masques","inputs":1,"name":"Edge","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[2, 0, 0, 0]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Edge","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1]],"pos":[420, 40],"text":"    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0)) * Radius;\n    float tl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, -t.y)).rgb, float3(.2126, .7152, .0722));\n    float tc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, -t.y)).rgb, float3(.2126, .7152, .0722));\n    float tr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, -t.y)).rgb, float3(.2126, .7152, .0722));\n    float ml = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float mr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float bl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, t.y)).rgb, float3(.2126, .7152, .0722));\n    float bc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722));\n    float br = dot(Texture2DSample(CyInput, CyInputSampler, UV + t).rgb, float3(.2126, .7152, .0722));\n    float gx = -tl - 2 * ml - bl + tr + 2 * mr + br;\n    float gy = -tl - 2 * tc - tr + bl + 2 * bc + br;\n    float v = saturate(length(float2(gx, gy)) * Strength);\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Radius float 1 0.25 16 "Radius"
//@param sx_Strength float 2 0 32 "Strength"
// Imported preset (Mask / Mask): Sobel luminance edge mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Radius, float sx_arg_Strength)
{
    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0)) * sx_arg_Radius;
    float tl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, -t.y)).rgb, float3(.2126, .7152, .0722));
    float tc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, -t.y)).rgb, float3(.2126, .7152, .0722));
    float tr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, -t.y)).rgb, float3(.2126, .7152, .0722));
    float ml = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float mr = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float bl = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(-t.x, t.y)).rgb, float3(.2126, .7152, .0722));
    float bc = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722));
    float br = dot(Texture2DSample(CyInput, CyInputSampler, UV + t).rgb, float3(.2126, .7152, .0722));
    float gx = -tl - 2 * ml - bl + tr + 2 * mr + br;
    float gy = -tl - 2 * tc - tr + bl + 2 * bc + br;
    float v = saturate(length(float2(gx, gy)) * sx_arg_Strength);
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Radius, sx_Strength);
}

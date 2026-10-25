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
//@name Dissolve
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Dissolve","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":512,"min":1,"name":"NoiseScale","op":"param","pos":[40, 550],"text":"Noise Scale","value":[12, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 660],"text":"Seed","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Threshold","op":"param","pos":[40, 770],"text":"Threshold","value":[0.5, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 880],"text":"Softness","value":[0.0299999993, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"EdgeWidth","op":"param","pos":[40, 990],"text":"Edge Width","value":[0.0799999982, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":8,"min":0,"name":"EdgeIntensity","op":"param","pos":[40, 1100],"text":"Edge Intensity","value":[0.800000012, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1210],"text":"Opacity","value":[1, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"EdgeColor","op":"paramcolor","pos":[40, 1320],"text":"Edge Color","value":[2, 0.349999994, 0.0199999996, 1]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Dissolve","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["NoiseScale",1],["Seed",1],["Threshold",1],["Softness",1],["EdgeWidth",1],["EdgeIntensity",1],["Opacity",1],["EdgeColor",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float2 p = UV * max(NoiseScale, 1.0) + Seed;\n    float2 id = floor(p), f = frac(p);\n    f = f * f * (3.0 - 2.0 * f);\n    float a0 = frac(sin(dot(id, float2(127.1, 311.7))) * 43758.5453), a1 = frac(sin(dot(id + float2(1, 0), float2(127.1, 311.7))) * 43758.5453), a2 = frac(sin(dot(id + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), a3 = frac(sin(dot(id + 1.0, float2(127.1, 311.7))) * 43758.5453);\n    float n = lerp(lerp(a0, a1, f.x), lerp(a2, a3, f.x), f.y);\n    float reveal = smoothstep(Threshold - Softness, Threshold + Softness, n);\n    float edge = 1.0 - saturate(abs(n - Threshold) / max(EdgeWidth, .0001));\n    float alpha = saturate(reveal * fg.a * mask * Opacity);\n    float3 front = lerp(fg.rgb, EdgeColor.rgb, edge * EdgeIntensity);\n    return float4(lerp(bg.rgb, front, alpha), alpha + bg.a * (1.0 - alpha));","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param sx_NoiseScale float 12 1 512 "Noise Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Threshold float 0.5 0 1 "Threshold"
//@param sx_Softness float 0.03 0.0001 0.5 "Softness"
//@param sx_EdgeWidth float 0.08 0.0001 0.5 "Edge Width"
//@param sx_EdgeIntensity float 0.8 0 8 "Edge Intensity"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_EdgeColor color 2 0.35 0.02 1 "Edge Color"
// Imported preset (Blend / Utility): Seeded procedural dissolve blend with threshold, feathered edge, and HDR edge color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_NoiseScale, float sx_arg_Seed, float sx_arg_Threshold, float sx_arg_Softness, float sx_arg_EdgeWidth, float sx_arg_EdgeIntensity, float sx_arg_Opacity, float3 sx_arg_EdgeColor)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float2 p = UV * max(sx_arg_NoiseScale, 1.0) + sx_arg_Seed;
    float2 id = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a0 = frac(sin(dot(id, float2(127.1, 311.7))) * 43758.5453), a1 = frac(sin(dot(id + float2(1, 0), float2(127.1, 311.7))) * 43758.5453), a2 = frac(sin(dot(id + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), a3 = frac(sin(dot(id + 1.0, float2(127.1, 311.7))) * 43758.5453);
    float n = lerp(lerp(a0, a1, f.x), lerp(a2, a3, f.x), f.y);
    float reveal = smoothstep(sx_arg_Threshold - sx_arg_Softness, sx_arg_Threshold + sx_arg_Softness, n);
    float edge = 1.0 - saturate(abs(n - sx_arg_Threshold) / max(sx_arg_EdgeWidth, .0001));
    float alpha = saturate(reveal * fg.a * mask * sx_arg_Opacity);
    float3 front = lerp(fg.rgb, sx_arg_EdgeColor.rgb, edge * sx_arg_EdgeIntensity);
    return float4(lerp(bg.rgb, front, alpha), alpha + bg.a * (1.0 - alpha));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_NoiseScale, sx_Seed, sx_Threshold, sx_Softness, sx_EdgeWidth, sx_EdgeIntensity, sx_Opacity, sx_EdgeColor.rgb);
}

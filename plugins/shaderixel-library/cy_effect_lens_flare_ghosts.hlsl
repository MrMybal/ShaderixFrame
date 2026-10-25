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
//@name Lens Flare Ghosts
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Lens Flare Ghosts","nextId":13,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"GhostScale","op":"param","pos":[40, 330],"text":"Ghost Scale","value":[1.20000005, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":16,"min":0,"name":"Threshold","op":"param","pos":[40, 440],"text":"Threshold","value":[0.75, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":4,"min":0.00100000005,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.25, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":32,"min":0,"name":"Intensity","op":"param","pos":[40, 660],"text":"Intensity","value":[1.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 770],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":9,"in":[],"index":1,"name":"GhostColorA","op":"paramcolor","pos":[40, 880],"text":"Ghost Color A","value":[1, 0.200000003, 0.0500000007, 1]},{"id":10,"in":[],"index":1,"name":"GhostColorB","op":"paramcolor","pos":[40, 990],"text":"Ghost Color B","value":[0.0500000007, 0.300000012, 1, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Lens Flare Ghosts","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["GhostScale",1],["Threshold",1],["Softness",1],["Intensity",1],["Center",3],["GhostColorA",3],["GhostColorB",3]],"pos":[420, 40],"text":"    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);\n    float2 p = UV - Center.xy;\n    float2 g1 = saturate(Center.xy - p * GhostScale);\n    float2 g2 = saturate(Center.xy + p * GhostScale * 0.55);\n    float3 a = Texture2DSample(CyInput, CyInputSampler, g1).rgb;\n    float3 b = Texture2DSample(CyInput, CyInputSampler, g2).rgb;\n    float la = dot(a, float3(.2126, .7152, .0722));\n    float lb = dot(b, float3(.2126, .7152, .0722));\n    a *= saturate((la - Threshold) / max(Softness, 0.001));\n    b *= saturate((lb - Threshold) / max(Softness, 0.001));\n    float3 ghosts = (a * GhostColorA.rgb + b * GhostColorB.rgb) * Intensity;\n    return float4(src.rgb + ghosts, src.a);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_GhostScale float 1.2 0.05 8 "Ghost Scale"
//@param sx_Threshold float 0.75 0 16 "Threshold"
//@param sx_Softness float 0.25 0.001 4 "Softness"
//@param sx_Intensity float 1.5 0 32 "Intensity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_GhostColorA color 1 0.2 0.05 1 "Ghost Color A"
//@param sx_GhostColorB color 0.05 0.3 1 1 "Ghost Color B"
// Imported preset (Effect / Color & Tone): Extracts bright regions into mirrored chromatic lens ghosts.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_GhostScale, float sx_arg_Threshold, float sx_arg_Softness, float sx_arg_Intensity, float3 sx_arg_Center, float3 sx_arg_GhostColorA, float3 sx_arg_GhostColorB)
{
    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);
    float2 p = UV - sx_arg_Center.xy;
    float2 g1 = saturate(sx_arg_Center.xy - p * sx_arg_GhostScale);
    float2 g2 = saturate(sx_arg_Center.xy + p * sx_arg_GhostScale * 0.55);
    float3 a = Texture2DSample(CyInput, CyInputSampler, g1).rgb;
    float3 b = Texture2DSample(CyInput, CyInputSampler, g2).rgb;
    float la = dot(a, float3(.2126, .7152, .0722));
    float lb = dot(b, float3(.2126, .7152, .0722));
    a *= saturate((la - sx_arg_Threshold) / max(sx_arg_Softness, 0.001));
    b *= saturate((lb - sx_arg_Threshold) / max(sx_arg_Softness, 0.001));
    float3 ghosts = (a * sx_arg_GhostColorA.rgb + b * sx_arg_GhostColorB.rgb) * sx_arg_Intensity;
    return float4(src.rgb + ghosts, src.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_GhostScale, sx_Threshold, sx_Softness, sx_Intensity, float3(sx_Center, 0.0), sx_GhostColorA.rgb, sx_GhostColorB.rgb);
}

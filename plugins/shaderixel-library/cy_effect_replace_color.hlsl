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
//@name Replace Color
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Replace Color","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Tolerance","op":"param","pos":[40, 330],"text":"Tolerance","value":[0.180000007, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.119999997, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"SourceColor","op":"paramcolor","pos":[40, 660],"text":"Source Color","value":[1, 0, 0, 1]},{"id":8,"in":[],"index":1,"name":"TargetColor","op":"paramcolor","pos":[40, 770],"text":"Target Color","value":[0, 0.400000006, 1, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Replace Color","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Tolerance",1],["Softness",1],["Strength",1],["SourceColor",3],["TargetColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float d = length(saturate(s.rgb) - SourceColor.rgb);\n    float m = 1.0 - smoothstep(Tolerance, Tolerance + max(Softness, 0.0001), d);\n    float sourceLum = max(dot(SourceColor.rgb, float3(.2126, .7152, .0722)), 0.0001);\n    float pixelLum = dot(s.rgb, float3(.2126, .7152, .0722));\n    float3 replacement = TargetColor.rgb * (pixelLum / sourceLum);\n    return float4(lerp(s.rgb, replacement, m * saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param sx_Tolerance float 0.18 0 1 "Tolerance"
//@param sx_Softness float 0.12 0.0001 1 "Softness"
//@param sx_Strength float 1 0 1 "Strength"
//@param sx_SourceColor color 1 0 0 1 "Source Color"
//@param sx_TargetColor color 0 0.4 1 1 "Target Color"
// Imported preset (Effect / Color & Tone): Replaces a selected color while preserving source shading.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Tolerance, float sx_arg_Softness, float sx_arg_Strength, float3 sx_arg_SourceColor, float3 sx_arg_TargetColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float d = length(saturate(s.rgb) - sx_arg_SourceColor.rgb);
    float m = 1.0 - smoothstep(sx_arg_Tolerance, sx_arg_Tolerance + max(sx_arg_Softness, 0.0001), d);
    float sourceLum = max(dot(sx_arg_SourceColor.rgb, float3(.2126, .7152, .0722)), 0.0001);
    float pixelLum = dot(s.rgb, float3(.2126, .7152, .0722));
    float3 replacement = sx_arg_TargetColor.rgb * (pixelLum / sourceLum);
    return float4(lerp(s.rgb, replacement, m * saturate(sx_arg_Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Tolerance, sx_Softness, sx_Strength, sx_SourceColor.rgb, sx_TargetColor.rgb);
}

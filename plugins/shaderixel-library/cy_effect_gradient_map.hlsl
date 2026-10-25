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
//@name Gradient Map
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Gradient Map","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.99000001,"min":0.00999999978,"name":"Midpoint","op":"param","pos":[40, 330],"text":"Midpoint","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ShadowColor","op":"paramcolor","pos":[40, 550],"text":"Shadow Color","value":[0, 0, 0, 1]},{"id":7,"in":[],"index":1,"name":"MidColor","op":"paramcolor","pos":[40, 660],"text":"Mid Color","value":[0.5, 0.5, 0.5, 1]},{"id":8,"in":[],"index":1,"name":"HighlightColor","op":"paramcolor","pos":[40, 770],"text":"Highlight Color","value":[1, 1, 1, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Gradient Map","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Midpoint",1],["Strength",1],["ShadowColor",3],["MidColor",3],["HighlightColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = saturate(dot(s.rgb, float3(.2126, .7152, .0722)));\n    float m = saturate(Midpoint);\n    float3 low = lerp(ShadowColor.rgb, MidColor.rgb, saturate(l / max(m, 0.0001)));\n    float3 high = lerp(MidColor.rgb, HighlightColor.rgb, saturate((l - m) / max(1.0 - m, 0.0001)));\n    float3 mapped = lerp(low, high, step(m, l));\n    return float4(lerp(s.rgb, mapped, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param sx_Midpoint float 0.5 0.01 0.99 "Midpoint"
//@param sx_Strength float 1 0 1 "Strength"
//@param sx_ShadowColor color 0 0 0 1 "Shadow Color"
//@param sx_MidColor color 0.5 0.5 0.5 1 "Mid Color"
//@param sx_HighlightColor color 1 1 1 1 "Highlight Color"
// Imported preset (Effect / Color & Tone): Maps luminance through shadow, midtone, and highlight HDR colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Midpoint, float sx_arg_Strength, float3 sx_arg_ShadowColor, float3 sx_arg_MidColor, float3 sx_arg_HighlightColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = saturate(dot(s.rgb, float3(.2126, .7152, .0722)));
    float m = saturate(sx_arg_Midpoint);
    float3 low = lerp(sx_arg_ShadowColor.rgb, sx_arg_MidColor.rgb, saturate(l / max(m, 0.0001)));
    float3 high = lerp(sx_arg_MidColor.rgb, sx_arg_HighlightColor.rgb, saturate((l - m) / max(1.0 - m, 0.0001)));
    float3 mapped = lerp(low, high, step(m, l));
    return float4(lerp(s.rgb, mapped, saturate(sx_arg_Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Midpoint, sx_Strength, sx_ShadowColor.rgb, sx_MidColor.rgb, sx_HighlightColor.rgb);
}

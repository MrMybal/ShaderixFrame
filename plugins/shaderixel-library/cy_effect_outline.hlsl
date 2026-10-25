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
//@name Outline
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Outline","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":128,"min":0.25,"name":"Thickness","op":"param","pos":[40, 330],"text":"Thickness","value":[2, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Inner","op":"param","pos":[40, 440],"text":"Inner","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 550],"text":"Opacity","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"OutlineColor","op":"paramcolor","pos":[40, 660],"text":"Outline Color","value":[1, 1, 1, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Outline","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Thickness",1],["Inner",1],["Opacity",1],["OutlineColor",3]],"pos":[420, 40],"text":"    float2 t = Thickness / max(CyCanvasSize.xy, 1.0);\n    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float a0 = Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).a;\n    float a1 = Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).a;\n    float a2 = Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).a;\n    float a3 = Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).a;\n    float expanded = max(max(a0, a1), max(a2, a3));\n    float contracted = min(min(a0, a1), min(a2, a3));\n    float edge = lerp(saturate(expanded - s.a), saturate(s.a - contracted), saturate(Inner));\n    float alpha = edge * Opacity * (1.0 - s.a * (1.0 - Inner));\n    return float4(s.rgb + OutlineColor.rgb * alpha, s.a + alpha);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Thickness float 2 0.25 128 "Thickness"
//@param sx_Inner float 0 0 1 "Inner"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_OutlineColor color 1 1 1 1 "Outline Color"
// Imported preset (Effect / Detail): Builds an inner or outer outline from source alpha.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Thickness, float sx_arg_Inner, float sx_arg_Opacity, float3 sx_arg_OutlineColor)
{
    float2 t = sx_arg_Thickness / max(CyCanvasSize.xy, 1.0);
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float a0 = Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).a;
    float a1 = Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).a;
    float a2 = Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).a;
    float a3 = Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).a;
    float expanded = max(max(a0, a1), max(a2, a3));
    float contracted = min(min(a0, a1), min(a2, a3));
    float edge = lerp(saturate(expanded - s.a), saturate(s.a - contracted), saturate(sx_arg_Inner));
    float alpha = edge * sx_arg_Opacity * (1.0 - s.a * (1.0 - sx_arg_Inner));
    return float4(s.rgb + sx_arg_OutlineColor.rgb * alpha, s.a + alpha);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Thickness, sx_Inner, sx_Opacity, sx_OutlineColor.rgb);
}

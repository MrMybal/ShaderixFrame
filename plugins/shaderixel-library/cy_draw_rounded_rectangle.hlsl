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
//@name Rounded Rectangle
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Rounded Rectangle","nextId":13,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":0.5,"min":0,"name":"CornerRadius","op":"param","pos":[40, 220],"text":"Corner Radius","value":[0.0500000007, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"BorderWidth","op":"param","pos":[40, 330],"text":"Border Width","value":[0.0199999996, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.00300000003, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Outline","op":"param","pos":[40, 550],"text":"Outline","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 660],"text":"Opacity","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 770],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":9,"in":[],"index":1,"name":"HalfSize","op":"param2","pos":[40, 880],"text":"Half Size","value":[0.300000012, 0.200000003, 0, 0]},{"id":10,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 990],"text":"Color","value":[1, 1, 1, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Rounded Rectangle","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["CornerRadius",1],["BorderWidth",1],["Softness",1],["Outline",1],["Opacity",1],["Center",3],["HalfSize",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = abs(UV - Center.xy) - HalfSize.xy + CornerRadius;\n    float d = min(max(p.x, p.y), 0.0) + length(max(p, 0.0)) - CornerRadius;\n    float fill = 1.0 - smoothstep(0.0, Softness, d);\n    float inside = 1.0 - smoothstep(-BorderWidth, -BorderWidth + Softness, d);\n    float a = lerp(fill, saturate(fill - inside), saturate(Outline));\n    return float4(Color.rgb, a * Opacity);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_CornerRadius float 0.05 0 0.5 "Corner Radius"
//@param sx_BorderWidth float 0.02 0.001 0.5 "Border Width"
//@param sx_Softness float 0.003 0.0001 0.1 "Softness"
//@param sx_Outline float 0 0 1 "Outline"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_HalfSize float2 0.3 0.2 0 1 "Half Size"
//@param sx_Color color 1 1 1 1 "Color"
// Imported preset (Draw / Shape & Gradient): Signed-distance rounded rectangle with border support.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_CornerRadius, float sx_arg_BorderWidth, float sx_arg_Softness, float sx_arg_Outline, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_HalfSize, float3 sx_arg_Color)
{
    float2 p = abs(UV - sx_arg_Center.xy) - sx_arg_HalfSize.xy + sx_arg_CornerRadius;
    float d = min(max(p.x, p.y), 0.0) + length(max(p, 0.0)) - sx_arg_CornerRadius;
    float fill = 1.0 - smoothstep(0.0, sx_arg_Softness, d);
    float inside = 1.0 - smoothstep(-sx_arg_BorderWidth, -sx_arg_BorderWidth + sx_arg_Softness, d);
    float a = lerp(fill, saturate(fill - inside), saturate(sx_arg_Outline));
    return float4(sx_arg_Color.rgb, a * sx_arg_Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_CornerRadius, sx_BorderWidth, sx_Softness, sx_Outline, sx_Opacity, float3(sx_Center, 0.0), float3(sx_HalfSize, 0.0), sx_Color.rgb);
}

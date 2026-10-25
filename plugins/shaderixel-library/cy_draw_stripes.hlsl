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
//@name Stripes
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Stripes","nextId":14,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":512,"min":1,"name":"Frequency","op":"param","pos":[40, 220],"text":"Frequency","value":[16, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 330],"text":"Angle","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":10,"min":-10,"name":"Phase","op":"param","pos":[40, 440],"text":"Phase","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.99000001,"min":0.00999999978,"name":"Duty","op":"param","pos":[40, 550],"text":"Duty","value":[0.5, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.00999999978, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"OpacityA","op":"param","pos":[40, 770],"text":"Opacity A","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"OpacityB","op":"param","pos":[40, 880],"text":"Opacity B","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 990],"text":"Color A","value":[1, 1, 1, 1]},{"id":11,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 1100],"text":"Color B","value":[0, 0, 0, 1]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Stripes","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Angle",1],["Phase",1],["Duty",1],["Softness",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV - 0.5;\n    float c = cos(Angle);\n    float s = sin(Angle);\n    float u = dot(p, float2(c, s)) * Frequency + Phase;\n    float v = frac(u);\n    float edge = max(Softness, 0.0001);\n    float m = 1.0 - smoothstep(Duty - edge, Duty + edge, v);\n    return float4(lerp(ColorB.rgb, ColorA.rgb, m), lerp(OpacityB, OpacityA, m));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Frequency float 16 1 512 "Frequency"
//@param sx_Angle float 0 -6.283 6.283 "Angle"
//@param sx_Phase float 0 -10 10 "Phase"
//@param sx_Duty float 0.5 0.01 0.99 "Duty"
//@param sx_Softness float 0.01 0.0001 0.25 "Softness"
//@param sx_OpacityA float 1 0 1 "Opacity A"
//@param sx_OpacityB float 1 0 1 "Opacity B"
//@param sx_ColorA color 1 1 1 1 "Color A"
//@param sx_ColorB color 0 0 0 1 "Color B"
// Imported preset (Draw / Shape & Gradient): Rotatable antialiased stripe generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Frequency, float sx_arg_Angle, float sx_arg_Phase, float sx_arg_Duty, float sx_arg_Softness, float sx_arg_OpacityA, float sx_arg_OpacityB, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 p = UV - 0.5;
    float c = cos(sx_arg_Angle);
    float s = sin(sx_arg_Angle);
    float u = dot(p, float2(c, s)) * sx_arg_Frequency + sx_arg_Phase;
    float v = frac(u);
    float edge = max(sx_arg_Softness, 0.0001);
    float m = 1.0 - smoothstep(sx_arg_Duty - edge, sx_arg_Duty + edge, v);
    return float4(lerp(sx_arg_ColorB.rgb, sx_arg_ColorA.rgb, m), lerp(sx_arg_OpacityB, sx_arg_OpacityA, m));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Frequency, sx_Angle, sx_Phase, sx_Duty, sx_Softness, sx_OpacityA, sx_OpacityB, sx_ColorA.rgb, sx_ColorB.rgb);
}

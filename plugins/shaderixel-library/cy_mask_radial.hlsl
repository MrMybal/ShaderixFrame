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
//@name Radial
//@category Masques
//@graph {"category":"Masques","inputs":0,"name":"Radial","nextId":10,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"Radius","op":"param","pos":[40, 220],"text":"Radius","value":[0.349999994, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 330],"text":"Softness","value":[0.100000001, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 440],"text":"Aspect","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Invert","op":"param","pos":[40, 550],"text":"Invert","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 660],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Radial","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Softness",1],["Aspect",1],["Invert",1],["Center",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float d = length(p);\n    float v = 1.0 - smoothstep(Radius - Softness, Radius + Softness, d);\n    v = lerp(v, 1.0 - v, saturate(Invert));\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Radius float 0.35 0.001 2 "Radius"
//@param sx_Softness float 0.1 0.0001 1 "Softness"
//@param sx_Aspect float 1 0.01 8 "Aspect"
//@param sx_Invert float 0 0 1 "Invert"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Mask / Mask): Configurable radial mask independent of source color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Radius, float sx_arg_Softness, float sx_arg_Aspect, float sx_arg_Invert, float3 sx_arg_Center)
{
    float2 p = UV - sx_arg_Center.xy;
    p.x *= sx_arg_Aspect;
    float d = length(p);
    float v = 1.0 - smoothstep(sx_arg_Radius - sx_arg_Softness, sx_arg_Radius + sx_arg_Softness, d);
    v = lerp(v, 1.0 - v, saturate(sx_arg_Invert));
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Radius, sx_Softness, sx_Aspect, sx_Invert, float3(sx_Center, 0.0));
}

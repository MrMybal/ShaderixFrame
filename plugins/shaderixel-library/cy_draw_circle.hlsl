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
//@name Circle
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Circle","nextId":13,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"Radius","op":"param","pos":[40, 220],"text":"Radius","value":[0.300000012, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"Thickness","op":"param","pos":[40, 330],"text":"Thickness","value":[0.0399999991, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.00499999989, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 550],"text":"Aspect","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Outline","op":"param","pos":[40, 660],"text":"Outline","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 770],"text":"Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 880],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":10,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 990],"text":"Color","value":[1, 1, 1, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Circle","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Thickness",1],["Softness",1],["Aspect",1],["Outline",1],["Opacity",1],["Center",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float d = length(p);\n    float outer = 1.0 - smoothstep(Radius, Radius + Softness, d);\n    float inner = 1.0 - smoothstep(max(Radius - Thickness, 0.0), max(Radius - Thickness, 0.0) + Softness, d);\n    float a = lerp(outer, saturate(outer - inner), saturate(Outline));\n    return float4(Color.rgb, a * Opacity);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Radius float 0.3 0.001 2 "Radius"
//@param sx_Thickness float 0.04 0.001 1 "Thickness"
//@param sx_Softness float 0.005 0.0001 0.25 "Softness"
//@param sx_Aspect float 1 0.01 8 "Aspect"
//@param sx_Outline float 0 0 1 "Outline"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_Color color 1 1 1 1 "Color"
// Imported preset (Draw / Shape & Gradient): Antialiased filled or outlined circle with HDR color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Radius, float sx_arg_Thickness, float sx_arg_Softness, float sx_arg_Aspect, float sx_arg_Outline, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_Color)
{
    float2 p = UV - sx_arg_Center.xy;
    p.x *= sx_arg_Aspect;
    float d = length(p);
    float outer = 1.0 - smoothstep(sx_arg_Radius, sx_arg_Radius + sx_arg_Softness, d);
    float inner = 1.0 - smoothstep(max(sx_arg_Radius - sx_arg_Thickness, 0.0), max(sx_arg_Radius - sx_arg_Thickness, 0.0) + sx_arg_Softness, d);
    float a = lerp(outer, saturate(outer - inner), saturate(sx_arg_Outline));
    return float4(sx_arg_Color.rgb, a * sx_arg_Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Radius, sx_Thickness, sx_Softness, sx_Aspect, sx_Outline, sx_Opacity, float3(sx_Center, 0.0), sx_Color.rgb);
}

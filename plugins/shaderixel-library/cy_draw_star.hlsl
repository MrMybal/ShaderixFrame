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
//@name Star
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Star","nextId":15,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":64,"min":2,"name":"Points","op":"param","pos":[40, 220],"text":"Points","value":[5, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"InnerRadius","op":"param","pos":[40, 330],"text":"Inner Radius","value":[0.119999997, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"OuterRadius","op":"param","pos":[40, 440],"text":"Outer Radius","value":[0.319999993, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"PointSharpness","op":"param","pos":[40, 550],"text":"Point Sharpness","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 660],"text":"Rotation","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 770],"text":"Aspect","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 880],"text":"Softness","value":[0.00400000019, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 990],"text":"Opacity","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1100],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":12,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 1210],"text":"Color","value":[1, 1, 1, 1]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Star","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Points",1],["InnerRadius",1],["OuterRadius",1],["PointSharpness",1],["Rotation",1],["Aspect",1],["Softness",1],["Opacity",1],["Center",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float a = atan2(p.y, p.x) + Rotation;\n    float count = max(Points, 2.0);\n    float wave = 0.5 + 0.5 * cos(a * count);\n    float r = lerp(InnerRadius, OuterRadius, pow(wave, max(PointSharpness, 0.05)));\n    float mask = 1.0 - smoothstep(r, r + Softness, length(p));\n    return float4(Color.rgb, mask * Opacity);","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Points float 5 2 64 "Points"
//@param sx_InnerRadius float 0.12 0.001 2 "Inner Radius"
//@param sx_OuterRadius float 0.32 0.001 2 "Outer Radius"
//@param sx_PointSharpness float 1 0.05 8 "Point Sharpness"
//@param sx_Rotation float 0 -6.283 6.283 "Rotation"
//@param sx_Aspect float 1 0.01 8 "Aspect"
//@param sx_Softness float 0.004 0.0001 0.1 "Softness"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_Color color 1 1 1 1 "Color"
// Imported preset (Draw / Shape & Gradient): Procedural star or flower shape with controllable point count.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Points, float sx_arg_InnerRadius, float sx_arg_OuterRadius, float sx_arg_PointSharpness, float sx_arg_Rotation, float sx_arg_Aspect, float sx_arg_Softness, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_Color)
{
    float2 p = UV - sx_arg_Center.xy;
    p.x *= sx_arg_Aspect;
    float a = atan2(p.y, p.x) + sx_arg_Rotation;
    float count = max(sx_arg_Points, 2.0);
    float wave = 0.5 + 0.5 * cos(a * count);
    float r = lerp(sx_arg_InnerRadius, sx_arg_OuterRadius, pow(wave, max(sx_arg_PointSharpness, 0.05)));
    float mask = 1.0 - smoothstep(r, r + sx_arg_Softness, length(p));
    return float4(sx_arg_Color.rgb, mask * sx_arg_Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Points, sx_InnerRadius, sx_OuterRadius, sx_PointSharpness, sx_Rotation, sx_Aspect, sx_Softness, sx_Opacity, float3(sx_Center, 0.0), sx_Color.rgb);
}

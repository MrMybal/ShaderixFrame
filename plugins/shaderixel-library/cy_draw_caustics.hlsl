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
//@name Caustics
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Caustics","nextId":15,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":64,"min":0.00999999978,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[3.5, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":0,"name":"Threshold","op":"param","pos":[40, 440],"text":"Threshold","value":[0.699999988, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":16,"min":0,"name":"Contrast","op":"param","pos":[40, 550],"text":"Contrast","value":[2.20000005, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":16,"min":0.0500000007,"name":"Sharpness","op":"param","pos":[40, 660],"text":"Sharpness","value":[2.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":64,"min":0,"name":"Intensity","op":"param","pos":[40, 770],"text":"Intensity","value":[2, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 880],"text":"Opacity","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"BackgroundOpacity","op":"param","pos":[40, 990],"text":"Background Opacity","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"DeepColor","op":"paramcolor","pos":[40, 1100],"text":"Deep Color","value":[0, 0.0149999997, 0.0399999991, 1]},{"id":12,"in":[],"index":1,"name":"LightColor","op":"paramcolor","pos":[40, 1210],"text":"Light Color","value":[0.0500000007, 0.850000024, 2.5, 1]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Caustics","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Threshold",1],["Contrast",1],["Sharpness",1],["Intensity",1],["Opacity",1],["BackgroundOpacity",1],["DeepColor",3],["LightColor",3]],"pos":[420, 40],"text":"    float2 p = (UV - 0.5) * max(Scale, 0.01);\n    float v = 0.0;\n    float amp = 0.55;\n    [unroll] for (int i = 0; i < 5; i++) {\n        p = float2(p.x * 1.62 - p.y * 1.17, p.x * 1.17 + p.y * 1.62) + float2(Seed * .17 + i * 1.31, Seed * .11 - i * .73);\n        float w = sin(p.x + sin(p.y * 1.37)) + sin(p.y + sin(p.x * 1.21));\n        v += abs(w) * amp;\n        amp *= 0.52;\n    }\n    v = pow(saturate(1.0 - abs(v - Threshold) * Contrast), max(Sharpness, 0.05));\n    float3 rgb = lerp(DeepColor.rgb, LightColor.rgb, v) * Intensity;\n    return float4(rgb, lerp(BackgroundOpacity, Opacity, v));","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Scale float 3.5 0.01 64 "Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Threshold float 0.7 0 4 "Threshold"
//@param sx_Contrast float 2.2 0 16 "Contrast"
//@param sx_Sharpness float 2.5 0.05 16 "Sharpness"
//@param sx_Intensity float 2 0 64 "Intensity"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_BackgroundOpacity float 1 0 1 "Background Opacity"
//@param sx_DeepColor color 0 0.015 0.04 1 "Deep Color"
//@param sx_LightColor color 0.05 0.85 2.5 1 "Light Color"
// Imported preset (Draw / Light & Space): Animated-looking layered caustic field for water light, energy, and emissive masks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Seed, float sx_arg_Threshold, float sx_arg_Contrast, float sx_arg_Sharpness, float sx_arg_Intensity, float sx_arg_Opacity, float sx_arg_BackgroundOpacity, float3 sx_arg_DeepColor, float3 sx_arg_LightColor)
{
    float2 p = (UV - 0.5) * max(sx_arg_Scale, 0.01);
    float v = 0.0;
    float amp = 0.55;
    [unroll] for (int i = 0; i < 5; i++) {
        p = float2(p.x * 1.62 - p.y * 1.17, p.x * 1.17 + p.y * 1.62) + float2(sx_arg_Seed * .17 + i * 1.31, sx_arg_Seed * .11 - i * .73);
        float w = sin(p.x + sin(p.y * 1.37)) + sin(p.y + sin(p.x * 1.21));
        v += abs(w) * amp;
        amp *= 0.52;
    }
    v = pow(saturate(1.0 - abs(v - sx_arg_Threshold) * sx_arg_Contrast), max(sx_arg_Sharpness, 0.05));
    float3 rgb = lerp(sx_arg_DeepColor.rgb, sx_arg_LightColor.rgb, v) * sx_arg_Intensity;
    return float4(rgb, lerp(sx_arg_BackgroundOpacity, sx_arg_Opacity, v));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Seed, sx_Threshold, sx_Contrast, sx_Sharpness, sx_Intensity, sx_Opacity, sx_BackgroundOpacity, sx_DeepColor.rgb, sx_LightColor.rgb);
}

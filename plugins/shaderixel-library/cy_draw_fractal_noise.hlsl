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
//@name Fractal Noise
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Fractal Noise","nextId":14,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":512,"min":0.100000001,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[6, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":1.10000002,"name":"Lacunarity","op":"param","pos":[40, 440],"text":"Lacunarity","value":[2, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.899999976,"min":0.100000001,"name":"Gain","op":"param","pos":[40, 550],"text":"Gain","value":[0.5, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":8,"min":0,"name":"Contrast","op":"param","pos":[40, 660],"text":"Contrast","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"OpacityA","op":"param","pos":[40, 770],"text":"Opacity A","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"OpacityB","op":"param","pos":[40, 880],"text":"Opacity B","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 990],"text":"Color A","value":[0, 0, 0, 1]},{"id":11,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 1100],"text":"Color B","value":[1, 1, 1, 1]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Fractal Noise","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Lacunarity",1],["Gain",1],["Contrast",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Scale, 1.0) + Seed;\n    float n = 0.0;\n    float amp = 0.5714;\n    float sum = 0.0;\n    [unroll] for (int o = 0; o < 5; o++) {\n        float2 i = floor(p);\n        float2 f = frac(p);\n        f = f * f * (3.0 - 2.0 * f);\n        float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);\n        float b = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);\n        float c = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);\n        float d = frac(sin(dot(i + 1.0, float2(127.1, 311.7))) * 43758.5453);\n        n += lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * amp;\n        sum += amp;\n        p = p * Lacunarity + 13.17;\n        amp *= Gain;\n    }\n    n = saturate((n / max(sum, 0.001) - 0.5) * Contrast + 0.5);\n    return float4(lerp(ColorA.rgb, ColorB.rgb, n), lerp(OpacityA, OpacityB, n));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Scale float 6 0.1 512 "Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Lacunarity float 2 1.1 4 "Lacunarity"
//@param sx_Gain float 0.5 0.1 0.9 "Gain"
//@param sx_Contrast float 1 0 8 "Contrast"
//@param sx_OpacityA float 1 0 1 "Opacity A"
//@param sx_OpacityB float 1 0 1 "Opacity B"
//@param sx_ColorA color 0 0 0 1 "Color A"
//@param sx_ColorB color 1 1 1 1 "Color B"
// Imported preset (Draw / Noise): Multi-octave procedural noise for clouds, masks, and roughness.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Seed, float sx_arg_Lacunarity, float sx_arg_Gain, float sx_arg_Contrast, float sx_arg_OpacityA, float sx_arg_OpacityB, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 p = UV * max(sx_arg_Scale, 1.0) + sx_arg_Seed;
    float n = 0.0;
    float amp = 0.5714;
    float sum = 0.0;
    [unroll] for (int o = 0; o < 5; o++) {
        float2 i = floor(p);
        float2 f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
        float b = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
        float c = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
        float d = frac(sin(dot(i + 1.0, float2(127.1, 311.7))) * 43758.5453);
        n += lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * amp;
        sum += amp;
        p = p * sx_arg_Lacunarity + 13.17;
        amp *= sx_arg_Gain;
    }
    n = saturate((n / max(sum, 0.001) - 0.5) * sx_arg_Contrast + 0.5);
    return float4(lerp(sx_arg_ColorA.rgb, sx_arg_ColorB.rgb, n), lerp(sx_arg_OpacityA, sx_arg_OpacityB, n));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Seed, sx_Lacunarity, sx_Gain, sx_Contrast, sx_OpacityA, sx_OpacityB, sx_ColorA.rgb, sx_ColorB.rgb);
}

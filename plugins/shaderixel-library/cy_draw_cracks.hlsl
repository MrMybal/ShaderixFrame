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
//@name Cracks
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Cracks","nextId":17,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":256,"min":1,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[14, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"Width","op":"param","pos":[40, 440],"text":"Width","value":[0.0350000001, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.200000003,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.0120000001, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Breakup","op":"param","pos":[40, 660],"text":"Breakup","value":[0.219999999, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"BreakupAmount","op":"param","pos":[40, 770],"text":"Breakup Amount","value":[0.180000007, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"FillThreshold","op":"param","pos":[40, 880],"text":"Fill Threshold","value":[0.879999995, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"FillAmount","op":"param","pos":[40, 990],"text":"Fill Amount","value":[0, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"CrackOpacity","op":"param","pos":[40, 1100],"text":"Crack Opacity","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"BackgroundOpacity","op":"param","pos":[40, 1210],"text":"Background Opacity","value":[0, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"CrackColor","op":"paramcolor","pos":[40, 1320],"text":"Crack Color","value":[1, 1, 1, 1]},{"id":14,"in":[],"index":1,"name":"BackgroundColor","op":"paramcolor","pos":[40, 1430],"text":"Background Color","value":[0, 0, 0, 1]},{"id":15,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0]],"name":"Cracks","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Width",1],["Softness",1],["Breakup",1],["BreakupAmount",1],["FillThreshold",1],["FillAmount",1],["CrackOpacity",1],["BackgroundOpacity",1],["CrackColor",3],["BackgroundColor",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Scale, 1.0);\n    float2 ip = floor(p), fp = frac(p);\n    float nearest = 10.0, second = 10.0, cellValue = 0.0;\n    [unroll] for (int y = -1; y <= 1; y++) {\n        [unroll] for (int x = -1; x <= 1; x++) {\n            float2 o = float2(x, y), cell = ip + o;\n            float2 rnd = frac(sin(float2(dot(cell, float2(127.1, 311.7)), dot(cell, float2(269.5, 183.3))) + Seed) * 43758.5453);\n            float d = length(o + rnd - fp);\n            if (d < nearest) {\n                second = nearest;\n                nearest = d;\n                cellValue = rnd.x;\n            } else if (d < second) second = d;\n        }\n    }\n    float edge = second - nearest;\n    float crack = 1.0 - smoothstep(Width, Width + Softness, edge);\n    float breakup = step(Breakup, frac(sin((ip.x + ip.y + cellValue) * 37.13 + Seed) * 43758.5453));\n    crack *= lerp(1.0, breakup, BreakupAmount);\n    float fill = step(FillThreshold, cellValue) * FillAmount;\n    float m = max(crack, fill);\n    return float4(lerp(BackgroundColor.rgb, CrackColor.rgb, m), lerp(BackgroundOpacity, CrackOpacity, m));","value":[0, 0, 0, 0]},{"id":16,"in":[[15, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Scale float 14 1 256 "Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Width float 0.035 0.001 0.5 "Width"
//@param sx_Softness float 0.012 0.0001 0.2 "Softness"
//@param sx_Breakup float 0.22 0 1 "Breakup"
//@param sx_BreakupAmount float 0.18 0 1 "Breakup Amount"
//@param sx_FillThreshold float 0.88 0 1 "Fill Threshold"
//@param sx_FillAmount float 0 0 1 "Fill Amount"
//@param sx_CrackOpacity float 1 0 1 "Crack Opacity"
//@param sx_BackgroundOpacity float 0 0 1 "Background Opacity"
//@param sx_CrackColor color 1 1 1 1 "Crack Color"
//@param sx_BackgroundColor color 0 0 0 1 "Background Color"
// Imported preset (Draw / Surface): Voronoi fracture network with branching breakup, variable width, and optional filled cells.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Seed, float sx_arg_Width, float sx_arg_Softness, float sx_arg_Breakup, float sx_arg_BreakupAmount, float sx_arg_FillThreshold, float sx_arg_FillAmount, float sx_arg_CrackOpacity, float sx_arg_BackgroundOpacity, float3 sx_arg_CrackColor, float3 sx_arg_BackgroundColor)
{
    float2 p = UV * max(sx_arg_Scale, 1.0);
    float2 ip = floor(p), fp = frac(p);
    float nearest = 10.0, second = 10.0, cellValue = 0.0;
    [unroll] for (int y = -1; y <= 1; y++) {
        [unroll] for (int x = -1; x <= 1; x++) {
            float2 o = float2(x, y), cell = ip + o;
            float2 rnd = frac(sin(float2(dot(cell, float2(127.1, 311.7)), dot(cell, float2(269.5, 183.3))) + sx_arg_Seed) * 43758.5453);
            float d = length(o + rnd - fp);
            if (d < nearest) {
                second = nearest;
                nearest = d;
                cellValue = rnd.x;
            } else if (d < second) second = d;
        }
    }
    float edge = second - nearest;
    float crack = 1.0 - smoothstep(sx_arg_Width, sx_arg_Width + sx_arg_Softness, edge);
    float breakup = step(sx_arg_Breakup, frac(sin((ip.x + ip.y + cellValue) * 37.13 + sx_arg_Seed) * 43758.5453));
    crack *= lerp(1.0, breakup, sx_arg_BreakupAmount);
    float fill = step(sx_arg_FillThreshold, cellValue) * sx_arg_FillAmount;
    float m = max(crack, fill);
    return float4(lerp(sx_arg_BackgroundColor.rgb, sx_arg_CrackColor.rgb, m), lerp(sx_arg_BackgroundOpacity, sx_arg_CrackOpacity, m));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Seed, sx_Width, sx_Softness, sx_Breakup, sx_BreakupAmount, sx_FillThreshold, sx_FillAmount, sx_CrackOpacity, sx_BackgroundOpacity, sx_CrackColor.rgb, sx_BackgroundColor.rgb);
}

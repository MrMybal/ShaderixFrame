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
//@name Film Grain
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Film Grain","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Amount","op":"param","pos":[40, 330],"text":"Amount","value":[0.0399999991, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":10000,"min":-10000,"name":"Seed","op":"param","pos":[40, 440],"text":"Seed","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Colored","op":"param","pos":[40, 550],"text":"Colored","value":[0, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Film Grain","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Seed",1],["Colored",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float2 p = floor(UV * CyCanvasSize.xy) + Seed;\n    float r = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;\n    float g = frac(sin(dot(p, float2(39.346, 11.135))) * 24634.6345) - 0.5;\n    float b = frac(sin(dot(p, float2(73.156, 52.235))) * 56445.2345) - 0.5;\n    float3 n = lerp(r.xxx, float3(r, g, b), saturate(Colored));\n    return float4(s.rgb + n * Amount, s.a);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param sx_Amount float 0.04 0 1 "Amount"
//@param sx_Seed float 0 -10000 10000 "Seed"
//@param sx_Colored float 0 0 1 "Colored"
// Imported preset (Effect / Stylize): Resolution-aware monochrome or colored film grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Amount, float sx_arg_Seed, float sx_arg_Colored)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float2 p = floor(UV * CyCanvasSize.xy) + sx_arg_Seed;
    float r = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
    float g = frac(sin(dot(p, float2(39.346, 11.135))) * 24634.6345) - 0.5;
    float b = frac(sin(dot(p, float2(73.156, 52.235))) * 56445.2345) - 0.5;
    float3 n = lerp(r.xxx, float3(r, g, b), saturate(sx_arg_Colored));
    return float4(s.rgb + n * sx_arg_Amount, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Amount, sx_Seed, sx_Colored);
}

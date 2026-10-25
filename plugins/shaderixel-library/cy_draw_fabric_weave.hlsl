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
//@name Fabric Weave
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Fabric Weave","nextId":13,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":512,"min":2,"name":"WarpThreads","op":"param","pos":[40, 220],"text":"Warp Threads","value":[48, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":512,"min":2,"name":"WeftThreads","op":"param","pos":[40, 330],"text":"Weft Threads","value":[48, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":16,"min":0.100000001,"name":"ThreadRoundness","op":"param","pos":[40, 440],"text":"Thread Roundness","value":[1.79999995, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.800000012,"min":0,"name":"Irregularity","op":"param","pos":[40, 550],"text":"Irregularity","value":[0.0799999982, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 660],"text":"Seed","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 770],"text":"Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"WarpColor","op":"paramcolor","pos":[40, 880],"text":"Warp Color","value":[0.0799999982, 0.180000007, 0.340000004, 1]},{"id":10,"in":[],"index":1,"name":"WeftColor","op":"paramcolor","pos":[40, 990],"text":"Weft Color","value":[0.550000012, 0.620000005, 0.699999988, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Fabric Weave","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["WarpThreads",1],["WeftThreads",1],["ThreadRoundness",1],["Irregularity",1],["Seed",1],["Opacity",1],["WarpColor",3],["WeftColor",3]],"pos":[420, 40],"text":"    float2 q = UV * float2(max(WarpThreads, 1.0), max(WeftThreads, 1.0));\n    float2 cell = floor(q);\n    float2 p = frac(q) - .5;\n    float over = fmod(cell.x + cell.y, 2.0);\n    float warp = pow(saturate(1.0 - abs(p.x) * 2.0), ThreadRoundness);\n    float weft = pow(saturate(1.0 - abs(p.y) * 2.0), ThreadRoundness);\n    float h = frac(sin(dot(cell + Seed, float2(127.1, 311.7))) * 43758.5453);\n    warp *= lerp(1.0 - Irregularity, 1.0 + Irregularity, h);\n    weft *= lerp(1.0 + Irregularity, 1.0 - Irregularity, h);\n    float3 rgb = lerp(WeftColor.rgb, WarpColor.rgb, lerp(weft, warp, over));\n    float shade = lerp(.55, 1.2, max(warp, weft));\n    return float4(rgb * shade, Opacity);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_WarpThreads float 48 2 512 "Warp Threads"
//@param sx_WeftThreads float 48 2 512 "Weft Threads"
//@param sx_ThreadRoundness float 1.8 0.1 16 "Thread Roundness"
//@param sx_Irregularity float 0.08 0 0.8 "Irregularity"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_WarpColor color 0.08 0.18 0.34 1 "Warp Color"
//@param sx_WeftColor color 0.55 0.62 0.7 1 "Weft Color"
// Imported preset (Draw / Surface): Procedural woven textile with over-under threads, fiber highlights, and weave irregularity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_WarpThreads, float sx_arg_WeftThreads, float sx_arg_ThreadRoundness, float sx_arg_Irregularity, float sx_arg_Seed, float sx_arg_Opacity, float3 sx_arg_WarpColor, float3 sx_arg_WeftColor)
{
    float2 q = UV * float2(max(sx_arg_WarpThreads, 1.0), max(sx_arg_WeftThreads, 1.0));
    float2 cell = floor(q);
    float2 p = frac(q) - .5;
    float over = fmod(cell.x + cell.y, 2.0);
    float warp = pow(saturate(1.0 - abs(p.x) * 2.0), sx_arg_ThreadRoundness);
    float weft = pow(saturate(1.0 - abs(p.y) * 2.0), sx_arg_ThreadRoundness);
    float h = frac(sin(dot(cell + sx_arg_Seed, float2(127.1, 311.7))) * 43758.5453);
    warp *= lerp(1.0 - sx_arg_Irregularity, 1.0 + sx_arg_Irregularity, h);
    weft *= lerp(1.0 + sx_arg_Irregularity, 1.0 - sx_arg_Irregularity, h);
    float3 rgb = lerp(sx_arg_WeftColor.rgb, sx_arg_WarpColor.rgb, lerp(weft, warp, over));
    float shade = lerp(.55, 1.2, max(warp, weft));
    return float4(rgb * shade, sx_arg_Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_WarpThreads, sx_WeftThreads, sx_ThreadRoundness, sx_Irregularity, sx_Seed, sx_Opacity, sx_WarpColor.rgb, sx_WeftColor.rgb);
}

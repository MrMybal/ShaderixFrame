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
//@name UV Transform
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"UV Transform","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 330],"text":"Rotation","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":16,"min":0.00999999978,"name":"Scale","op":"param","pos":[40, 440],"text":"Scale","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Pivot","op":"param2","pos":[40, 550],"text":"Pivot","value":[0.5, 0.5, 0, 0]},{"id":7,"in":[],"index":1,"name":"Offset","op":"param2","pos":[40, 660],"text":"Offset","value":[0, 0, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"UV Transform","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Rotation",1],["Scale",1],["Pivot",3],["Offset",3]],"pos":[420, 40],"text":"    float2 p = UV - Pivot.xy - Offset.xy;\n    float c = cos(Rotation);\n    float s = sin(Rotation);\n    p = mul(float2x2(c, -s, s, c), p) / max(Scale, 0.0001);\n    float2 q = Pivot.xy + p;\n    return Texture2DSample(CyInput, CyInputSampler, q);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Rotation float 0 -6.283 6.283 "Rotation"
//@param sx_Scale float 1 0.01 16 "Scale"
//@param sx_Pivot float2 0.5 0.5 0 1 "Pivot"
//@param sx_Offset float2 0 0 0 1 "Offset"
// Imported preset (Effect / Distort): Full-screen translate, rotate, scale, and pivot transform.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Rotation, float sx_arg_Scale, float3 sx_arg_Pivot, float3 sx_arg_Offset)
{
    float2 p = UV - sx_arg_Pivot.xy - sx_arg_Offset.xy;
    float c = cos(sx_arg_Rotation);
    float s = sin(sx_arg_Rotation);
    p = mul(float2x2(c, -s, s, c), p) / max(sx_arg_Scale, 0.0001);
    float2 q = sx_arg_Pivot.xy + p;
    return Texture2DSample(CyInput, CyInputSampler, q);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Rotation, sx_Scale, float3(sx_Pivot, 0.0), float3(sx_Offset, 0.0));
}

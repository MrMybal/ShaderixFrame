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
//@name Photo Filter
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Photo Filter","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Density","op":"param","pos":[40, 330],"text":"Density","value":[0.25, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"PreserveLuminosity","op":"param","pos":[40, 440],"text":"Preserve Luminosity","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"FilterColor","op":"paramcolor","pos":[40, 550],"text":"Filter Color","value":[1, 0.550000012, 0.200000003, 1]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Photo Filter","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Density",1],["PreserveLuminosity",1],["FilterColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float3 v = lerp(s.rgb, s.rgb * FilterColor.rgb, saturate(Density));\n    float vl = dot(v, float3(.2126, .7152, .0722));\n    v += PreserveLuminosity * (l - vl);\n    return float4(v, s.a);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param sx_Density float 0.25 0 1 "Density"
//@param sx_PreserveLuminosity float 1 0 1 "Preserve Luminosity"
//@param sx_FilterColor color 1 0.55 0.2 1 "Filter Color"
// Imported preset (Effect / Color & Tone): Photographic warming/cooling filter with density and optional luminance preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Density, float sx_arg_PreserveLuminosity, float3 sx_arg_FilterColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float3 v = lerp(s.rgb, s.rgb * sx_arg_FilterColor.rgb, saturate(sx_arg_Density));
    float vl = dot(v, float3(.2126, .7152, .0722));
    v += sx_arg_PreserveLuminosity * (l - vl);
    return float4(v, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Density, sx_PreserveLuminosity, sx_FilterColor.rgb);
}

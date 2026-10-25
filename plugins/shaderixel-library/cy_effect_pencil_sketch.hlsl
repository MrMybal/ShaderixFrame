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
//@name Pencil Sketch
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Pencil Sketch","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[1.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":0,"name":"EdgeStrength","op":"param","pos":[40, 440],"text":"Edge Strength","value":[5, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"EdgePower","op":"param","pos":[40, 550],"text":"Edge Power","value":[0.699999988, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2,"min":0,"name":"Shading","op":"param","pos":[40, 660],"text":"Shading","value":[0.449999988, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":4096,"min":2,"name":"GrainScale","op":"param","pos":[40, 770],"text":"Grain Scale","value":[512, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"GrainAmount","op":"param","pos":[40, 880],"text":"Grain Amount","value":[0.0799999982, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"PreserveColor","op":"param","pos":[40, 990],"text":"Preserve Color","value":[0, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 1100],"text":"Strength","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"PencilColor","op":"paramcolor","pos":[40, 1210],"text":"Pencil Color","value":[0.0149999997, 0.0120000001, 0.00999999978, 1]},{"id":13,"in":[],"index":1,"name":"PaperColor","op":"paramcolor","pos":[40, 1320],"text":"Paper Color","value":[0.899999976, 0.860000014, 0.74000001, 1]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Pencil Sketch","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["EdgeStrength",1],["EdgePower",1],["Shading",1],["GrainScale",1],["GrainAmount",1],["PreserveColor",1],["Strength",1],["PencilColor",3],["PaperColor",3]],"pos":[420, 40],"text":"    float2 t = Radius / max(CyCanvasSize.xy, 1.0);\n    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(c.rgb, float3(.2126, .7152, .0722));\n    float lx = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float ly = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, float3(.2126, .7152, .0722));\n    float edge = pow(saturate(length(float2(lx, ly)) * EdgeStrength), EdgePower);\n    float grain = frac(sin(dot(floor(UV * GrainScale), float2(12.9898, 78.233))) * 43758.5453);\n    float shade = saturate(1.0 - edge - (1.0 - l) * Shading - (grain - .5) * GrainAmount);\n    float3 mono = lerp(PencilColor.rgb, PaperColor.rgb, shade);\n    float3 outColor = lerp(mono, mono * c.rgb * 1.5, PreserveColor);\n    return float4(lerp(c.rgb, outColor, Strength), c.a);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Radius float 1.5 0.25 16 "Radius"
//@param sx_EdgeStrength float 5 0 64 "Edge Strength"
//@param sx_EdgePower float 0.7 0.05 8 "Edge Power"
//@param sx_Shading float 0.45 0 2 "Shading"
//@param sx_GrainScale float 512 2 4096 "Grain Scale"
//@param sx_GrainAmount float 0.08 0 1 "Grain Amount"
//@param sx_PreserveColor float 0 0 1 "Preserve Color"
//@param sx_Strength float 1 0 1 "Strength"
//@param sx_PencilColor color 0.015 0.012 0.01 1 "Pencil Color"
//@param sx_PaperColor color 0.9 0.86 0.74 1 "Paper Color"
// Imported preset (Effect / Stylize): Pencil-sketch conversion with multi-scale edges, paper tone, grain, and source-color retention.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Radius, float sx_arg_EdgeStrength, float sx_arg_EdgePower, float sx_arg_Shading, float sx_arg_GrainScale, float sx_arg_GrainAmount, float sx_arg_PreserveColor, float sx_arg_Strength, float3 sx_arg_PencilColor, float3 sx_arg_PaperColor)
{
    float2 t = sx_arg_Radius / max(CyCanvasSize.xy, 1.0);
    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(c.rgb, float3(.2126, .7152, .0722));
    float lx = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float ly = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, float3(.2126, .7152, .0722));
    float edge = pow(saturate(length(float2(lx, ly)) * sx_arg_EdgeStrength), sx_arg_EdgePower);
    float grain = frac(sin(dot(floor(UV * sx_arg_GrainScale), float2(12.9898, 78.233))) * 43758.5453);
    float shade = saturate(1.0 - edge - (1.0 - l) * sx_arg_Shading - (grain - .5) * sx_arg_GrainAmount);
    float3 mono = lerp(sx_arg_PencilColor.rgb, sx_arg_PaperColor.rgb, shade);
    float3 outColor = lerp(mono, mono * c.rgb * 1.5, sx_arg_PreserveColor);
    return float4(lerp(c.rgb, outColor, sx_arg_Strength), c.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Radius, sx_EdgeStrength, sx_EdgePower, sx_Shading, sx_GrainScale, sx_GrainAmount, sx_PreserveColor, sx_Strength, sx_PencilColor.rgb, sx_PaperColor.rgb);
}

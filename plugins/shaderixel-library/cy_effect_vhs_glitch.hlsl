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
//@name VHS Glitch
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"VHS Glitch","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":4096,"min":8,"name":"LineDensity","op":"param","pos":[40, 330],"text":"Line Density","value":[240, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":100,"min":-100,"name":"Phase","op":"param","pos":[40, 440],"text":"Phase","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.999000013,"min":0,"name":"GlitchThreshold","op":"param","pos":[40, 550],"text":"Glitch Threshold","value":[0.870000005, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":16,"min":0.100000001,"name":"GlitchSharpness","op":"param","pos":[40, 660],"text":"Glitch Sharpness","value":[2, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":0.5,"min":0,"name":"HorizontalJitter","op":"param","pos":[40, 770],"text":"Horizontal Jitter","value":[0.0799999982, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":2048,"min":0,"name":"TrackingFrequency","op":"param","pos":[40, 880],"text":"Tracking Frequency","value":[85, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":0.100000001,"min":0,"name":"TrackingAmount","op":"param","pos":[40, 990],"text":"Tracking Amount","value":[0.00300000003, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":64,"min":0,"name":"ColorDelay","op":"param","pos":[40, 1100],"text":"Color Delay","value":[3, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Scanlines","op":"param","pos":[40, 1210],"text":"Scanlines","value":[0.25, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Dropout","op":"param","pos":[40, 1320],"text":"Dropout","value":[0.349999994, 0, 0, 0]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"VHS Glitch","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LineDensity",1],["Phase",1],["GlitchThreshold",1],["GlitchSharpness",1],["HorizontalJitter",1],["TrackingFrequency",1],["TrackingAmount",1],["ColorDelay",1],["Scanlines",1],["Dropout",1]],"pos":[420, 40],"text":"    float scanLine = floor(UV.y * max(LineDensity, 1.0));\n    float n = frac(sin(scanLine * 12.9898 + Phase * 7.13) * 43758.5453);\n    float burst = pow(saturate((n - GlitchThreshold) / max(1.0 - GlitchThreshold, .0001)), GlitchSharpness);\n    float jitter = (n - .5) * HorizontalJitter + sin(UV.y * TrackingFrequency + Phase) * TrackingAmount;\n    float2 uv = UV + float2(jitter * burst, 0);\n    float delay = ColorDelay / max(CyCanvasSize.x, 1.0);\n    float r = Texture2DSample(CyInput, CyInputSampler, uv + float2(delay, 0)).r;\n    float g = Texture2DSample(CyInput, CyInputSampler, uv).g;\n    float b = Texture2DSample(CyInput, CyInputSampler, uv - float2(delay, 0)).b;\n    float scan = lerp(1.0, 0.75 + 0.25 * sin(UV.y * CyCanvasSize.y * 3.14159), Scanlines);\n    float dropout = 1.0 - burst * Dropout;\n    float4 src = Texture2DSample(CyInput, CyInputSampler, uv);\n    return float4(float3(r, g, b) * scan * dropout, src.a);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_LineDensity float 240 8 4096 "Line Density"
//@param sx_Phase float 0 -100 100 "Phase"
//@param sx_GlitchThreshold float 0.87 0 0.999 "Glitch Threshold"
//@param sx_GlitchSharpness float 2 0.1 16 "Glitch Sharpness"
//@param sx_HorizontalJitter float 0.08 0 0.5 "Horizontal Jitter"
//@param sx_TrackingFrequency float 85 0 2048 "Tracking Frequency"
//@param sx_TrackingAmount float 0.003 0 0.1 "Tracking Amount"
//@param sx_ColorDelay float 3 0 64 "Color Delay"
//@param sx_Scanlines float 0.25 0 1 "Scanlines"
//@param sx_Dropout float 0.35 0 1 "Dropout"
// Imported preset (Effect / Stylize): Analog VHS displacement with tracking noise, color delay, dropout, and scanline controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_LineDensity, float sx_arg_Phase, float sx_arg_GlitchThreshold, float sx_arg_GlitchSharpness, float sx_arg_HorizontalJitter, float sx_arg_TrackingFrequency, float sx_arg_TrackingAmount, float sx_arg_ColorDelay, float sx_arg_Scanlines, float sx_arg_Dropout)
{
    float scanLine = floor(UV.y * max(sx_arg_LineDensity, 1.0));
    float n = frac(sin(scanLine * 12.9898 + sx_arg_Phase * 7.13) * 43758.5453);
    float burst = pow(saturate((n - sx_arg_GlitchThreshold) / max(1.0 - sx_arg_GlitchThreshold, .0001)), sx_arg_GlitchSharpness);
    float jitter = (n - .5) * sx_arg_HorizontalJitter + sin(UV.y * sx_arg_TrackingFrequency + sx_arg_Phase) * sx_arg_TrackingAmount;
    float2 uv = UV + float2(jitter * burst, 0);
    float delay = sx_arg_ColorDelay / max(CyCanvasSize.x, 1.0);
    float r = Texture2DSample(CyInput, CyInputSampler, uv + float2(delay, 0)).r;
    float g = Texture2DSample(CyInput, CyInputSampler, uv).g;
    float b = Texture2DSample(CyInput, CyInputSampler, uv - float2(delay, 0)).b;
    float scan = lerp(1.0, 0.75 + 0.25 * sin(UV.y * CyCanvasSize.y * 3.14159), sx_arg_Scanlines);
    float dropout = 1.0 - burst * sx_arg_Dropout;
    float4 src = Texture2DSample(CyInput, CyInputSampler, uv);
    return float4(float3(r, g, b) * scan * dropout, src.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_LineDensity, sx_Phase, sx_GlitchThreshold, sx_GlitchSharpness, sx_HorizontalJitter, sx_TrackingFrequency, sx_TrackingAmount, sx_ColorDelay, sx_Scanlines, sx_Dropout);
}

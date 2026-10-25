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
//@name Color Grading
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Color Grading","nextId":18,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":-2,"name":"LiftLuma","op":"param","pos":[40, 330],"text":"Lift Luma","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":-0.949999988,"name":"GammaLuma","op":"param","pos":[40, 440],"text":"Gamma Luma","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":-1,"name":"GainLuma","op":"param","pos":[40, 550],"text":"Gain Luma","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":1,"min":-1,"name":"Temperature","op":"param","pos":[40, 660],"text":"Temperature","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":1,"min":-1,"name":"Tint","op":"param","pos":[40, 770],"text":"Tint","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":4,"min":0,"name":"Saturation","op":"param","pos":[40, 880],"text":"Saturation","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":4,"min":0,"name":"Contrast","op":"param","pos":[40, 990],"text":"Contrast","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0,"name":"ContrastPivot","op":"param","pos":[40, 1100],"text":"Contrast Pivot","value":[0.5, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 1210],"text":"Strength","value":[1, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"LiftWheel","op":"paramcolor","pos":[40, 1320],"text":"Lift Wheel","value":[0, 0, 1, 1]},{"id":14,"in":[],"index":1,"name":"GammaWheel","op":"paramcolor","pos":[40, 1430],"text":"Gamma Wheel","value":[0, 0, 1, 1]},{"id":15,"in":[],"index":1,"name":"GainWheel","op":"paramcolor","pos":[40, 1540],"text":"Gain Wheel","value":[0, 0, 1, 1]},{"id":16,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0]],"name":"Color Grading","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LiftLuma",1],["GammaLuma",1],["GainLuma",1],["Temperature",1],["Tint",1],["Saturation",1],["Contrast",1],["ContrastPivot",1],["Strength",1],["LiftWheel",3],["GammaWheel",3],["GainWheel",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 lp = abs(frac(LiftWheel.r / 360.0 + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0);\n    float3 gp = abs(frac(GammaWheel.r / 360.0 + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0);\n    float3 ap = abs(frac(GainWheel.r / 360.0 + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0);\n    float3 lc = lerp(1.0.xxx, saturate(lp - 1.0), saturate(LiftWheel.g));\n    float3 gc = lerp(1.0.xxx, saturate(gp - 1.0), saturate(GammaWheel.g));\n    float3 ac = lerp(1.0.xxx, saturate(ap - 1.0), saturate(GainWheel.g));\n    lc -= dot(lc, float3(.333333, .333333, .333333));\n    gc -= dot(gc, float3(.333333, .333333, .333333));\n    ac -= dot(ac, float3(.333333, .333333, .333333));\n    float3 v = max(s.rgb + LiftLuma + lc * 0.25, 0.0);\n    v = pow(v, 1.0 / max(1.0 + GammaLuma + gc * 0.35, 0.05));\n    v *= max(1.0 + GainLuma + ac * 0.5, 0.0);\n    v *= max(float3(1.0 + Temperature * .12 - Tint * .04, 1.0 + Tint * .08, 1.0 - Temperature * .12 - Tint * .04), .001);\n    float l = dot(v, float3(.2126, .7152, .0722));\n    v = lerp(l.xxx, v, Saturation);\n    v = (v - ContrastPivot) * Contrast + ContrastPivot;\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":17,"in":[[16, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param sx_LiftLuma float 0 -2 2 "Lift Luma"
//@param sx_GammaLuma float 0 -0.95 4 "Gamma Luma"
//@param sx_GainLuma float 0 -1 8 "Gain Luma"
//@param sx_Temperature float 0 -1 1 "Temperature"
//@param sx_Tint float 0 -1 1 "Tint"
//@param sx_Saturation float 1 0 4 "Saturation"
//@param sx_Contrast float 1 0 4 "Contrast"
//@param sx_ContrastPivot float 0.5 0 2 "Contrast Pivot"
//@param sx_Strength float 1 0 1 "Strength"
//@param sx_LiftWheel color 0 0 1 1 "Lift Wheel"
//@param sx_GammaWheel color 0 0 1 1 "Gamma Wheel"
//@param sx_GainWheel color 0 0 1 1 "Gain Wheel"
// Imported preset (Effect / Color & Tone): Professional lift, gamma, and gain color wheels with temperature, tint, saturation, and contrast.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_LiftLuma, float sx_arg_GammaLuma, float sx_arg_GainLuma, float sx_arg_Temperature, float sx_arg_Tint, float sx_arg_Saturation, float sx_arg_Contrast, float sx_arg_ContrastPivot, float sx_arg_Strength, float3 sx_arg_LiftWheel, float3 sx_arg_GammaWheel, float3 sx_arg_GainWheel)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 lp = abs(frac(sx_arg_LiftWheel.r / 360.0 + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0);
    float3 gp = abs(frac(sx_arg_GammaWheel.r / 360.0 + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0);
    float3 ap = abs(frac(sx_arg_GainWheel.r / 360.0 + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0);
    float3 lc = lerp(1.0.xxx, saturate(lp - 1.0), saturate(sx_arg_LiftWheel.g));
    float3 gc = lerp(1.0.xxx, saturate(gp - 1.0), saturate(sx_arg_GammaWheel.g));
    float3 ac = lerp(1.0.xxx, saturate(ap - 1.0), saturate(sx_arg_GainWheel.g));
    lc -= dot(lc, float3(.333333, .333333, .333333));
    gc -= dot(gc, float3(.333333, .333333, .333333));
    ac -= dot(ac, float3(.333333, .333333, .333333));
    float3 v = max(s.rgb + sx_arg_LiftLuma + lc * 0.25, 0.0);
    v = pow(v, 1.0 / max(1.0 + sx_arg_GammaLuma + gc * 0.35, 0.05));
    v *= max(1.0 + sx_arg_GainLuma + ac * 0.5, 0.0);
    v *= max(float3(1.0 + sx_arg_Temperature * .12 - sx_arg_Tint * .04, 1.0 + sx_arg_Tint * .08, 1.0 - sx_arg_Temperature * .12 - sx_arg_Tint * .04), .001);
    float l = dot(v, float3(.2126, .7152, .0722));
    v = lerp(l.xxx, v, sx_arg_Saturation);
    v = (v - sx_arg_ContrastPivot) * sx_arg_Contrast + sx_arg_ContrastPivot;
    return float4(lerp(s.rgb, v, saturate(sx_arg_Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_LiftLuma, sx_GammaLuma, sx_GainLuma, sx_Temperature, sx_Tint, sx_Saturation, sx_Contrast, sx_ContrastPivot, sx_Strength, sx_LiftWheel.rgb, sx_GammaWheel.rgb, sx_GainWheel.rgb);
}

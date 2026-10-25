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
//@name Anamorphic Lens Flare
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Anamorphic Lens Flare","nextId":17,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":8,"min":0.100000001,"name":"Aspect","op":"param","pos":[40, 220],"text":"Aspect","value":[1, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":128,"min":0.100000001,"name":"CoreFalloff","op":"param","pos":[40, 330],"text":"Core Falloff","value":[18, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":0,"name":"CoreIntensity","op":"param","pos":[40, 440],"text":"Core Intensity","value":[6, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"StreakWidth","op":"param","pos":[40, 550],"text":"Streak Width","value":[0.00600000005, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":4,"min":0.00100000005,"name":"StreakLength","op":"param","pos":[40, 660],"text":"Streak Length","value":[0.75, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":16,"min":0.100000001,"name":"StreakFalloff","op":"param","pos":[40, 770],"text":"Streak Falloff","value":[2, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":64,"min":0,"name":"StreakIntensity","op":"param","pos":[40, 880],"text":"Streak Intensity","value":[4, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":0.100000001,"min":0,"name":"ColorSplit","op":"param","pos":[40, 990],"text":"Color Split","value":[0.00400000019, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1100],"text":"Opacity","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1210],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":13,"in":[],"index":1,"name":"CoreColor","op":"paramcolor","pos":[40, 1320],"text":"Core Color","value":[1, 0.75, 0.349999994, 1]},{"id":14,"in":[],"index":1,"name":"StreakColor","op":"paramcolor","pos":[40, 1430],"text":"Streak Color","value":[0.180000007, 0.449999988, 1, 1]},{"id":15,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0]],"name":"Anamorphic Lens Flare","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["CoreFalloff",1],["CoreIntensity",1],["StreakWidth",1],["StreakLength",1],["StreakFalloff",1],["StreakIntensity",1],["ColorSplit",1],["Opacity",1],["Center",3],["CoreColor",3],["StreakColor",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    float core = exp(-length(float2(p.x * Aspect, p.y)) * CoreFalloff) * CoreIntensity;\n    float streak = exp(-abs(p.y) / max(StreakWidth, 0.0001)) * pow(saturate(1.0 - abs(p.x) / max(StreakLength, 0.0001)), StreakFalloff) * StreakIntensity;\n    float blue = exp(-abs(p.y + ColorSplit) / max(StreakWidth, 0.0001));\n    float red = exp(-abs(p.y - ColorSplit) / max(StreakWidth, 0.0001));\n    float3 rgb = CoreColor.rgb * core + StreakColor.rgb * streak * float3(red, 1.0, blue);\n    float a = saturate(max(max(rgb.r, rgb.g), rgb.b) * Opacity);\n    return float4(rgb * Opacity, a);","value":[0, 0, 0, 0]},{"id":16,"in":[[15, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param sx_Aspect float 1 0.1 8 "Aspect"
//@param sx_CoreFalloff float 18 0.1 128 "Core Falloff"
//@param sx_CoreIntensity float 6 0 64 "Core Intensity"
//@param sx_StreakWidth float 0.006 0.0001 0.25 "Streak Width"
//@param sx_StreakLength float 0.75 0.001 4 "Streak Length"
//@param sx_StreakFalloff float 2 0.1 16 "Streak Falloff"
//@param sx_StreakIntensity float 4 0 64 "Streak Intensity"
//@param sx_ColorSplit float 0.004 0 0.1 "Color Split"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_CoreColor color 1 0.75 0.35 1 "Core Color"
//@param sx_StreakColor color 0.18 0.45 1 1 "Streak Color"
// Imported preset (Draw / Light & Space): Wide cinematic anamorphic flare with horizontal streaks and spectral split.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Aspect, float sx_arg_CoreFalloff, float sx_arg_CoreIntensity, float sx_arg_StreakWidth, float sx_arg_StreakLength, float sx_arg_StreakFalloff, float sx_arg_StreakIntensity, float sx_arg_ColorSplit, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_CoreColor, float3 sx_arg_StreakColor)
{
    float2 p = UV - sx_arg_Center.xy;
    float core = exp(-length(float2(p.x * sx_arg_Aspect, p.y)) * sx_arg_CoreFalloff) * sx_arg_CoreIntensity;
    float streak = exp(-abs(p.y) / max(sx_arg_StreakWidth, 0.0001)) * pow(saturate(1.0 - abs(p.x) / max(sx_arg_StreakLength, 0.0001)), sx_arg_StreakFalloff) * sx_arg_StreakIntensity;
    float blue = exp(-abs(p.y + sx_arg_ColorSplit) / max(sx_arg_StreakWidth, 0.0001));
    float red = exp(-abs(p.y - sx_arg_ColorSplit) / max(sx_arg_StreakWidth, 0.0001));
    float3 rgb = sx_arg_CoreColor.rgb * core + sx_arg_StreakColor.rgb * streak * float3(red, 1.0, blue);
    float a = saturate(max(max(rgb.r, rgb.g), rgb.b) * sx_arg_Opacity);
    return float4(rgb * sx_arg_Opacity, a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Aspect, sx_CoreFalloff, sx_CoreIntensity, sx_StreakWidth, sx_StreakLength, sx_StreakFalloff, sx_StreakIntensity, sx_ColorSplit, sx_Opacity, float3(sx_Center, 0.0), sx_CoreColor.rgb, sx_StreakColor.rgb);
}

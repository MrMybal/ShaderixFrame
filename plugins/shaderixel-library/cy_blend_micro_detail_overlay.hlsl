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
//@name Micro Detail Overlay
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Micro Detail Overlay","nextId":13,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":-8,"name":"Neutral","op":"param","pos":[40, 550],"text":"Neutral","value":[0.5, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":16,"min":0,"name":"Strength","op":"param","pos":[40, 660],"text":"Strength","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 770],"text":"Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"DetailChannel","op":"paramcolor","pos":[40, 880],"text":"Detail Channel","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"TargetChannels","op":"paramcolor","pos":[40, 990],"text":"Target Channels","value":[1, 1, 1, 0]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Micro Detail Overlay","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["Neutral",1],["Strength",1],["Opacity",1],["DetailChannel",4],["TargetChannels",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float detail = dot(fg, DetailChannel) - Neutral;\n    float a = saturate(fg.a * mask * Opacity);\n    float3 headroom = min(bg.rgb, 1.0 - bg.rgb) * 2.0;\n    float3 modified = bg.rgb + detail * Strength * headroom * TargetChannels.rgb;\n    float3 rgb = lerp(bg.rgb, modified, a);\n    return float4(rgb, bg.a);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param sx_Neutral float 0.5 -8 8 "Neutral"
//@param sx_Strength float 1 0 16 "Strength"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_DetailChannel color 1 0 0 0 "Detail Channel"
//@param sx_TargetChannels color 1 1 1 0 "Target Channels"
// Imported preset (Blend / PBR): Adds signed micro-detail to selected PBR channels with range-safe attenuation near black and white.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Neutral, float sx_arg_Strength, float sx_arg_Opacity, float4 sx_arg_DetailChannel, float3 sx_arg_TargetChannels)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float detail = dot(fg, sx_arg_DetailChannel) - sx_arg_Neutral;
    float a = saturate(fg.a * mask * sx_arg_Opacity);
    float3 headroom = min(bg.rgb, 1.0 - bg.rgb) * 2.0;
    float3 modified = bg.rgb + detail * sx_arg_Strength * headroom * sx_arg_TargetChannels.rgb;
    float3 rgb = lerp(bg.rgb, modified, a);
    return float4(rgb, bg.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Neutral, sx_Strength, sx_Opacity, sx_DetailChannel, sx_TargetChannels.rgb);
}

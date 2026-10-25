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
//@name Channel Replace
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Channel Replace","nextId":14,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ReplaceAlpha","op":"param","pos":[40, 550],"text":"Replace Alpha","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 660],"text":"Opacity","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"SourceForRed","op":"paramcolor","pos":[40, 770],"text":"Source For Red","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"SourceForGreen","op":"paramcolor","pos":[40, 880],"text":"Source For Green","value":[0, 1, 0, 0]},{"id":10,"in":[],"index":1,"name":"SourceForBlue","op":"paramcolor","pos":[40, 990],"text":"Source For Blue","value":[0, 0, 1, 0]},{"id":11,"in":[],"index":1,"name":"ReplaceChannels","op":"paramcolor","pos":[40, 1100],"text":"Replace Channels","value":[1, 1, 1, 0]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Channel Replace","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["ReplaceAlpha",1],["Opacity",1],["SourceForRed",4],["SourceForGreen",4],["SourceForBlue",4],["ReplaceChannels",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float3 mapped = float3(dot(fg, SourceForRed), dot(fg, SourceForGreen), dot(fg, SourceForBlue));\n    float3 weights = saturate(ReplaceChannels.rgb);\n    float a = saturate(mask * fg.a * Opacity);\n    float3 blend = lerp(bg.rgb, mapped, weights);\n    return float4(lerp(bg.rgb, blend, a), lerp(bg.a, fg.a, saturate(ReplaceAlpha * mask * Opacity)));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param sx_ReplaceAlpha float 0 0 1 "Replace Alpha"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_SourceForRed color 1 0 0 0 "Source For Red"
//@param sx_SourceForGreen color 0 1 0 0 "Source For Green"
//@param sx_SourceForBlue color 0 0 1 0 "Source For Blue"
//@param sx_ReplaceChannels color 1 1 1 0 "Replace Channels"
// Imported preset (Blend / Utility): Replaces any weighted combination of background RGB channels with foreground channels.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_ReplaceAlpha, float sx_arg_Opacity, float4 sx_arg_SourceForRed, float4 sx_arg_SourceForGreen, float4 sx_arg_SourceForBlue, float3 sx_arg_ReplaceChannels)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float3 mapped = float3(dot(fg, sx_arg_SourceForRed), dot(fg, sx_arg_SourceForGreen), dot(fg, sx_arg_SourceForBlue));
    float3 weights = saturate(sx_arg_ReplaceChannels.rgb);
    float a = saturate(mask * fg.a * sx_arg_Opacity);
    float3 blend = lerp(bg.rgb, mapped, weights);
    return float4(lerp(bg.rgb, blend, a), lerp(bg.a, fg.a, saturate(sx_arg_ReplaceAlpha * mask * sx_arg_Opacity)));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_ReplaceAlpha, sx_Opacity, sx_SourceForRed, sx_SourceForGreen, sx_SourceForBlue, sx_ReplaceChannels.rgb);
}

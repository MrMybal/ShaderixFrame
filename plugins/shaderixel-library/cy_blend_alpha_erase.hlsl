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
//@name Alpha Erase
//@category Fusions
//@graph {"category":"Fusions","inputs":3,"kind":1,"name":"Alpha Erase","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyBackground","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"CyForeground","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":2,"name":"CyMask","op":"texture","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"uv","pos":[40, 370],"value":[0, 0, 0, 0]},{"id":5,"in":[],"op":"canvassize","pos":[40, 460],"value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 550],"text":"Opacity","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"ReplaceRGB","op":"param","pos":[40, 660],"text":"Replace RGB","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"EraseColor","op":"paramcolor","pos":[40, 770],"text":"Erase Color","value":[0, 0, 0, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Alpha Erase","op":"custom","outs":[["Resultat",4]],"pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["Opacity",1],["ReplaceRGB",1],["EraseColor",3]],"pos":[420, 40],"text":"    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);\n    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);\n    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;\n    float erase = saturate(fg.a * mask * Opacity);\n    float outAlpha = bg.a * (1.0 - erase);\n    float3 rgb = lerp(bg.rgb, EraseColor.rgb, erase * ReplaceRGB);\n    return float4(rgb, outAlpha);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_ReplaceRGB float 0 0 1 "Replace RGB"
//@param sx_EraseColor color 0 0 0 1 "Erase Color"
// Imported preset (Blend / Utility): Uses foreground alpha and mask as a nondestructive eraser while optionally retaining RGB data.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Opacity, float sx_arg_ReplaceRGB, float3 sx_arg_EraseColor)
{
    float4 bg = Texture2DSample(CyBackground, CyBackgroundSampler, UV);
    float4 fg = Texture2DSample(CyForeground, CyForegroundSampler, UV);
    float mask = Texture2DSample(CyMask, CyMaskSampler, UV).r;
    float erase = saturate(fg.a * mask * sx_arg_Opacity);
    float outAlpha = bg.a * (1.0 - erase);
    float3 rgb = lerp(bg.rgb, sx_arg_EraseColor.rgb, erase * sx_arg_ReplaceRGB);
    return float4(rgb, outAlpha);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Opacity, sx_ReplaceRGB, sx_EraseColor.rgb);
}

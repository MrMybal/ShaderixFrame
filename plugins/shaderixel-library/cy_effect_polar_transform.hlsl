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
//@name Polar Transform
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Polar Transform","nextId":14,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.0500000007,"name":"Aspect","op":"param","pos":[40, 330],"text":"Aspect","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0.00999999978,"name":"RadialScale","op":"param","pos":[40, 440],"text":"Radial Scale","value":[2, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":-8,"name":"RadialOffset","op":"param","pos":[40, 550],"text":"Radial Offset","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":4,"min":-4,"name":"Rotation","op":"param","pos":[40, 660],"text":"Rotation","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":16,"min":-16,"name":"Twist","op":"param","pos":[40, 770],"text":"Twist","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":64,"min":1,"name":"AngularRepeat","op":"param","pos":[40, 880],"text":"Angular Repeat","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"ToPolar","op":"param","pos":[40, 990],"text":"To Polar","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1100],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Polar Transform","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Aspect",1],["RadialScale",1],["RadialOffset",1],["Rotation",1],["Twist",1],["AngularRepeat",1],["ToPolar",1],["Center",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float radius = length(p) * RadialScale + RadialOffset;\n    float angle = atan2(p.y, p.x) / 6.283185 + 0.5 + Rotation + radius * Twist;\n    float2 polar = float2(frac(angle * AngularRepeat), frac(radius));\n    float2 target = lerp(UV, polar, saturate(ToPolar));\n    float4 s = Texture2DSample(CyInput, CyInputSampler, target);\n    return s;","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param sx_Aspect float 1 0.05 16 "Aspect"
//@param sx_RadialScale float 2 0.01 32 "Radial Scale"
//@param sx_RadialOffset float 0 -8 8 "Radial Offset"
//@param sx_Rotation float 0 -4 4 "Rotation"
//@param sx_Twist float 0 -16 16 "Twist"
//@param sx_AngularRepeat float 1 1 64 "Angular Repeat"
//@param sx_ToPolar float 1 0 1 "To Polar"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Distort): Cartesian-to-polar transform with repeat, twist, radial offset, and seamless angular wrapping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Aspect, float sx_arg_RadialScale, float sx_arg_RadialOffset, float sx_arg_Rotation, float sx_arg_Twist, float sx_arg_AngularRepeat, float sx_arg_ToPolar, float3 sx_arg_Center)
{
    float2 p = UV - sx_arg_Center.xy;
    p.x *= sx_arg_Aspect;
    float radius = length(p) * sx_arg_RadialScale + sx_arg_RadialOffset;
    float angle = atan2(p.y, p.x) / 6.283185 + 0.5 + sx_arg_Rotation + radius * sx_arg_Twist;
    float2 polar = float2(frac(angle * sx_arg_AngularRepeat), frac(radius));
    float2 target = lerp(UV, polar, saturate(sx_arg_ToPolar));
    float4 s = Texture2DSample(CyInput, CyInputSampler, target);
    return s;
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Aspect, sx_RadialScale, sx_RadialOffset, sx_Rotation, sx_Twist, sx_AngularRepeat, sx_ToPolar, float3(sx_Center, 0.0));
}

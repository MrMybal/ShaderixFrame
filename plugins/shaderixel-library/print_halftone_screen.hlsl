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
//@name Print Halftone Screen
//@category Print and Halftone
//@input Image
//@param sx_spacing float 8 2 128 "Cell Spacing (px)"
//@param sx_angle float 45 -180 180 "Screen Angle (degrees)"
//@param sx_pattern int 0 0 2 "Pattern"
//@param sx_gamma float 1 0.1 4 "Ink Curve"
//@param sx_ink color 0.01 0.01 0.01 1 "Ink Color"
//@param sx_paper color 1 1 1 1 "Paper Color"
//@param sx_transparent_paper bool 0 "Transparent Paper"
//@param sx_invert bool 0 "Invert Tone"
// Pointwise tone modulation; no cell-center sampling or neighboring reads.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = saturate(Luma(LinearToSrgb(max(c.rgb, 0.0))));
    y = y < 0.000001 ? 0.0 : (y > 0.999999 ? 1.0 : y);
    float tone = pow(sx_invert != 0 ? y : 1.0 - y, max(sx_gamma, 0.1));
    float a = radians(sx_angle), s = max(sx_spacing, 2.0);
    float2 p = ((float2)(ctx.px)) + 0.5;
    float2 q = float2(dot(p, float2(cos(a), sin(a))), dot(p, float2(-sin(a), cos(a))));
    float2 cell = frac(q / s) - 0.5;
    float metric = sx_pattern == 0 ? length(cell) : (sx_pattern == 1 ? abs(cell.x) : min(abs(cell.x), abs(cell.y)));
    float radius = sx_pattern == 0 ? sqrt(tone / 3.14159265) : (sx_pattern == 1 ? tone * 0.5 : (1.0 - sqrt(1.0 - tone)) * 0.5);
    float aa = 0.65 / s;
    float m = 1.0 - smoothstep(radius - aa, radius + aa, metric);
    // Transfer-function roundoff must not leave faint ink on pure white.
    if (tone <= 0.000001) m = 0.0;
    if (tone >= 0.999999) m = 1.0;
    float ia = saturate(m * sx_ink.a), pa = sx_transparent_paper != 0 ? 0.0 : saturate(sx_paper.a);
    float outAlpha = ia + pa * (1.0 - ia);
    float3 rgb = sx_ink.rgb * ia + sx_paper.rgb * pa * (1.0 - ia);
    return float4(outAlpha > 0.000001 ? rgb / outAlpha : 0.0, outAlpha * c.a);
}

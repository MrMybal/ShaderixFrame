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
//@name Lab Adjustment
//@category Adjustments
//@param sx_lightness float 0 -100 100 "Luminance L (Offset)"
//@param sx_contrast float 1 0 3 "Contraste L"
//@param sx_a_shift float 0 -100 100 "a (vert <-> rouge)"
//@param sx_b_shift float 0 -100 100 "b (bleu <-> jaune)"
//@param sx_chroma float 1 0 3 "Chroma (a, b)"
// Edits in CIE Lab (D50, Bradford from sRGB D65): lightness / contrast on L without
// touching the colors, color balance and chroma on a and b.
float3 LabF(float3 t)
{
    return select(t > 216.0 / 24389.0, pow(max(t, 0.0), 1.0 / 3.0), (24389.0 / 27.0 * t + 16.0) / 116.0);
}
float3 LabFinv(float3 f)
{
    float3 t = f * f * f;
    return select(t > 216.0 / 24389.0, t, (116.0 * f - 16.0) / (24389.0 / 27.0));
}

float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    const float3x3 toXyz = float3x3(0.4360747, 0.3850649, 0.1430804, 0.2225045, 0.7168786, 0.0606169, 0.0139322, 0.0971045, 0.7141733);
    const float3x3 toRgb = float3x3(3.1338561, -1.6168667, -0.4906146, -0.9787684, 1.9161415, 0.0334540, 0.0719453, -0.2289914, 1.4052427);
    const float3 white = float3(0.9642, 1.0, 0.8251);
    float3 f = LabF(mul(toXyz, max(c.rgb, 0.0)) / white);
    float L = 116.0 * f.y - 16.0, A = 500.0 * (f.x - f.y), B = 200.0 * (f.y - f.z);
    L = (L - 50.0) * sx_contrast + 50.0 + sx_lightness;
    A = A * sx_chroma + sx_a_shift;
    B = B * sx_chroma + sx_b_shift;
    float fy = (L + 16.0) / 116.0;
    float3 xyz = LabFinv(float3(fy + A / 500.0, fy, fy - B / 200.0)) * white;
    c.rgb = max(mul(toRgb, xyz), 0.0);
    return c;
}

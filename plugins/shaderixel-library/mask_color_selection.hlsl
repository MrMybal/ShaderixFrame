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
//@name Color Range Mask
//@category Masks and Compositing
//@param sx_key_color color 0 1 0 1 "Selected Color"
//@param sx_tolerance float 0.12 0 1 "Color Tolerance"
//@param sx_softness float 0.08 0.001 1 "Selection Softness"
//@param sx_metric int 0 0 1 "Distance Metric"
//@param sx_invert bool 0 "Invert Selection"
//@param sx_respect_alpha bool 1 "Respect Source Alpha"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 p = LinearToSrgb(max(c.rgb, 0.0));
    float3 k = LinearToSrgb(max(sx_key_color.rgb, 0.0));
    float d;
    if (sx_metric != 0)
    {
        p /= max(p.r + p.g + p.b, 0.0001);
        k /= max(k.r + k.g + k.b, 0.0001);
        d = distance(p, k) * 0.70710678;
    }
    else d = distance(p, k) * 0.57735027;
    float m = 1.0 - smoothstep(sx_tolerance, sx_tolerance + max(sx_softness, 0.001), d);
    m = sx_invert != 0 ? 1.0 - m : m;
    if (sx_respect_alpha != 0) m *= c.a;
    // Masks store linear coverage; downstream mask effects read the red channel directly.
    return float4(saturate(m).xxx, 1.0);
}

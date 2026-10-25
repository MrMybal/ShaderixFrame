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
//@name Mask Feather and Levels
//@category Masks and Compositing
//@input Mask
//@halo radius 3 2
//@passes 3
//@param sx_radius float 1 0 16 "Feather Radius (px)"
//@param sx_black float 0 0 1 "Black Point"
//@param sx_white float 1 0 1 "White Point"
//@param sx_gamma float 1 0.1 4 "Coverage Curve"
//@param sx_invert bool 0 "Invert Mask"
float4 Effect(EffectContext ctx)
{
    if (ctx.pass < 2)
    {
        float sigma = clamp(sx_radius, 0.0, 16.0);
        if (sigma < 0.01) return float4(saturate(Prev(ctx.px).r).xxx, 1.0);
        int r = (int)ceil(sigma * 3.0);
        int2 axis = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
        float sum = 0.0, weights = 0.0;
        for (int i = -r; i <= r; ++i)
        {
            int2 p = ctx.px + axis * i;
            float v = any(p < 0) || any(p >= ((int2)(ctx.size))) ? 0.0 : saturate(Prev(p).r);
            float w = exp(-0.5 * float(i * i) / (sigma * sigma));
            sum += v * w;
            weights += w;
        }
        return float4((sum / weights).xxx, 1.0);
    }
    float span = sx_white - sx_black;
    span = abs(span) < 0.0001 ? (span < 0.0 ? -0.0001 : 0.0001) : span;
    float m = pow(saturate((Prev(ctx.px).r - sx_black) / span), max(sx_gamma, 0.1));
    m = sx_invert != 0 ? 1.0 - m : m;
    return float4(m.xxx, 1.0);
}

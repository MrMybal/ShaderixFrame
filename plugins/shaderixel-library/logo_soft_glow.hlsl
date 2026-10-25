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
//@name Logo Soft Glow
//@category Text and Logo Styles
//@premultiplied
//@passes 2
//@halo radius 3 2
//@param sx_radius float 8 0 64 "Glow Radius (px)"
//@param sx_color color 0.02 0.6 1 1 "Glow Color"
//@param sx_strength float 1 0 4 "Glow Strength"
float4 Effect(EffectContext ctx)
{
    float sigma = clamp(sx_radius, 0.0, 64.0);
    if (sigma < 0.01) return ctx.pass == 0 ? float4(0, 0, 0, 0) : Input(0, ctx.px);
    int n = (int)ceil(sigma * 3.0);
    int2 axis = ctx.pass == 0 ? int2(1, 0) : int2(0, 1);
    float sum = 0.0, weights = 0.0;
    for (int i = -n; i <= n; ++i)
    {
        int2 p = ctx.px + axis * i;
        float v = any(p < 0) || any(p >= ((int2)(ctx.size))) ? 0.0 : Prev(p).a;
        float w = exp(-0.5 * float(i * i) / (sigma * sigma));
        sum += v * w;
        weights += w;
    }
    float m = sum / weights;
    if (ctx.pass == 0) return float4(0, 0, 0, m);
    float4 c = Input(0, ctx.px);
    float alpha = saturate(m * sx_color.a * sx_strength);
    return c + float4(sx_color.rgb * alpha, alpha) * (1.0 - c.a);
}

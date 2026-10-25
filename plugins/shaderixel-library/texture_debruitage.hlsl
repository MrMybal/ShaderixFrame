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
//@name Edge-Preserving Texture Denoise
//@category Texture Processing
//@halo rayon 1 2
//@param sx_rayon int 2 1 5 "Radius (px)"
//@param sx_seuil float 0.08 0.001 1 "Color Tolerance"
//@param sx_force float 0.7 0 1 "Denoise Strength"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float4 sum = 0.0;
    float weights = 0.0;
    int r = max(1, (int)round(sx_rayon));
    float sigma = max(sx_seuil, 0.001);
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
        {
            float4 tap = Input(0, ctx.px + int2(x, y));
            float3 delta = LinearToSrgb(max(tap.rgb, 0.0)) - LinearToSrgb(max(c.rgb, 0.0));
            float w = exp(-dot(delta, delta) / (2.0 * sigma * sigma)
                          - float(x * x + y * y) / (2.0 * r * r)) * saturate(tap.a);
            sum += Premultiply(tap) * w;
            weights += w;
        }
    float4 filtered = weights > 0.00001 ? Unpremultiply(sum / weights) : c;
    return float4(lerp(c.rgb, filtered.rgb, saturate(sx_force)), c.a);
}

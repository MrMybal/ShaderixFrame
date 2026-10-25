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
//@name Sprite Four-Color Palette
//@category 2D Game Art
//@param sx_color_a color 0.01 0.015 0.025 1 "Palette Color 1"
//@param sx_color_b color 0.05 0.16 0.12 1 "Palette Color 2"
//@param sx_color_c color 0.35 0.55 0.22 1 "Palette Color 3"
//@param sx_color_d color 0.8 0.9 0.5 1 "Palette Color 4"
//@param sx_dither float 0.15 0 1 "Ordered Dither"
//@param sx_amount float 1 0 1 "Palette Strength"
static const float bayer4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int index = (ctx.px.y & 3) * 4 + (ctx.px.x & 3);
    float3 p = LinearToSrgb(max(c.rgb, 0.0)) + ((bayer4[index] + 0.5) / 16.0 - 0.5) * sx_dither * 0.25;
    float4 colors[4] = {sx_color_a, sx_color_b, sx_color_c, sx_color_d};
    float best = 1e30;
    float4 chosen = sx_color_a;
    for (int i = 0; i < 4; ++i)
    {
        float3 delta = p - LinearToSrgb(max(colors[i].rgb, 0.0));
        float d = dot(delta, delta);
        if (d < best) { best = d; chosen = colors[i]; }
    }
    float strength = saturate(sx_amount);
    return float4(lerp(c.rgb, chosen.rgb, strength), c.a * lerp(1.0, chosen.a, strength));
}

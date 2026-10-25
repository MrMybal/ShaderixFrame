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
//@name Tileable Procedural Microrelief
//@category Texture Generators
//@generator
//@param sx_cellules int 16 2 128 "Cells per Side"
//@param sx_graine int 0 0 9999 "Seed"
//@param sx_octaves int 4 1 6 "Detail Octaves"
//@param sx_amplitude float 0.2 0 0.5 "Amplitude"
//@param sx_centre float 0.5 0 1 "Midpoint Height"
float PeriodicNoise(float2 uv, float cells)
{
    float2 p = uv * cells;
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float2 a = i - floor(i / cells) * cells;
    float2 b = i + 1.0 - floor((i + 1.0) / cells) * cells;
    float2 seed = float2(sx_graine * 0.137, sx_graine * 0.719);
    float n00 = Hash12(a + seed);
    float n10 = Hash12(float2(b.x, a.y) + seed);
    float n01 = Hash12(float2(a.x, b.y) + seed);
    float n11 = Hash12(b + seed);
    return lerp(lerp(n00, n10, f.x), lerp(n01, n11, f.x), f.y);
}
float4 Effect(EffectContext ctx)
{
    float sum = 0.0, weights = 0.0, w = 1.0;
    float cells = max(2.0, round(sx_cellules));
    for (int i = 0; i < clamp((int)round(sx_octaves), 1, 6); ++i)
    {
        sum += PeriodicNoise(ctx.uv, cells) * w;
        weights += w;
        cells *= 2.0;
        w *= 0.5;
    }
    float h = saturate(sx_centre + (sum / weights - 0.5) * 2.0 * sx_amplitude);
    return float4(SrgbToLinear(h.xxx), 1.0);
}

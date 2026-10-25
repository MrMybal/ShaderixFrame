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
//@name Print Paper
//@category Print and Halftone
//@generator
//@param sx_color color 0.92 0.86 0.72 1 "Paper Color"
//@param sx_grain float 0.035 0 0.3 "Fine Grain"
//@param sx_mottling float 0.04 0 0.3 "Mottling"
//@param sx_scale float 64 4 512 "Mottling Scale (px)"
//@param sx_fibers float 0.015 0 0.2 "Fiber Strength"
//@param sx_seed int 1 0 65535 "Seed"
float4 Effect(EffectContext ctx)
{
    float2 p = ((float2)(ctx.px)) + float2(sx_seed * 17.0, sx_seed * 31.0);
    float2 q = p / max(sx_scale, 4.0), base = floor(q), f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float broad = lerp(lerp(Hash12(base), Hash12(base + float2(1, 0)), f.x),
                       lerp(Hash12(base + float2(0, 1)), Hash12(base + 1.0), f.x), f.y);
    float fiber = Hash12(floor(p / float2(24.0, 1.0)));
    float variation = (Hash12(p) - 0.5) * sx_grain + (broad - 0.5) * sx_mottling + (fiber - 0.5) * sx_fibers;
    float3 rgb = SrgbToLinear(saturate(LinearToSrgb(max(sx_color.rgb, 0.0)) + variation));
    return float4(rgb, saturate(sx_color.a));
}

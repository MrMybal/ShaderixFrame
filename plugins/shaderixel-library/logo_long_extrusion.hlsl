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
//@name Logo Long Extrusion
//@category Text and Logo Styles
//@premultiplied
//@halo distance 1 2
//@param sx_distance int 24 0 128 "Extrusion Length (px)"
//@param sx_angle float 45 -180 180 "Extrusion Angle (degrees)"
//@param sx_color color 0.08 0.01 0.04 1 "Extrusion Color"
//@param sx_fade float 0 0 1 "Fade Along Extrusion"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int n = clamp(sx_distance, 0, 128);
    float a = radians(sx_angle), coverage = 0.0;
    float2 dir = float2(cos(a), sin(a));
    for (int i = 1; i <= n; ++i)
    {
        int2 p = ctx.px - ((int2)(round(dir * i)));
        float v = any(p < 0) || any(p >= ((int2)(ctx.size))) ? 0.0 : Input(0, p).a;
        coverage = max(coverage, v * (1.0 - saturate(sx_fade) * float(i) / max(float(n), 1.0)));
    }
    float alpha = saturate(coverage * sx_color.a);
    return c + float4(sx_color.rgb * alpha, alpha) * (1.0 - c.a);
}

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
//@name Bevel and Emboss
//@category Layer Styles
//@halo size 1.5 4
//@premultiplied
//@param sx_depth float 3 0 20 "Depth"
//@param sx_size float 6 1 40 "Size (px)"
//@param sx_angle float 135 -180 180 "Light Angle"
//@param sx_highlight color 1 1 1 1 "Lights"
//@param sx_shadow color 0 0 0 1 "Ombres"
//@param sx_opacity float 0.75 0 1 "Opacity"
float H(EffectContext ctx, float2 o)
{
    // Height = blurred alpha (cheap 3x3 at the bevel size).
    float s = 0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            s += InputUV(0, ctx.uv + (o + float2(x, y) * sx_size * 0.5) * ctx.texel).a;
    return s / 9.0;
}
float4 Effect(EffectContext ctx)
{
    float4 o = Input(0, ctx.px);
    if (o.a <= 0.0) return o;
    float e = max(sx_size * 0.5, 1.0);
    float2 grad = float2(H(ctx, float2(e, 0)) - H(ctx, float2(-e, 0)), H(ctx, float2(0, e)) - H(ctx, float2(0, -e)));
    float3 n = normalize(float3(-grad * sx_depth, 1.0));
    float a = radians(sx_angle);
    float3 l = normalize(float3(cos(a), -sin(a), 0.8));
    float lit = dot(n, l) - l.z;  // 0 on flat areas
    float4 c = lit > 0 ? float4(sx_highlight.rgb, 1) : float4(sx_shadow.rgb, 1);
    float k = saturate(abs(lit) * 2.0) * sx_opacity * o.a;
    o.rgb = lerp(o.rgb, c.rgb * o.a, k);
    return o;
}

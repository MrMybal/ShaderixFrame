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
//@name Inner Shadow
//@category Layer Styles
//@halo size 1.5 dist 1 4
//@passes 2
//@premultiplied
//@param sx_color color 0 0 0 1 "Color"
//@param sx_opacity float 0.75 0 1 "Opacity"
//@param sx_angle float 135 -180 180 "Light Angle"
//@param sx_dist float 8 0 500 "Distance (px)"
//@param sx_size float 10 0 250 "Size (px)"
float BlurInv(EffectContext ctx, int2 dir, int2 offset)
{
    float sigma = max(sx_size * 0.5, 0.01);
    int n = int(ceil(sigma * 3.0));
    int stride = max(1, n / 64);
    float s = 0, w = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float k = exp(-0.5 * (i * i) / (sigma * sigma));
        float a = ctx.pass == 0 ? 1.0 - Input(0, ctx.px - offset + dir * i).a : Prev(ctx.px + dir * i).a;
        s += a * k;
        w += k;
    }
    return s / w;
}
float4 Effect(EffectContext ctx)
{
    float a = radians(sx_angle);
    int2 offset = ((int2)(round(float2(-cos(a), sin(a)) * sx_dist)));
    if (ctx.pass == 0)
        return float4(0, 0, 0, BlurInv(ctx, int2(1, 0), offset));
    float4 o = Input(0, ctx.px);
    float k = saturate(BlurInv(ctx, int2(0, 1), int2(0, 0))) * sx_opacity * sx_color.a;
    o.rgb = lerp(o.rgb, sx_color.rgb * o.a, k);
    return o;
}

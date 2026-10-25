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
//@name Feather
//@category Layer Styles
//@halo width 1 3
//@premultiplied
//@param sx_color color 1 1 1 1 "Color"
//@param sx_width float 4 0.5 64 "Size (px)"
//@param sx_position int 0 0 2 "Position"
//@param sx_opacity float 1 0 1 "Opacity"
float4 Effect(EffectContext ctx)
{
    float4 o = Input(0, ctx.px);
    // Disc search of the max (outside) and min (inside) alpha around the pixel.
    float mx = o.a, mn = o.a;
    const int A = 24;
    int R = max(1, int(ceil(sx_width / 4.0)));
    for (int j = 1; j <= 4; ++j)
    {
        float r = sx_width * j / 4.0;
        for (int i = 0; i < A; ++i)
        {
            float ang = 6.2831853 * (i + 0.5 * (j & 1)) / A;
            float sa = InputUV(0, ctx.uv + float2(cos(ang), sin(ang)) * r * ctx.texel).a;
            mx = max(mx, sa);
            mn = min(mn, sa);
        }
    }
    float s;
    if (sx_position == 1) s = o.a * (1.0 - mn);          // inside edge band
    else if (sx_position == 2) s = max(mx * (1.0 - o.a), o.a * (1.0 - mn));
    else s = mx;                                       // outside: under the content
    float4 st = float4(sx_color.rgb * sx_color.a, sx_color.a) * saturate(s) * sx_opacity;
    if (sx_position == 0) return o + st * (1.0 - o.a);
    return st + o * (1.0 - st.a);
}

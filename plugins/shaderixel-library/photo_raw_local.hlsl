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
//@name Local Retouch
//@category Adjustments
//@param sx_kind int 0 0 1 "Shape"
//@param sx_p0 float2 0.5 0.25 0 1 "Point A / Center"
//@param sx_invert bool 0 "Invert"
//@param sx_amount float 100 0 100 "Intensity"
//@param sx_show_mask bool 0 "Show Mask"
//@group Gradient
//@param sx_p1 float2 0.5 0.6 0 1 "Point B"
//@group Radial
//@param sx_radius float2 0.25 0.3 0.01 1.5 "Radii"
//@param sx_angle float 0 -180 180 "Angle"
//@param sx_feather float 50 0 100 "Feather"
//@group Adjustments
//@param sx_temperature float 0 -100 100 "Temperature"
//@param sx_tint float 0 -100 100 "Hue"
//@param sx_exposure float 0 -4 4 "Exposure (EV)"
//@param sx_contrast float 0 -100 100 "Contrast"
//@param sx_highlights float 0 -100 100 "Highlights"
//@param sx_shadows float 0 -100 100 "Shadows"
//@param sx_whites float 0 -100 100 "Whites"
//@param sx_blacks float 0 -100 100 "Blacks"
//@param sx_saturation float 0 -100 100 "Saturation"
//@param sx_haze float 0 -100 100 "Dehaze"
//@param sx_color color 1 1 1 1 "Color (Added Tint)"
//@param sx_color_amount float 0 0 100 "Color Strength"
// Local adjustment of the Photo RAW workspace (graduated / radial filter): a mask from the shape, the
// adjustments applied through it on the linear image. Linear: full effect at A, none at B.
// Radial: ellipse centered on A, effect inside (outside when inverted).

float LocalMask(EffectContext ctx)
{
    float2 px = ((float2)(ctx.px)) + 0.5;
    float m;
    if (sx_kind == 0)
    {
        float2 a = sx_p0 * ctx.size, b = sx_p1 * ctx.size;
        float2 ab = b - a;
        float t = dot(px - a, ab) / max(dot(ab, ab), 1e-3);
        m = 1.0 - smoothstep(0.0, 1.0, t);
    }
    else
    {
        float r = radians(sx_angle);
        float2 q = px - sx_p0 * ctx.size;
        q = float2(q.x * cos(r) + q.y * sin(r), -q.x * sin(r) + q.y * cos(r));
        float2 rr = max(sx_radius * ctx.size, 1.0);
        float e = length(q / rr);
        float f = max(sx_feather / 100.0, 0.001);
        m = 1.0 - smoothstep(1.0 - f, 1.0, e);
    }
    if (sx_invert) m = 1.0 - m;
    return saturate(m) * sx_amount / 100.0;
}

float4 Effect(EffectContext ctx)
{
    float4 src = Input(0, ctx.px);
    float m = LocalMask(ctx);
    if (sx_show_mask) return float4(lerp(src.rgb, float3(1.0, 0.1, 0.1), m * 0.6), src.a);
    if (m <= 0.0) return src;
    float3 c = max(src.rgb, 0.0);
    float t = sx_temperature / 100.0, g = sx_tint / 100.0;
    c *= float3(1.0 + 0.35 * t, 1.0 - 0.25 * g, 1.0 - 0.35 * t) * float3(1.0 + 0.1 * g, 1.0, 1.0 + 0.1 * g);
    c *= exp2(sx_exposure);
    if (sx_haze != 0.0)
    {
        float veil = min(c.r, min(c.g, c.b));
        float d = sx_haze / 100.0;
        c = d > 0.0 ? max(c - veil * d * 0.7, 0.0) / max(1.0 - veil * d * 0.7, 0.3) : lerp(c, c + (0.5 - c) * 0.3, -d);
    }
    float3 p = pow(c, 1.0 / 2.2);
    float L = Luma(p);
    float hl = smoothstep(0.45, 1.0, L), sh = 1.0 - smoothstep(0.0, 0.55, L);
    float wh = smoothstep(0.75, 1.1, L), bk = 1.0 - smoothstep(0.0, 0.25, L);
    p += sx_highlights / 100.0 * 0.35 * hl + sx_shadows / 100.0 * 0.35 * sh + sx_whites / 100.0 * 0.25 * wh + sx_blacks / 100.0 * 0.2 * bk;
    float k = sx_contrast / 100.0;
    p = k > 0 ? lerp(p, saturate(p) * saturate(p) * (3.0 - 2.0 * saturate(p)), k) : lerp(p, 0.5 + (p - 0.5) * 0.6, -k);
    float Lp = Luma(p);
    p = Lp + (p - Lp) * max(1.0 + sx_saturation / 100.0, 0.0);
    if (sx_color_amount > 0.0)
    {
        float3 tc = pow(max(sx_color.rgb, 0.0), 1.0 / 2.2);
        p = lerp(p, p * tc / max(Luma(tc), 1e-3), sx_color_amount / 100.0 * 0.5);
    }
    c = pow(max(p, 0.0), 2.2);
    return float4(lerp(src.rgb, c, m), src.a);
}

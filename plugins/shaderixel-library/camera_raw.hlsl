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
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}
//@name Camera Raw
//@category Réglages
//@halo full
//@passes 3
//@group Base
//@param sx_temperature float 0 -100 100 "Température"
//@param sx_tint float 0 -100 100 "Teinte"
//@param sx_exposure float 0 -5 5 "Exposition (IL)"
//@param sx_contrast float 0 -100 100 "Contraste"
//@param sx_highlights float 0 -100 100 "Hautes lumières"
//@param sx_shadows float 0 -100 100 "Ombres"
//@param sx_whites float 0 -100 100 "Blancs"
//@param sx_blacks float 0 -100 100 "Noirs"
//@group Présence
//@param sx_fine_detail float 0 -100 100 "Texture"
//@param sx_clarity float 0 -100 100 "Clarté"
//@param sx_dehaze float 0 -100 100 "Correction du voile"
//@param sx_vibrance float 0 -100 100 "Vibrance"
//@param sx_saturation float 0 -100 100 "Saturation"
//@group Détail
//@param sx_sharpen float 0 0 150 "Netteté"
//@param sx_radius float 1 0.5 3 "Rayon de netteté"
//@group Effets
//@param sx_vignette float 0 -100 100 "Vignettage"
//@param sx_vig_mid float 50 0 100 "Milieu du vignettage"
//@param sx_grain float 0 0 100 "Grain"
// Raw development style controls on linear light. Passes 0 and 1 blur the image
// (radius ~ 1/60 of the image) for clarity / dehaze; pass 2 develops the pixel.


float4 Blur1D(EffectContext ctx, int2 dir)
{
    float sigma = max(4.0, max(ctx.size.x, ctx.size.y) / 60.0);
    int n = int(ceil(sigma * 2.5));
    int stride = max(1, n / 48);
    float4 sum = 0;
    float wsum = 0;
    for (int i = -n; i <= n; i += stride)
    {
        float w = exp(-0.5 * (i * i) / (sigma * sigma));
        sum += Prev(ctx.px + dir * i) * w;
        wsum += w;
    }
    return sum / wsum;
}

float4 Effect(EffectContext ctx)
{
    if (ctx.pass == 0) return Blur1D(ctx, int2(1, 0));
    if (ctx.pass == 1) return Blur1D(ctx, int2(0, 1));

    float4 src = Input(0, ctx.px);
    float3 c = max(src.rgb, 0.0);
    float3 blurred = max(Prev(ctx.px).rgb, 0.0);

    // Sharpening (unsharp mask, small radius) and texture (fine detail) on the original.
    if (sx_sharpen > 0.0 || sx_fine_detail != 0.0)
    {
        float r = sx_radius;
        float3 n4 = 0;
        n4 += Input(0, ctx.px + int2(int(round(r)), 0)).rgb;
        n4 += Input(0, ctx.px - int2(int(round(r)), 0)).rgb;
        n4 += Input(0, ctx.px + int2(0, int(round(r)))).rgb;
        n4 += Input(0, ctx.px - int2(0, int(round(r)))).rgb;
        float3 detail = c - n4 * 0.25;
        c = max(c + detail * (sx_sharpen / 100.0 + sx_fine_detail / 100.0), 0.0);
    }

    // White balance: temperature along blue <-> amber, tint along green <-> magenta.
    float t = sx_temperature / 100.0, g = sx_tint / 100.0;
    c *= float3(1.0 + 0.35 * t, 1.0 - 0.25 * g, 1.0 - 0.35 * t) * float3(1.0 + 0.1 * g, 1.0, 1.0 + 0.1 * g);
    c *= exp2(sx_exposure);

    // Dehaze: remove a fraction of the local veil (dark channel of the blurred image).
    if (sx_dehaze != 0.0)
    {
        float haze = min(min(blurred.r, blurred.g), blurred.b) * exp2(sx_exposure);
        float d = sx_dehaze / 100.0;
        c = d > 0.0 ? max(c - haze * d * 0.8, 0.0) / max(1.0 - haze * d * 0.8, 0.2) : lerp(c, c + (0.5 - c) * 0.3, -d);
    }

    // Tones on a perceptual scale.
    float3 p = pow(max(c, 0.0), 1.0 / 2.2);
    float L = Luma(p);
    float hl = smoothstep(0.45, 1.0, L), sh = 1.0 - smoothstep(0.0, 0.55, L);
    float wh = smoothstep(0.75, 1.1, L), bk = 1.0 - smoothstep(0.0, 0.25, L);
    float delta = sx_highlights / 100.0 * 0.35 * hl + sx_shadows / 100.0 * 0.35 * sh + sx_whites / 100.0 * 0.25 * wh + sx_blacks / 100.0 * 0.2 * bk;
    p += delta;
    // Contrast: S curve around middle gray.
    float k = sx_contrast / 100.0;
    p = lerp(p, saturate(p) * saturate(p) * (3.0 - 2.0 * saturate(p)), k > 0 ? k : 0.0);
    p = k < 0 ? lerp(p, 0.5 + (p - 0.5) * 0.6, -k) : p;

    // Clarity: local contrast against the blurred image (midtones only).
    if (sx_clarity != 0.0)
    {
        float bl = Luma(pow(blurred * exp2(sx_exposure), 1.0 / 2.2));
        float mid = 1.0 - abs(L - 0.5) * 2.0;
        p += (Luma(p) - bl) * sx_clarity / 100.0 * 0.6 * saturate(mid + 0.3);
    }

    // Vibrance (protects saturated colors) and saturation.
    float Lp = Luma(p);
    float sat = (max(p.r, max(p.g, p.b)) - min(p.r, min(p.g, p.b))) / max(max(p.r, max(p.g, p.b)), 1e-4);
    float vib = sx_vibrance / 100.0 * (1.0 - sat);
    p = Lp + (p - Lp) * max(1.0 + sx_saturation / 100.0 + vib, 0.0);

    // Vignette (post crop style, centered).
    if (sx_vignette != 0.0)
    {
        float2 q = (ctx.uv - 0.5) * 2.0;
        float r = length(q * float2(1.0, ctx.size.y / ctx.size.x)) / 1.2;
        float m = smoothstep(sx_vig_mid / 100.0, 1.0, r);
        p = sx_vignette < 0 ? p * (1.0 + sx_vignette / 100.0 * m) : lerp(p, 1.0, sx_vignette / 100.0 * m);
    }

    // Grain (deterministic noise on luminance).
    if (sx_grain > 0.0)
    {
        float n = frac(sin(dot(float2(ctx.px), float2(12.9898, 78.233))) * 43758.5453) - 0.5;
        p += n * sx_grain / 100.0 * 0.12;
    }

    c = pow(max(p, 0.0), 2.2);
    return float4(c, src.a);
}

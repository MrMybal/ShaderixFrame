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
float tone_eval(float x){return saturate(x);}
//@name Photo RAW
//@category Adjustments
//@halo full
//@passes 3
//@group Base
//@param sx_temperature float 0 -100 100 "Temperature"
//@param sx_tint float 0 -100 100 "Hue"
//@param sx_exposure float 0 -5 5 "Exposition (IL)"
//@param sx_contrast float 0 -100 100 "Contrast"
//@param sx_highlights float 0 -100 100 "Highlights"
//@param sx_shadows float 0 -100 100 "Ombres"
//@param sx_whites float 0 -100 100 "Whites"
//@param sx_blacks float 0 -100 100 "Blacks"
//@group Presence
//@param sx_fine_detail float 0 -100 100 "Texture"
//@param sx_clarity float 0 -100 100 "Clarity"
//@param sx_dehaze float 0 -100 100 "Dehaze"
//@param sx_vibrance float 0 -100 100 "Vibrance"
//@param sx_saturation float 0 -100 100 "Saturation"
//@group Detail
//@param sx_sharpen float 0 0 150 "Sharpness"
//@param sx_radius float 1 0.5 3 "Sharpening Radius"
//@group Effects
//@param sx_vignette float 0 -100 100 "Vignettage"
//@param sx_vig_mid float 50 0 100 "Vignette Midpoint"
//@param sx_grain float 0 0 100 "Grain"
//@group Tone Curve
//@group Mixer: Hue
//@param sx_mix_h_red float 0 -100 100 "Rouges"
//@param sx_mix_h_orange float 0 -100 100 "Oranges"
//@param sx_mix_h_yellow float 0 -100 100 "Jaunes"
//@param sx_mix_h_green float 0 -100 100 "Verts"
//@param sx_mix_h_aqua float 0 -100 100 "Cyans"
//@param sx_mix_h_blue float 0 -100 100 "Bleus"
//@param sx_mix_h_purple float 0 -100 100 "Violets"
//@param sx_mix_h_magenta float 0 -100 100 "Magentas"
//@group Mixer: Saturation
//@param sx_mix_s_red float 0 -100 100 "Rouges"
//@param sx_mix_s_orange float 0 -100 100 "Oranges"
//@param sx_mix_s_yellow float 0 -100 100 "Jaunes"
//@param sx_mix_s_green float 0 -100 100 "Verts"
//@param sx_mix_s_aqua float 0 -100 100 "Cyans"
//@param sx_mix_s_blue float 0 -100 100 "Bleus"
//@param sx_mix_s_purple float 0 -100 100 "Violets"
//@param sx_mix_s_magenta float 0 -100 100 "Magentas"
//@group Mixer: Luminance
//@param sx_mix_l_red float 0 -100 100 "Rouges"
//@param sx_mix_l_orange float 0 -100 100 "Oranges"
//@param sx_mix_l_yellow float 0 -100 100 "Jaunes"
//@param sx_mix_l_green float 0 -100 100 "Verts"
//@param sx_mix_l_aqua float 0 -100 100 "Cyans"
//@param sx_mix_l_blue float 0 -100 100 "Bleus"
//@param sx_mix_l_purple float 0 -100 100 "Violets"
//@param sx_mix_l_magenta float 0 -100 100 "Magentas"
//@group Color Grading
//@param sx_cg_sh_hue float 220 0 360 "Ombres : teinte"
//@param sx_cg_sh_sat float 0 0 100 "Ombres : saturation"
//@param sx_cg_mid_hue float 40 0 360 "Tons moyens : teinte"
//@param sx_cg_mid_sat float 0 0 100 "Tons moyens : saturation"
//@param sx_cg_hl_hue float 45 0 360 "Highlight Hue"
//@param sx_cg_hl_sat float 0 0 100 "Highlight Saturation"
//@param sx_cg_balance float 0 -100 100 "Balance"
//@group Noise Reduction
//@param sx_nr_luma float 0 0 100 "Luminance"
//@param sx_nr_color float 0 0 100 "Color"
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

float3 CrRgbToHsv(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-6)), d / (q.x + 1e-6), q.x);
}

float3 CrHsvToRgb(float3 c)
{
    float3 p = abs(frac(c.xxx + float3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return c.z * lerp(float3(1, 1, 1), saturate(p - 1.0), c.y);
}

// Color mixer: per hue band (red 0, orange 30, yellow 60, green 120, aqua 180, blue 240, purple 270,
// magenta 300 degrees) hue shift / saturation / luminance, interpolated between the two nearest bands.
float3 ColorMixer(float3 p)
{
    float hs[8] = {sx_mix_h_red, sx_mix_h_orange, sx_mix_h_yellow, sx_mix_h_green, sx_mix_h_aqua, sx_mix_h_blue, sx_mix_h_purple, sx_mix_h_magenta};
    float ss[8] = {sx_mix_s_red, sx_mix_s_orange, sx_mix_s_yellow, sx_mix_s_green, sx_mix_s_aqua, sx_mix_s_blue, sx_mix_s_purple, sx_mix_s_magenta};
    float ls[8] = {sx_mix_l_red, sx_mix_l_orange, sx_mix_l_yellow, sx_mix_l_green, sx_mix_l_aqua, sx_mix_l_blue, sx_mix_l_purple, sx_mix_l_magenta};
    float centers[9] = {0, 30, 60, 120, 180, 240, 270, 300, 360};
    float3 hsv = CrRgbToHsv(saturate(p));
    float deg = hsv.x * 360.0;
    int i = 0;
    for (int k = 0; k < 8; ++k)
        if (deg >= centers[k]) i = k;
    float t = saturate((deg - centers[i]) / (centers[i + 1] - centers[i]));
    int j = (i + 1) % 8;
    float dh = lerp(hs[i], hs[j], t), ds = lerp(ss[i], ss[j], t), dl = lerp(ls[i], ls[j], t);
    float colored = smoothstep(0.02, 0.25, hsv.y);  // grays are not touched
    float3 o = hsv;
    o.x = frac(hsv.x + dh / 100.0 * (30.0 / 360.0) * colored + 1.0);
    o.y = saturate(hsv.y * (1.0 + ds / 100.0 * colored));
    float3 rgb = CrHsvToRgb(o) + max(p - saturate(p), 0.0);  // keep values above 1
    float L0 = Luma(rgb);
    float L1 = L0 * (1.0 + dl / 100.0 * 0.6 * colored);
    return L0 > 1e-5 ? rgb * (L1 / L0) : rgb;
}

// Color grading wheels: tints of the shadows / midtones / highlights (luminance preserved).
float3 ColorGrade(float3 p)
{
    float L = saturate(Luma(p));
    float b = sx_cg_balance / 100.0;
    float ws = 1.0 - smoothstep(0.0, 0.5 + 0.35 * b, L);
    float wh = smoothstep(0.5 + 0.35 * b, 1.0, L);
    float wm = saturate(1.0 - ws - wh);
    float3 sx_local_tint = 0;
    float3 c0 = CrHsvToRgb(float3(sx_cg_sh_hue / 360.0, 1, 1)), c1 = CrHsvToRgb(float3(sx_cg_mid_hue / 360.0, 1, 1)), c2 = CrHsvToRgb(float3(sx_cg_hl_hue / 360.0, 1, 1));
    sx_local_tint += (c0 - Luma(c0)) * sx_cg_sh_sat / 100.0 * ws;
    sx_local_tint += (c1 - Luma(c1)) * sx_cg_mid_sat / 100.0 * wm;
    sx_local_tint += (c2 - Luma(c2)) * sx_cg_hl_sat / 100.0 * wh;
    float3 o = max(p + sx_local_tint * 0.35, 0.0);
    float Lo = Luma(o);
    return Lo > 1e-5 ? o * (Luma(p) / Lo) : o;
}

// Noise reduction: edge-preserving average of the luminance (5x5, range weights from the noise level)
// and wide chroma average (9x9, step 2) guided by the luminance.
float3 Denoise(EffectContext ctx, float3 c)
{
    float L = Luma(c);
    float3 chroma = c - L;
    float sl = sx_nr_luma / 100.0, sc = sx_nr_color / 100.0;
    if (sl > 0.0)
    {
        float sigma = (0.004 + 0.06 * sl * sl) * (sqrt(max(L, 0.0)) + 0.05);
        float acc = 0, wsum = 0;
        for (int y = -2; y <= 2; ++y)
            for (int x = -2; x <= 2; ++x)
            {
                float n = Luma(max(Input(0, ctx.px + int2(x, y)).rgb, 0.0));
                float d = (n - L) / sigma;
                float w = exp(-0.5 * d * d) * exp(-0.18 * (x * x + y * y));
                acc += n * w;
                wsum += w;
            }
        L = lerp(L, acc / wsum, saturate(sl * 1.5));
    }
    if (sc > 0.0)
    {
        float3 acc = 0;
        float wsum = 0;
        float sigma = 0.02 + 0.1 * sc;
        for (int y = -4; y <= 4; y += 2)
            for (int x = -4; x <= 4; x += 2)
            {
                float3 n = max(Input(0, ctx.px + int2(x, y)).rgb, 0.0);
                float nl = Luma(n);
                float d = (nl - Luma(c)) / (sigma * (Luma(c) + 0.05) * 4.0);
                float w = exp(-0.5 * d * d);
                acc += (n - nl) * w;
                wsum += w;
            }
        chroma = lerp(chroma, acc / wsum, saturate(sc * 1.2));
    }
    return max(L + chroma, 0.0);
}

float4 Effect(EffectContext ctx)
{
    if (ctx.pass == 0) return Blur1D(ctx, int2(1, 0));
    if (ctx.pass == 1) return Blur1D(ctx, int2(0, 1));

    float4 src = Input(0, ctx.px);
    float3 c = max(src.rgb, 0.0);
    float3 blurred = max(Prev(ctx.px).rgb, 0.0);
    if (sx_nr_luma > 0.0 || sx_nr_color > 0.0) c = Denoise(ctx, c);

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

    // Tone curve (on the display-referred values; above 1 keeps the slope of the end of the curve).
    {
        float3 q = saturate(p);
        float3 over = p - q;
        p = float3(tone_eval(q.r), tone_eval(q.g), tone_eval(q.b)) + over;
    }

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

    float mixAny = 0;
    mixAny = abs(sx_mix_h_red) + abs(sx_mix_h_orange) + abs(sx_mix_h_yellow) + abs(sx_mix_h_green) + abs(sx_mix_h_aqua) + abs(sx_mix_h_blue) + abs(sx_mix_h_purple) + abs(sx_mix_h_magenta)
        + abs(sx_mix_s_red) + abs(sx_mix_s_orange) + abs(sx_mix_s_yellow) + abs(sx_mix_s_green) + abs(sx_mix_s_aqua) + abs(sx_mix_s_blue) + abs(sx_mix_s_purple) + abs(sx_mix_s_magenta)
        + abs(sx_mix_l_red) + abs(sx_mix_l_orange) + abs(sx_mix_l_yellow) + abs(sx_mix_l_green) + abs(sx_mix_l_aqua) + abs(sx_mix_l_blue) + abs(sx_mix_l_purple) + abs(sx_mix_l_magenta);
    if (mixAny > 0.0) p = ColorMixer(p);
    if (sx_cg_sh_sat > 0.0 || sx_cg_mid_sat > 0.0 || sx_cg_hl_sat > 0.0) p = ColorGrade(p);

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
        float n = frac(sin(dot(((float2)(ctx.px)), float2(12.9898, 78.233))) * 43758.5453) - 0.5;
        p += n * sx_grain / 100.0 * 0.12;
    }

    c = pow(max(p, 0.0), 2.2);
    return float4(c, src.a);
}

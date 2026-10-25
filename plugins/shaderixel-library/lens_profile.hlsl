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
//@name Lens Correction
//@category Distortion
//@halo full
//@premultiplied
//@param sx_mode int 0 0 1 "Mode"
//@group Distorsion
//@param sx_dist_model int 0 0 3 "Template"
//@param sx_d_a float 0 -0.5 0.5 "k1 / a"
//@param sx_d_b float 0 -0.5 0.5 "k2 / b"
//@param sx_d_c float 0 -0.5 0.5 "c"
//@param sx_dist_amount float 1 0 2 "Distortion Strength"
//@group Aberrations chromatiques
//@param sx_tca_model int 0 0 2 "Template"
//@param sx_tca_vr float 1 0.99 1.01 "Red: kr / vr"
//@param sx_tca_vb float 1 0.99 1.01 "Blue: kb / vb"
//@param sx_tca_cr float 0 -0.01 0.01 "Red: cr"
//@param sx_tca_cb float 0 -0.01 0.01 "Blue: cb"
//@param sx_tca_br float 0 -0.01 0.01 "Red: br"
//@param sx_tca_bb float 0 -0.01 0.01 "Blue: bb"
//@param sx_tca_amount float 1 0 4 "Aberration Strength"
//@group Vignetage
//@param sx_vig bool 0 "Vignetting (pa Model)"
//@param sx_v_k1 float 0 -2 2 "k1"
//@param sx_v_k2 float 0 -2 2 "k2"
//@param sx_v_k3 float 0 -2 2 "k3"
//@param sx_vig_amount float 1 0 2 "Vignette Strength"
//@group Framing
//@param sx_crop_ratio float 1 0.3 3 "Calibration Crop / Sensor Crop"
//@param sx_autoscale bool 1 "Fill Frame"
//@param sx_zoom float 1 0.5 2 "Zoom"
// Lens profiles of the lensfun database (distortion ptlens / poly3 / poly5, TCA linear / poly3,
// vignetting pa), in lensfun's conventions: the models map an undistorted radius to the distorted one,
// r = 1 is the half short edge of a 3:2 sensor at the calibration crop factor (geometry) and the image
// corner (vignetting). "Corriger" removes the lens defects; "Simuler" adds them to any image (inverse
// mapping by Newton iterations), the image being taken as shot with that lens.

float Distort(float ru)
{
    // Rd = f(Ru), amount 0..2 blends with the identity.
    float rd = ru;
    if (sx_dist_model == 1) rd = ru * (1.0 - sx_d_a + sx_d_a * ru * ru);
    else if (sx_dist_model == 2) rd = ru * (1.0 + sx_d_a * ru * ru + sx_d_b * ru * ru * ru * ru);
    else if (sx_dist_model == 3) rd = ru * (sx_d_a * ru * ru * ru + sx_d_b * ru * ru + sx_d_c * ru + 1.0 - sx_d_a - sx_d_b - sx_d_c);
    return ru + (rd - ru) * sx_dist_amount;
}

float DistortInverse(float rd)
{
    // Ru with f(Ru) = Rd (Newton, numeric derivative).
    float ru = rd;
    for (int i = 0; i < 8; ++i)
    {
        float f = Distort(ru) - rd;
        float d = (Distort(ru + 1e-3) - Distort(ru - 1e-3)) / 2e-3;
        ru -= f / max(abs(d), 1e-4) * sign(d);
    }
    return ru;
}

// Distorted radius of red (c = 0) or blue (c = 1) for a green radius r.
float Tca(float r, int c)
{
    float k = 1.0;
    if (sx_tca_model == 1) k = c == 0 ? sx_tca_vr : sx_tca_vb;
    else if (sx_tca_model == 2) k = c == 0 ? (sx_tca_br * r * r + sx_tca_cr * r + sx_tca_vr) : (sx_tca_bb * r * r + sx_tca_cb * r + sx_tca_vb);
    return r * (1.0 + (k - 1.0) * sx_tca_amount);
}

float Vignette(float rv)
{
    float r2 = rv * rv;
    float p = 1.0 + sx_v_k1 * r2 + sx_v_k2 * r2 * r2 + sx_v_k3 * r2 * r2 * r2;
    return lerp(1.0, max(p, 0.02), sx_vig_amount);
}

float4 Effect(EffectContext ctx)
{
    const float2 hsz = ctx.size * 0.5;
    const float halfDiag = length(hsz);
    // Pixels -> normalized radius (geometry: 3:2 hsz short edge at the calibration crop; vignetting: corner).
    const float kGeom = 21.6333 * sx_crop_ratio / (12.0 * halfDiag);
    const float kVig = sx_crop_ratio / halfDiag;
    float2 p = ctx.px + 0.5 - hsz;
    float scale = 1.0 / max(sx_zoom, 0.05);
    if (sx_mode == 0 && sx_autoscale && sx_dist_model != 0)
    {
        // Largest zoom-out keeping the frame filled: corner and edge middles land inside the source.
        float s = 1.0;
        float2 pts[3] = {hsz, float2(hsz.x, 0), float2(0, hsz.y)};
        for (int i = 0; i < 3; ++i)
        {
            float ru = length(pts[i]) * kGeom;
            float rd = Distort(ru);
            float2 q = pts[i] * (rd / max(ru, 1e-6));
            s = min(s, min(hsz.x / max(abs(q.x), 1e-3), hsz.y / max(abs(q.y), 1e-3)));
        }
        scale *= s;
    }
    if (sx_mode == 0)
    {
        // Correction: output (undistorted) -> source (distorted) position, per channel.
        float2 pu = p * scale;
        float ru = length(pu) * kGeom;
        float2 pg = pu * (Distort(ru) / max(ru, 1e-6));
        float rg = length(pg) * kGeom;
        float2 pr = pg * (Tca(rg, 0) / max(rg, 1e-6));
        float2 pb = pg * (Tca(rg, 1) / max(rg, 1e-6));
        float4 cg = InputUV(0, (pg + hsz) * ctx.texel);
        float4 c = float4(InputUV(0, (pr + hsz) * ctx.texel).r, cg.g, InputUV(0, (pb + hsz) * ctx.texel).b, cg.a);
        if (sx_vig) c.rgb /= Vignette(length(pg) * kVig);
        return c;
    }
    // Simulation: output (as photographed) -> the image (undistorted) position, per channel.
    float2 q = p * scale;
    float4 o = 0;
    for (int ch = 0; ch < 3; ++ch)
    {
        // Channel position -> green position (TCA inverse, fixed point), then distortion inverse.
        float2 g = q;
        if (ch != 1)
            for (int i = 0; i < 3; ++i)
            {
                float r = length(g) * kGeom;
                g = q * (r / max(Tca(r, ch == 0 ? 0 : 1), 1e-6));
            }
        float rd = length(g) * kGeom;
        float2 u = g * (DistortInverse(rd) / max(rd, 1e-6));
        float4 s = InputUV(0, (u + hsz) * ctx.texel);
        if (ch == 0) o.r = s.r;
        else if (ch == 1) { o.g = s.g; o.a = s.a; }
        else o.b = s.b;
    }
    if (sx_vig) o.rgb *= Vignette(length(q) * kVig);
    return o;
}

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
//@name Image Adjustments
//@category Adjustments
//@param sx_exposure float 0 -5 5 "Exposure (EV)"
//@param sx_brightness float 0 -1 1 "Luminosity"
//@param sx_contrast float 0 -1 1 "Contrast"
//@param sx_gamma float 1 0.2 5 "Gamma"
//@param sx_saturation float 0 -1 1 "Saturation"
//@param sx_vibrance float 0 -1 1 "Vibrance"
//@param sx_hue float 0 -180 180 "Hue (°)"
//@param sx_temperature float 0 -1 1 "Temperature"
//@param sx_tint float 0 -1 1 "Tint"
// Base image adjustments of an image layer (Propriétés > Réglages de l'image), in one pass.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 l = c.rgb * exp2(sx_exposure);
    // White balance (linear): warm / cool, green / magenta.
    l *= float3(1.0 + sx_temperature * 0.25, 1.0 - sx_tint * 0.15, 1.0 - sx_temperature * 0.25) * float3(1.0 + sx_tint * 0.08, 1.0, 1.0 + sx_tint * 0.08);
    float3 s = LinearToSrgb(max(l, 0.0));
    s += sx_brightness;
    float k = sx_contrast >= 0.0 ? 1.0 / max(1.0 - sx_contrast, 0.01) : 1.0 + sx_contrast;
    s = (s - 0.5) * k + 0.5;
    s = pow(max(s, 0.0), 1.0 / max(sx_gamma, 0.01));
    // Saturation and vibrance (vibrance protects colors already saturated).
    float y = Luma(s);
    float sat = max(s.r, max(s.g, s.b)) - min(s.r, min(s.g, s.b));
    s = lerp(float3(y, y, y), s, 1.0 + sx_saturation + sx_vibrance * (1.0 - saturate(sat)));
    if (abs(sx_hue) > 0.01)
    {
        float3 hsv = RGBToHSV(saturate(s));
        hsv.x = frac(hsv.x + sx_hue / 360.0);
        s = HSVToRGB(hsv) + (s - saturate(s));
    }
    return float4(SrgbToLinear(max(s, 0.0)), c.a);
}

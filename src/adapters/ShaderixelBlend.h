// Generated from the locked Shaderixel blend shader.
#pragma once
namespace sf { inline constexpr const char *shaderixelBlendModes = R"SF(// Photoshop-compatible blend modes (separable + non-separable), operating on
// STRAIGHT colors. Indices match sx::BlendMode in src/doc/Document.h.
// Math ported from CyTexture (CyTextureLayer.usf).

float BlendLum(float3 c) { return dot(c, float3(0.3, 0.59, 0.11)); }
float BlendSat(float3 c) { return max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b)); }

float3 ClipBlendColor(float3 c)
{
    float l = BlendLum(c);
    float n = min(c.r, min(c.g, c.b));
    float x = max(c.r, max(c.g, c.b));
    if (n < 0.0) c = l + ((c - l) * l) / max(l - n, 1e-5);
    if (x > 1.0) c = l + ((c - l) * (1.0 - l)) / max(x - l, 1e-5);
    return c;
}

float3 SetBlendLum(float3 c, float l) { return ClipBlendColor(c + (l - BlendLum(c))); }

float3 SetBlendSat(float3 c, float s)
{
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    return mx > mn ? (c - mn) * s / (mx - mn) : float3(0, 0, 0);
}

float SoftLight1(float b, float f)
{
    return f <= 0.5 ? b - (1.0 - 2.0 * f) * b * (1.0 - b)
                    : b + (2.0 * f - 1.0) * ((b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(max(b, 0.0))) - b);
}

float3 ColorDodge(float3 b, float3 f) { return (b <= 0.0 ? 0.0 : min(1.0, b / max(1.0 - f, 1e-5))); }
float3 ColorBurn(float3 b, float3 f) { return (b >= 1.0 ? 1.0 : 1.0 - min(1.0, (1.0 - b) / max(f, 1e-5))); }
float3 VividLight(float3 b, float3 f) { return lerp(ColorBurn(b, 2.0 * f), ColorDodge(b, 2.0 * (f - 0.5)), step(0.5, f)); }

// b = backdrop, f = source (straight colors)
float3 BlendColor(float3 b, float3 f, uint mode)
{
    switch (mode)
    {
    case 1: return b * f;                                                       // Multiply
    case 2: return b + f - b * f;                                               // Screen
    case 3: return lerp(2.0 * b * f, 1.0 - 2.0 * (1.0 - b) * (1.0 - f), step(0.5, b)); // Overlay
    case 4: return b + f;                                                       // Linear Dodge (Add)
    case 5: return b - f;                                                       // Subtract
    case 6: return min(b, f);                                                   // Darken
    case 7: return max(b, f);                                                   // Lighten
    case 8: return abs(b - f);                                                  // Difference
    case 9: return ColorDodge(b, f);                                            // Color Dodge
    case 10: return ColorBurn(b, f);                                            // Color Burn
    case 11: return lerp(2.0 * b * f, 1.0 - 2.0 * (1.0 - b) * (1.0 - f), step(0.5, f)); // Hard Light
    case 12: return float3(SoftLight1(b.r, f.r), SoftLight1(b.g, f.g), SoftLight1(b.b, f.b)); // Soft Light
    case 13: return f;                                                          // Dissolve (alpha handled by caller)
    case 14: return b + f - 1.0;                                                // Linear Burn
    case 15: return VividLight(b, f);                                           // Vivid Light
    case 16: return b + 2.0 * f - 1.0;                                          // Linear Light
    case 17: return lerp(min(b, 2.0 * f), max(b, 2.0 * (f - 0.5)), step(0.5, f)); // Pin Light
    case 18: return step(1.0, b + f);                                           // Hard Mix
    case 19: return b + f - 2.0 * b * f;                                        // Exclusion
    case 20: return b / max(f, 1e-5);                                           // Divide
    case 21: return SetBlendLum(SetBlendSat(f, BlendSat(b)), BlendLum(b));      // Hue
    case 22: return SetBlendLum(SetBlendSat(b, BlendSat(f)), BlendLum(b));      // Saturation
    case 23: return SetBlendLum(f, BlendLum(b));                                // Color
    case 24: return SetBlendLum(b, BlendLum(f));                                // Luminosity
    case 25: return BlendLum(f) < BlendLum(b) ? f : b;                          // Darker Color
    case 26: return BlendLum(f) > BlendLum(b) ? f : b;                          // Lighter Color
    default: return f;                                                          // Normal
    }
}
)SF"; }
namespace sf { inline constexpr const char *shaderixelComposite = R"SF(//@param gMode int 0 0 26
//@param gOpacity float 1 0 1
//@param gFlags int 12 0 65535
//@param sfSrgb bool 0
//@param sfSelf00 float 0 0 1
//@param sfSelf01 float 0 0 1
//@param sfSelf02 float 1 0 1
//@param sfSelf03 float 1 0 1
//@param sfSelf10 float 0 0 1
//@param sfSelf11 float 0 0 1
//@param sfSelf12 float 1 0 1
//@param sfSelf13 float 1 0 1
//@param sfSelf20 float 0 0 1
//@param sfSelf21 float 0 0 1
//@param sfSelf22 float 1 0 1
//@param sfSelf23 float 1 0 1
//@param sfSelf30 float 0 0 1
//@param sfSelf31 float 0 0 1
//@param sfSelf32 float 1 0 1
//@param sfSelf33 float 1 0 1
//@param sfUnder00 float 0 0 1
//@param sfUnder01 float 0 0 1
//@param sfUnder02 float 1 0 1
//@param sfUnder03 float 1 0 1
//@param sfUnder10 float 0 0 1
//@param sfUnder11 float 0 0 1
//@param sfUnder12 float 1 0 1
//@param sfUnder13 float 1 0 1
//@param sfUnder20 float 0 0 1
//@param sfUnder21 float 0 0 1
//@param sfUnder22 float 1 0 1
//@param sfUnder23 float 1 0 1
//@param sfUnder30 float 0 0 1
//@param sfUnder31 float 0 0 1
//@param sfUnder32 float 1 0 1
//@param sfUnder33 float 1 0 1
// Photoshop-compatible blend modes (separable + non-separable), operating on
// STRAIGHT colors. Indices match sx::BlendMode in src/doc/Document.h.
// Math ported from CyTexture (CyTextureLayer.usf).

float BlendLum(float3 c) { return dot(c, float3(0.3, 0.59, 0.11)); }
float BlendSat(float3 c) { return max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b)); }

float3 ClipBlendColor(float3 c)
{
    float l = BlendLum(c);
    float n = min(c.r, min(c.g, c.b));
    float x = max(c.r, max(c.g, c.b));
    if (n < 0.0) c = l + ((c - l) * l) / max(l - n, 1e-5);
    if (x > 1.0) c = l + ((c - l) * (1.0 - l)) / max(x - l, 1e-5);
    return c;
}

float3 SetBlendLum(float3 c, float l) { return ClipBlendColor(c + (l - BlendLum(c))); }

float3 SetBlendSat(float3 c, float s)
{
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    return mx > mn ? (c - mn) * s / (mx - mn) : float3(0, 0, 0);
}

float SoftLight1(float b, float f)
{
    return f <= 0.5 ? b - (1.0 - 2.0 * f) * b * (1.0 - b)
                    : b + (2.0 * f - 1.0) * ((b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(max(b, 0.0))) - b);
}

float3 ColorDodge(float3 b, float3 f) { return (b <= 0.0 ? 0.0 : min(1.0, b / max(1.0 - f, 1e-5))); }
float3 ColorBurn(float3 b, float3 f) { return (b >= 1.0 ? 1.0 : 1.0 - min(1.0, (1.0 - b) / max(f, 1e-5))); }
float3 VividLight(float3 b, float3 f) { return lerp(ColorBurn(b, 2.0 * f), ColorDodge(b, 2.0 * (f - 0.5)), step(0.5, f)); }

// b = backdrop, f = source (straight colors)
float3 BlendColor(float3 b, float3 f, uint mode)
{
    switch (mode)
    {
    case 1: return b * f;                                                       // Multiply
    case 2: return b + f - b * f;                                               // Screen
    case 3: return lerp(2.0 * b * f, 1.0 - 2.0 * (1.0 - b) * (1.0 - f), step(0.5, b)); // Overlay
    case 4: return b + f;                                                       // Linear Dodge (Add)
    case 5: return b - f;                                                       // Subtract
    case 6: return min(b, f);                                                   // Darken
    case 7: return max(b, f);                                                   // Lighten
    case 8: return abs(b - f);                                                  // Difference
    case 9: return ColorDodge(b, f);                                            // Color Dodge
    case 10: return ColorBurn(b, f);                                            // Color Burn
    case 11: return lerp(2.0 * b * f, 1.0 - 2.0 * (1.0 - b) * (1.0 - f), step(0.5, f)); // Hard Light
    case 12: return float3(SoftLight1(b.r, f.r), SoftLight1(b.g, f.g), SoftLight1(b.b, f.b)); // Soft Light
    case 13: return f;                                                          // Dissolve (alpha handled by caller)
    case 14: return b + f - 1.0;                                                // Linear Burn
    case 15: return VividLight(b, f);                                           // Vivid Light
    case 16: return b + 2.0 * f - 1.0;                                          // Linear Light
    case 17: return lerp(min(b, 2.0 * f), max(b, 2.0 * (f - 0.5)), step(0.5, f)); // Pin Light
    case 18: return step(1.0, b + f);                                           // Hard Mix
    case 19: return b + f - 2.0 * b * f;                                        // Exclusion
    case 20: return b / max(f, 1e-5);                                           // Divide
    case 21: return SetBlendLum(SetBlendSat(f, BlendSat(b)), BlendLum(b));      // Hue
    case 22: return SetBlendLum(SetBlendSat(b, BlendSat(f)), BlendLum(b));      // Saturation
    case 23: return SetBlendLum(f, BlendLum(b));                                // Color
    case 24: return SetBlendLum(b, BlendLum(f));                                // Luminosity
    case 25: return BlendLum(f) < BlendLum(b) ? f : b;                          // Darker Color
    case 26: return BlendLum(f) > BlendLum(b) ? f : b;                          // Lighter Color
    default: return f;                                                          // Normal
    }
}
float SrgbToLinear1(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float LinearToSrgb1(float c) { c = max(c, 0.0); return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
float3 SrgbToLinear(float3 c) { return float3(SrgbToLinear1(c.r), SrgbToLinear1(c.g), SrgbToLinear1(c.b)); }
float3 LinearToSrgb(float3 c) { return float3(LinearToSrgb1(c.r), LinearToSrgb1(c.g), LinearToSrgb1(c.b)); }

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float RangeWeight(float y, float4 r)
{
    float up = r.y > r.x ? saturate((y - r.x) / (r.y - r.x)) : (y >= r.x ? 1.0 : 0.0);
    float down = r.w > r.z ? saturate((r.w - y) / (r.w - r.z)) : (y <= r.w ? 1.0 : 0.0);
    return up * down;
}

float4 Effect(EffectContext ctx){int2 ap=ctx.px;float4 base=Premultiply(Input(0,ctx.px)),top=Premultiply(Input(1,ctx.px));
float4 gSelfRange[4]={float4(sfSelf00,sfSelf01,sfSelf02,sfSelf03),float4(sfSelf10,sfSelf11,sfSelf12,sfSelf13),float4(sfSelf20,sfSelf21,sfSelf22,sfSelf23),float4(sfSelf30,sfSelf31,sfSelf32,sfSelf33)};
float4 gUnderRange[4]={float4(sfUnder00,sfUnder01,sfUnder02,sfUnder03),float4(sfUnder10,sfUnder11,sfUnder12,sfUnder13),float4(sfUnder20,sfUnder21,sfUnder22,sfUnder23),float4(sfUnder30,sfUnder31,sfUnder32,sfUnder33)};
    float mask = (gFlags & 1) ? saturate(Input(2,ctx.px).r) : 1.0;
    float as = saturate(top.a * gOpacity * mask);
    if (gMode == 13) as = Hash12(float2(ap)) <= as ? 1.0 : 0.0;   // Dissolve
    float ab = saturate(base.a);

    float3 cs = Unpremultiply(top).rgb;
    float3 cb = Unpremultiply(base).rgb;
    if (gFlags & 32)
    {
        const float3 w = float3(0.299, 0.587, 0.114);
        float3 s = sfSrgb ? saturate(cs) : LinearToSrgb(saturate(cs)), u = sfSrgb ? saturate(cb) : LinearToSrgb(saturate(cb));
        as *= RangeWeight(dot(s, w), gSelfRange[0]) * RangeWeight(dot(u, w), gUnderRange[0]);
        for (int c = 0; c < 3; ++c)
            as *= RangeWeight(s[c], gSelfRange[c + 1]) * RangeWeight(u[c], gUnderRange[c + 1]);
    }
    float3 mixed;
    if (gFlags & 2)
        mixed = SrgbToLinear(BlendColor(LinearToSrgb(cb), LinearToSrgb(cs), gMode));
    else
        mixed = BlendColor(cb, cs, gMode);

    // W3C compositing: separable blend then source-over, premultiplied result.
    float3 rgb = as * (1.0 - ab) * cs + as * ab * mixed + (1.0 - as) * ab * cb;
    float a = as + ab * (1.0 - as);
    if (gFlags & 16)
    {
        // Clipping mask: the top only shows where the base exists; base alpha is kept.
        rgb = as * ab * mixed + (1.0 - as) * ab * cb;
        a = ab;
    }
    float4 result = float4(rgb, a);
    uint keep = (gFlags >> 8) & 15;
    if (keep)
    {
        // Channel mask: unchecked channels stay as they were below (premultiplied values).
        [unroll] for (int c = 0; c < 4; ++c)
            if (keep & (1u << c)) result[c] = base[c];
    }
return Unpremultiply(result);}
)SF"; }

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
//@name Estimated Height from Photo
//@category PBR Textures
//@input Photo
//@input Blurred Lighting
//@param sx_detail float 1 0 8 "Detail Height"
//@param sx_forme float 0.25 0 2 "Large-Scale Shapes"
//@param sx_centre float 0.5 0 1 "Midpoint Height"
//@param sx_gamma float 1 0.1 4 "Height Curve"
//@param sx_inverser bool 0 "Invert Height"
// SrgbToLinear encodes data to compensate for the engine PNG encoding.
// All new map effects decode these values with LinearToSrgb.
float4 Effect(EffectContext ctx)
{
    float y = Luma(LinearToSrgb(max(Input(0, ctx.px).rgb, 0.0)));
    float b = HasInput(1) ? Luma(LinearToSrgb(max(Input(1, ctx.px).rgb, 0.0))) : y;
    float h = saturate(sx_centre + (y - b) * sx_detail + (b - 0.5) * sx_forme);
    h = pow(h, max(sx_gamma, 0.1));
    h = sx_inverser > 0.5 ? 1.0 - h : h;
    return float4(SrgbToLinear(h.xxx), 1.0);
}

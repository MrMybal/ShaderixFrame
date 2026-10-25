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
//@name Data Map Adjustment
//@category PBR Textures
//@input Map
//@param sx_canal int 0 0 4 "Source Channel"
//@param sx_noir float 0 0 1 "Black Point"
//@param sx_blanc float 1 0 1 "White Point"
//@param sx_gamma float 1 0.1 4 "Curve"
//@param sx_inverser bool 0 "Invert"
//@param sx_minimum float 0 0 1 "Minimum Value"
//@param sx_maximum float 1 0 1 "Maximum Value"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float3 data = LinearToSrgb(max(c.rgb, 0.0));
    float v = sx_canal < 0.5 ? data.r : sx_canal < 1.5 ? data.g : sx_canal < 2.5 ? data.b : sx_canal < 3.5 ? c.a : Luma(data);
    float span = sx_blanc - sx_noir;
    span = abs(span) < 0.0001 ? (span < 0.0 ? -0.0001 : 0.0001) : span;
    v = pow(saturate((v - sx_noir) / span), max(sx_gamma, 0.1));
    v = sx_inverser > 0.5 ? 1.0 - v : v;
    v = saturate(lerp(sx_minimum, sx_maximum, v));
    return float4(SrgbToLinear(v.xxx), 1.0);
}

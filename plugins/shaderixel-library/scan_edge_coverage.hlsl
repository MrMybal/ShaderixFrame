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
//@name Scan Edge Coverage
//@category Scan and Line Art
//@halo radius 1 1
//@param sx_radius int 1 1 8 "Edge Radius (px)"
//@param sx_strength float 3 0 16 "Edge Strength"
//@param sx_threshold float 0.05 0 1 "Edge Threshold"
//@param sx_softness float 0.1 0.001 1 "Edge Softness"
float ScanLuma(int2 p)
{
    float4 c = Input(0, p);
    return Luma(lerp(float3(1, 1, 1), saturate(LinearToSrgb(max(c.rgb, 0.0))), saturate(c.a)));
}
float4 Effect(EffectContext ctx)
{
    int r = clamp(sx_radius, 1, 8);
    int2 last = ((int2)(ctx.size)) - 1;
    float a = ScanLuma(clamp(ctx.px + int2(-r, -r), ((int2)(0)), last));
    float b = ScanLuma(clamp(ctx.px + int2(0, -r), ((int2)(0)), last));
    float c = ScanLuma(clamp(ctx.px + int2(r, -r), ((int2)(0)), last));
    float d = ScanLuma(clamp(ctx.px + int2(-r, 0), ((int2)(0)), last));
    float f = ScanLuma(clamp(ctx.px + int2(r, 0), ((int2)(0)), last));
    float g = ScanLuma(clamp(ctx.px + int2(-r, r), ((int2)(0)), last));
    float h = ScanLuma(clamp(ctx.px + int2(0, r), ((int2)(0)), last));
    float i = ScanLuma(clamp(ctx.px + int2(r, r), ((int2)(0)), last));
    float2 grad = float2(c + 2.0*f + i - a - 2.0*d - g, g + 2.0*h + i - a - 2.0*b - c) * 0.25;
    float m = smoothstep(sx_threshold, sx_threshold + max(sx_softness, 0.001), length(grad) * sx_strength);
    return float4((m * saturate(Input(0, ctx.px).a)).xxx, 1.0);
}

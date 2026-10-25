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
//@name Logo Alpha Bevel
//@category Text and Logo Styles
//@halo width 1 1
//@param sx_width int 2 1 16 "Bevel Width (px)"
//@param sx_depth float 3 0 12 "Bevel Depth"
//@param sx_angle float 135 -180 180 "Light Angle (degrees)"
//@param sx_highlight color 1 1 1 1 "Highlight Color"
//@param sx_shadow color 0 0 0 1 "Shadow Color"
//@param sx_amount float 0.7 0 1 "Bevel Strength"
float LogoAlpha(int2 p, float2 size)
{
    return any(p < 0) || any(p >= ((int2)(size))) ? 0.0 : Input(0, p).a;
}
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int r = clamp(sx_width, 1, 16);
    float2 grad = float2(LogoAlpha(ctx.px + int2(r, 0), ctx.size) - LogoAlpha(ctx.px - int2(r, 0), ctx.size),
                        LogoAlpha(ctx.px + int2(0, r), ctx.size) - LogoAlpha(ctx.px - int2(0, r), ctx.size));
    float3 n = normalize(float3(-grad * sx_depth, 1.0));
    float a = radians(sx_angle);
    float3 light = normalize(float3(cos(a), sin(a), 0.8));
    float lit = dot(n, light) - light.z;
    float4 tint = lit > 0.0 ? sx_highlight : sx_shadow;
    return float4(lerp(c.rgb, tint.rgb, saturate(abs(lit) * 2.0 * sx_amount * tint.a)), c.a);
}

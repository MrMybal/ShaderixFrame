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
//@name Torn Paper
//@category Procedural Patterns
//@generator
//@param sx_center float2 0.5 0.5 0 1 "Paper Center (UV)"
//@param sx_half_size float2 0.35 0.35 0.01 1 "Paper Half Size (UV)"
//@param sx_angle float -6 -180 180 "Rotation (degrees)"
//@param sx_roughness float 12 0 128 "Tear Amplitude (px)"
//@param sx_scale float 32 2 256 "Tear Scale (px)"
//@param sx_softness float 1.2 0.1 8 "Edge Softness (px)"
//@param sx_grain float 0.03 0 0.3 "Paper Grain"
//@param sx_seed int 9 0 65535 "Seed"
//@param sx_color color 0.9 0.83 0.67 1 "Paper Color"

// Rotatable paper cutout with multiscale torn edges, fine grain and transparent surroundings.
float ExtNoise(float2 p)
{
    float2 cell = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash12(cell), Hash12(cell + float2(1,0)), f.x),
                lerp(Hash12(cell + float2(0,1)), Hash12(cell + 1.0), f.x), f.y);
}
float ExtTerrain(float2 p)
{
    return ExtNoise(p)*0.5714286 + ExtNoise(p*2.03+19.7)*0.2857143 + ExtNoise(p*4.11+71.3)*0.1428571;
}

float4 Effect(EffectContext ctx)
{
    float a=radians(sx_angle); float2 p=(ctx.uv-sx_center)*ctx.size;
    p=float2(dot(p,float2(cos(a),sin(a))),dot(p,float2(-sin(a),cos(a))));
    float2 halfSize=sx_half_size*ctx.size;
    float dx=abs(p.x)-halfSize.x+(ExtTerrain(float2(p.y/max(sx_scale,2.0),sign(p.x)*17.0)+sx_seed*9.1)-0.5)*sx_roughness;
    float dy=abs(p.y)-halfSize.y+(ExtTerrain(float2(p.x/max(sx_scale,2.0),sign(p.y)*31.0)+sx_seed*7.3)-0.5)*sx_roughness;
    float coverage=1.0-smoothstep(-max(sx_softness,0.1)*0.5,max(sx_softness,0.1)*0.5,max(dx,dy));
    float variation=(Hash12(((float2)(ctx.px))+sx_seed*19.7)-0.5)*sx_grain;
    float3 rgb=SrgbToLinear(saturate(LinearToSrgb(max(sx_color.rgb,0.0))+variation));
    return float4(rgb,coverage*sx_color.a);
}

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
//@name Screen Print Wear
//@category Illustration and Print
//@input Ink
//@input Mask
//@param sx_amount float 0.25 0 1 "Wear Amount"
//@param sx_scale float 5 1 256 "Hole Scale (px)"
//@param sx_softness float 0.06 0.001 0.3 "Hole Softness"
//@param sx_scratches float 0.2 0 1 "Scratch Contribution"
//@param sx_seed int 3 0 65535 "Seed"

// Remove ink alpha with deterministic holes and elongated scratches, optionally restricted by a linear coverage mask.
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
    float4 c=Input(0,ctx.px);
    if(sx_amount<=0.0) return c;
    float2 p=((float2)(ctx.px))+float2(sx_seed*17.0,sx_seed*31.0);
    float holes=ExtNoise(p/max(sx_scale,1.0));
    float streak=ExtNoise(p/float2(max(sx_scale,1.0)*12.0,max(sx_scale,1.0)*0.3));
    float field=lerp(holes,min(holes,streak),sx_scratches);
    float worn=sx_amount>=1.0?1.0:1.0-smoothstep(sx_amount-sx_softness,sx_amount+sx_softness,field);
    float mask=HasInput(1)?saturate(Input(1,ctx.px).r*Input(1,ctx.px).a):1.0;
    return float4(c.rgb,c.a*(1.0-worn*mask));
}

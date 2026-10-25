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
//@name Selective Detail
//@category Texture Processing
//@input Image
//@input Mask
//@halo radius 1 1
//@param sx_amount float 0.7 -1 3 "Detail Strength"
//@param sx_radius int 2 1 8 "Detail Radius (px)"
//@param sx_threshold float 0.025 0 0.5 "Detail Threshold"
//@param sx_low float 0.05 0 1 "Luminance Low"
//@param sx_high float 0.95 0 1 "Luminance High"
//@param sx_softness float 0.08 0.001 0.5 "Luminance Softness"

// Sparse alpha-weighted high-pass detail with luminance gating and a flat-region threshold.
float4 Effect(EffectContext ctx)
{
    float4 c=Input(0,ctx.px);
    if(sx_amount==0.0) return c;
    float3 sum=0.0; float weight=0.0; int r=clamp(sx_radius,1,8);
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float4 t=Input(0,ctx.px+int2(x,y)*r);
        sum+=LinearToSrgb(max(t.rgb,0.0))*t.a; weight+=t.a;
    }
    float3 rgb=LinearToSrgb(max(c.rgb,0.0));
    float3 d=rgb-(weight>0.000001?sum/weight:rgb);
    float detail=smoothstep(sx_threshold,sx_threshold+0.02,max(abs(d.r),max(abs(d.g),abs(d.b))));
    if(detail<=0.0) return c;
    float l=Luma(rgb), lo=min(sx_low,sx_high), hi=max(sx_low,sx_high);
    float range=smoothstep(lo-sx_softness,lo,l)*(1.0-smoothstep(hi,hi+sx_softness,l));
    float mask=HasInput(1)?saturate(Input(1,ctx.px).r*Input(1,ctx.px).a):1.0;
    return float4(SrgbToLinear(max(rgb+d*sx_amount*detail*range*mask,0.0)),c.a);
}

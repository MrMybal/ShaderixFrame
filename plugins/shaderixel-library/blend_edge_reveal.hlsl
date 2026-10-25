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
//@name Edge Reveal
//@category Creative Blends
//@blend
//@input Background
//@input Foreground
//@input Mask
//@halo reach 1 1
//@param sx_progress float 0.5 0 1 "Transition Progress"
//@param sx_reach int 24 1 64 "Search Reach (px)"
//@param sx_softness float 2 0.1 16 "Front Softness (px)"
//@param sx_opacity float 1 0 1 "Foreground Opacity"

// Reveal inward from a mask contour using bounded eight-direction distance approximation; progress one reveals the full mask.
float4 ExtMix(float4 a, float4 b, float t)
{
    return Unpremultiply(lerp(Premultiply(a), Premultiply(b), saturate(t)));
}
float4 ExtOver(float4 a, float4 b, float coverage)
{
    b.a = saturate(b.a * coverage);
    float4 ap = Premultiply(a), bp = Premultiply(b);
    return Unpremultiply(bp + ap * (1.0 - b.a));
}

float ExtCoverage(int2 p, float2 size)
{
    if(any(p<0)||any(p>=((int2)(size)))) return 0.0;
    return HasInput(2)?saturate(Input(2,p).r*Input(2,p).a):saturate(Input(1,p).a);
}
float4 Effect(EffectContext ctx)
{
    float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
    if(sx_progress<=0.0||sx_opacity<=0.0) return a;
    float m=HasInput(2)?ExtCoverage(ctx.px,ctx.size):1.0;
    if(sx_progress>=1.0) return ExtOver(a,b,m*sx_opacity);
    int r=clamp(sx_reach,1,64); float nearest=float(r)+sx_softness+1.0;
    for(int dir=0;dir<8;++dir)
    {
        float angle=radians(float(dir)*45.0); float2 axis=float2(cos(angle),sin(angle));
        for(int i=1;i<=r;++i)
        {
            if(ExtCoverage(ctx.px+((int2)(round(axis*i))),ctx.size)<0.5) {nearest=min(nearest,float(i));break;}
        }
    }
    float front=sx_progress*(float(r)+sx_softness);
    float reveal=1.0-smoothstep(front-sx_softness,front+sx_softness,nearest);
    return ExtOver(a,b,m*reveal*sx_opacity);
}

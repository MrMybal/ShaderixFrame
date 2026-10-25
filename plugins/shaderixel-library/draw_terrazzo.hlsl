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
//@name Terrazzo
//@category Procedural Patterns
//@generator
//@param sx_size float 36 4 512 "Chip Cell Size (px)"
//@param sx_density float 0.8 0 1 "Chip Density"
//@param sx_variation float 0.7 0 1 "Chip Size Variation"
//@param sx_seed int 11 0 65535 "Seed"
//@param sx_background color 0.78 0.72 0.6 1 "Binder Color"
//@param sx_color_a color 0.04 0.12 0.18 1 "Chip Color A"
//@param sx_color_b color 0.5 0.12 0.06 1 "Chip Color B"
//@param sx_color_c color 0.9 0.83 0.6 1 "Chip Color C"

// Deterministic convex polygon chips with three palette colors, density and size variation.
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

float4 Effect(EffectContext ctx)
{
    float s=max(sx_size,4.0); float2 p=(((float2)(ctx.px))+0.5)/s, cell=floor(p);
    float4 result=sx_background;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float2 id=cell+float2(x,y), key=id+sx_seed*7.3;
        if(Hash12(key+53.7)>=sx_density) continue;
        float2 center=id+0.5+(Hash22(key)-0.5)*0.35;
        float2 q=(p-center)*s;
        float radius=s*lerp(0.34,0.15+Hash12(key+9.8)*0.25,sx_variation);
        float rot=Hash12(key+19.1)*6.2831853, d=-1e10;
        for(int i=0;i<6;++i)
        {
            float a=rot+i*1.04719755;
            float side=radius*(0.65+Hash12(key+float(i)*3.9)*0.35);
            d=max(d,dot(q,float2(cos(a),sin(a)))-side);
        }
        float coverage=1.0-smoothstep(-0.7,0.7,d);
        float choose=Hash12(key+103.7);
        float4 chip=choose<0.333333?sx_color_a:(choose<0.666667?sx_color_b:sx_color_c);
        result=ExtOver(result,chip,coverage);
    }
    return result;
}

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
//@name Topographic Lines
//@category Procedural Patterns
//@generator
//@param sx_scale float 180 4 2048 "Terrain Scale (px)"
//@param sx_seed int 7 0 65535 "Seed"

//@param sx_levels int 18 2 128 "Contour Levels"
//@param sx_width float 1 0.2 8 "Line Width (px)"
//@param sx_major_every int 5 1 16 "Major Contour Interval"
//@param sx_major_width float 2 0.2 12 "Major Line Width (px)"
//@param sx_offset float 0 -1 1 "Contour Offset"
//@param sx_line color 0.02 0.14 0.12 1 "Line Color"
//@param sx_background color 0.72 0.86 0.72 1 "Background Color"

// Procedural terrain contours with pixel-sized lines and emphasized major contours.
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

float4 ExtContours(float h, float gradient)
{
    if (gradient < 0.000001) return sx_background; // Constant plateaus have no contour direction.
    float count=clamp(float(sx_levels),2.0,128.0), v=(h+sx_offset)*count;
    float nearest=floor(v+0.5), dist=abs(v-nearest)/max(gradient*count,0.00001);
    float major=abs(nearest-round(nearest/max(float(sx_major_every),1.0))*max(float(sx_major_every),1.0))<0.1?1.0:0.0;
    float stroke=lerp(sx_width,sx_major_width,major);
    float coverage=1.0-smoothstep(stroke*0.5,stroke*0.5+0.75,dist);
    return ExtOver(sx_background,sx_line,coverage);
}

float4 Effect(EffectContext ctx)
{
    float2 p=(((float2)(ctx.px))+0.5)/max(sx_scale,4.0)+sx_seed*13.1;
    float t=1.0/max(sx_scale,4.0), h=ExtTerrain(p);
    float dx=(ExtTerrain(p+float2(t,0))-ExtTerrain(p-float2(t,0)))*0.5;
    float dy=(ExtTerrain(p+float2(0,t))-ExtTerrain(p-float2(0,t)))*0.5;
    return ExtContours(h,length(float2(dx,dy)));
}

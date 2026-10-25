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
//@name Flow Lines
//@category Procedural Patterns
//@generator
//@param sx_spacing float 14 2 256 "Line Spacing (px)"
//@param sx_width float 1.5 0.2 16 "Line Width (px)"
//@param sx_scale float 160 4 2048 "Flow Scale (px)"
//@param sx_bend float 70 0 512 "Bend Amplitude (px)"
//@param sx_angle float 0 -180 180 "Direction (degrees)"
//@param sx_seed int 4 0 65535 "Seed"
//@param sx_line color 0.015 0.1 0.18 1 "Line Color"
//@param sx_background color 0.66 0.85 0.91 1 "Background Color"

// Curved contours of a smooth directional field with gradient-corrected stroke width.
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

float ExtFlow(float2 p)
{
    float a=radians(sx_angle); float2 q=float2(dot(p,float2(cos(a),sin(a))),dot(p,float2(-sin(a),cos(a))));
    return q.y+(ExtTerrain(q/max(sx_scale,4.0)+sx_seed*17.3)-0.5)*sx_bend;
}
float4 Effect(EffectContext ctx)
{
    float2 p=((float2)(ctx.px))+0.5; float s=max(sx_spacing,2.0), f=ExtFlow(p);
    float gradient=length(float2(ExtFlow(p+float2(0.5,0))-ExtFlow(p-float2(0.5,0)),
                                 ExtFlow(p+float2(0,0.5))-ExtFlow(p-float2(0,0.5))));
    float d=abs(frac(f/s+0.5)-0.5)*s/max(gradient,0.1);
    return ExtOver(sx_background,sx_line,1.0-smoothstep(sx_width*0.5,sx_width*0.5+0.75,d));
}

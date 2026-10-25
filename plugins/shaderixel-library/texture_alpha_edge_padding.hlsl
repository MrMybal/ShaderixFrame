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
//@name Alpha Edge Padding
//@category Texture Processing
//@input Image
//@halo radius 1 1
//@param sx_radius int 8 0 32 "Padding Radius (px)"
//@param sx_source_alpha float 0.1 0.001 1 "Minimum Source Alpha"
//@param sx_carrier_alpha float 0.0001 0.000002 0.001 "RGB Carrier Alpha"

// Extend nearest nontransparent RGB into empty alpha; tiny carrier alpha quantizes to zero in 8-bit PNG.
float4 Effect(EffectContext ctx)
{
    float4 c=Input(0,ctx.px);
    int r=clamp(sx_radius,0,32);
    if(c.a>0.000001 || r==0) return c;
    float best=1e20; float3 rgb=0.0; bool found=false;
    for(int y=-r;y<=r;++y) for(int x=-r;x<=r;++x)
    {
        float d=float(x*x+y*y);
        if(d>float(r*r)||d>=best) continue;
        int2 p=ctx.px+int2(x,y);
        if(any(p<0)||any(p>=((int2)(ctx.size)))) continue;
        float4 t=Input(0,p);
        if(t.a>=max(sx_source_alpha,0.001)) { best=d; rgb=t.rgb; found=true; }
    }
    // Keep carrier above Unpremultiply's 1e-6 threshold but below half one RGBA8 alpha step.
    return found?float4(rgb,clamp(sx_carrier_alpha,0.000002,0.001)):c;
}

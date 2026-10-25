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
//@name Isometric Tiles
//@category Procedural Patterns
//@generator
//@param sx_size float 32 4 256 "Tile Radius (px)"
//@param sx_mode int 1 0 1 "Tile Style"
//@param sx_border_width float 0.8 0 8 "Border Width (px)"
//@param sx_offset float2 0 0 -2048 2048 "Offset (px)"
//@param sx_top color 0.46 0.73 0.83 1 "Top Color"
//@param sx_left color 0.1 0.28 0.4 1 "Left Color"
//@param sx_right color 0.2 0.46 0.6 1 "Right Color"
//@param sx_border color 0.015 0.06 0.09 1 "Border Color"

// Isometric diamond or three-face cube tiling with independent palette, border and pixel offset.
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
    float r=max(sx_size,4.0); float2 p=((float2)(ctx.px))+0.5-sx_offset;
    if(sx_mode==0)
    {
        float2 q=float2(p.x/1.7320508+p.y,-p.x/1.7320508+p.y)/r;
        float2 f=frac(q); float parity=frac((floor(q.x)+floor(q.y))*0.5);
        float4 face=parity<0.25?sx_top:sx_right;
        float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y))*r*0.8660254;
        return ExtMix(face,sx_border,sx_border_width<=0.0?0.0:1.0-smoothstep(sx_border_width,sx_border_width+0.7,edge));
    }
    float row=floor(p.y/(1.5*r)), col=floor(p.x/(1.7320508*r)-row*0.5);
    float nearest=1e20; float2 chosen=0.0;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float rr=row+y; float2 c=float2((col+x+rr*0.5)*1.7320508*r,rr*1.5*r);
        float d=dot(p-c,p-c); if(d<nearest) {nearest=d; chosen=c;}
    }
    float2 local=p-chosen;
    float4 face=local.y<-abs(local.x)*0.57735027?sx_top:(local.x<0.0?sx_left:sx_right);
    float edge=1e20;
    for(int i=0;i<6;++i)
    {
        float a=radians(60.0*i); float2 n=float2(cos(a),sin(a));
        edge=min(edge,r*0.8660254-dot(local,n));
    }
    return ExtMix(face,sx_border,sx_border_width<=0.0?0.0:1.0-smoothstep(sx_border_width,sx_border_width+0.7,edge));
}

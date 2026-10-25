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
//@name Geometry
//@category Distortion
//@halo full
//@premultiplied
//@param sx_vertical float 0 -100 100 "Perspective verticale"
//@param sx_horizontal float 0 -100 100 "Perspective horizontale"
//@param sx_rotate float 0 -45 45 "Rotation (°)"
//@param sx_aspect float 0 -100 100 "Aspect"
//@param sx_scale float 100 50 150 "Scale (%)"
//@param sx_offset_x float 0 -100 100 "Offset X"
//@param sx_offset_y float 0 -100 100 "Offset Y"
//@param sx_autoscale bool 1 "Fill Frame"
//@param sx_grid bool 0 "Show Grid"
// Perspective / straightening of the Photo RAW workspace (Geometry panel): output pixel -> source
// position through scale / offset, aspect, rotation and a keystone (projective) term, in coordinates
// centered on the image and normalized by its half height. Vertical > 0 brings converging verticals
// (camera tilted up) back parallel.

float2 SourcePos(float2 q)
{
    // q: output position (centered, / half height) -> source position, same space.
    q.x *= exp2(-sx_aspect / 100.0 * 0.5);
    q.y *= exp2(sx_aspect / 100.0 * 0.5);
    float r = radians(sx_rotate);
    q = float2(q.x * cos(r) - q.y * sin(r), q.x * sin(r) + q.y * cos(r));
    float kv = sx_vertical / 100.0 * 0.45, kh = sx_horizontal / 100.0 * 0.45;
    float w = 1.0 - kv * q.y - kh * q.x;  // top (q.y < 0) magnified when sx_vertical > 0
    return q / max(w, 0.05);
}

float4 Effect(EffectContext ctx)
{
    const float2 hsz = ctx.size * 0.5;
    const float n = hsz.y;
    const float2 ext = hsz / n;  // half extent in normalized units
    float2 p = (((float2)(ctx.px)) + 0.5 - hsz) / n;
    // Output -> source; "fill" divides by the largest overshoot of the corners and edge middles.
    float k = max(sx_scale / 100.0, 0.05);
    if (sx_autoscale)
    {
        float need = 0.0;
        float2 pts[8] = {float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, 1), float2(0, -1), float2(0, 1), float2(-1, 0), float2(1, 0)};
        for (int i = 0; i < 8; ++i)
        {
            float2 sp = SourcePos(pts[i] * ext);
            need = max(need, max(abs(sp.x) / ext.x, abs(sp.y) / ext.y));
        }
        k *= max(need, 1e-3);
    }
    float2 q = p - float2(sx_offset_x, sx_offset_y) / 100.0 * ext;
    float2 src = SourcePos(q) / k;
    float2 uv = (src * n + hsz) * ctx.texel;
    float4 c = (any(uv < 0.0) || any(uv > 1.0)) ? float4(0, 0, 0, 0) : InputUV(0, uv);
    if (sx_grid)
    {
        float2 g = abs(frac((((float2)(ctx.px)) + 0.5) / (ctx.size / 8.0) + 0.5) - 0.5) * (ctx.size / 8.0);
        if (min(g.x, g.y) < 1.0) c = lerp(c, float4(1, 1, 1, 1), 0.5);
    }
    return c;
}

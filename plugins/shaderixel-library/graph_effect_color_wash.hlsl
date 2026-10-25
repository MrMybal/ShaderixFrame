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
//@name Graph Color Wash
//@category Graph Effects
//@graph {"name":"Graph Color Wash","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":19,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.15,0.4,0.8,1],"name":"tint","text":"Tint Color (RGB)","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[0.5,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[1,0,0,0],"name":"preserve","text":"Luminance Preservation","min":0,"max":1,"index":1},{"id":5,"op":"luma","pos":[280,40],"in":[[1,1]],"value":[0,0,0,0]},{"id":6,"op":"mask","pos":[280,170],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":7,"op":"luma","pos":[520,40],"in":[[6,0]],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,560],"in":[],"value":[0.0001,0,0,0]},{"id":9,"op":"max","pos":[760,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"mask","pos":[280,300],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":11,"op":"div","pos":[1000,40],"in":[[5,0],[9,0]],"value":[0,0,0,0]},{"id":12,"op":"mul","pos":[1240,40],"in":[[10,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"mask","pos":[280,430],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":14,"op":"lerp","pos":[1480,40],"in":[[13,0],[12,0],[4,0]],"value":[0,0,0,0]},{"id":15,"op":"lerp","pos":[1720,40],"in":[[1,1],[14,0],[3,0]],"value":[0,0,0,0]},{"id":16,"op":"const","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":17,"op":"const","pos":[40,820],"in":[],"value":[1,0,0,0]},{"id":18,"op":"output","pos":[1960,40],"in":[[1,0],[15,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param sx_tint color 0.15 0.4 0.8 1.0 "Tint Color (RGB)"
//@param sx_amount float 0.5 0.0 1.0 "Strength"
//@param sx_preserve float 1.0 0.0 1.0 "Luminance Preservation"

// Linear color wash with optional luminance preservation and original alpha.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float g5_0 = Luma(g1_1);
    float3 g6_0 = (sx_tint).xyz;
    float g7_0 = Luma(g6_0);
    float g9_0 = max(g7_0,0.0001);
    float3 g10_0 = (sx_tint).xyz;
    float g11_0 = (g5_0/g9_0);
    float3 g12_0 = (g10_0*(float3)(g11_0));
    float3 g13_0 = (sx_tint).xyz;
    float3 g14_0 = lerp(g13_0,g12_0,sx_preserve);
    float3 g15_0 = lerp(g1_1,g14_0,sx_amount);
    return float4((float4(g15_0,(g1_0).a)).rgb,g1_5);
}

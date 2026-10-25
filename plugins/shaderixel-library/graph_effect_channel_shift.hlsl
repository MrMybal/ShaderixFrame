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
//@name Graph RGB Channel Shift
//@category Graph Effects
//@graph {"name":"Graph RGB Channel Shift","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"param2","pos":[40,170],"in":[],"value":[2,0,0,0],"name":"offset","text":"Channel Offset (px)","min":-64,"max":64,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[1,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":4,"op":"uv","pos":[40,430],"in":[],"value":[0,0,0,0]},{"id":5,"op":"size","pos":[40,560],"in":[],"value":[0,0,0,0]},{"id":6,"op":"div","pos":[280,40],"in":[[2,0],[5,0]],"value":[0,0,0,0]},{"id":7,"op":"add","pos":[520,40],"in":[[4,0],[6,0]],"value":[0,0,0,0]},{"id":8,"op":"sample","pos":[760,40],"in":[[7,0]],"value":[0,0,0,0]},{"id":9,"op":"sub","pos":[520,170],"in":[[4,0],[6,0]],"value":[0,0,0,0]},{"id":10,"op":"sample","pos":[760,170],"in":[[9,0]],"value":[0,0,0,0]},{"id":11,"op":"make3","pos":[1000,40],"in":[[8,2],[1,3],[10,4]],"value":[0,0,0,0]},{"id":12,"op":"lerp","pos":[1240,40],"in":[[1,1],[11,0],[3,0]],"value":[0,0,0,0]},{"id":13,"op":"const","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":14,"op":"const","pos":[40,820],"in":[],"value":[1,0,0,0]},{"id":15,"op":"output","pos":[1480,40],"in":[[1,0],[12,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param sx_offset float2 2.0 0.0 -64.0 64.0 "Channel Offset (px)"
//@param sx_amount float 1.0 0.0 1.0 "Strength"

// Opposing red/blue sampling offsets with central green and alpha; boundaries clamp.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float2 g5_0 = ctx.size;
    float2 g6_0 = (sx_offset/g5_0);
    float2 g7_0 = (ctx.uv+g6_0);
    float4 g8_0 = InputUV(0,g7_0);
    float3 g8_1 = (InputUV(0,g7_0)).rgb;
    float g8_2 = (InputUV(0,g7_0)).r;
    float g8_3 = (InputUV(0,g7_0)).g;
    float g8_4 = (InputUV(0,g7_0)).b;
    float g8_5 = (InputUV(0,g7_0)).a;
    float2 g9_0 = (ctx.uv-g6_0);
    float4 g10_0 = InputUV(0,g9_0);
    float3 g10_1 = (InputUV(0,g9_0)).rgb;
    float g10_2 = (InputUV(0,g9_0)).r;
    float g10_3 = (InputUV(0,g9_0)).g;
    float g10_4 = (InputUV(0,g9_0)).b;
    float g10_5 = (InputUV(0,g9_0)).a;
    float3 g11_0 = float3(g8_2,g1_3,g10_4);
    float3 g12_0 = lerp(g1_1,g11_0,sx_amount);
    return float4((float4(g12_0,(g1_0).a)).rgb,g1_5);
}

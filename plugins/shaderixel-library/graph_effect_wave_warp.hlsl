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
//@name Graph Wave Warp
//@category Graph Effects
//@graph {"name":"Graph Wave Warp","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":37,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"param","pos":[40,170],"in":[],"value":[4,0,0,0],"name":"amplitude","text":"Wave Amplitude (px)","min":0,"max":128,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[64,0,0,0],"name":"period","text":"Wave Period (px)","min":2,"max":1024,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[0,0,0,0],"name":"phase","text":"Wave Phase","min":0,"max":1,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[0,0,0,0],"name":"angle","text":"Displacement Angle (degrees)","min":-180,"max":180,"index":1},{"id":6,"op":"const","pos":[40,690],"in":[],"value":[0.017453292519943295,0,0,0]},{"id":7,"op":"mul","pos":[280,40],"in":[[5,0],[6,0]],"value":[0,0,0,0]},{"id":8,"op":"cos","pos":[520,40],"in":[[7,0]],"value":[0,0,0,0]},{"id":9,"op":"sin","pos":[520,170],"in":[[7,0]],"value":[0,0,0,0]},{"id":10,"op":"make2","pos":[760,40],"in":[[8,0],[9,0]],"value":[0,0,0,0]},{"id":11,"op":"const","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":12,"op":"mask","pos":[1000,40],"in":[[10,0]],"value":[0,0,0,0],"index":2},{"id":13,"op":"sub","pos":[1240,40],"in":[[11,0],[12,0]],"value":[0,0,0,0]},{"id":14,"op":"mask","pos":[1000,170],"in":[[10,0]],"value":[0,0,0,0],"index":1},{"id":15,"op":"make2","pos":[1480,40],"in":[[13,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"pixel","pos":[40,950],"in":[],"value":[0,0,0,0]},{"id":17,"op":"const","pos":[40,1080],"in":[],"value":[0.5,0,0,0]},{"id":18,"op":"add","pos":[280,170],"in":[[16,0],[17,0]],"value":[0,0,0,0]},{"id":19,"op":"dot","pos":[1720,40],"in":[[18,0],[15,0]],"value":[0,0,0,0]},{"id":20,"op":"const","pos":[40,1210],"in":[],"value":[0.0001,0,0,0]},{"id":21,"op":"max","pos":[280,300],"in":[[3,0],[20,0]],"value":[0,0,0,0]},{"id":22,"op":"div","pos":[1960,40],"in":[[19,0],[21,0]],"value":[0,0,0,0]},{"id":23,"op":"add","pos":[2200,40],"in":[[22,0],[4,0]],"value":[0,0,0,0]},{"id":24,"op":"const","pos":[40,1340],"in":[],"value":[6.283185307179586,0,0,0]},{"id":25,"op":"mul","pos":[2440,40],"in":[[23,0],[24,0]],"value":[0,0,0,0]},{"id":26,"op":"sin","pos":[2680,40],"in":[[25,0]],"value":[0,0,0,0]},{"id":27,"op":"mul","pos":[2920,40],"in":[[26,0],[2,0]],"value":[0,0,0,0]},{"id":28,"op":"mul","pos":[3160,40],"in":[[10,0],[27,0]],"value":[0,0,0,0]},{"id":29,"op":"size","pos":[40,1470],"in":[],"value":[0,0,0,0]},{"id":30,"op":"div","pos":[3400,40],"in":[[28,0],[29,0]],"value":[0,0,0,0]},{"id":31,"op":"uv","pos":[40,1600],"in":[],"value":[0,0,0,0]},{"id":32,"op":"add","pos":[3640,40],"in":[[31,0],[30,0]],"value":[0,0,0,0]},{"id":33,"op":"sample","pos":[3880,40],"in":[[32,0]],"value":[0,0,0,0]},{"id":34,"op":"const","pos":[40,1730],"in":[],"value":[0,0,0,0]},{"id":35,"op":"const","pos":[40,1860],"in":[],"value":[1,0,0,0]},{"id":36,"op":"output","pos":[4120,40],"in":[[33,0],[0,0],[33,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param sx_amplitude float 4.0 0.0 128.0 "Wave Amplitude (px)"
//@param sx_period float 64.0 2.0 1024.0 "Wave Period (px)"
//@param sx_phase float 0.0 0.0 1.0 "Wave Phase"
//@param sx_angle float 0.0 -180.0 180.0 "Displacement Angle (degrees)"

// Pixel-scale sinusoidal image displacement with direction and phase; boundaries clamp.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float g7_0 = (sx_angle*0.017453293);
    float g8_0 = cos(g7_0);
    float g9_0 = sin(g7_0);
    float2 g10_0 = float2(g8_0,g9_0);
    float g12_0 = (float4(g10_0,0.0,1.0)).y;
    float g13_0 = (0.0-g12_0);
    float g14_0 = (float4(g10_0,0.0,1.0)).x;
    float2 g15_0 = float2(g13_0,g14_0);
    float2 g16_0 = ((float2)(ctx.px));
    float2 g18_0 = (g16_0+(float2)(0.5));
    float g19_0 = dot(g18_0,g15_0);
    float g21_0 = max(sx_period,0.0001);
    float g22_0 = (g19_0/g21_0);
    float g23_0 = (g22_0+sx_phase);
    float g25_0 = (g23_0*6.2831853);
    float g26_0 = sin(g25_0);
    float g27_0 = (g26_0*sx_amplitude);
    float2 g28_0 = (g10_0*(float2)(g27_0));
    float2 g29_0 = ctx.size;
    float2 g30_0 = (g28_0/g29_0);
    float2 g32_0 = (ctx.uv+g30_0);
    float4 g33_0 = InputUV(0,g32_0);
    float3 g33_1 = (InputUV(0,g32_0)).rgb;
    float g33_2 = (InputUV(0,g32_0)).r;
    float g33_3 = (InputUV(0,g32_0)).g;
    float g33_4 = (InputUV(0,g32_0)).b;
    float g33_5 = (InputUV(0,g32_0)).a;
    return float4((g33_0).rgb,g33_5);
}

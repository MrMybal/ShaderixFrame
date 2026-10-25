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
//@name Graph Directional Relief
//@category Graph Effects
//@graph {"name":"Graph Directional Relief","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":28,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"param","pos":[40,170],"in":[],"value":[1,0,0,0],"name":"radius","text":"Sample Distance (px)","min":0,"max":16,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[135,0,0,0],"name":"angle","text":"Light Angle (degrees)","min":-180,"max":180,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[2,0,0,0],"name":"depth","text":"Relief Strength","min":0,"max":8,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[0.6,0,0,0],"name":"amount","text":"Mix Strength","min":0,"max":1,"index":1},{"id":6,"op":"const","pos":[40,690],"in":[],"value":[0.017453292519943295,0,0,0]},{"id":7,"op":"mul","pos":[280,40],"in":[[3,0],[6,0]],"value":[0,0,0,0]},{"id":8,"op":"cos","pos":[520,40],"in":[[7,0]],"value":[0,0,0,0]},{"id":9,"op":"sin","pos":[520,170],"in":[[7,0]],"value":[0,0,0,0]},{"id":10,"op":"make2","pos":[760,40],"in":[[8,0],[9,0]],"value":[0,0,0,0]},{"id":11,"op":"mul","pos":[1000,40],"in":[[10,0],[2,0]],"value":[0,0,0,0]},{"id":12,"op":"size","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":13,"op":"div","pos":[1240,40],"in":[[11,0],[12,0]],"value":[0,0,0,0]},{"id":14,"op":"uv","pos":[40,950],"in":[],"value":[0,0,0,0]},{"id":15,"op":"add","pos":[1480,40],"in":[[14,0],[13,0]],"value":[0,0,0,0]},{"id":16,"op":"sample","pos":[1720,40],"in":[[15,0]],"value":[0,0,0,0]},{"id":17,"op":"sub","pos":[1480,170],"in":[[14,0],[13,0]],"value":[0,0,0,0]},{"id":18,"op":"sample","pos":[1720,170],"in":[[17,0]],"value":[0,0,0,0]},{"id":19,"op":"luma","pos":[1960,40],"in":[[16,1]],"value":[0,0,0,0]},{"id":20,"op":"luma","pos":[1960,170],"in":[[18,1]],"value":[0,0,0,0]},{"id":21,"op":"sub","pos":[2200,40],"in":[[19,0],[20,0]],"value":[0,0,0,0]},{"id":22,"op":"mul","pos":[2440,40],"in":[[21,0],[4,0]],"value":[0,0,0,0]},{"id":23,"op":"add","pos":[2680,40],"in":[[1,1],[22,0]],"value":[0,0,0,0]},{"id":24,"op":"lerp","pos":[2920,40],"in":[[1,1],[23,0],[5,0]],"value":[0,0,0,0]},{"id":25,"op":"const","pos":[40,1080],"in":[],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1210],"in":[],"value":[1,0,0,0]},{"id":27,"op":"output","pos":[3160,40],"in":[[1,0],[24,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param sx_radius float 1.0 0.0 16.0 "Sample Distance (px)"
//@param sx_angle float 135.0 -180.0 180.0 "Light Angle (degrees)"
//@param sx_depth float 2.0 0.0 8.0 "Relief Strength"
//@param sx_amount float 0.6 0.0 1.0 "Mix Strength"

// Directional luminance relief from offset samples, preserving source alpha and float-range detail.
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
    float2 g11_0 = (g10_0*(float2)(sx_radius));
    float2 g12_0 = ctx.size;
    float2 g13_0 = (g11_0/g12_0);
    float2 g15_0 = (ctx.uv+g13_0);
    float4 g16_0 = InputUV(0,g15_0);
    float3 g16_1 = (InputUV(0,g15_0)).rgb;
    float g16_2 = (InputUV(0,g15_0)).r;
    float g16_3 = (InputUV(0,g15_0)).g;
    float g16_4 = (InputUV(0,g15_0)).b;
    float g16_5 = (InputUV(0,g15_0)).a;
    float2 g17_0 = (ctx.uv-g13_0);
    float4 g18_0 = InputUV(0,g17_0);
    float3 g18_1 = (InputUV(0,g17_0)).rgb;
    float g18_2 = (InputUV(0,g17_0)).r;
    float g18_3 = (InputUV(0,g17_0)).g;
    float g18_4 = (InputUV(0,g17_0)).b;
    float g18_5 = (InputUV(0,g17_0)).a;
    float g19_0 = Luma(g16_1);
    float g20_0 = Luma(g18_1);
    float g21_0 = (g19_0-g20_0);
    float g22_0 = (g21_0*sx_depth);
    float3 g23_0 = (g1_1+(float3)(g22_0));
    float3 g24_0 = lerp(g1_1,g23_0,sx_amount);
    return float4((float4(g24_0,(g1_0).a)).rgb,g1_5);
}

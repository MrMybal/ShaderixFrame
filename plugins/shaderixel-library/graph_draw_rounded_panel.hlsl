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
//@name Graph Rounded Panel
//@category Graph Layers
//@graph {"name":"Graph Rounded Panel","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":38,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.04,0.1,0.24,1],"name":"color","text":"Panel Color","min":0,"max":1,"index":1},{"id":2,"op":"param2","pos":[40,170],"in":[],"value":[0.5,0.5,0,0],"name":"center","text":"Center (UV)","min":0,"max":1,"index":1},{"id":3,"op":"param2","pos":[40,300],"in":[],"value":[240,120,0,0],"name":"half_size","text":"Half Size (px)","min":1,"max":4096,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[24,0,0,0],"name":"radius","text":"Corner Radius (px)","min":0,"max":1024,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"softness","text":"Edge Softness (px)","min":0.1,"max":8,"index":1},{"id":6,"op":"mask","pos":[280,40],"in":[[3,0]],"value":[0,0,0,0],"index":1},{"id":7,"op":"mask","pos":[280,170],"in":[[3,0]],"value":[0,0,0,0],"index":2},{"id":8,"op":"min","pos":[520,40],"in":[[6,0],[7,0]],"value":[0,0,0,0]},{"id":9,"op":"min","pos":[760,40],"in":[[4,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"pixel","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":11,"op":"const","pos":[40,820],"in":[],"value":[0.5,0,0,0]},{"id":12,"op":"add","pos":[280,300],"in":[[10,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"size","pos":[40,950],"in":[],"value":[0,0,0,0]},{"id":14,"op":"mul","pos":[280,430],"in":[[13,0],[2,0]],"value":[0,0,0,0]},{"id":15,"op":"sub","pos":[520,170],"in":[[12,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"abs","pos":[760,170],"in":[[15,0]],"value":[0,0,0,0]},{"id":17,"op":"sub","pos":[1000,40],"in":[[16,0],[3,0]],"value":[0,0,0,0]},{"id":18,"op":"add","pos":[1240,40],"in":[[17,0],[9,0]],"value":[0,0,0,0]},{"id":19,"op":"const","pos":[40,1080],"in":[],"value":[0,0,0,0]},{"id":20,"op":"max","pos":[1480,40],"in":[[18,0],[19,0]],"value":[0,0,0,0]},{"id":21,"op":"length","pos":[1720,40],"in":[[20,0]],"value":[0,0,0,0]},{"id":22,"op":"mask","pos":[1480,170],"in":[[18,0]],"value":[0,0,0,0],"index":1},{"id":23,"op":"mask","pos":[1480,300],"in":[[18,0]],"value":[0,0,0,0],"index":2},{"id":24,"op":"max","pos":[1720,170],"in":[[22,0],[23,0]],"value":[0,0,0,0]},{"id":25,"op":"const","pos":[40,1210],"in":[],"value":[0,0,0,0]},{"id":26,"op":"min","pos":[1960,40],"in":[[24,0],[25,0]],"value":[0,0,0,0]},{"id":27,"op":"add","pos":[2200,40],"in":[[21,0],[26,0]],"value":[0,0,0,0]},{"id":28,"op":"sub","pos":[2440,40],"in":[[27,0],[9,0]],"value":[0,0,0,0]},{"id":29,"op":"const","pos":[40,1340],"in":[],"value":[0,0,0,0]},{"id":30,"op":"sub","pos":[280,560],"in":[[29,0],[5,0]],"value":[0,0,0,0]},{"id":31,"op":"smoothstep","pos":[2680,40],"in":[[30,0],[5,0],[28,0]],"value":[0,0,0,0]},{"id":32,"op":"oneminus","pos":[2920,40],"in":[[31,0]],"value":[0,0,0,0]},{"id":33,"op":"mask","pos":[280,690],"in":[[1,0]],"value":[0,0,0,0],"index":8},{"id":34,"op":"mul","pos":[3160,40],"in":[[32,0],[33,0]],"value":[0,0,0,0]},{"id":35,"op":"const","pos":[40,1470],"in":[],"value":[0,0,0,0]},{"id":36,"op":"const","pos":[40,1600],"in":[],"value":[1,0,0,0]},{"id":37,"op":"output","pos":[3400,40],"in":[[1,0],[0,0],[34,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_color color 0.04 0.1 0.24 1.0 "Panel Color"
//@param sx_center float2 0.5 0.5 0.0 1.0 "Center (UV)"
//@param sx_half_size float2 240.0 120.0 1.0 4096.0 "Half Size (px)"
//@param sx_radius float 24.0 0.0 1024.0 "Corner Radius (px)"
//@param sx_softness float 1.0 0.1 8.0 "Edge Softness (px)"

// Native-node rounded rectangle signed distance with pixel dimensions and transparent edges.
float4 Effect(EffectContext ctx)
{
    float g6_0 = (float4(sx_half_size,0.0,1.0)).x;
    float g7_0 = (float4(sx_half_size,0.0,1.0)).y;
    float g8_0 = min(g6_0,g7_0);
    float g9_0 = min(sx_radius,g8_0);
    float2 g10_0 = ((float2)(ctx.px));
    float2 g12_0 = (g10_0+(float2)(0.5));
    float2 g13_0 = ctx.size;
    float2 g14_0 = (g13_0*sx_center);
    float2 g15_0 = (g12_0-g14_0);
    float2 g16_0 = abs(g15_0);
    float2 g17_0 = (g16_0-sx_half_size);
    float2 g18_0 = (g17_0+(float2)(g9_0));
    float2 g20_0 = max(g18_0,(float2)(0.0));
    float g21_0 = length(g20_0);
    float g22_0 = (float4(g18_0,0.0,1.0)).x;
    float g23_0 = (float4(g18_0,0.0,1.0)).y;
    float g24_0 = max(g22_0,g23_0);
    float g26_0 = min(g24_0,0.0);
    float g27_0 = (g21_0+g26_0);
    float g28_0 = (g27_0-g9_0);
    float g30_0 = (0.0-sx_softness);
    float g31_0 = smoothstep(g30_0,sx_softness,g28_0);
    float g32_0 = (1.0-g31_0);
    float g33_0 = (sx_color).w;
    float g34_0 = (g32_0*g33_0);
    return float4((sx_color).rgb,g34_0);
}

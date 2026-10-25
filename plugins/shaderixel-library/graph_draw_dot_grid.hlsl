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
//@name Graph Dot Grid
//@category Graph Layers
//@graph {"name":"Graph Dot Grid","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":36,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.8,0.15,0.025,1],"name":"dot_color","text":"Dot Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0,0,0,0],"name":"background","text":"Background Color","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[24,0,0,0],"name":"spacing","text":"Cell Spacing (px)","min":2,"max":256,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[4,0,0,0],"name":"radius","text":"Dot Radius (px)","min":0,"max":64,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"softness","text":"Edge Softness (px)","min":0.1,"max":8,"index":1},{"id":6,"op":"param2","pos":[40,690],"in":[],"value":[0,0,0,0],"name":"offset","text":"Offset (px)","min":-256,"max":256,"index":1},{"id":7,"op":"pixel","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,950],"in":[],"value":[0.5,0,0,0]},{"id":9,"op":"add","pos":[280,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"sub","pos":[520,40],"in":[[9,0],[6,0]],"value":[0,0,0,0]},{"id":11,"op":"const","pos":[40,1080],"in":[],"value":[0.0001,0,0,0]},{"id":12,"op":"max","pos":[280,170],"in":[[3,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"div","pos":[760,40],"in":[[10,0],[12,0]],"value":[0,0,0,0]},{"id":14,"op":"frac","pos":[1000,40],"in":[[13,0]],"value":[0,0,0,0]},{"id":15,"op":"const","pos":[40,1210],"in":[],"value":[0.5,0,0,0]},{"id":16,"op":"sub","pos":[1240,40],"in":[[14,0],[15,0]],"value":[0,0,0,0]},{"id":17,"op":"mul","pos":[1480,40],"in":[[16,0],[3,0]],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1340],"in":[],"value":[0.5,0,0,0]},{"id":19,"op":"mul","pos":[280,300],"in":[[3,0],[18,0]],"value":[0,0,0,0]},{"id":20,"op":"min","pos":[520,170],"in":[[4,0],[19,0]],"value":[0,0,0,0]},{"id":21,"op":"sub","pos":[760,170],"in":[[20,0],[5,0]],"value":[0,0,0,0]},{"id":22,"op":"add","pos":[760,300],"in":[[20,0],[5,0]],"value":[0,0,0,0]},{"id":23,"op":"length","pos":[1720,40],"in":[[17,0]],"value":[0,0,0,0]},{"id":24,"op":"smoothstep","pos":[1960,40],"in":[[21,0],[22,0],[23,0]],"value":[0,0,0,0]},{"id":25,"op":"oneminus","pos":[2200,40],"in":[[24,0]],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1470],"in":[],"value":[0,0,0,0]},{"id":27,"op":"const","pos":[40,1600],"in":[],"value":[1,0,0,0]},{"id":28,"op":"clamp","pos":[280,430],"in":[[4,0],[26,0],[27,0]],"value":[0,0,0,0]},{"id":29,"op":"mul","pos":[2440,40],"in":[[25,0],[28,0]],"value":[0,0,0,0]},{"id":30,"op":"saturate","pos":[2680,40],"in":[[29,0]],"value":[0,0,0,0]},{"id":31,"op":"lerp","pos":[2920,40],"in":[[2,0],[1,0],[30,0]],"value":[0,0,0,0]},{"id":32,"op":"mask","pos":[3160,40],"in":[[31,0]],"value":[0,0,0,0],"index":8},{"id":33,"op":"const","pos":[40,1730],"in":[],"value":[0,0,0,0]},{"id":34,"op":"const","pos":[40,1860],"in":[],"value":[1,0,0,0]},{"id":35,"op":"output","pos":[3400,40],"in":[[31,0],[0,0],[32,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_dot_color color 0.8 0.15 0.025 1.0 "Dot Color"
//@param sx_background color 0.0 0.0 0.0 0.0 "Background Color"
//@param sx_spacing float 24.0 2.0 256.0 "Cell Spacing (px)"
//@param sx_radius float 4.0 0.0 64.0 "Dot Radius (px)"
//@param sx_softness float 1.0 0.1 8.0 "Edge Softness (px)"
//@param sx_offset float2 0.0 0.0 -256.0 256.0 "Offset (px)"

// Repeating antialiased dots in document pixels with phase and transparent background.
float4 Effect(EffectContext ctx)
{
    float2 g7_0 = ((float2)(ctx.px));
    float2 g9_0 = (g7_0+(float2)(0.5));
    float2 g10_0 = (g9_0-sx_offset);
    float g12_0 = max(sx_spacing,0.0001);
    float2 g13_0 = (g10_0/(float2)(g12_0));
    float2 g14_0 = frac(g13_0);
    float2 g16_0 = (g14_0-(float2)(0.5));
    float2 g17_0 = (g16_0*(float2)(sx_spacing));
    float g19_0 = (sx_spacing*0.5);
    float g20_0 = min(sx_radius,g19_0);
    float g21_0 = (g20_0-sx_softness);
    float g22_0 = (g20_0+sx_softness);
    float g23_0 = length(g17_0);
    float g24_0 = smoothstep(g21_0,g22_0,g23_0);
    float g25_0 = (1.0-g24_0);
    float g28_0 = clamp(sx_radius,0.0,1.0);
    float g29_0 = (g25_0*g28_0);
    float g30_0 = saturate(g29_0);
    float4 g31_0 = lerp(sx_background,sx_dot_color,g30_0);
    float g32_0 = (g31_0).w;
    return float4((g31_0).rgb,g32_0);
}

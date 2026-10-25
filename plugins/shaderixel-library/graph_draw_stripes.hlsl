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
//@name Graph Diagonal Stripes
//@category Graph Layers
//@graph {"name":"Graph Diagonal Stripes","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":55,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.015,0.025,0.08,1],"name":"color_a","text":"Stripe Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.12,0.3,0.6,1],"name":"color_b","text":"Background Color","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[32,0,0,0],"name":"spacing","text":"Stripe Period (px)","min":2,"max":256,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[0.5,0,0,0],"name":"duty","text":"Stripe Coverage","min":0,"max":1,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[45,0,0,0],"name":"angle","text":"Angle (degrees)","min":-180,"max":180,"index":1},{"id":6,"op":"param","pos":[40,690],"in":[],"value":[0,0,0,0],"name":"phase","text":"Phase","min":0,"max":1,"index":1},{"id":7,"op":"pixel","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,950],"in":[],"value":[0.5,0,0,0]},{"id":9,"op":"add","pos":[280,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"const","pos":[40,1080],"in":[],"value":[0.017453292519943295,0,0,0]},{"id":11,"op":"mul","pos":[280,170],"in":[[5,0],[10,0]],"value":[0,0,0,0]},{"id":12,"op":"cos","pos":[520,40],"in":[[11,0]],"value":[0,0,0,0]},{"id":13,"op":"sin","pos":[520,170],"in":[[11,0]],"value":[0,0,0,0]},{"id":14,"op":"make2","pos":[760,40],"in":[[12,0],[13,0]],"value":[0,0,0,0]},{"id":15,"op":"dot","pos":[1000,40],"in":[[9,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"const","pos":[40,1210],"in":[],"value":[0.0001,0,0,0]},{"id":17,"op":"max","pos":[280,300],"in":[[3,0],[16,0]],"value":[0,0,0,0]},{"id":18,"op":"div","pos":[1240,40],"in":[[15,0],[17,0]],"value":[0,0,0,0]},{"id":19,"op":"add","pos":[1480,40],"in":[[18,0],[6,0]],"value":[0,0,0,0]},{"id":20,"op":"frac","pos":[1720,40],"in":[[19,0]],"value":[0,0,0,0]},{"id":21,"op":"const","pos":[40,1340],"in":[],"value":[0.5,0,0,0]},{"id":22,"op":"sub","pos":[1960,40],"in":[[20,0],[21,0]],"value":[0,0,0,0]},{"id":23,"op":"abs","pos":[2200,40],"in":[[22,0]],"value":[0,0,0,0]},{"id":24,"op":"const","pos":[40,1470],"in":[],"value":[0.5,0,0,0]},{"id":25,"op":"mul","pos":[280,430],"in":[[4,0],[24,0]],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1600],"in":[],"value":[0.65,0,0,0]},{"id":27,"op":"const","pos":[40,1730],"in":[],"value":[0.0001,0,0,0]},{"id":28,"op":"max","pos":[280,560],"in":[[3,0],[27,0]],"value":[0,0,0,0]},{"id":29,"op":"div","pos":[520,300],"in":[[26,0],[28,0]],"value":[0,0,0,0]},{"id":30,"op":"sub","pos":[760,170],"in":[[25,0],[29,0]],"value":[0,0,0,0]},{"id":31,"op":"add","pos":[760,300],"in":[[25,0],[29,0]],"value":[0,0,0,0]},{"id":32,"op":"smoothstep","pos":[2440,40],"in":[[30,0],[31,0],[23,0]],"value":[0,0,0,0]},{"id":33,"op":"oneminus","pos":[2680,40],"in":[[32,0]],"value":[0,0,0,0]},{"id":34,"op":"const","pos":[40,1860],"in":[],"value":[100000,0,0,0]},{"id":35,"op":"mul","pos":[280,690],"in":[[4,0],[34,0]],"value":[0,0,0,0]},{"id":36,"op":"const","pos":[40,1990],"in":[],"value":[0,0,0,0]},{"id":37,"op":"const","pos":[40,2120],"in":[],"value":[1,0,0,0]},{"id":38,"op":"clamp","pos":[520,430],"in":[[35,0],[36,0],[37,0]],"value":[0,0,0,0]},{"id":39,"op":"mul","pos":[2920,40],"in":[[33,0],[38,0]],"value":[0,0,0,0]},{"id":40,"op":"const","pos":[40,2250],"in":[],"value":[1,0,0,0]},{"id":41,"op":"const","pos":[40,2380],"in":[],"value":[0.99999,0,0,0]},{"id":42,"op":"sub","pos":[280,820],"in":[[4,0],[41,0]],"value":[0,0,0,0]},{"id":43,"op":"const","pos":[40,2510],"in":[],"value":[100000,0,0,0]},{"id":44,"op":"mul","pos":[520,560],"in":[[42,0],[43,0]],"value":[0,0,0,0]},{"id":45,"op":"const","pos":[40,2640],"in":[],"value":[0,0,0,0]},{"id":46,"op":"const","pos":[40,2770],"in":[],"value":[1,0,0,0]},{"id":47,"op":"clamp","pos":[760,430],"in":[[44,0],[45,0],[46,0]],"value":[0,0,0,0]},{"id":48,"op":"lerp","pos":[3160,40],"in":[[39,0],[40,0],[47,0]],"value":[0,0,0,0]},{"id":49,"op":"saturate","pos":[3400,40],"in":[[48,0]],"value":[0,0,0,0]},{"id":50,"op":"lerp","pos":[3640,40],"in":[[2,0],[1,0],[49,0]],"value":[0,0,0,0]},{"id":51,"op":"mask","pos":[3880,40],"in":[[50,0]],"value":[0,0,0,0],"index":8},{"id":52,"op":"const","pos":[40,2900],"in":[],"value":[0,0,0,0]},{"id":53,"op":"const","pos":[40,3030],"in":[],"value":[1,0,0,0]},{"id":54,"op":"output","pos":[4120,40],"in":[[50,0],[0,0],[51,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_color_a color 0.015 0.025 0.08 1.0 "Stripe Color"
//@param sx_color_b color 0.12 0.3 0.6 1.0 "Background Color"
//@param sx_spacing float 32.0 2.0 256.0 "Stripe Period (px)"
//@param sx_duty float 0.5 0.0 1.0 "Stripe Coverage"
//@param sx_angle float 45.0 -180.0 180.0 "Angle (degrees)"
//@param sx_phase float 0.0 0.0 1.0 "Phase"

// Rotatable pixel-scale stripes with coverage and phase, assembled from native nodes.
float4 Effect(EffectContext ctx)
{
    float2 g7_0 = ((float2)(ctx.px));
    float2 g9_0 = (g7_0+(float2)(0.5));
    float g11_0 = (sx_angle*0.017453293);
    float g12_0 = cos(g11_0);
    float g13_0 = sin(g11_0);
    float2 g14_0 = float2(g12_0,g13_0);
    float g15_0 = dot(g9_0,g14_0);
    float g17_0 = max(sx_spacing,0.0001);
    float g18_0 = (g15_0/g17_0);
    float g19_0 = (g18_0+sx_phase);
    float g20_0 = frac(g19_0);
    float g22_0 = (g20_0-0.5);
    float g23_0 = abs(g22_0);
    float g25_0 = (sx_duty*0.5);
    float g28_0 = max(sx_spacing,0.0001);
    float g29_0 = (0.65/g28_0);
    float g30_0 = (g25_0-g29_0);
    float g31_0 = (g25_0+g29_0);
    float g32_0 = smoothstep(g30_0,g31_0,g23_0);
    float g33_0 = (1.0-g32_0);
    float g35_0 = (sx_duty*100000.0);
    float g38_0 = clamp(g35_0,0.0,1.0);
    float g39_0 = (g33_0*g38_0);
    float g42_0 = (sx_duty-0.99999);
    float g44_0 = (g42_0*100000.0);
    float g47_0 = clamp(g44_0,0.0,1.0);
    float g48_0 = lerp(g39_0,1.0,g47_0);
    float g49_0 = saturate(g48_0);
    float4 g50_0 = lerp(sx_color_b,sx_color_a,g49_0);
    float g51_0 = (g50_0).w;
    return float4((g50_0).rgb,g51_0);
}

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
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}
//@name Truchet Tiles
//@category Dessin
//@graph {"name":"Truchet Tiles","category":"Dessin","inputs":0,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[10.0,0.0,0.0,0.0],"name":"Tiling","text":"Tiling","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0350000001,0.0,0.0,0.0],"name":"Thickness","text":"Thickness","index":1,"min":0.00100000005,"max":0.25},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.00999999978,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.100000001},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.180000007,0.0,0.0,0.0],"name":"GlowRadius","text":"Glow Radius","index":1,"min":0.00100000005,"max":0.699999988},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"GlowFalloff","text":"Glow Falloff","index":1,"min":0.100000001,"max":16.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"GlowIntensity","text":"Glow Intensity","index":1,"min":0.0,"max":16.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"LineOpacity","text":"Line Opacity","index":1},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BackgroundOpacity","text":"Background Opacity","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.150000006,0.800000012,1.79999995,1.0],"name":"LineColor","text":"Line Color","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundColor","text":"Background Color","index":1},{"id":14,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0]],"value":[0,0,0,0],"name":"Truchet Tiles","text":"    float2 q=UV*max(Tiling,1.0); float2 id=floor(q); float2 p=frac(q); float rnd=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); if(rnd>0.5)p.x=1.0-p.x; float d=min(abs(length(p)-0.5),abs(length(p-1.0)-0.5)); float lineMask=1.0-smoothstep(Thickness,Thickness+Softness,d); float glow=pow(saturate(1.0-d/max(GlowRadius,0.0001)),max(GlowFalloff,0.1)); float3 rgb=BackgroundColor.rgb+LineColor.rgb*(lineMask+glow*GlowIntensity); return float4(rgb,lerp(BackgroundOpacity,LineOpacity,max(lineMask,glow)));","pins":[["UV",2],["CyCanvasSize",4],["Tiling",1],["Seed",1],["Thickness",1],["Softness",1],["GlowRadius",1],["GlowFalloff",1],["GlowIntensity",1],["LineOpacity",1],["BackgroundOpacity",1],["LineColor",3],["BackgroundColor",3]],"outs":[["Resultat",4]]},{"id":15,"op":"output","pos":[780,40],"in":[[14,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Tiling float 10 1 256 "Tiling"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Thickness float 0.035 0.001 0.25 "Thickness"
//@param sx_Softness float 0.01 0.0001 0.1 "Softness"
//@param sx_GlowRadius float 0.18 0.001 0.7 "Glow Radius"
//@param sx_GlowFalloff float 3 0.1 16 "Glow Falloff"
//@param sx_GlowIntensity float 0.25 0 16 "Glow Intensity"
//@param sx_LineOpacity float 1 0 1 "Line Opacity"
//@param sx_BackgroundOpacity float 1 0 1 "Background Opacity"
//@param sx_LineColor color 0.15 0.8 1.8 1 "Line Color"
//@param sx_BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Pattern): Seeded Truchet arc tiles for procedural ornament, paths, and seamless patterns.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Tiling, float sx_arg_Seed, float sx_arg_Thickness, float sx_arg_Softness, float sx_arg_GlowRadius, float sx_arg_GlowFalloff, float sx_arg_GlowIntensity, float sx_arg_LineOpacity, float sx_arg_BackgroundOpacity, float3 sx_arg_LineColor, float3 sx_arg_BackgroundColor)
{
    float2 q=UV*max(sx_arg_Tiling,1.0); float2 id=floor(q); float2 p=frac(q); float rnd=frac(sin(dot(id+sx_arg_Seed,float2(127.1,311.7)))*43758.5453); if(rnd>0.5)p.x=1.0-p.x; float d=min(abs(length(p)-0.5),abs(length(p-1.0)-0.5)); float lineMask=1.0-smoothstep(sx_arg_Thickness,sx_arg_Thickness+sx_arg_Softness,d); float glow=pow(saturate(1.0-d/max(sx_arg_GlowRadius,0.0001)),max(sx_arg_GlowFalloff,0.1)); float3 rgb=sx_arg_BackgroundColor.rgb+sx_arg_LineColor.rgb*(lineMask+glow*sx_arg_GlowIntensity); return float4(rgb,lerp(sx_arg_BackgroundOpacity,sx_arg_LineOpacity,max(lineMask,glow)));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Tiling, sx_Seed, sx_Thickness, sx_Softness, sx_GlowRadius, sx_GlowFalloff, sx_GlowIntensity, sx_LineOpacity, sx_BackgroundOpacity, sx_LineColor.rgb, sx_BackgroundColor.rgb);
}

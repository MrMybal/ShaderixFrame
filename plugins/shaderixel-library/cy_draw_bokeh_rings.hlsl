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
//@name Bokeh Rings
//@category Dessin
//@graph {"name":"Bokeh Rings","category":"Dessin","inputs":0,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.100000001,"max":8.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.219999999,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00100000005,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"RingWidth","text":"Ring Width","index":1,"min":0.00100000005,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":0.100000001,"max":16.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":64.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"param2","pos":[40,880],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":10,"op":"param2","pos":[40,990],"in":[],"value":[0.200000003,0.0700000003,0.0,0.0],"name":"Offset","text":"Offset","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[1.0,0.449999988,0.119999997,1.0],"name":"ColorA","text":"Color A","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.100000001,0.400000006,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Bokeh Rings","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float ring1=pow(saturate(1.0-abs(length(p)-Radius)/max(RingWidth,0.0001)),Softness); float ring2=pow(saturate(1.0-abs(length(p-Offset.xy)-Radius*0.55)/max(RingWidth*0.7,0.0001)),Softness); float ring3=pow(saturate(1.0-abs(length(p+Offset.xy*1.4)-Radius*0.35)/max(RingWidth*0.55,0.0001)),Softness); float3 rgb=ColorA.rgb*ring1+ColorB.rgb*(ring2+ring3); float a=saturate((ring1+ring2+ring3)*Opacity); return float4(rgb*Intensity,a);","pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["RingWidth",1],["Softness",1],["Intensity",1],["Opacity",1],["Center",3],["Offset",3],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Aspect float 1 0.1 8 "Aspect"
//@param sx_Radius float 0.22 0.001 2 "Radius"
//@param sx_RingWidth float 0.025 0.001 0.5 "Ring Width"
//@param sx_Softness float 2 0.1 16 "Softness"
//@param sx_Intensity float 2 0 64 "Intensity"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_Offset float2 0.2 0.07 0 1 "Offset"
//@param sx_ColorA color 1 0.45 0.12 1 "Color A"
//@param sx_ColorB color 0.1 0.4 1 1 "Color B"

// Imported preset (Draw / Light & Space): Procedural bokeh ring cluster for flare and depth-of-field overlays.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Aspect, float sx_arg_Radius, float sx_arg_RingWidth, float sx_arg_Softness, float sx_arg_Intensity, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_Offset, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 p=UV-sx_arg_Center.xy; p.x*=sx_arg_Aspect; float ring1=pow(saturate(1.0-abs(length(p)-sx_arg_Radius)/max(sx_arg_RingWidth,0.0001)),sx_arg_Softness); float ring2=pow(saturate(1.0-abs(length(p-sx_arg_Offset.xy)-sx_arg_Radius*0.55)/max(sx_arg_RingWidth*0.7,0.0001)),sx_arg_Softness); float ring3=pow(saturate(1.0-abs(length(p+sx_arg_Offset.xy*1.4)-sx_arg_Radius*0.35)/max(sx_arg_RingWidth*0.55,0.0001)),sx_arg_Softness); float3 rgb=sx_arg_ColorA.rgb*ring1+sx_arg_ColorB.rgb*(ring2+ring3); float a=saturate((ring1+ring2+ring3)*sx_arg_Opacity); return float4(rgb*sx_arg_Intensity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Aspect, sx_Radius, sx_RingWidth, sx_Softness, sx_Intensity, sx_Opacity, float3(sx_Center, 0.0), float3(sx_Offset, 0.0), sx_ColorA.rgb, sx_ColorB.rgb);
}

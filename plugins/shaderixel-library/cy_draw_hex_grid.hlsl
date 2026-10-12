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
//@name Hex Grid
//@category Dessin
//@graph {"name":"Hex Grid","category":"Dessin","inputs":0,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"LineWidth","text":"Line Width","index":1,"min":0.00100000005,"max":0.25},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.00800000038,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.100000001},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0549999997,0.0,0.0,0.0],"name":"NodeSize","text":"Node Size","index":1,"min":0.00100000005,"max":0.300000012},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"NodeIntensity","text":"Node Intensity","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"LineOpacity","text":"Line Opacity","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"CellOpacity","text":"Cell Opacity","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0500000007,0.800000012,1.39999998,1.0],"name":"LineColor","text":"Line Color","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[0.00499999989,0.0149999997,0.0250000004,1.0],"name":"CellColor","text":"Cell Color","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Hex Grid","text":"    float2 p=UV*max(Scale,1.0); p.x*=1.1547005; float2 a=frac(p)-0.5; float2 b=frac(p+0.5)-0.5; float2 g=dot(a,a)<dot(b,b)?a:b; float h=max(abs(g.y),abs(g.x)*0.866025+abs(g.y)*0.5); float lineMask=1.0-smoothstep(LineWidth,LineWidth+Softness,abs(0.5-h)); float node=1.0-smoothstep(NodeSize,NodeSize+Softness,length(g)); float m=saturate(max(lineMask,node*NodeIntensity)); return float4(lerp(CellColor.rgb,LineColor.rgb,m),lerp(CellOpacity,LineOpacity,m));","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["LineWidth",1],["Softness",1],["NodeSize",1],["NodeIntensity",1],["LineOpacity",1],["CellOpacity",1],["LineColor",3],["CellColor",3]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Scale float 12 1 256 "Scale"
//@param sx_LineWidth float 0.025 0.001 0.25 "Line Width"
//@param sx_Softness float 0.008 0.0001 0.1 "Softness"
//@param sx_NodeSize float 0.055 0.001 0.3 "Node Size"
//@param sx_NodeIntensity float 0.5 0 1 "Node Intensity"
//@param sx_LineOpacity float 1 0 1 "Line Opacity"
//@param sx_CellOpacity float 1 0 1 "Cell Opacity"
//@param sx_LineColor color 0.05 0.8 1.4 1 "Line Color"
//@param sx_CellColor color 0.005 0.015 0.025 1 "Cell Color"

// Imported preset (Draw / Pattern): Hexagonal technical grid with independently controlled cells, borders, and corner nodes.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_LineWidth, float sx_arg_Softness, float sx_arg_NodeSize, float sx_arg_NodeIntensity, float sx_arg_LineOpacity, float sx_arg_CellOpacity, float3 sx_arg_LineColor, float3 sx_arg_CellColor)
{
    float2 p=UV*max(sx_arg_Scale,1.0); p.x*=1.1547005; float2 a=frac(p)-0.5; float2 b=frac(p+0.5)-0.5; float2 g=dot(a,a)<dot(b,b)?a:b; float h=max(abs(g.y),abs(g.x)*0.866025+abs(g.y)*0.5); float lineMask=1.0-smoothstep(sx_arg_LineWidth,sx_arg_LineWidth+sx_arg_Softness,abs(0.5-h)); float node=1.0-smoothstep(sx_arg_NodeSize,sx_arg_NodeSize+sx_arg_Softness,length(g)); float m=saturate(max(lineMask,node*sx_arg_NodeIntensity)); return float4(lerp(sx_arg_CellColor.rgb,sx_arg_LineColor.rgb,m),lerp(sx_arg_CellOpacity,sx_arg_LineOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_LineWidth, sx_Softness, sx_NodeSize, sx_NodeIntensity, sx_LineOpacity, sx_CellOpacity, sx_LineColor.rgb, sx_CellColor.rgb);
}

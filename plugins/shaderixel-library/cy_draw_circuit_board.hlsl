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
//@name Circuit Board
//@category Dessin
//@graph {"name":"Circuit Board","category":"Dessin","inputs":0,"premultiplied":false,"nextId":19,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[18.0,0.0,0.0,0.0],"name":"Grid","text":"Grid","index":1,"min":2.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0549999997,0.0,0.0,0.0],"name":"TraceWidth","text":"Trace Width","index":1,"min":0.00100000005,"max":0.449999988},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.140000001,0.0,0.0,0.0],"name":"PadRadius","text":"Pad Radius","index":1,"min":0.00100000005,"max":0.699999988},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0149999997,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"BranchDensity","text":"Branch Density","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.720000029,0.0,0.0,0.0],"name":"PadDensity","text":"Pad Density","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"PulseFrequency","text":"Pulse Frequency","index":1,"min":0.0,"max":32.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-20.0,"max":20.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"PulseAmount","text":"Pulse Amount","index":1},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"TraceOpacity","text":"Trace Opacity","index":1},{"id":14,"op":"param","pos":[40,1430],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BoardOpacity","text":"Board Opacity","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.0500000007,1.60000002,0.449999988,1.0],"name":"TraceColor","text":"Trace Color","index":1},{"id":16,"op":"paramcolor","pos":[40,1650],"in":[],"value":[0.00499999989,0.0250000004,0.0120000001,1.0],"name":"BoardColor","text":"Board Color","index":1},{"id":17,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0]],"value":[0,0,0,0],"name":"Circuit Board","text":"    float2 q=UV*max(Grid,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float r=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453); float axis=lerp(abs(p.x),abs(p.y),step(0.5,r)); float trace=1.0-smoothstep(TraceWidth,TraceWidth+Softness,axis); float pad=1.0-smoothstep(PadRadius,PadRadius+Softness,length(p)); float gate=step(BranchDensity,frac(r*17.31)); float m=max(trace*gate,pad*step(PadDensity,r)); float pulse=lerp(1.0,0.55+0.45*sin((q.x+q.y)*PulseFrequency+Phase),PulseAmount); return float4(lerp(BoardColor.rgb,TraceColor.rgb,m*pulse),lerp(BoardOpacity,TraceOpacity,m));","pins":[["UV",2],["CyCanvasSize",4],["Grid",1],["Seed",1],["TraceWidth",1],["PadRadius",1],["Softness",1],["BranchDensity",1],["PadDensity",1],["PulseFrequency",1],["Phase",1],["PulseAmount",1],["TraceOpacity",1],["BoardOpacity",1],["TraceColor",3],["BoardColor",3]],"outs":[["Resultat",4]]},{"id":18,"op":"output","pos":[780,40],"in":[[17,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Grid float 18 2 256 "Grid"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_TraceWidth float 0.055 0.001 0.45 "Trace Width"
//@param sx_PadRadius float 0.14 0.001 0.7 "Pad Radius"
//@param sx_Softness float 0.015 0.0001 0.2 "Softness"
//@param sx_BranchDensity float 0.35 0 1 "Branch Density"
//@param sx_PadDensity float 0.72 0 1 "Pad Density"
//@param sx_PulseFrequency float 1.5 0 32 "Pulse Frequency"
//@param sx_Phase float 0 -20 20 "Phase"
//@param sx_PulseAmount float 0 0 1 "Pulse Amount"
//@param sx_TraceOpacity float 1 0 1 "Trace Opacity"
//@param sx_BoardOpacity float 1 0 1 "Board Opacity"
//@param sx_TraceColor color 0.05 1.6 0.45 1 "Trace Color"
//@param sx_BoardColor color 0.005 0.025 0.012 1 "Board Color"

// Imported preset (Draw / Pattern): Procedural circuit traces, pads, and substrate for technical masks and graphic design.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Grid, float sx_arg_Seed, float sx_arg_TraceWidth, float sx_arg_PadRadius, float sx_arg_Softness, float sx_arg_BranchDensity, float sx_arg_PadDensity, float sx_arg_PulseFrequency, float sx_arg_Phase, float sx_arg_PulseAmount, float sx_arg_TraceOpacity, float sx_arg_BoardOpacity, float3 sx_arg_TraceColor, float3 sx_arg_BoardColor)
{
    float2 q=UV*max(sx_arg_Grid,1.0); float2 id=floor(q); float2 p=frac(q)-0.5; float r=frac(sin(dot(id+sx_arg_Seed,float2(127.1,311.7)))*43758.5453); float axis=lerp(abs(p.x),abs(p.y),step(0.5,r)); float trace=1.0-smoothstep(sx_arg_TraceWidth,sx_arg_TraceWidth+sx_arg_Softness,axis); float pad=1.0-smoothstep(sx_arg_PadRadius,sx_arg_PadRadius+sx_arg_Softness,length(p)); float gate=step(sx_arg_BranchDensity,frac(r*17.31)); float m=max(trace*gate,pad*step(sx_arg_PadDensity,r)); float pulse=lerp(1.0,0.55+0.45*sin((q.x+q.y)*sx_arg_PulseFrequency+sx_arg_Phase),sx_arg_PulseAmount); return float4(lerp(sx_arg_BoardColor.rgb,sx_arg_TraceColor.rgb,m*pulse),lerp(sx_arg_BoardOpacity,sx_arg_TraceOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Grid, sx_Seed, sx_TraceWidth, sx_PadRadius, sx_Softness, sx_BranchDensity, sx_PadDensity, sx_PulseFrequency, sx_Phase, sx_PulseAmount, sx_TraceOpacity, sx_BoardOpacity, sx_TraceColor.rgb, sx_BoardColor.rgb);
}

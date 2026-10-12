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
//@name Brick Bond
//@category Dessin
//@graph {"name":"Brick Bond","category":"Dessin","inputs":0,"premultiplied":false,"nextId":18,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Columns","text":"Columns","index":1,"min":1.0,"max":128.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"Rows","text":"Rows","index":1,"min":1.0,"max":256.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0549999997,0.0,0.0,0.0],"name":"MortarWidth","text":"Mortar Width","index":1,"min":0.00100000005,"max":0.449999988},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.00999999978,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"BevelWidth","text":"Bevel Width","index":1,"min":0.00100000005,"max":0.5},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.649999976,0.0,0.0,0.0],"name":"BevelDarkening","text":"Bevel Darkening","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"GrainScale","text":"Grain Scale","index":1,"min":1.0,"max":256.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"GrainAmount","text":"Grain Amount","index":1},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[0.280000001,0.0549999997,0.0250000004,1.0],"name":"BrickColorA","text":"Brick Color A","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.550000012,0.140000001,0.0549999997,1.0],"name":"BrickColorB","text":"Brick Color B","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.280000001,0.25,0.200000003,1.0],"name":"MortarColor","text":"Mortar Color","index":1},{"id":16,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0]],"value":[0,0,0,0],"name":"Brick Bond","text":"    float2 q=UV*float2(max(Columns,1.0),max(Rows,1.0));float row=floor(q.y);q.x+=fmod(row,2.0)*.5;float2 id=floor(q);float2 p=frac(q);float h=frac(sin(dot(id+Seed,float2(127.1,311.7)))*43758.5453);float2 edge=min(p,1.0-p);float mortar=1.0-smoothstep(MortarWidth,MortarWidth+Softness,min(edge.x,edge.y));float bevel=saturate(min(edge.x,edge.y)/max(BevelWidth,.0001));float grain=frac(sin(dot(floor(q*GrainScale)+Seed,float2(12.9898,78.233)))*43758.5453);float3 brick=lerp(BrickColorA.rgb,BrickColorB.rgb,h)*lerp(1.0-GrainAmount,1.0+GrainAmount,grain)*lerp(BevelDarkening,1.0,bevel);return float4(lerp(brick,MortarColor.rgb,mortar),Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Columns",1],["Rows",1],["Seed",1],["MortarWidth",1],["Softness",1],["BevelWidth",1],["BevelDarkening",1],["GrainScale",1],["GrainAmount",1],["Opacity",1],["BrickColorA",3],["BrickColorB",3],["MortarColor",3]],"outs":[["Resultat",4]]},{"id":17,"op":"output","pos":[780,40],"in":[[16,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Columns float 8 1 128 "Columns"
//@param sx_Rows float 16 1 256 "Rows"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_MortarWidth float 0.055 0.001 0.45 "Mortar Width"
//@param sx_Softness float 0.01 0.0001 0.2 "Softness"
//@param sx_BevelWidth float 0.12 0.001 0.5 "Bevel Width"
//@param sx_BevelDarkening float 0.65 0 1 "Bevel Darkening"
//@param sx_GrainScale float 12 1 256 "Grain Scale"
//@param sx_GrainAmount float 0.12 0 1 "Grain Amount"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_BrickColorA color 0.28 0.055 0.025 1 "Brick Color A"
//@param sx_BrickColorB color 0.55 0.14 0.055 1 "Brick Color B"
//@param sx_MortarColor color 0.28 0.25 0.2 1 "Mortar Color"

// Imported preset (Draw / Surface): Seamless running-bond brick generator with mortar, bevel, variation, and surface grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Columns, float sx_arg_Rows, float sx_arg_Seed, float sx_arg_MortarWidth, float sx_arg_Softness, float sx_arg_BevelWidth, float sx_arg_BevelDarkening, float sx_arg_GrainScale, float sx_arg_GrainAmount, float sx_arg_Opacity, float3 sx_arg_BrickColorA, float3 sx_arg_BrickColorB, float3 sx_arg_MortarColor)
{
    float2 q=UV*float2(max(sx_arg_Columns,1.0),max(sx_arg_Rows,1.0));float row=floor(q.y);q.x+=fmod(row,2.0)*.5;float2 id=floor(q);float2 p=frac(q);float h=frac(sin(dot(id+sx_arg_Seed,float2(127.1,311.7)))*43758.5453);float2 edge=min(p,1.0-p);float mortar=1.0-smoothstep(sx_arg_MortarWidth,sx_arg_MortarWidth+sx_arg_Softness,min(edge.x,edge.y));float bevel=saturate(min(edge.x,edge.y)/max(sx_arg_BevelWidth,.0001));float grain=frac(sin(dot(floor(q*sx_arg_GrainScale)+sx_arg_Seed,float2(12.9898,78.233)))*43758.5453);float3 brick=lerp(sx_arg_BrickColorA.rgb,sx_arg_BrickColorB.rgb,h)*lerp(1.0-sx_arg_GrainAmount,1.0+sx_arg_GrainAmount,grain)*lerp(sx_arg_BevelDarkening,1.0,bevel);return float4(lerp(brick,sx_arg_MortarColor.rgb,mortar),sx_arg_Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Columns, sx_Rows, sx_Seed, sx_MortarWidth, sx_Softness, sx_BevelWidth, sx_BevelDarkening, sx_GrainScale, sx_GrainAmount, sx_Opacity, sx_BrickColorA.rgb, sx_BrickColorB.rgb, sx_MortarColor.rgb);
}

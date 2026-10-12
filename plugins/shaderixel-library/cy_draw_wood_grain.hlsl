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
//@name Wood Grain
//@category Dessin
//@graph {"name":"Wood Grain","category":"Dessin","inputs":0,"premultiplied":false,"nextId":18,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[7.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.100000001,"max":128.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.79999995,0.0,0.0,0.0],"name":"WarpFrequency","text":"Warp Frequency","index":1,"min":0.100000001,"max":64.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.219999999,0.0,0.0,0.0],"name":"WarpAmount","text":"Warp Amount","index":1,"min":0.0,"max":2.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"RingWobble","text":"Ring Wobble","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[2.20000005,0.0,0.0,0.0],"name":"RingSharpness","text":"Ring Sharpness","index":1,"min":0.100000001,"max":16.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[38.0,0.0,0.0,0.0],"name":"PoreScale","text":"Pore Scale","index":1,"min":1.0,"max":1024.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"PoreAmount","text":"Pore Amount","index":1},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"PoreStrength","text":"Pore Strength","index":1,"min":0.0,"max":2.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[0.119999997,0.0350000001,0.00800000038,1.0],"name":"EarlyWood","text":"Early Wood","index":1},{"id":15,"op":"paramcolor","pos":[40,1540],"in":[],"value":[0.550000012,0.219999999,0.0450000018,1.0],"name":"LateWood","text":"Late Wood","index":1},{"id":16,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0]],"value":[0,0,0,0],"name":"Wood Grain","text":"    float2 p=UV*max(Scale,0.01);float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float warp=sin(p.y*WarpFrequency+Seed)+.5*sin(p.y*WarpFrequency*2.17-p.x*.7+Seed);float rings=frac((p.x+warp*WarpAmount)+sin(p.y*1.7)*RingWobble);float band=pow(saturate(1.0-abs(rings-.5)*2.0),RingSharpness);float pore=frac(sin(dot(floor(p*PoreScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pores=smoothstep(1.0-PoreAmount,1.0,pore)*PoreStrength;float3 rgb=lerp(EarlyWood.rgb,LateWood.rgb,saturate(band+pores));return float4(rgb,Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Rotation",1],["Seed",1],["WarpFrequency",1],["WarpAmount",1],["RingWobble",1],["RingSharpness",1],["PoreScale",1],["PoreAmount",1],["PoreStrength",1],["Opacity",1],["EarlyWood",3],["LateWood",3]],"outs":[["Resultat",4]]},{"id":17,"op":"output","pos":[780,40],"in":[[16,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Scale float 7 0.1 128 "Scale"
//@param sx_Rotation float 0 -6.283 6.283 "Rotation"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_WarpFrequency float 2.8 0.1 64 "Warp Frequency"
//@param sx_WarpAmount float 0.22 0 2 "Warp Amount"
//@param sx_RingWobble float 0.08 0 1 "Ring Wobble"
//@param sx_RingSharpness float 2.2 0.1 16 "Ring Sharpness"
//@param sx_PoreScale float 38 1 1024 "Pore Scale"
//@param sx_PoreAmount float 0.08 0 1 "Pore Amount"
//@param sx_PoreStrength float 0.25 0 2 "Pore Strength"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_EarlyWood color 0.12 0.035 0.008 1 "Early Wood"
//@param sx_LateWood color 0.55 0.22 0.045 1 "Late Wood"

// Imported preset (Draw / Surface): Seamless procedural wood rings with warped fibers, pores, and adjustable early/late wood color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Rotation, float sx_arg_Seed, float sx_arg_WarpFrequency, float sx_arg_WarpAmount, float sx_arg_RingWobble, float sx_arg_RingSharpness, float sx_arg_PoreScale, float sx_arg_PoreAmount, float sx_arg_PoreStrength, float sx_arg_Opacity, float3 sx_arg_EarlyWood, float3 sx_arg_LateWood)
{
    float2 p=UV*max(sx_arg_Scale,0.01);float c=cos(sx_arg_Rotation),s=sin(sx_arg_Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float warp=sin(p.y*sx_arg_WarpFrequency+sx_arg_Seed)+.5*sin(p.y*sx_arg_WarpFrequency*2.17-p.x*.7+sx_arg_Seed);float rings=frac((p.x+warp*sx_arg_WarpAmount)+sin(p.y*1.7)*sx_arg_RingWobble);float band=pow(saturate(1.0-abs(rings-.5)*2.0),sx_arg_RingSharpness);float pore=frac(sin(dot(floor(p*sx_arg_PoreScale)+sx_arg_Seed,float2(12.9898,78.233)))*43758.5453);float pores=smoothstep(1.0-sx_arg_PoreAmount,1.0,pore)*sx_arg_PoreStrength;float3 rgb=lerp(sx_arg_EarlyWood.rgb,sx_arg_LateWood.rgb,saturate(band+pores));return float4(rgb,sx_arg_Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Rotation, sx_Seed, sx_WarpFrequency, sx_WarpAmount, sx_RingWobble, sx_RingSharpness, sx_PoreScale, sx_PoreAmount, sx_PoreStrength, sx_Opacity, sx_EarlyWood.rgb, sx_LateWood.rgb);
}

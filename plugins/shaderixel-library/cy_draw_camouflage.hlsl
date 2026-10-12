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
//@name Camouflage
//@category Dessin
//@graph {"name":"Camouflage","category":"Dessin","inputs":0,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[3.5,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.100000001,"max":64.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.20000005,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":8.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.319999993,0.0,0.0,0.0],"name":"ThresholdA","text":"Threshold A","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.529999971,0.0,0.0,0.0],"name":"ThresholdB","text":"Threshold B","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.74000001,0.0,0.0,0.0],"name":"ThresholdC","text":"Threshold C","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0350000001,0.0549999997,0.0250000004,1.0],"name":"ColorA","text":"Color A","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[0.119999997,0.180000007,0.0599999987,1.0],"name":"ColorB","text":"Color B","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.319999993,0.280000001,0.119999997,1.0],"name":"ColorC","text":"Color C","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[0.0799999982,0.0700000003,0.0450000018,1.0],"name":"ColorD","text":"Color D","index":1},{"id":14,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0]],"value":[0,0,0,0],"name":"Camouflage","text":"    float2 p=UV*max(Scale,.01)+Seed;float a=sin(p.x*2.1+sin(p.y*1.7))+sin(p.y*2.7-p.x*.8);float b=sin(p.x*5.3-p.y*3.1+Seed)+sin((p.x+p.y)*4.7);float c=sin(p.x*11.3+p.y*7.9-Seed);float n=saturate((a*.28+b*.15+c*.07)*Contrast+.5);float3 rgb=ColorA.rgb;if(n>ThresholdA)rgb=ColorB.rgb;if(n>ThresholdB)rgb=ColorC.rgb;if(n>ThresholdC)rgb=ColorD.rgb;return float4(rgb,Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Contrast",1],["ThresholdA",1],["ThresholdB",1],["ThresholdC",1],["Opacity",1],["ColorA",3],["ColorB",3],["ColorC",3],["ColorD",3]],"outs":[["Resultat",4]]},{"id":15,"op":"output","pos":[780,40],"in":[[14,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Scale float 3.5 0.1 64 "Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Contrast float 1.2 0 8 "Contrast"
//@param sx_ThresholdA float 0.32 0 1 "Threshold A"
//@param sx_ThresholdB float 0.53 0 1 "Threshold B"
//@param sx_ThresholdC float 0.74 0 1 "Threshold C"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_ColorA color 0.035 0.055 0.025 1 "Color A"
//@param sx_ColorB color 0.12 0.18 0.06 1 "Color B"
//@param sx_ColorC color 0.32 0.28 0.12 1 "Color C"
//@param sx_ColorD color 0.08 0.07 0.045 1 "Color D"

// Imported preset (Draw / Surface): Multi-scale organic camouflage regions with four colors, warped boundaries, and seamless UV input.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Seed, float sx_arg_Contrast, float sx_arg_ThresholdA, float sx_arg_ThresholdB, float sx_arg_ThresholdC, float sx_arg_Opacity, float3 sx_arg_ColorA, float3 sx_arg_ColorB, float3 sx_arg_ColorC, float3 sx_arg_ColorD)
{
    float2 p=UV*max(sx_arg_Scale,.01)+sx_arg_Seed;float a=sin(p.x*2.1+sin(p.y*1.7))+sin(p.y*2.7-p.x*.8);float b=sin(p.x*5.3-p.y*3.1+sx_arg_Seed)+sin((p.x+p.y)*4.7);float c=sin(p.x*11.3+p.y*7.9-sx_arg_Seed);float n=saturate((a*.28+b*.15+c*.07)*sx_arg_Contrast+.5);float3 rgb=sx_arg_ColorA.rgb;if(n>sx_arg_ThresholdA)rgb=sx_arg_ColorB.rgb;if(n>sx_arg_ThresholdB)rgb=sx_arg_ColorC.rgb;if(n>sx_arg_ThresholdC)rgb=sx_arg_ColorD.rgb;return float4(rgb,sx_arg_Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Seed, sx_Contrast, sx_ThresholdA, sx_ThresholdB, sx_ThresholdC, sx_Opacity, sx_ColorA.rgb, sx_ColorB.rgb, sx_ColorC.rgb, sx_ColorD.rgb);
}

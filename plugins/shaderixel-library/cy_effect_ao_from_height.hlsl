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
//@name AO From Height
//@category Traitements
//@graph {"name":"AO From Height","category":"Traitements","inputs":1,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":1.0,"max":512.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Steps","text":"Steps","index":1,"min":4.0,"max":48.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.00499999989,0.0,0.0,0.0],"name":"Bias","text":"Bias","index":1,"min":-1.0,"max":1.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[5.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":64.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.20000005,0.0,0.0,0.0],"name":"Power","text":"Power","index":1,"min":0.0500000007,"max":8.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Invert","text":"Invert","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"HeightChannel","text":"Height Channel","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"AO From Height","text":"    float2 texel=1.0/max(CyCanvasSize.xy,1.0);float center=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,HeightChannel.rgb);int count=clamp((int)round(Steps),4,48);float occ=0;[loop]for(int i=0;i<48;i++){if(i>=count)break;float f=(i+.5)/count;float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*Radius*sqrt(f)*texel;float h=dot(Texture2DSample(CyInput,CyInputSampler,UV+o).rgb,HeightChannel.rgb);occ+=saturate((h-center-Bias)*Strength/(sqrt(f)+.05));}occ=pow(saturate(occ/count),Power);float ao=lerp(1.0-occ,occ,Invert);return float4(ao.xxx,Texture2DSample(CyInput,CyInputSampler,UV).a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Steps",1],["Bias",1],["Strength",1],["Power",1],["Invert",1],["HeightChannel",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param sx_Radius float 24 1 512 "Radius"
//@param sx_Steps float 24 4 48 "Steps"
//@param sx_Bias float 0.005 -1 1 "Bias"
//@param sx_Strength float 5 0 64 "Strength"
//@param sx_Power float 1.2 0.05 8 "Power"
//@param sx_Invert float 0 0 1 "Invert"
//@param sx_HeightChannel color 1 0 0 0 "Height Channel"

// Imported preset (Effect / PBR): Generates a horizon-style ambient-occlusion mask from a linear height texture with adjustable sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Radius, float sx_arg_Steps, float sx_arg_Bias, float sx_arg_Strength, float sx_arg_Power, float sx_arg_Invert, float3 sx_arg_HeightChannel)
{
    float2 texel=1.0/max(CyCanvasSize.xy,1.0);float center=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,sx_arg_HeightChannel.rgb);int count=clamp((int)round(sx_arg_Steps),4,48);float occ=0;[loop]for(int i=0;i<48;i++){if(i>=count)break;float f=(i+.5)/count;float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*sx_arg_Radius*sqrt(f)*texel;float h=dot(Texture2DSample(CyInput,CyInputSampler,UV+o).rgb,sx_arg_HeightChannel.rgb);occ+=saturate((h-center-sx_arg_Bias)*sx_arg_Strength/(sqrt(f)+.05));}occ=pow(saturate(occ/count),sx_arg_Power);float ao=lerp(1.0-occ,occ,sx_arg_Invert);return float4(ao.xxx,Texture2DSample(CyInput,CyInputSampler,UV).a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Radius, sx_Steps, sx_Bias, sx_Strength, sx_Power, sx_Invert, sx_HeightChannel.rgb);
}

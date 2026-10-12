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
//@name Roughness Remap
//@category Traitements
//@graph {"name":"Roughness Remap","category":"Traitements","inputs":1,"premultiplied":false,"nextId":17,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"InputMinimum","text":"Input Minimum","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"InputMaximum","text":"Input Maximum","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0399999991,0.0,0.0,0.0],"name":"OutputMinimum","text":"Output Minimum","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OutputMaximum","text":"Output Maximum","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Gamma","text":"Gamma","index":1,"min":0.00999999978,"max":8.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0199999996,0.0,0.0,0.0],"name":"PhysicalMinimum","text":"Physical Minimum","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[256.0,0.0,0.0,0.0],"name":"MicroScale","text":"Micro Scale","index":1,"min":1.0,"max":4096.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.0299999993,0.0,0.0,0.0],"name":"MicroVariation","text":"Micro Variation","index":1,"min":0.0,"max":0.5},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"SourceChannel","text":"Source Channel","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"OutputChannels","text":"Output Channels","index":1},{"id":15,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0]],"value":[0,0,0,0],"name":"Roughness Remap","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float rough=dot(s.rgb,SourceChannel.rgb);rough=saturate((rough-InputMinimum)/max(InputMaximum-InputMinimum,.0001));rough=pow(rough,max(Gamma,.01));rough=lerp(OutputMinimum,OutputMaximum,rough);float n=frac(sin(dot(floor(UV*MicroScale)+Seed,float2(12.9898,78.233)))*43758.5453)-.5;rough=clamp(rough+n*MicroVariation,PhysicalMinimum,1.0);float3 outColor=lerp(s.rgb,rough.xxx,OutputChannels.rgb);return float4(outColor,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",1],["InputMaximum",1],["OutputMinimum",1],["OutputMaximum",1],["Gamma",1],["PhysicalMinimum",1],["MicroScale",1],["Seed",1],["MicroVariation",1],["SourceChannel",3],["OutputChannels",3]],"outs":[["Resultat",4]]},{"id":16,"op":"output","pos":[780,40],"in":[[15,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param sx_InputMinimum float 0 0 1 "Input Minimum"
//@param sx_InputMaximum float 1 0 1 "Input Maximum"
//@param sx_OutputMinimum float 0.04 0 1 "Output Minimum"
//@param sx_OutputMaximum float 1 0 1 "Output Maximum"
//@param sx_Gamma float 1 0.01 8 "Gamma"
//@param sx_PhysicalMinimum float 0.02 0 1 "Physical Minimum"
//@param sx_MicroScale float 256 1 4096 "Micro Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_MicroVariation float 0.03 0 0.5 "Micro Variation"
//@param sx_SourceChannel color 1 0 0 0 "Source Channel"
//@param sx_OutputChannels color 1 1 1 0 "Output Channels"

// Imported preset (Effect / PBR): PBR roughness remap with range compression, gamma, microvariation, and physical clamping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_InputMinimum, float sx_arg_InputMaximum, float sx_arg_OutputMinimum, float sx_arg_OutputMaximum, float sx_arg_Gamma, float sx_arg_PhysicalMinimum, float sx_arg_MicroScale, float sx_arg_Seed, float sx_arg_MicroVariation, float3 sx_arg_SourceChannel, float3 sx_arg_OutputChannels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float rough=dot(s.rgb,sx_arg_SourceChannel.rgb);rough=saturate((rough-sx_arg_InputMinimum)/max(sx_arg_InputMaximum-sx_arg_InputMinimum,.0001));rough=pow(rough,max(sx_arg_Gamma,.01));rough=lerp(sx_arg_OutputMinimum,sx_arg_OutputMaximum,rough);float n=frac(sin(dot(floor(UV*sx_arg_MicroScale)+sx_arg_Seed,float2(12.9898,78.233)))*43758.5453)-.5;rough=clamp(rough+n*sx_arg_MicroVariation,sx_arg_PhysicalMinimum,1.0);float3 outColor=lerp(s.rgb,rough.xxx,sx_arg_OutputChannels.rgb);return float4(outColor,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_InputMinimum, sx_InputMaximum, sx_OutputMinimum, sx_OutputMaximum, sx_Gamma, sx_PhysicalMinimum, sx_MicroScale, sx_Seed, sx_MicroVariation, sx_SourceChannel.rgb, sx_OutputChannels.rgb);
}

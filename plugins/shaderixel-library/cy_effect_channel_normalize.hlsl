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
//@name Channel Normalize
//@category Traitements
//@graph {"name":"Channel Normalize","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"paramcolor","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"InputMinimum","text":"Input Minimum","index":1},{"id":5,"op":"paramcolor","pos":[40,440],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"InputMaximum","text":"Input Maximum","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"Gamma","text":"Gamma","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"OutputMinimum","text":"Output Minimum","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"OutputMaximum","text":"Output Maximum","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"Channels","text":"Channels","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Channel Normalize","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 v=saturate((s.rgb-InputMinimum.rgb)/max(InputMaximum.rgb-InputMinimum.rgb,.0001));v=pow(v,max(Gamma.rgb,.01));v=lerp(OutputMinimum.rgb,OutputMaximum.rgb,v);return float4(lerp(s.rgb,v,Channels.rgb),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",3],["InputMaximum",3],["Gamma",3],["OutputMinimum",3],["OutputMaximum",3],["Channels",3]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param sx_InputMinimum color 0 0 0 0 "Input Minimum"
//@param sx_InputMaximum color 1 1 1 0 "Input Maximum"
//@param sx_Gamma color 1 1 1 0 "Gamma"
//@param sx_OutputMinimum color 0 0 0 0 "Output Minimum"
//@param sx_OutputMaximum color 1 1 1 0 "Output Maximum"
//@param sx_Channels color 1 1 1 0 "Channels"

// Imported preset (Effect / PBR): Normalizes and remaps independently selected packed PBR channels with per-channel gamma.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float3 sx_arg_InputMinimum, float3 sx_arg_InputMaximum, float3 sx_arg_Gamma, float3 sx_arg_OutputMinimum, float3 sx_arg_OutputMaximum, float3 sx_arg_Channels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 v=saturate((s.rgb-sx_arg_InputMinimum.rgb)/max(sx_arg_InputMaximum.rgb-sx_arg_InputMinimum.rgb,.0001));v=pow(v,max(sx_arg_Gamma.rgb,.01));v=lerp(sx_arg_OutputMinimum.rgb,sx_arg_OutputMaximum.rgb,v);return float4(lerp(s.rgb,v,sx_arg_Channels.rgb),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_InputMinimum.rgb, sx_InputMaximum.rgb, sx_Gamma.rgb, sx_OutputMinimum.rgb, sx_OutputMaximum.rgb, sx_Channels.rgb);
}

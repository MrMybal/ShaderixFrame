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
//@name Scanlines
//@category Traitements
//@graph {"name":"Scanlines","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[540.0,0.0,0.0,0.0],"name":"Frequency","text":"Frequency","index":1,"min":1.0,"max":8192.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-10.0,"max":10.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"Sharpness","text":"Sharpness","index":1,"min":0.100000001,"max":32.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"LineColor","text":"Line Color","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Scanlines","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float scanline=0.5+0.5*cos((UV.y*Frequency+Phase)*6.2831853); scanline=pow(scanline,max(Sharpness,0.1)); float3 v=s.rgb*lerp(1.0,LineColor.rgb,scanline*Intensity); return float4(v,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Frequency",1],["Phase",1],["Sharpness",1],["Intensity",1],["LineColor",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param sx_Frequency float 540 1 8192 "Frequency"
//@param sx_Phase float 0 -10 10 "Phase"
//@param sx_Sharpness float 3 0.1 32 "Sharpness"
//@param sx_Intensity float 0.25 0 1 "Intensity"
//@param sx_LineColor color 0 0 0 1 "Line Color"

// Imported preset (Effect / Stylize): CRT-style scanlines with phase and tint.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Frequency, float sx_arg_Phase, float sx_arg_Sharpness, float sx_arg_Intensity, float3 sx_arg_LineColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float scanline=0.5+0.5*cos((UV.y*sx_arg_Frequency+sx_arg_Phase)*6.2831853); scanline=pow(scanline,max(sx_arg_Sharpness,0.1)); float3 v=s.rgb*lerp(1.0,sx_arg_LineColor.rgb,scanline*sx_arg_Intensity); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Frequency, sx_Phase, sx_Sharpness, sx_Intensity, sx_LineColor.rgb);
}

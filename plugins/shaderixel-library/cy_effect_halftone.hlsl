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
//@name Halftone
//@category Traitements
//@graph {"name":"Halftone","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[96.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":2.0,"max":2048.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.785000026,0.0,0.0,0.0],"name":"Angle","text":"Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0299999993,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.5},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"PreserveColor","text":"Preserve Color","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"InkColor","text":"Ink Color","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"PaperColor","text":"Paper Color","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Halftone","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float c=cos(Angle),sn=sin(Angle);float2 p=float2(c*UV.x-sn*UV.y,sn*UV.x+c*UV.y)*Scale;float2 cell=frac(p)-.5;float l=dot(s.rgb,float3(.2126,.7152,.0722));float dotMask=1.0-smoothstep(sqrt(saturate(1.0-l))*.48,sqrt(saturate(1.0-l))*.48+Softness,length(cell));float3 mono=lerp(PaperColor.rgb,InkColor.rgb,dotMask);return float4(lerp(mono,s.rgb,saturate(PreserveColor)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Scale",1],["Angle",1],["Softness",1],["PreserveColor",1],["InkColor",3],["PaperColor",3]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param sx_Scale float 96 2 2048 "Scale"
//@param sx_Angle float 0.785 -6.283 6.283 "Angle"
//@param sx_Softness float 0.03 0.0001 0.5 "Softness"
//@param sx_PreserveColor float 0 0 1 "Preserve Color"
//@param sx_InkColor color 0 0 0 1 "Ink Color"
//@param sx_PaperColor color 1 1 1 1 "Paper Color"

// Imported preset (Effect / Stylize): Print-style halftone dots with angle, scale, softness, and color preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Angle, float sx_arg_Softness, float sx_arg_PreserveColor, float3 sx_arg_InkColor, float3 sx_arg_PaperColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float c=cos(sx_arg_Angle),sn=sin(sx_arg_Angle);float2 p=float2(c*UV.x-sn*UV.y,sn*UV.x+c*UV.y)*sx_arg_Scale;float2 cell=frac(p)-.5;float l=dot(s.rgb,float3(.2126,.7152,.0722));float dotMask=1.0-smoothstep(sqrt(saturate(1.0-l))*.48,sqrt(saturate(1.0-l))*.48+sx_arg_Softness,length(cell));float3 mono=lerp(sx_arg_PaperColor.rgb,sx_arg_InkColor.rgb,dotMask);return float4(lerp(mono,s.rgb,saturate(sx_arg_PreserveColor)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Angle, sx_Softness, sx_PreserveColor, sx_InkColor.rgb, sx_PaperColor.rgb);
}

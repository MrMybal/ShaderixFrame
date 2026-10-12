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
//@name Voronoi
//@category Dessin
//@graph {"name":"Voronoi","category":"Dessin","inputs":0,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":16.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"BorderContrast","text":"Border Contrast","index":1,"min":0.0,"max":64.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BorderMode","text":"Border Mode","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Voronoi","text":"    float2 p=UV*max(Scale,1.0); float2 ip=floor(p); float2 fp=frac(p); float nearest=10.0; float second=10.0; [unroll] for(int y=-1;y<=1;y++){[unroll] for(int x=-1;x<=1;x++){float2 o=float2(x,y); float2 cell=ip+o; float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453); float d=length(o+rnd-fp); if(d<nearest){second=nearest;nearest=d;}else if(d<second){second=d;}}} float border=saturate((second-nearest)*BorderContrast); float v=lerp(nearest,border,saturate(BorderMode)); v=saturate(v*Contrast); return float4(lerp(ColorA.rgb,ColorB.rgb,v),Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Contrast",1],["BorderContrast",1],["BorderMode",1],["Opacity",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Scale float 12 1 256 "Scale"
//@param sx_Seed float 0 -1000 1000 "Seed"
//@param sx_Contrast float 1.5 0 16 "Contrast"
//@param sx_BorderContrast float 8 0 64 "Border Contrast"
//@param sx_BorderMode float 1 0 1 "Border Mode"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_ColorA color 0 0 0 1 "Color A"
//@param sx_ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Noise): Voronoi cell distance and border generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Scale, float sx_arg_Seed, float sx_arg_Contrast, float sx_arg_BorderContrast, float sx_arg_BorderMode, float sx_arg_Opacity, float3 sx_arg_ColorA, float3 sx_arg_ColorB)
{
    float2 p=UV*max(sx_arg_Scale,1.0); float2 ip=floor(p); float2 fp=frac(p); float nearest=10.0; float second=10.0; [unroll] for(int y=-1;y<=1;y++){[unroll] for(int x=-1;x<=1;x++){float2 o=float2(x,y); float2 cell=ip+o; float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+sx_arg_Seed)*43758.5453); float d=length(o+rnd-fp); if(d<nearest){second=nearest;nearest=d;}else if(d<second){second=d;}}} float border=saturate((second-nearest)*sx_arg_BorderContrast); float v=lerp(nearest,border,saturate(sx_arg_BorderMode)); v=saturate(v*sx_arg_Contrast); return float4(lerp(sx_arg_ColorA.rgb,sx_arg_ColorB.rgb,v),sx_arg_Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Scale, sx_Seed, sx_Contrast, sx_BorderContrast, sx_BorderMode, sx_Opacity, sx_ColorA.rgb, sx_ColorB.rgb);
}

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
//@name Star
//@category Dessin
//@graph {"name":"Star","category":"Dessin","inputs":0,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[5.0,0.0,0.0,0.0],"name":"Points","text":"Points","index":1,"min":2.0,"max":64.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"InnerRadius","text":"Inner Radius","index":1,"min":0.00100000005,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.319999993,0.0,0.0,0.0],"name":"OuterRadius","text":"Outer Radius","index":1,"min":0.00100000005,"max":2.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"PointSharpness","text":"Point Sharpness","index":1,"min":0.0500000007,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.00400000019,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.100000001},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":11,"op":"param2","pos":[40,1100],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"Color","text":"Color","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Star","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float a=atan2(p.y,p.x)+Rotation; float count=max(Points,2.0); float wave=0.5+0.5*cos(a*count); float r=lerp(InnerRadius,OuterRadius,pow(wave,max(PointSharpness,0.05))); float mask=1.0-smoothstep(r,r+Softness,length(p)); return float4(Color.rgb,mask*Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Points",1],["InnerRadius",1],["OuterRadius",1],["PointSharpness",1],["Rotation",1],["Aspect",1],["Softness",1],["Opacity",1],["Center",3],["Color",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Points float 5 2 64 "Points"
//@param sx_InnerRadius float 0.12 0.001 2 "Inner Radius"
//@param sx_OuterRadius float 0.32 0.001 2 "Outer Radius"
//@param sx_PointSharpness float 1 0.05 8 "Point Sharpness"
//@param sx_Rotation float 0 -6.283 6.283 "Rotation"
//@param sx_Aspect float 1 0.01 8 "Aspect"
//@param sx_Softness float 0.004 0.0001 0.1 "Softness"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.5 0.5 0 1 "Center"
//@param sx_Color color 1 1 1 1 "Color"

// Imported preset (Draw / Shape & Gradient): Procedural star or flower shape with controllable point count.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Points, float sx_arg_InnerRadius, float sx_arg_OuterRadius, float sx_arg_PointSharpness, float sx_arg_Rotation, float sx_arg_Aspect, float sx_arg_Softness, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_Color)
{
    float2 p=UV-sx_arg_Center.xy; p.x*=sx_arg_Aspect; float a=atan2(p.y,p.x)+sx_arg_Rotation; float count=max(sx_arg_Points,2.0); float wave=0.5+0.5*cos(a*count); float r=lerp(sx_arg_InnerRadius,sx_arg_OuterRadius,pow(wave,max(sx_arg_PointSharpness,0.05))); float mask=1.0-smoothstep(r,r+sx_arg_Softness,length(p)); return float4(sx_arg_Color.rgb,mask*sx_arg_Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Points, sx_InnerRadius, sx_OuterRadius, sx_PointSharpness, sx_Rotation, sx_Aspect, sx_Softness, sx_Opacity, float3(sx_Center, 0.0), sx_Color.rgb);
}

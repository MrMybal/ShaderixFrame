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
//@name Lens Flare
//@category Dessin
//@graph {"name":"Lens Flare","category":"Dessin","inputs":0,"premultiplied":false,"nextId":26,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.100000001,"max":8.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0599999987,0.0,0.0,0.0],"name":"CoreRadius","text":"Core Radius","index":1,"min":0.00100000005,"max":1.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"CoreFalloff","text":"Core Falloff","index":1,"min":0.100000001,"max":16.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"CoreIntensity","text":"Core Intensity","index":1,"min":0.0,"max":64.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.219999999,0.0,0.0,0.0],"name":"HaloRadius","text":"Halo Radius","index":1,"min":0.00100000005,"max":2.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"HaloWidth","text":"Halo Width","index":1,"min":0.00100000005,"max":0.5},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"HaloFalloff","text":"Halo Falloff","index":1,"min":0.100000001,"max":16.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"HaloIntensity","text":"Halo Intensity","index":1,"min":0.0,"max":32.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"RayCount","text":"Ray Count","index":1,"min":1.0,"max":64.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"RaySharpness","text":"Ray Sharpness","index":1,"min":1.0,"max":64.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[0.449999988,0.0,0.0,0.0],"name":"RayLength","text":"Ray Length","index":1,"min":0.00100000005,"max":2.0},{"id":14,"op":"param","pos":[40,1430],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"RayIntensity","text":"Ray Intensity","index":1,"min":0.0,"max":32.0},{"id":15,"op":"param","pos":[40,1540],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":16,"op":"param","pos":[40,1650],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"GhostSpacing","text":"Ghost Spacing","index":1,"min":0.0,"max":3.0},{"id":17,"op":"param","pos":[40,1760],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"GhostRadius","text":"Ghost Radius","index":1,"min":0.00100000005,"max":1.0},{"id":18,"op":"param","pos":[40,1870],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"GhostIntensity","text":"Ghost Intensity","index":1,"min":0.0,"max":32.0},{"id":19,"op":"param","pos":[40,1980],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":20,"op":"param2","pos":[40,2090],"in":[],"value":[0.379999995,0.419999987,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":21,"op":"param2","pos":[40,2200],"in":[],"value":[0.280000001,0.0799999982,0.0,0.0],"name":"Direction","text":"Direction","index":1},{"id":22,"op":"paramcolor","pos":[40,2310],"in":[],"value":[1.0,0.649999976,0.25,1.0],"name":"PrimaryColor","text":"Primary Color","index":1},{"id":23,"op":"paramcolor","pos":[40,2420],"in":[],"value":[0.180000007,0.449999988,1.0,1.0],"name":"SecondaryColor","text":"Secondary Color","index":1},{"id":24,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0],[17,0],[18,0],[19,0],[20,0],[21,0],[22,0],[23,0]],"value":[0,0,0,0],"name":"Lens Flare","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float core=pow(saturate(1.0-d/max(CoreRadius,0.0001)),max(CoreFalloff,0.1))*CoreIntensity; float halo=pow(saturate(1.0-abs(d-HaloRadius)/max(HaloWidth,0.0001)),max(HaloFalloff,0.1))*HaloIntensity; float angle=atan2(p.y,p.x)+Rotation; float rays=pow(abs(cos(angle*max(RayCount,1.0))),max(RaySharpness,1.0))*pow(saturate(1.0-d/max(RayLength,0.0001)),2.0)*RayIntensity; float2 axis=Direction.xy; float g1=pow(saturate(1.0-length(p-axis*GhostSpacing)/max(GhostRadius,0.0001)),2.0); float g2=pow(saturate(1.0-length(p+axis*GhostSpacing*1.7)/max(GhostRadius*0.7,0.0001)),2.0); float3 rgb=PrimaryColor.rgb*(core+halo+rays)+SecondaryColor.rgb*(g1+g2)*GhostIntensity; float a=saturate(max(max(rgb.r,rgb.g),rgb.b)*Opacity); return float4(rgb*Opacity,a);","pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["CoreRadius",1],["CoreFalloff",1],["CoreIntensity",1],["HaloRadius",1],["HaloWidth",1],["HaloFalloff",1],["HaloIntensity",1],["RayCount",1],["RaySharpness",1],["RayLength",1],["RayIntensity",1],["Rotation",1],["GhostSpacing",1],["GhostRadius",1],["GhostIntensity",1],["Opacity",1],["Center",3],["Direction",3],["PrimaryColor",3],["SecondaryColor",3]],"outs":[["Resultat",4]]},{"id":25,"op":"output","pos":[780,40],"in":[[24,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param sx_Aspect float 1 0.1 8 "Aspect"
//@param sx_CoreRadius float 0.06 0.001 1 "Core Radius"
//@param sx_CoreFalloff float 2 0.1 16 "Core Falloff"
//@param sx_CoreIntensity float 8 0 64 "Core Intensity"
//@param sx_HaloRadius float 0.22 0.001 2 "Halo Radius"
//@param sx_HaloWidth float 0.025 0.001 0.5 "Halo Width"
//@param sx_HaloFalloff float 2 0.1 16 "Halo Falloff"
//@param sx_HaloIntensity float 1.5 0 32 "Halo Intensity"
//@param sx_RayCount float 8 1 64 "Ray Count"
//@param sx_RaySharpness float 12 1 64 "Ray Sharpness"
//@param sx_RayLength float 0.45 0.001 2 "Ray Length"
//@param sx_RayIntensity float 1.5 0 32 "Ray Intensity"
//@param sx_Rotation float 0 -6.283 6.283 "Rotation"
//@param sx_GhostSpacing float 0.5 0 3 "Ghost Spacing"
//@param sx_GhostRadius float 0.08 0.001 1 "Ghost Radius"
//@param sx_GhostIntensity float 1 0 32 "Ghost Intensity"
//@param sx_Opacity float 1 0 1 "Opacity"
//@param sx_Center float2 0.38 0.42 0 1 "Center"
//@param sx_Direction float2 0.28 0.08 0 1 "Direction"
//@param sx_PrimaryColor color 1 0.65 0.25 1 "Primary Color"
//@param sx_SecondaryColor color 0.18 0.45 1 1 "Secondary Color"

// Imported preset (Draw / Light & Space): HDR cinematic lens flare with core, halo, rays, and colored ghosts.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Aspect, float sx_arg_CoreRadius, float sx_arg_CoreFalloff, float sx_arg_CoreIntensity, float sx_arg_HaloRadius, float sx_arg_HaloWidth, float sx_arg_HaloFalloff, float sx_arg_HaloIntensity, float sx_arg_RayCount, float sx_arg_RaySharpness, float sx_arg_RayLength, float sx_arg_RayIntensity, float sx_arg_Rotation, float sx_arg_GhostSpacing, float sx_arg_GhostRadius, float sx_arg_GhostIntensity, float sx_arg_Opacity, float3 sx_arg_Center, float3 sx_arg_Direction, float3 sx_arg_PrimaryColor, float3 sx_arg_SecondaryColor)
{
    float2 p=UV-sx_arg_Center.xy; p.x*=sx_arg_Aspect; float d=length(p); float core=pow(saturate(1.0-d/max(sx_arg_CoreRadius,0.0001)),max(sx_arg_CoreFalloff,0.1))*sx_arg_CoreIntensity; float halo=pow(saturate(1.0-abs(d-sx_arg_HaloRadius)/max(sx_arg_HaloWidth,0.0001)),max(sx_arg_HaloFalloff,0.1))*sx_arg_HaloIntensity; float angle=atan2(p.y,p.x)+sx_arg_Rotation; float rays=pow(abs(cos(angle*max(sx_arg_RayCount,1.0))),max(sx_arg_RaySharpness,1.0))*pow(saturate(1.0-d/max(sx_arg_RayLength,0.0001)),2.0)*sx_arg_RayIntensity; float2 axis=sx_arg_Direction.xy; float g1=pow(saturate(1.0-length(p-axis*sx_arg_GhostSpacing)/max(sx_arg_GhostRadius,0.0001)),2.0); float g2=pow(saturate(1.0-length(p+axis*sx_arg_GhostSpacing*1.7)/max(sx_arg_GhostRadius*0.7,0.0001)),2.0); float3 rgb=sx_arg_PrimaryColor.rgb*(core+halo+rays)+sx_arg_SecondaryColor.rgb*(g1+g2)*sx_arg_GhostIntensity; float a=saturate(max(max(rgb.r,rgb.g),rgb.b)*sx_arg_Opacity); return float4(rgb*sx_arg_Opacity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Aspect, sx_CoreRadius, sx_CoreFalloff, sx_CoreIntensity, sx_HaloRadius, sx_HaloWidth, sx_HaloFalloff, sx_HaloIntensity, sx_RayCount, sx_RaySharpness, sx_RayLength, sx_RayIntensity, sx_Rotation, sx_GhostSpacing, sx_GhostRadius, sx_GhostIntensity, sx_Opacity, float3(sx_Center, 0.0), float3(sx_Direction, 0.0), sx_PrimaryColor.rgb, sx_SecondaryColor.rgb);
}

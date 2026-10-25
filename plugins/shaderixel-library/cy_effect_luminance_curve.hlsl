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
    float3 p3 = frac(((float3)(p.xyx)) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash22(float2 p)
{
    float3 p3 = frac(((float3)(p.xyx)) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}
//@name Luminance Curve
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Luminance Curve","nextId":19,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 330],"text":"Strength","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0.00999999978,"name":"InputRange","op":"param","pos":[40, 440],"text":"Input Range","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":32,"min":0,"name":"OutputRange","op":"param","pos":[40, 550],"text":"Output Range","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"CurvePoint0","op":"param2","pos":[40, 660],"text":"Curve Point0","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"CurvePoint1","op":"param2","pos":[40, 770],"text":"Curve Point1","value":[0.333332986, 0.333332986, 0, 0]},{"id":9,"in":[],"index":1,"name":"CurvePoint2","op":"param2","pos":[40, 880],"text":"Curve Point2","value":[0.666666985, 0.666666985, 0, 0]},{"id":10,"in":[],"index":1,"name":"CurvePoint3","op":"param2","pos":[40, 990],"text":"Curve Point3","value":[1, 1, 0, 0]},{"id":11,"in":[],"index":1,"name":"CurveHandle0Out","op":"param2","pos":[40, 1100],"text":"Curve Handle0 Out","value":[0.111111, 0.111111, 0, 0]},{"id":12,"in":[],"index":1,"name":"CurveHandle1In","op":"param2","pos":[40, 1210],"text":"Curve Handle1 In","value":[0.222222, 0.222222, 0, 0]},{"id":13,"in":[],"index":1,"name":"CurveHandle1Out","op":"param2","pos":[40, 1320],"text":"Curve Handle1 Out","value":[0.444444001, 0.444444001, 0, 0]},{"id":14,"in":[],"index":1,"name":"CurveHandle2In","op":"param2","pos":[40, 1430],"text":"Curve Handle2 In","value":[0.555555999, 0.555555999, 0, 0]},{"id":15,"in":[],"index":1,"name":"CurveHandle2Out","op":"param2","pos":[40, 1540],"text":"Curve Handle2 Out","value":[0.777778029, 0.777778029, 0, 0]},{"id":16,"in":[],"index":1,"name":"CurveHandle3In","op":"param2","pos":[40, 1650],"text":"Curve Handle3 In","value":[0.888889015, 0.888889015, 0, 0]},{"id":17,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0]],"name":"Luminance Curve","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Strength",1],["InputRange",1],["OutputRange",1],["CurvePoint0",3],["CurvePoint1",3],["CurvePoint2",3],["CurvePoint3",3],["CurveHandle0Out",3],["CurveHandle1In",3],["CurveHandle1Out",3],["CurveHandle2In",3],["CurveHandle2Out",3],["CurveHandle3In",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float x = saturate(l / max(InputRange, 0.00001));\n    float2 p0 = CurvePoint0.xy, p1 = CurvePoint1.xy, p2 = CurvePoint2.xy, p3 = CurvePoint3.xy;\n    float2 a, b, c, d;\n    if (x <= p1.x) {\n        a = p0;\n        b = CurveHandle0Out.xy;\n        c = CurveHandle1In.xy;\n        d = p1;\n    } else if (x <= p2.x) {\n        a = p1;\n        b = CurveHandle1Out.xy;\n        c = CurveHandle2In.xy;\n        d = p2;\n    } else {\n        a = p2;\n        b = CurveHandle2Out.xy;\n        c = CurveHandle3In.xy;\n        d = p3;\n    }\n    float lo = 0, hi = 1, t = .5;\n    [unroll] for (int j = 0; j < 10; j++) {\n        t = (lo + hi) * .5;\n        float o = 1 - t;\n        float bx = o * o * o * a.x + 3 * o * o * t * b.x + 3 * o * t * t * c.x + t * t * t * d.x;\n        if (bx < x) lo = t;\n        else hi = t;\n    }\n    t = (lo + hi) * .5;\n    float o = 1 - t;\n    float y = o * o * o * a.y + 3 * o * o * t * b.y + 3 * o * t * t * c.y + t * t * t * d.y;\n    float mapped = y * OutputRange;\n    float3 curved = l > 0.00001 ? s.rgb * (mapped / l) : mapped.xxx;\n    return float4(lerp(s.rgb, curved, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":18,"in":[[17, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param sx_Strength float 1 0 1 "Strength"
//@param sx_InputRange float 1 0.01 32 "Input Range"
//@param sx_OutputRange float 1 0 32 "Output Range"
//@param sx_CurvePoint0 float2 0 0 0 1 "Curve Point0"
//@param sx_CurvePoint1 float2 0.333333 0.333333 0 1 "Curve Point1"
//@param sx_CurvePoint2 float2 0.666667 0.666667 0 1 "Curve Point2"
//@param sx_CurvePoint3 float2 1 1 0 1 "Curve Point3"
//@param sx_CurveHandle0Out float2 0.111111 0.111111 0 1 "Curve Handle0 Out"
//@param sx_CurveHandle1In float2 0.222222 0.222222 0 1 "Curve Handle1 In"
//@param sx_CurveHandle1Out float2 0.444444 0.444444 0 1 "Curve Handle1 Out"
//@param sx_CurveHandle2In float2 0.555556 0.555556 0 1 "Curve Handle2 In"
//@param sx_CurveHandle2Out float2 0.777778 0.777778 0 1 "Curve Handle2 Out"
//@param sx_CurveHandle3In float2 0.888889 0.888889 0 1 "Curve Handle3 In"
// Imported preset (Effect / Color & Tone): HDR-aware luminance remapping with a dedicated interactive four-point curve editor.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float sx_arg_Strength, float sx_arg_InputRange, float sx_arg_OutputRange, float3 sx_arg_CurvePoint0, float3 sx_arg_CurvePoint1, float3 sx_arg_CurvePoint2, float3 sx_arg_CurvePoint3, float3 sx_arg_CurveHandle0Out, float3 sx_arg_CurveHandle1In, float3 sx_arg_CurveHandle1Out, float3 sx_arg_CurveHandle2In, float3 sx_arg_CurveHandle2Out, float3 sx_arg_CurveHandle3In)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float x = saturate(l / max(sx_arg_InputRange, 0.00001));
    float2 p0 = sx_arg_CurvePoint0.xy, p1 = sx_arg_CurvePoint1.xy, p2 = sx_arg_CurvePoint2.xy, p3 = sx_arg_CurvePoint3.xy;
    float2 a, b, c, d;
    if (x <= p1.x) {
        a = p0;
        b = sx_arg_CurveHandle0Out.xy;
        c = sx_arg_CurveHandle1In.xy;
        d = p1;
    } else if (x <= p2.x) {
        a = p1;
        b = sx_arg_CurveHandle1Out.xy;
        c = sx_arg_CurveHandle2In.xy;
        d = p2;
    } else {
        a = p2;
        b = sx_arg_CurveHandle2Out.xy;
        c = sx_arg_CurveHandle3In.xy;
        d = p3;
    }
    float lo = 0, hi = 1, t = .5;
    [unroll] for (int j = 0; j < 10; j++) {
        t = (lo + hi) * .5;
        float o = 1 - t;
        float bx = o * o * o * a.x + 3 * o * o * t * b.x + 3 * o * t * t * c.x + t * t * t * d.x;
        if (bx < x) lo = t;
        else hi = t;
    }
    t = (lo + hi) * .5;
    float o = 1 - t;
    float y = o * o * o * a.y + 3 * o * o * t * b.y + 3 * o * t * t * c.y + t * t * t * d.y;
    float mapped = y * sx_arg_OutputRange;
    float3 curved = l > 0.00001 ? s.rgb * (mapped / l) : mapped.xxx;
    return float4(lerp(s.rgb, curved, saturate(sx_arg_Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), sx_Strength, sx_InputRange, sx_OutputRange, float3(sx_CurvePoint0, 0.0), float3(sx_CurvePoint1, 0.0), float3(sx_CurvePoint2, 0.0), float3(sx_CurvePoint3, 0.0), float3(sx_CurveHandle0Out, 0.0), float3(sx_CurveHandle1In, 0.0), float3(sx_CurveHandle1Out, 0.0), float3(sx_CurveHandle2In, 0.0), float3(sx_CurveHandle2Out, 0.0), float3(sx_CurveHandle3In, 0.0));
}

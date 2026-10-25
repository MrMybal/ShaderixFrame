//@param exposure float 0 -5 5 "Exposure"
//@param saturation float 1 0 2 "Saturation"
//@param contrast float 1 0 2 "Contrast"
float4 Effect(EffectContext ctx) {
    float4 c=InputUV(0,ctx.uv);
    c.rgb*=exp2(exposure);
    c.rgb=lerp(dot(c.rgb,float3(0.2126,0.7152,0.0722)).xxx,c.rgb,saturation);
    c.rgb=(c.rgb-0.5)*contrast+0.5;
    return float4(c.rgb,c.a);
}

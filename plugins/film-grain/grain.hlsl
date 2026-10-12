//@param amount float 0.06 0 0.3 "Grain"
float4 Effect(EffectContext ctx) {
    float4 c=InputUV(0,ctx.uv);
    float n=frac(sin(dot(ctx.px,float2(12.9898,78.233))+ctx.frame*37.12)*43758.5453)-0.5;
    return float4(saturate(c.rgb+n*amount),c.a);
}

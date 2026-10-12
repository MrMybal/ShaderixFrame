// Input 0: composition with the incoming clip; input 1: layers beneath it.
float4 Effect(EffectContext ctx) {
    return lerp(InputUV(1, ctx.uv), InputUV(0, ctx.uv), saturate(ctx.transitionProgress));
}

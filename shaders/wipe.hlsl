//@param transition_softness float 0.03 0 1 "Wipe feather"
//@param transition_direction int 0 0 3 "Direction (right/left/down/up)"
float4 Effect(EffectContext ctx) {
    float progress = saturate(ctx.transitionProgress);
    float4 before = InputUV(1, ctx.uv), after = InputUV(0, ctx.uv);
    if (progress <= 0) return before;
    if (progress >= 1) return after;
    float axis = transition_direction < 2 ? ctx.uv.x : ctx.uv.y;
    if (transition_direction == 1 || transition_direction == 3) axis = 1 - axis;
    float feather = max(transition_softness, transition_direction < 2 ? ctx.texel.x : ctx.texel.y);
    float coverage = 1 - smoothstep(progress - feather * 0.5, progress + feather * 0.5, axis);
    return lerp(before, after, coverage);
}

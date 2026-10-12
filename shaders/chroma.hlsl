//@param keyColor color 0 1 0 1 "Couleur clé"
//@param threshold float 0.3 0 1 "Seuil"
//@param softness float 0.1 0.001 0.5 "Adoucissement"
float4 Effect(EffectContext ctx) {
    float4 c=InputUV(0,ctx.uv);
    c.a*=smoothstep(threshold,threshold+softness,length(c.rgb-keyColor.rgb));
    return c;
}

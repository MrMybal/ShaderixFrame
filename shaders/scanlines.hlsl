//@param amount float 0.2 0 1 "Intensité"
//@param drift float 0.2 0 2 "Vitesse"
float4 Effect(EffectContext ctx) {
    float4 c=InputUV(0,ctx.uv);
    c.rgb*=1-amount*(0.5+0.5*sin(ctx.px.y*3.14159265+ctx.time*drift));
    return c;
}

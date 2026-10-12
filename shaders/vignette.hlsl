//@param amount float 0.5 0 1 "Intensité"
//@param radius float 0.65 0.1 1.4 "Rayon"
float4 Effect(EffectContext ctx) {
    float4 c=InputUV(0,ctx.uv);
    float2 p=(ctx.uv-0.5)*float2(ctx.size.x/ctx.size.y,1);
    c.rgb*=1-amount*smoothstep(radius*0.4,radius,length(p));
    return c;
}

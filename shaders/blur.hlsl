//@param radius float 3 0 30 "Rayon en pixels"
float4 Effect(EffectContext ctx) {
    float4 c=0;float weight=0;
    for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){float w=exp(-0.5*(x*x+y*y));c+=InputUV(0,ctx.uv+float2(x,y)*ctx.texel*radius)*w;weight+=w;}
    return c/weight;
}

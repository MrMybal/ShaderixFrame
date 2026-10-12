#include "render/Grading.h"
#include <iomanip>
#include <sstream>
namespace sf {
static std::string number(float x) {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(9) << std::showpoint << x;
    return s.str();
}
static std::string convert(const std::string &value, const std::string &from, const std::string &to) {
    if (from == to)
        return value;
    if (from == "logc3-awg3-ei800")
        return convert("mul(float3x3(1.617523,-.537287,-.080237,-.070573,1.334613,-.264040,-.021102,-.226954,"
                       "1.248056),sgLogDecode(" +
                           value + "))",
                       "linear709", to);
    if (to == "logc3-awg3-ei800")
        return "sgLogEncode(mul(float3x3(.63132127,.27080104,.09787833,.03682010,.79303706,.17014288,."
               "01736990,.14878930,.83384082)," +
               convert(value, from, "linear709") + "))";
    auto s = from == "srgb" ? "sgDecode(" + value + ")" : value;
    if (from == "linear2020" && to != "linear2020")
        s = "mul(float3x3(1.660491,-.587641,-.072850,-.124550,1.132900,-.008349,-.018151,-.100579,1.118730)"
            "," +
            s + ")";
    else if (from != "linear2020" && to == "linear2020")
        s = "mul(float3x3(.627404,.329283,.043313,.069097,.919540,.011362,.016391,.088013,.895595)," + s +
            ")";
    return to == "srgb" ? "sgEncode(" + s + ")" : s;
}
std::string gradeShader(const Effect &e, const std::string &workingSpace) {
    // Project/import validation owns the large LUT checks; shader generation only reads configuration.
    std::string code;
    for (auto &p : gradeControls())
        code += "//@param " + std::string(p.key) + " float " + number(p.value) + " " + number(p.minimum) +
                " " + number(p.maximum) + " \"" + p.label + "\"\n";
    code += R"(
Texture3D<float4> sfGradeLut:register(t5);
Texture1D<float4> sfGradeLut1:register(t6);
float sgLogD(float x){return x>.149658?(pow(10,(x-.385537)/.247190)-.052272)/5.555556:(x-.092809)/5.367655;}
float sgLogE(float x){return x>.010591?.247190*log10(5.555556*x+.052272)+.385537:5.367655*x+.092809;}
float3 sgLogDecode(float3 x){return float3(sgLogD(x.r),sgLogD(x.g),sgLogD(x.b));}
float3 sgLogEncode(float3 x){return float3(sgLogE(x.r),sgLogE(x.g),sgLogE(x.b));}
float3 sgDecode(float3 x){return float3(x.r<=.04045?x.r/12.92:pow((x.r+.055)/1.055,2.4),x.g<=.04045?x.g/12.92:pow((x.g+.055)/1.055,2.4),x.b<=.04045?x.b/12.92:pow((x.b+.055)/1.055,2.4));}
float3 sgEncode(float3 x){return float3(x.r<=.0031308?x.r*12.92:1.055*pow(x.r,1/2.4)-.055,x.g<=.0031308?x.g*12.92:1.055*pow(x.g,1/2.4)-.055,x.b<=.0031308?x.b*12.92:1.055*pow(x.b,1/2.4)-.055);}
float3 sgHSV(float3 c){float hi=max(c.r,max(c.g,c.b)),lo=min(c.r,min(c.g,c.b)),d=hi-lo,h=0;
if(d>1e-7)h=(hi==c.r?(c.g-c.b)/d:hi==c.g?2+(c.b-c.r)/d:4+(c.r-c.g)/d)/6;
return float3(frac(h),hi>1e-7?d/hi:0,hi);}
float3 sgRGB(float3 h){return h.z*lerp(1,saturate(abs(frac(h.x+float3(0,2.0/3,1.0/3))*6-3)-1),h.y);}
float sgBand(float x,float lo,float hi){if(lo>hi)return 0;return smoothstep(lo-qual_soft,lo,x)*(1-smoothstep(hi,hi+qual_soft,x));}
)";
    auto &curves = e.grading["curves"];
    for (auto name : {"master", "r", "g", "b", "hue_hue", "hue_sat", "hue_luma", "luma_sat"}) {
        auto &pts = curves[name];
        code += "float sgCurve_" + std::string(name) + "(float x){";
        if (pts.IsNull())
            code += "return " +
                    std::string(std::string(name) == "hue_hue"                     ? "0"
                                : std::string(name).find("_") != std::string::npos ? "1"
                                                                                   : "x") +
                    ";";
        else
            for (size_t i = 1; i < pts.Size(); ++i) {
                if (i + 1 < pts.Size())
                    code += "if(x<=" + number(pts.At(i).At(0).F()) + ")";
                code += "return lerp(" + number(pts.At(i - 1).At(1).F()) + "," + number(pts.At(i).At(1).F()) +
                        ",(x-" + number(pts.At(i - 1).At(0).F()) + ")/" +
                        number(pts.At(i).At(0).F() - pts.At(i - 1).At(0).F()) + ");";
            }
        code += "}\n";
    }
    code += R"(
float sgSelection(float3 c,float2 uv){float weight=1;
if(qualifier>.5){c=max(sgEncode(c),0);float3 h=sgHSV(c);float d=abs(h.x-hue);d=min(d,1-d);
float hi=max(c.r,max(c.g,c.b)),lo=min(c.r,min(c.g,c.b)),l=(hi+lo)*.5;
float s=l>0&&l<1?(hi-lo)/max(1e-6,1-abs(2*l-1)):0;
weight=(1-smoothstep(hue_width,hue_width+hue_soft,d))*sgBand(s,sat_min,sat_max)*sgBand(l,luma_min,luma_max);
if(qual_invert>.5)weight=1-weight;}
if(mask_mode>.5){float angle=mask_rotation*.01745329252;float2 q=uv-.5-float2(mask_x,mask_y);
q=mul(float2x2(cos(angle),sin(angle),-sin(angle),cos(angle)),q);float2 size=float2(mask_width,mask_height)*.5;float distance;
if(mask_mode<1.5)distance=max(abs(q.x)-size.x,abs(q.y)-size.y);
else if(mask_mode<2.5)distance=(length(q/size)-1)*min(size.x,size.y);
else{distance=10;bool inside=false;
)";
    auto &points = e.grading["maskPoints"];
    for (size_t i = 0; i < points.Size(); ++i) {
        auto &a = points.At(i), &b = points.At((i + 1) % points.Size());
        code += "{float2 a=float2(" + number(a.At(0).F()) + "," + number(a.At(1).F()) + ")*size*2,b=float2(" +
                number(b.At(0).F()) + "," + number(b.At(1).F()) +
                ")*size*2;float2 d=b-a;float "
                "t=saturate(dot(q-a,d)/"
                "max(1e-9,dot(d,d)));distance=min(distance,length(q-a-t*d));if((a.y>q.y)!=(b.y>q.y)&&q.x<(b."
                "x-a.x)*(q.y-a.y)/(b.y-a.y)+a.x)inside=!inside;}\n";
    }
    code += R"(if(inside)distance=-distance;}
float m=mask_feather>0?1-smoothstep(-mask_feather*.5,mask_feather*.5,distance):distance<=0?1:0;
if(mask_invert>.5)m=1-m;weight*=m;}return weight;}
float4 Effect(EffectContext ctx){float4 input=Input(0,ctx.px);
)";
    code += "float3 initial=" + convert("input.rgb", workingSpace, "linear709") + ",c=initial;\n";
    code += R"(
float weight=sgSelection(initial,ctx.uv);
if(matte>.5)return float4(weight.xxx,input.a);
if(intensity<=0)return input;
c*=float3(wb_r,wb_g,wb_b)*exp2(float3(temperature/6000+tint*.5,-tint*.5,-temperature/6000+tint*.5));
c*=exp2(exposure);c+=(c-pivot)*(contrast-1);
float l=dot(c,float3(.2126,.7152,.0722)),sh=1-smoothstep(0,.5,l),hi=smoothstep(.18,1,l),mid=max(0,1-sh-hi);
c+=blacks*sh+whites*hi+float3(shadow_r,shadow_g,shadow_b)*sh+float3(mid_r,mid_g,mid_b)*mid+float3(high_r,high_g,high_b)*hi+float3(offset_r,offset_g,offset_b);
l=dot(c,float3(.2126,.7152,.0722));float sat=saturation*(1+vibrance*(1-sgHSV(max(c,0)).y));c+=(c-l)*(sat-1);
c=float3(sgCurve_r(sgCurve_master(c.r)),sgCurve_g(sgCurve_master(c.g)),sgCurve_b(sgCurve_master(c.b)));
)";
    if (!curves["hue_hue"].IsNull() || !curves["hue_sat"].IsNull() || !curves["hue_luma"].IsNull())
        code += "float3 positive=max(c,0),h=sgHSV(positive);float "
                "old=h.x;h.x+=sgCurve_hue_hue(old);h.y*=sgCurve_hue_sat(old);h.z*=sgCurve_hue_luma(old);c+="
                "sgRGB(h)-positive;\n";
    code += "l=dot(c,float3(.2126,.7152,.0722));c+=(c-l)*(sgCurve_luma_sat(saturate(l))-1);\n";
    auto &lut = e.grading["lut"];
    if (!lut.IsNull()) {
        int n = lut["size"].I();
        code += "float3 coord=" + convert("c", "linear709", e.grading["lutInput"].S("linear709")) + ";\n";
        std::string lo = "float3(", span = "float3(";
        for (int i = 0; i < 3; ++i) {
            if (i) {
                lo += ",";
                span += ",";
            }
            lo += number(lut["min"].At(i).F());
            span += number(lut["max"].At(i).F() - lut["min"].At(i).F());
        }
        lo += ")";
        span += ")";
        // Explicit float interpolation avoids the sampler's fixed-point interpolation fractions.
        code += "coord=saturate((coord-" + lo + ")/" + span + ")*" + number(float(n - 1)) +
                ";int3 lower=(int3)floor(coord),upper=min(lower+1," + std::to_string(n - 1) +
                ");float3 fraction=coord-lower;\n";
        if (lut["dimension"].I() == 3)
            code += R"(
float3 z0=lerp(lerp(sfGradeLut.Load(int4(lower.x,lower.y,lower.z,0)).rgb,sfGradeLut.Load(int4(upper.x,lower.y,lower.z,0)).rgb,fraction.x),
               lerp(sfGradeLut.Load(int4(lower.x,upper.y,lower.z,0)).rgb,sfGradeLut.Load(int4(upper.x,upper.y,lower.z,0)).rgb,fraction.x),fraction.y);
float3 z1=lerp(lerp(sfGradeLut.Load(int4(lower.x,lower.y,upper.z,0)).rgb,sfGradeLut.Load(int4(upper.x,lower.y,upper.z,0)).rgb,fraction.x),
               lerp(sfGradeLut.Load(int4(lower.x,upper.y,upper.z,0)).rgb,sfGradeLut.Load(int4(upper.x,upper.y,upper.z,0)).rgb,fraction.x),fraction.y);
float3 mapped=lerp(z0,z1,fraction.z);
)";
        else
            code += "float3 "
                    "mapped=float3(lerp(sfGradeLut1.Load(int2(lower.x,0)).r,sfGradeLut1.Load(int2(upper.x,0))"
                    ".r,fraction.x),lerp(sfGradeLut1.Load(int2(lower.y,0)).g,sfGradeLut1.Load(int2(upper.y,0)"
                    ").g,fraction.y),lerp(sfGradeLut1.Load(int2(lower.z,0)).b,sfGradeLut1.Load(int2(upper.z,"
                    "0)).b,fraction.z));\n";
        code += "c=lerp(c," + convert("mapped", e.grading["lutOutput"].S("linear709"), "linear709") +
                ",lut_intensity);\n";
    }
    code += "c=initial+(c-initial)*(intensity*weight);return float4(" +
            convert("c", "linear709", workingSpace) + ",input.a);}\n";
    return code;
}
} // namespace sf

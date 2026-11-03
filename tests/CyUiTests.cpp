#include "ui/CodeFields.h"
#include "ui/ParameterCurve.h"
#include "ui/UiPreferences.h"
#include <cyui/modules/Nodes.h>
#include <cyui/modules/Inspector.h>
#include <imgui_internal.h>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);std::cout<<why<<"\n";}
int main(){
    auto* first=ImGui::CreateContext();
    try {
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,600};io.DeltaTime=1.f/60;
        sf::initializeUiPreferences({},1);
        cyui::loadFonts();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        cyui::setLocale("en");auto english=ImHashStr(cyui::trLabel("Save").c_str());
        cyui::setLocale("fr");require(ImHashStr(cyui::trLabel("Save").c_str())==english,"Live locale switches preserve widget identities");
        require(cyui::tr("Appearance")!="Appearance","Host catalogue is registered without changing English keys");
        cyui::setTheme("violet");const auto violet=cyui::palette().accent;
        auto* second=ImGui::CreateContext();ImGui::SetCurrentContext(second);cyui::Config config;config.themeKey="light-grey";config.density=cyui::Density::comfortable;config.dpiScale=1.5f;cyui::apply(config);
        require(cyui::density()==cyui::Density::comfortable&&std::abs(cyui::dp(20)-30)<.001f,"Comfortable metrics and DPI apply to the current context");
        ImGui::SetCurrentContext(first);require(cyui::palette().accent==violet&&cyui::locale()=="fr"&&cyui::dpi()==1,"Theme, locale and DPI remain isolated between contexts");
        ImGui::DestroyContext(second);ImGui::SetCurrentContext(first);
        sf::Parameter parameter;parameter.value=.3f;parameter.keys={{0,.2f,sf::Curve::Shaped,.7f},{1,.8f,sf::Curve::Smooth,0},{2,.4f,sf::Curve::Hold,0},{3,.9f,sf::Curve::Linear,0}};
        std::vector<cyui::CurvePoint> points;for(size_t i=0;i<parameter.keys.size();++i)points.push_back({i+1,parameter.keys[i].time,parameter.keys[i].value});
        auto roundtrip=sf::curveParameter(parameter,{points.data(),points.size()});
        bool exact=true;for(int i=0;i<301;++i)exact&=std::abs(parameter.at(i/100.)-roundtrip.at(i/100.))<1e-7;require(exact,"Host interpolation survives common curve proposals");
        cyui::CodeEditor code;std::string source="float4 Effect() {\r\n\treturn 1;\r\n}";code.setText(source);code.setLanguage(cyui::CodeLanguage::hlsl);require(code.text()==source,"Code editor preserves CRLF and an absent final newline");
        auto tokens=cyui::highlightCode(source,cyui::CodeLanguage::hlsl);require(!tokens.empty(),"Shared HLSL syntax backend is linked and operational");
        cyui::NodeState nodes;nodes.dragging=true;nodes.revision=1;nodes.before={{7,{2,3}}};nodes.preview={{7,{12,13}}};
        cyui::NodeOptions options;options.revision=2;std::vector<cyui::Node> model{{7,{2,3},"Host node"}};
        ImGui::NewFrame();ImGui::Begin("Integration");
        auto cancelled=cyui::nodeEditor("nodes",nodes,model,{}, {400,200},options);
        require(cancelled.moved.phase==cyui::EditPhase::cancel&&!nodes.dragging&&model[0].position.x==2,"External revisions cancel node drags without mutating host data");
        cyui::CurveState curve;curve.dragging=true;curve.revision=1;cyui::CurveOptions curveOptions;curveOptions.revision=2;
        auto cancelledCurve=cyui::curveEditor("curve",curve,points,{400,120},curveOptions);
        require(cancelledCurve.phase==cyui::EditPhase::cancel&&!curve.dragging,"External revisions cancel curve drags");
        ImGui::End();ImGui::Render();
        ImGui::DestroyContext(first);return 0;
    }catch(const std::exception& e){std::cout<<"Exception: "<<e.what()<<"\n"<<std::flush;ImGui::DestroyContext(first);return 1;}
}

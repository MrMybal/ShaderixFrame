#include "ui/UiPreferences.h"
#include "ui/Translations.h"
#include "core/AtomicFile.h"
#include "core/Json.h"
#include <cyui/Core.h>
#include <fstream>
namespace sf {
namespace {
std::filesystem::path preferencesFile;
void save() {
    if(preferencesFile.empty())return;
    sx::Json data=sx::Json::Object();data["theme"]=cyui::currentThemeKey();
    data["density"]=cyui::density()==cyui::Density::compact?"compact":"comfortable";data["locale"]=cyui::locale();
    try {AtomicFile file(sx::Narrow(preferencesFile.wstring()));{std::ofstream stream(file.path(),std::ios::binary);stream<<data.Dump(2);if(!stream)return;}file.commit();}
    catch(const std::exception& e){cyui::notify(e.what(),cyui::Tone::warning);}
}
}
void initializeUiPreferences(const std::filesystem::path& file,float dpi) {
    preferencesFile=file;registerUiTranslations();
    cyui::Config config;config.squareWindows=true;config.dpiScale=dpi;
    std::string theme="mid-grey",locale="en",density="compact",text;
    if(!file.empty()&&sx::ReadTextFile(sx::Narrow(file.wstring()),text)) {
        sx::Json data;if(sx::Json::Parse(text,data)) {theme=data["theme"].S("mid-grey");locale=data["locale"].S("en");density=data["density"].S("compact");}
    }
    config.themeKey=theme.c_str();config.density=density=="comfortable"?cyui::Density::comfortable:cyui::Density::compact;
    cyui::apply(config);cyui::setLocale(locale.c_str());
}
void uiPreferencesMenu() {
    if(ImGui::BeginMenu(cyui::trLabel("Appearance").c_str())) {
        if(ImGui::BeginMenu(cyui::trLabel("Theme").c_str())) {
            for(auto& theme:cyui::builtInThemes())if(ImGui::MenuItem(cyui::trLabel(theme.name.c_str()).c_str(),nullptr,cyui::currentThemeKey()==theme.key)){cyui::setTheme(theme.key.c_str());save();}
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(cyui::trLabel("Density").c_str())) {
            for(auto density:{cyui::Density::compact,cyui::Density::comfortable})if(ImGui::MenuItem(cyui::trLabel(density==cyui::Density::compact?"Compact":"Comfortable").c_str(),nullptr,cyui::density()==density)){cyui::setDensity(density);save();}
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(cyui::trLabel("Language").c_str())) {
            for(auto& locale:cyui::availableLocales())if(ImGui::MenuItem(locale.name.c_str(),nullptr,cyui::locale()==locale.code)){cyui::setLocale(locale.code.c_str());save();}
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
}
}

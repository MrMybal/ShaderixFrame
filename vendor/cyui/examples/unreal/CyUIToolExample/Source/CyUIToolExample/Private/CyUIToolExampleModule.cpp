#include "CyUIModule.h"
#include "SCyUIPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "cyui/Core.h"
#if defined(CYUI_HAS_GALLERY)
#include "cyui/Gallery.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace {
const FName ExampleTabId("CyUIToolExample.Panel");
}

// A consuming plugin owns its tab, menu, state and callback lifetime.
class FCyUIToolExampleModule : public IModuleInterface {
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCyUIConsumerPluginTest;
#endif
  public:
    void StartupModule() override {
        if (IsRunningCommandlet())
            return;
        FCyUIModule::Get();
        cyui::registerTranslations(
            "fr", {{"CyUI inside another plugin", "CyUI dans un autre plugin"},
                   {"This callback and its state belong to CyUIToolExample.",
                    "Le plugin utilisateur gère ce rappel et son état."},
                   {"Strength", "Intensité"},
                   {"Run tool", "Exécuter l’outil"},
                   {"Open gallery", "Ouvrir la galerie"},
                   {"Consuming plugin action received", "Action du plugin utilisateur reçue"}});
        FGlobalTabmanager::Get()
            ->RegisterNomadTabSpawner(ExampleTabId,
                                      FOnSpawnTab::CreateRaw(this, &FCyUIToolExampleModule::SpawnTab))
            .SetDisplayName(NSLOCTEXT("CyUIToolExample", "Title", "Plugin using CyUI"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCyUIToolExampleModule::RegisterMenus));
    }
    void ShutdownModule() override {
        if (IsRunningCommandlet())
            return;
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        for (auto &Weak : Panels)
            if (auto Panel = Weak.Pin())
                Panel->Detach();
        if (auto Tab = ActiveTab.Pin())
            Tab->RequestCloseTab();
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExampleTabId);
        Panels.Empty();
    }

  private:
    TArray<TWeakPtr<SCyUIPanel>> Panels;
    TWeakPtr<SDockTab> ActiveTab;
    char Name[128] = "My plugin tool";
    float Strength = .5f;
    bool Enabled = true;
    bool ShowGallery = false;
    cyui::TextFilter Filter;
    int DrawCalls = 0;
    void RegisterMenus() {
        FToolMenuOwnerScoped Owner(this);
        auto &Section =
            UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window")->AddSection("CyUIToolExample");
        Section.AddMenuEntry("OpenCyUIConsumer",
                             NSLOCTEXT("CyUIToolExample", "Open", "CyUI Consumer Example"), FText::GetEmpty(),
                             FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([] {
                                 FGlobalTabmanager::Get()->TryInvokeTab(ExampleTabId);
                             })));
    }
    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs &) {
        auto Panel = StaticCastSharedRef<SCyUIPanel>(
            FCyUIModule::Get().MakePanel(FOnDrawCyUITool::CreateRaw(this, &FCyUIToolExampleModule::Draw)));
        Panels.RemoveAll([](const auto &Weak) { return !Weak.IsValid(); });
        Panels.Add(Panel);
        // The rest of this plugin's layout remains ordinary Slate.
        auto Tab = SNew(SDockTab).TabRole(
            ETabRole::NomadTab)[SNew(SVerticalBox) +
                                SVerticalBox::Slot().AutoHeight()
                                    [SNew(STextBlock)
                                         .Text(NSLOCTEXT("CyUIToolExample", "Slate",
                                                         "Slate layout owned by the consuming plugin"))] +
                                SVerticalBox::Slot().FillHeight(1)[Panel]];
        ActiveTab = Tab;
        return Tab;
    }
    void Draw() {
        ++DrawCalls;
        int Language = cyui::locale() == "fr" ? 1 : 0;
        const char *Languages[]{"English", "Français"};
        if (cyui::combo("##language", &Language, Languages, 2))
            cyui::setLocale(Language == 1 ? "fr" : "en");
        const auto Subtitle = cyui::tr("This callback and its state belong to CyUIToolExample.");
        cyui::panelHeader(cyui::tr("CyUI inside another plugin").c_str(), Subtitle.c_str(), &Filter);
        if (cyui::beginProperties("Plugin properties")) {
            if (Filter.matches("Name")) {
                cyui::property(cyui::tr("Name").c_str());
                cyui::textField("##name", Name, sizeof(Name));
            }
            if (Filter.matches("Enabled")) {
                cyui::property(cyui::tr("Enabled").c_str());
                cyui::toggleSwitch("##enabled", &Enabled);
            }
            if (Filter.matches("Strength")) {
                cyui::property(cyui::tr("Strength").c_str());
                cyui::sliderBar("##strength", &Strength, 0, 1);
            }
            cyui::endProperties();
        }
        if (cyui::primaryButton(cyui::trLabel("Run tool").c_str()))
            cyui::notify(cyui::tr("Consuming plugin action received").c_str(), cyui::Tone::success);
#if defined(CYUI_HAS_GALLERY)
        if (cyui::ghostButton(cyui::trLabel("Open gallery").c_str()))
            ShowGallery = true;
        if (ShowGallery)
            cyui::showGallery(&ShowGallery);
#endif
    }
};
IMPLEMENT_MODULE(FCyUIToolExampleModule, CyUIToolExample)

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyUIConsumerPluginTest, "CyUI.ConsumerPlugin.EmbeddedPanel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyUIConsumerPluginTest::RunTest(const FString &) {
    TestTrue(TEXT("Shared CyUI service module is loaded"), FCyUIModule::IsAvailable());
    TestFalse(TEXT("Shared module has no built-in demo"), FCyUIModule::Get().OpenTool("CyUI.Example"));
    auto &Consumer = FModuleManager::LoadModuleChecked<FCyUIToolExampleModule>("CyUIToolExample");
    const int BeforeDraw = Consumer.DrawCalls;
    auto *Previous = ImGui::GetCurrentContext();
    auto Tab = FGlobalTabmanager::Get()->TryInvokeTab(ExampleTabId);
    TestTrue(TEXT("Another plugin embeds a CyUI panel in its own tab"), Tab.IsValid());
    TestTrue(TEXT("Consumer panel restores the calling context"), Previous == ImGui::GetCurrentContext());
    if (!Tab)
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDelayedFunctionLatentCommand>(
        [this, Tab, &Consumer, BeforeDraw, Previous] {
            TArray<FColor> Pixels;
            FIntVector Size;
            TestTrue(TEXT("Consuming plugin's Slate layout renders"),
                     FSlateApplication::Get().TakeScreenshot(Tab->GetContent(), Pixels, Size));
            TestTrue(TEXT("Draw callback executes in the consuming plugin DLL"),
                     Consumer.DrawCalls > BeforeDraw);
            TestTrue(TEXT("Cross-plugin draw restores the calling context"),
                     Previous == ImGui::GetCurrentContext());
            Tab->RequestCloseTab();
        },
        .25f));
    return true;
}
#endif

using UnrealBuildTool;

public class CyUIToolExample : ModuleRules
{
    public CyUIToolExample(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CyUI", "Slate", "SlateCore", "ToolMenus" });
    }
}

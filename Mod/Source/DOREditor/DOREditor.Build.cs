using UnrealBuildTool;

public class DOREditor : ModuleRules
{
    public DOREditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "FSD",
            "UMG"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetRegistry",
            "BlueprintGraph",
            "InputCore",
            "Json",
            "JsonUtilities",
            "Kismet",
            "KismetCompiler",
            "Slate",
            "SlateCore",
            "UMGEditor",
            "UnrealEd"
        });
    }
}

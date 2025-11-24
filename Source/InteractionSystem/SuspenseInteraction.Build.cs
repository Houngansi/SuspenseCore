using UnrealBuildTool;

public class SuspenseInteraction : ModuleRules
{
    public SuspenseInteraction(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "DeveloperSettings",
                "GameplayTags",
                "GameplayAbilities",
                "MedComShared"  // TODO: Update to SuspenseShared when available
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Niagara"
            }
        );
    }
}

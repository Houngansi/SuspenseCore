using UnrealBuildTool;

public class MedComShared : ModuleRules
{
    public MedComShared(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",
                "GameplayTags",
                "UMG",
                "Niagara",
                "PhysicsCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );
    }
}
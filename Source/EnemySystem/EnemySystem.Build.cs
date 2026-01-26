using UnrealBuildTool;

public class EnemySystem : ModuleRules
{
    public EnemySystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                "EnemySystem/Public"
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                "EnemySystem/Private"
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "AIModule",
                "NavigationSystem",
                "GameplayTasks",
                "GameplayAbilities",
                "GameplayTags",
                "BridgeSystem",
                "GAS"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
            }
        );
    }
}

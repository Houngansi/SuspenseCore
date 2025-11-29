using UnrealBuildTool;

public class PlayerCore : ModuleRules
{
    public PlayerCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",  // For FKey::ToString()
                "CinematicCamera",
                "EnhancedInput",
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",
                "BridgeSystem",
                "GAS",
                "InventorySystem",
                "EquipmentSystem",
                "UISystem",
                "InteractionSystem"
            }
        );

        // UMG for widgets
        PrivateIncludePathModuleNames.Add("UMG");
        PrivateDependencyModuleNames.Add("UMG");

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
            }
        );
    }
}

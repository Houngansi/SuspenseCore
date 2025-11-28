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
                "CinematicCamera",
                "GameplayAbilities",
                "BridgeSystem",
                "InventorySystem",
                "EquipmentSystem",
                "UISystem",
                "GAS"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore", "UISystem", "EnhancedInput"
            }
        );
    }
}

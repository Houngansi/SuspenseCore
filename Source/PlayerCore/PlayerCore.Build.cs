using UnrealBuildTool;

public class PlayerCore : ModuleRules
{
    // ═══════════════════════════════════════════════════════════════════════════════
    // MODULE FEATURE FLAGS
    // Set to true to enable optional module dependencies
    // ═══════════════════════════════════════════════════════════════════════════════

    private static readonly bool bWithEquipmentSystem = false;
    private static readonly bool bWithInventorySystem = false;
    private static readonly bool bWithInteractionSystem = false;

    public PlayerCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // ═══════════════════════════════════════════════════════════════════════════════
        // CORE DEPENDENCIES (Always required)
        // ═══════════════════════════════════════════════════════════════════════════════

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
                "GAS"
            }
        );

        // ═══════════════════════════════════════════════════════════════════════════════
        // OPTIONAL MODULE DEPENDENCIES
        // ═══════════════════════════════════════════════════════════════════════════════

        if (bWithEquipmentSystem)
        {
            PublicDependencyModuleNames.Add("EquipmentSystem");
            PublicDefinitions.Add("WITH_EQUIPMENT_SYSTEM=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_EQUIPMENT_SYSTEM=0");
        }

        if (bWithInventorySystem)
        {
            PublicDependencyModuleNames.Add("InventorySystem");
            PublicDefinitions.Add("WITH_INVENTORY_SYSTEM=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_INVENTORY_SYSTEM=0");
        }

        if (bWithInteractionSystem)
        {
            PublicDependencyModuleNames.Add("InteractionSystem");
            PublicDefinitions.Add("WITH_INTERACTION_SYSTEM=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_INTERACTION_SYSTEM=0");
        }

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

using UnrealBuildTool;

public class UISystem : ModuleRules
{
    public UISystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        bEnableExceptions = true;

        // Additional flags for Windows
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("_HAS_EXCEPTIONS=1");
        }

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "GameplayTags",
                "GameplayAbilities",
                "BridgeSystem",      // Migrated from MedComShared
                "InputCore",
                "UMG",
                "EquipmentSystem",   // For Equipment UI Bridge
                "InventorySystem"    // For Inventory UI Bridge
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
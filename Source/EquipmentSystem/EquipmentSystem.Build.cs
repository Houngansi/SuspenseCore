// Copyright Suspense Team. All Rights Reserved.

using UnrealBuildTool;

public class EquipmentSystem : ModuleRules
{
    public EquipmentSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GameplayAbilities",  // GAS integration
                "GameplayTags",       // Tag system
                "GameplayTasks",      // Task system

                // Suspense modules
                "BridgeSystem",
                "GAS",
                "PlayerCore"  // For CameraShake system
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "InputCore",
                "NetCore",
                "Niagara",
                "Json",
                "JsonUtilities",
                "OnlineSubsystem",
                "OnlineSubsystemUtils"
            }
        );
    }
}
// Copyright Suspense Team. All Rights Reserved.

using UnrealBuildTool;

public class BridgeSystem : ModuleRules
{
    public BridgeSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "DeveloperSettings",
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",
                "Niagara",
                "UMG",
                "PhysicsCore",
                "NetCore",
                "ReplicationGraph",
                // Equipment system for EquipmentDataStore access in UIProvider
                "EquipmentSystem"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "Json",
                "JsonUtilities"
            }
        );
    }
}

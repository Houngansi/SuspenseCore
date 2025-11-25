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
                "BridgeSystem",       // Migrated from MedComShared
                "GAS"                 // Migrated from MedComGAS
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "InputCore",          // Input bindings
                "NetCore",            // Network core
                "IrisCore",           // Replication
                "Niagara",            // VFX system
                "Json",               // JSON serialization
                "JsonUtilities",      // JSON utilities
                "OnlineSubsystem",    // Online features
                "OnlineSubsystemUtils" // Online utilities
            }
        );
    }
}
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
                "GameplayAbilities",  // For GAS integration
                "GameplayTags",       // Tag system
                "GameplayTasks",      // Task system
                "UMG",                // UI widgets
                "Niagara",            // VFX system
                "PhysicsCore"         // Physics types
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "Json",               // JSON serialization
                "JsonUtilities"       // JSON utilities
            }
        );
    }
}
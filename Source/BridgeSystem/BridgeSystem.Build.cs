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
                "DeveloperSettings",  // For USuspenseCoreProjectSettings
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",
                "Niagara",
                "UMG",
                "PhysicsCore"
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
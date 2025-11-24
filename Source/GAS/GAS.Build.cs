// Copyright Houngansi. All Rights Reserved.

using UnrealBuildTool;

public class GAS : ModuleRules
{
    public GAS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "SuspenseCore",

                // Gameplay Ability System dependencies
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "NetCore",           // For replication
                "Slate",
                "SlateCore"
            }
        );
    }
}
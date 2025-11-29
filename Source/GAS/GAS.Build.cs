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

                // Gameplay Ability System dependencies
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",

                // Clean Architecture - EventBus and Services
                "BridgeSystem"
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

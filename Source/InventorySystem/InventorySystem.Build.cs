// Copyright Suspense Team. All Rights Reserved.

using UnrealBuildTool;

public class InventorySystem : ModuleRules
{
	public InventorySystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",

				// Suspense modules
				"BridgeSystem",
				"InteractionSystem"    // Migrated from MedComInteraction
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"NetCore",               // For replication support
				"GameplayAbilities",     // GAS integration
				"Json",                  // JSON serialization
				"JsonUtilities"          // JSON utilities
			}
		);
	}
}

// SuspenseCore.Build.cs
// Main SuspenseCore module - Re-exports core interfaces
// Copyright Suspense Team. All Rights Reserved.

using UnrealBuildTool;

public class SuspenseCore : ModuleRules
{
	public SuspenseCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"GameplayAbilities",

				// SuspenseCore plugin modules - re-exported
				"BridgeSystem",
				"GAS",
				"PlayerCore",
				"InteractionSystem"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
			}
		);
	}
}

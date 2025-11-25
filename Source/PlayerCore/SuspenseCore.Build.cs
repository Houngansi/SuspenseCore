// Copyright Suspense Team. All Rights Reserved.

using UnrealBuildTool;

public class SuspenseCore : ModuleRules
{
	public SuspenseCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"EnhancedInput",
				"InputCore",
				"MedComGAS",               // TODO: Update to SuspenseGAS when available
				"MedComShared",            // TODO: Update to SuspenseShared when available
				"MedComInteraction",       // TODO: Update to SuspenseInteraction when available
				"MedComInventory",         // TODO: Update to SuspenseInventory when available
				"MedComEquipment",         // TODO: Update to SuspenseEquipment when available
				"MedComUI",                // TODO: Update to SuspenseUI when available
				"CinematicCamera",

				// UI modules
				"UMG",          // Required for UUserWidget and CreateWidget
				"Slate",        // Slate types
				"SlateCore"     // FSlateColor and other types
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}

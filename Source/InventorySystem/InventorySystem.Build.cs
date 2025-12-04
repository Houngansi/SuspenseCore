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
				"BridgeSystem",
				"InteractionSystem",
				"SuspenseCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"NetCore",
				"GameplayAbilities",
				"Json",
				"JsonUtilities"
			}
		);
	}
}

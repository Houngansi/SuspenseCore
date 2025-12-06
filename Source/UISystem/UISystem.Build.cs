using UnrealBuildTool;

public class UISystem : ModuleRules
{
	public UISystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				"GameplayAbilities",
				"BridgeSystem",   // SuspenseCore UI architecture hub
				"InputCore",
				"UMG"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			}
		);

		// NOTE: SuspenseCore/ widgets do NOT depend on InventorySystem, EquipmentSystem, GAS directly
		// They use ISuspenseCoreUIDataProvider interface from BridgeSystem
		// Legacy widgets in Widgets/ folder may still need these (to be removed later)
	}
}

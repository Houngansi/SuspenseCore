// Plugins/MedComCore/Source/MedComEquipment/MedComEquipment.Build.cs
using UnrealBuildTool;

public class MedComEquipment : ModuleRules
{
	public MedComEquipment(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"MedComShared",
			"MedComGAS"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"NetCore",
			"IrisCore",
			"Niagara",
			"Json",
			"JsonUtilities",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});
	}
}
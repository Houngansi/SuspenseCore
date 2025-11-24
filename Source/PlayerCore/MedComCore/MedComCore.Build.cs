// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MedComCore : ModuleRules
{
	public MedComCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
        
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
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
				"MedComGAS",
				"MedComShared",
				"MedComInteraction",
				"MedComInventory",
				"MedComEquipment",
				"MedComUI",
				"CinematicCamera",
                
				// ДОБАВЛЕННЫЕ МОДУЛИ ДЛЯ РАБОТЫ С UI
				"UMG",          // Необходим для UUserWidget и CreateWidget
				"Slate",        // Перемещаем в публичные для доступа к SlateCore типам
				"SlateCore"     // Перемещаем в публичные для FSlateColor и других типов
			}
		);
        
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				// Slate и SlateCore перемещены в PublicDependencyModuleNames
				// так как они могут понадобиться в заголовочных файлах
			}
		);
        
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
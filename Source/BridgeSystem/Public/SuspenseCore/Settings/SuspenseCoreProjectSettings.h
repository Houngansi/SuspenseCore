// SuspenseCoreProjectSettings.h
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SuspenseCoreProjectSettings.generated.h"

/**
 * USuspenseCoreProjectSettings
 *
 * Centralized project settings for SuspenseCore.
 * Access via Project Settings → Game → SuspenseCore
 *
 * USAGE:
 *   const USuspenseCoreProjectSettings* Settings = GetDefault<USuspenseCoreProjectSettings>();
 *   FString Path = Settings->CharacterClassAssetPath.Path;
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "SuspenseCore"))
class BRIDGESYSTEM_API USuspenseCoreProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USuspenseCoreProjectSettings();

	// ═══════════════════════════════════════════════════════════════════════════
	// CHARACTER CLASSES
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Path to character class Data Assets folder.
	 * Example: /Game/Blueprints/Core/Data
	 *
	 * Data Assets in this folder must be of type SuspenseCoreCharacterClassData.
	 * The system will scan this folder and load all CharacterClass assets.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Character Classes", meta = (
		ContentDir,
		DisplayName = "Character Class Assets Path",
		ToolTip = "Path to folder containing CharacterClass Data Assets (e.g., /Game/Blueprints/Core/Data)"
	))
	FDirectoryPath CharacterClassAssetPath;

	/**
	 * Primary Asset Type name for character classes.
	 * Used by Asset Manager to identify CharacterClass assets.
	 * Default: "CharacterClass"
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Character Classes", meta = (
		DisplayName = "Character Class Asset Type",
		ToolTip = "Primary Asset Type name used by Asset Manager"
	))
	FString CharacterClassAssetType = TEXT("CharacterClass");

	/**
	 * If true, logs detailed information about class loading.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Character Classes|Debug")
	bool bLogClassLoading = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// MAPS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Main menu map name.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath MainMenuMap;

	/**
	 * Character selection map name.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath CharacterSelectMap;

	/**
	 * Default gameplay map name.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath DefaultGameMap;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATIC ACCESSOR
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get project settings instance.
	 */
	static const USuspenseCoreProjectSettings* Get()
	{
		return GetDefault<USuspenseCoreProjectSettings>();
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// UDeveloperSettings Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual FName GetCategoryName() const override { return FName("Game"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("SuspenseCore", "SettingsSection", "SuspenseCore"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("SuspenseCore", "SettingsDesc", "Configure SuspenseCore plugin settings"); }
#endif
};

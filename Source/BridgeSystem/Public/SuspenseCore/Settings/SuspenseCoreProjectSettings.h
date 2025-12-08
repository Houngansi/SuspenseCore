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
	// MAPS & NAVIGATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Lobby map - main hub for players with existing characters.
	 * Shows: current character 3D model, progression, Play/Operators/Settings buttons.
	 * Player returns here after gameplay.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath LobbyMap;

	/**
	 * Character selection map - for creating/switching characters.
	 * Shows: character preview, class selection, character list.
	 * New players start here, existing players can access via "Operators" button.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath CharacterSelectMap;

	/**
	 * Default gameplay map.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath DefaultGameMap;

	// ═══════════════════════════════════════════════════════════════════════════
	// GAME FLOW
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * If true, new players (no saved characters) go to CharacterSelectMap.
	 * If false, all players start on LobbyMap.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Game Flow", meta = (
		DisplayName = "New Players Start at Character Select",
		ToolTip = "New players without saved characters will be sent to CharacterSelectMap"
	))
	bool bNewPlayersToCharacterSelect = true;

	/**
	 * Map to return to when exiting gameplay (pause menu "Exit to Lobby").
	 * Usually LobbyMap.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Game Flow", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath ExitToLobbyMap;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPER METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get lobby map name as FName */
	FName GetLobbyMapName() const;

	/** Get character select map name as FName */
	FName GetCharacterSelectMapName() const;

	/** Get default game map name as FName */
	FName GetDefaultGameMapName() const;

	/** Get exit to lobby map name as FName */
	FName GetExitToLobbyMapName() const;

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


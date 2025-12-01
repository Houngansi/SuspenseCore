// SuspenseCoreProjectSettings.cpp
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Settings/SuspenseCoreProjectSettings.h"

USuspenseCoreProjectSettings::USuspenseCoreProjectSettings()
{
	// Default paths - user should configure in Project Settings
	CharacterClassAssetPath.Path = TEXT("/Game/Blueprints/Core/Data");
	CharacterClassAssetType = TEXT("CharacterClass");
	bLogClassLoading = true;

	// Default maps - user should configure in Project Settings
	// These are placeholder names that should be overridden
	bNewPlayersToCharacterSelect = true;
}

FName USuspenseCoreProjectSettings::GetLobbyMapName() const
{
	if (LobbyMap.IsValid())
	{
		// Extract map name from path: /Game/Maps/LobbyMap.LobbyMap -> LobbyMap
		FString MapPath = LobbyMap.GetAssetName();
		return FName(*MapPath);
	}
	return FName("LobbyMap");
}

FName USuspenseCoreProjectSettings::GetCharacterSelectMapName() const
{
	if (CharacterSelectMap.IsValid())
	{
		FString MapPath = CharacterSelectMap.GetAssetName();
		return FName(*MapPath);
	}
	return FName("CharacterSelectMap");
}

FName USuspenseCoreProjectSettings::GetDefaultGameMapName() const
{
	if (DefaultGameMap.IsValid())
	{
		FString MapPath = DefaultGameMap.GetAssetName();
		return FName(*MapPath);
	}
	return FName("GameMap");
}

FName USuspenseCoreProjectSettings::GetExitToLobbyMapName() const
{
	if (ExitToLobbyMap.IsValid())
	{
		FString MapPath = ExitToLobbyMap.GetAssetName();
		return FName(*MapPath);
	}
	// Fallback to LobbyMap if not set
	return GetLobbyMapName();
}

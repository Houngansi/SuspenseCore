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
}

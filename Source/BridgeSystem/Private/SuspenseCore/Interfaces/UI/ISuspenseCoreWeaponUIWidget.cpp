// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/UI/ISuspenseCoreWeaponUIWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseWeaponUIWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
}

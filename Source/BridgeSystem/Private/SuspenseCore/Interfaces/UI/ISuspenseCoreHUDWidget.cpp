// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/UI/ISuspenseCoreHUDWidget.h"
#include "SuspenseCore/Delegates/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseHUDWidget::GetDelegateManagerStatic(const UObject* WorldContextObject)
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
    
	return GameInstance->GetSubsystem<USuspenseEventManager>();
}
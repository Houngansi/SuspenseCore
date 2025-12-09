// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/UI/ISuspenseCoreCrosshairWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseCoreCrosshairWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseCoreCrosshairWidgetInterface::BroadcastCrosshairUpdated(const UObject* Widget, float Spread, float Recoil)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// Manager->NotifyCrosshairUpdated(Spread, Recoil);
	}
}

void ISuspenseCoreCrosshairWidgetInterface::BroadcastCrosshairColorChanged(const UObject* Widget, const FLinearColor& NewColor)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// Manager->NotifyCrosshairColorChanged(NewColor);
	}
}
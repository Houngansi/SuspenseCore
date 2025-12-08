// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseCrosshairWidget.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseCrosshairWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseCrosshairWidgetInterface::BroadcastCrosshairUpdated(const UObject* Widget, float Spread, float Recoil)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		Manager->NotifyCrosshairUpdated(Spread, Recoil);
	}
}

void ISuspenseCrosshairWidgetInterface::BroadcastCrosshairColorChanged(const UObject* Widget, const FLinearColor& NewColor)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		Manager->NotifyCrosshairColorChanged(NewColor);
	}
}
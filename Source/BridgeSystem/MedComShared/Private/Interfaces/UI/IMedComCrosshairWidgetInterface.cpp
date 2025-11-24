// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComCrosshairWidgetInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComCrosshairWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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
    
	return GameInstance->GetSubsystem<UEventDelegateManager>();
}

void IMedComCrosshairWidgetInterface::BroadcastCrosshairUpdated(const UObject* Widget, float Spread, float Recoil)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		Manager->NotifyCrosshairUpdated(Spread, Recoil);
	}
}

void IMedComCrosshairWidgetInterface::BroadcastCrosshairColorChanged(const UObject* Widget, const FLinearColor& NewColor)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		Manager->NotifyCrosshairColorChanged(NewColor);
	}
}
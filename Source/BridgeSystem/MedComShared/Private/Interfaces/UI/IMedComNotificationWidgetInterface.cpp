// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComNotificationWidgetInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComNotificationWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComNotificationWidgetInterface::BroadcastNotification(const UObject* Widget, const FString& Message, float Duration)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		Manager->NotifyUI(Message, Duration);
	}
}
// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseNotificationWidget.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseNotificationWidget::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseNotificationWidget::BroadcastNotification(const UObject* Widget, const FString& Message, float Duration)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		Manager->NotifyUI(Message, Duration);
	}
}
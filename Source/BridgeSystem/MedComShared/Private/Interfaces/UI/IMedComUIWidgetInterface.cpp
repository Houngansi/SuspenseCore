// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComUIWidgetInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"

UEventDelegateManager* IMedComUIWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComUIWidgetInterface::BroadcastWidgetCreated(const UObject* Widget)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		// Cast to UUserWidget for the notification
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			Manager->NotifyUIWidgetCreated(const_cast<UUserWidget*>(UserWidget));
		}
	}
}

void IMedComUIWidgetInterface::BroadcastWidgetDestroyed(const UObject* Widget)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			Manager->NotifyUIWidgetDestroyed(const_cast<UUserWidget*>(UserWidget));
		}
	}
}

void IMedComUIWidgetInterface::BroadcastVisibilityChanged(const UObject* Widget, bool bIsVisible)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			Manager->NotifyUIVisibilityChanged(const_cast<UUserWidget*>(UserWidget), bIsVisible);
		}
	}
}
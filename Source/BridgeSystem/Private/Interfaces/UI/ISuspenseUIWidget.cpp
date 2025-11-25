// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseUIWidget.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"

USuspenseEventManager* ISuspenseUIWidget::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseUIWidget::BroadcastWidgetCreated(const UObject* Widget)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		// Cast to UUserWidget for the notification
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			Manager->NotifyUIWidgetCreated(const_cast<UUserWidget*>(UserWidget));
		}
	}
}

void ISuspenseUIWidget::BroadcastWidgetDestroyed(const UObject* Widget)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			Manager->NotifyUIWidgetDestroyed(const_cast<UUserWidget*>(UserWidget));
		}
	}
}

void ISuspenseUIWidget::BroadcastVisibilityChanged(const UObject* Widget, bool bIsVisible)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			Manager->NotifyUIVisibilityChanged(const_cast<UUserWidget*>(UserWidget), bIsVisible);
		}
	}
}
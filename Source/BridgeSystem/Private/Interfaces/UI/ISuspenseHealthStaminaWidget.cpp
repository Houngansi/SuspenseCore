// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/UI/ISuspenseHealthStaminaWidget.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseHealthStaminaWidget::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseHealthStaminaWidget::BroadcastHealthUpdated(const UObject* Widget, float CurrentHealth, float MaxHealth)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		float HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
		Manager->NotifyHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
	}
}

void ISuspenseHealthStaminaWidget::BroadcastStaminaUpdated(const UObject* Widget, float CurrentStamina, float MaxStamina)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		float StaminaPercent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
		Manager->NotifyStaminaUpdated(CurrentStamina, MaxStamina, StaminaPercent);
	}
}
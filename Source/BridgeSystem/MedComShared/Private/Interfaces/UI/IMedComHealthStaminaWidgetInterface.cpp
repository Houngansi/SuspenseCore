// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/UI/IMedComHealthStaminaWidgetInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComHealthStaminaWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComHealthStaminaWidgetInterface::BroadcastHealthUpdated(const UObject* Widget, float CurrentHealth, float MaxHealth)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		float HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
		Manager->NotifyHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
	}
}

void IMedComHealthStaminaWidgetInterface::BroadcastStaminaUpdated(const UObject* Widget, float CurrentStamina, float MaxStamina)
{
	if (!Widget)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		float StaminaPercent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
		Manager->NotifyStaminaUpdated(CurrentStamina, MaxStamina, StaminaPercent);
	}
}
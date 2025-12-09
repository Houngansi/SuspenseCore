// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/UI/ISuspenseCoreHealthStaminaWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseCoreHealthStaminaWidgetInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseCoreHealthStaminaWidgetInterface::BroadcastHealthUpdated(const UObject* Widget, float CurrentHealth, float MaxHealth)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// float HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
		// Manager->NotifyHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
	}
}

void ISuspenseCoreHealthStaminaWidgetInterface::BroadcastStaminaUpdated(const UObject* Widget, float CurrentStamina, float MaxStamina)
{
	if (!Widget)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Widget);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// float StaminaPercent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
		// Manager->NotifyStaminaUpdated(CurrentStamina, MaxStamina, StaminaPercent);
	}
}
// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Core/ISuspenseAttributeProvider.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseAttributeProvider::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseAttributeProvider::BroadcastHealthUpdate(const UObject* Provider, float CurrentHealth, float MaxHealth)
{
	if (!Provider)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Provider);
	if (Manager)
	{
		float HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
		Manager->NotifyHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
	}
}

void ISuspenseAttributeProvider::BroadcastStaminaUpdate(const UObject* Provider, float CurrentStamina, float MaxStamina)
{
	if (!Provider)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Provider);
	if (Manager)
	{
		float StaminaPercent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
		Manager->NotifyStaminaUpdated(CurrentStamina, MaxStamina, StaminaPercent);
	}
}
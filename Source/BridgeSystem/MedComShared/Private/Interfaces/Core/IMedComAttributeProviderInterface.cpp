// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Core/IMedComAttributeProviderInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComAttributeProviderInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComAttributeProviderInterface::BroadcastHealthUpdate(const UObject* Provider, float CurrentHealth, float MaxHealth)
{
	if (!Provider)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Provider);
	if (Manager)
	{
		float HealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
		Manager->NotifyHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
	}
}

void IMedComAttributeProviderInterface::BroadcastStaminaUpdate(const UObject* Provider, float CurrentStamina, float MaxStamina)
{
	if (!Provider)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Provider);
	if (Manager)
	{
		float StaminaPercent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
		Manager->NotifyStaminaUpdated(CurrentStamina, MaxStamina, StaminaPercent);
	}
}
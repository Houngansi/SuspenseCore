// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Weapon/ISuspenseFireModeProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseFireModeProvider::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	return USuspenseCoreEventManager::Get(WorldContextObject);
}

void ISuspenseFireModeProvider::BroadcastFireModeChanged(
	const UObject* FireModeProvider,
	const FGameplayTag& NewFireMode,
	float CurrentSpread)
{
	if (!FireModeProvider)
	{
		return;
	}

	if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(FireModeProvider))
	{
		if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
		{
			FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(FireModeProvider))
				.SetString(TEXT("FireModeTag"), NewFireMode.ToString())
				.SetFloat(TEXT("CurrentSpread"), CurrentSpread);

			EventBus->Publish(
				FGameplayTag::RequestGameplayTag(TEXT("Weapon.Event.FireModeChanged")),
				EventData
			);
		}
	}
}

void ISuspenseFireModeProvider::BroadcastFireModeAvailabilityChanged(
	const UObject* FireModeProvider,
	const FGameplayTag& FireModeTag,
	bool bEnabled)
{
	if (!FireModeProvider)
	{
		return;
	}

	if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(FireModeProvider))
	{
		if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
		{
			FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(FireModeProvider))
				.SetString(TEXT("FireModeTag"), FireModeTag.ToString())
				.SetBool(TEXT("Enabled"), bEnabled);

			EventBus->Publish(
				FGameplayTag::RequestGameplayTag(TEXT("Weapon.Event.FireModeAvailabilityChanged")),
				EventData
			);
		}
	}
}
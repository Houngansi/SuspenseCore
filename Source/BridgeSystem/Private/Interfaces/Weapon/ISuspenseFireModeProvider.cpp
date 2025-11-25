// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Weapon/ISuspenseFireModeProvider.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseFireModeProvider::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseFireModeProvider::BroadcastFireModeChanged(
	const UObject* FireModeProvider,
	const FGameplayTag& NewFireMode,
	float CurrentSpread)
{
	if (!FireModeProvider)
	{
		return;
	}
    
	if (USuspenseEventManager* Manager = GetDelegateManagerStatic(FireModeProvider))
	{
		Manager->NotifyFireModeChanged(NewFireMode, CurrentSpread);
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
    
	if (USuspenseEventManager* Manager = GetDelegateManagerStatic(FireModeProvider))
	{
		Manager->NotifyFireModeProviderChanged(FireModeTag, bEnabled);
	}
}
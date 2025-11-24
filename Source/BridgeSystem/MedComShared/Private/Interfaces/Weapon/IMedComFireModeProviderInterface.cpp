// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Weapon/IMedComFireModeProviderInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComFireModeProviderInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComFireModeProviderInterface::BroadcastFireModeChanged(
	const UObject* FireModeProvider,
	const FGameplayTag& NewFireMode,
	float CurrentSpread)
{
	if (!FireModeProvider)
	{
		return;
	}
    
	if (UEventDelegateManager* Manager = GetDelegateManagerStatic(FireModeProvider))
	{
		Manager->NotifyFireModeChanged(NewFireMode, CurrentSpread);
	}
}

void IMedComFireModeProviderInterface::BroadcastFireModeAvailabilityChanged(
	const UObject* FireModeProvider,
	const FGameplayTag& FireModeTag,
	bool bEnabled)
{
	if (!FireModeProvider)
	{
		return;
	}
    
	if (UEventDelegateManager* Manager = GetDelegateManagerStatic(FireModeProvider))
	{
		Manager->NotifyFireModeProviderChanged(FireModeTag, bEnabled);
	}
}
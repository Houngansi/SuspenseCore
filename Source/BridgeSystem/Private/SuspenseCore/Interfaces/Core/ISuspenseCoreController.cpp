// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Core/ISuspenseController.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseController::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseController::BroadcastControllerWeaponChanged(const UObject* Controller, AActor* NewWeapon)
{
	if (!Controller)
	{
		return;
	}
    
	USuspenseEventManager* Manager = GetDelegateManagerStatic(Controller);
	if (Manager)
	{
		// Notify about controller weapon change
		FGameplayTag EventTag = NewWeapon 
			? FGameplayTag::RequestGameplayTag(TEXT("Controller.Event.WeaponEquipped"))
			: FGameplayTag::RequestGameplayTag(TEXT("Controller.Event.WeaponUnequipped"));
            
		FString EventData = NewWeapon ? NewWeapon->GetName() : TEXT("None");
		Manager->NotifyEquipmentEvent(const_cast<UObject*>(Controller), EventTag, EventData);
	}
}
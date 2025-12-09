// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/Core/ISuspenseCoreController.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseCoreController::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseCoreController::BroadcastControllerWeaponChanged(const UObject* Controller, AActor* NewWeapon)
{
	if (!Controller)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Controller);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// Notify about controller weapon change
		// FGameplayTag EventTag = NewWeapon
		// 	? FGameplayTag::RequestGameplayTag(TEXT("Controller.Event.WeaponEquipped"))
		// 	: FGameplayTag::RequestGameplayTag(TEXT("Controller.Event.WeaponUnequipped"));

		// FString EventData = NewWeapon ? NewWeapon->GetName() : TEXT("None");
		// Manager->NotifyEquipmentEvent(const_cast<UObject*>(Controller), EventTag, EventData);
	}
}
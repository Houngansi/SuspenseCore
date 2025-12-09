// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/Core/ISuspenseCoreCharacter.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"

USuspenseCoreEventManager* ISuspenseCoreCharacterInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseCoreCharacterInterface::BroadcastWeaponChanged(const UObject* Character, AActor* NewWeapon, bool bHasWeapon)
{
	if (!Character)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Character);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// Notify about active weapon change
		// Manager->NotifyActiveWeaponChanged(NewWeapon);

		// Notify about equipment event
		// FGameplayTag EventTag = bHasWeapon
		// 	? FGameplayTag::RequestGameplayTag(TEXT("Character.Event.WeaponEquipped"))
		// 	: FGameplayTag::RequestGameplayTag(TEXT("Character.Event.WeaponUnequipped"));

		// Manager->NotifyEquipmentEvent(const_cast<UObject*>(Character), EventTag, TEXT(""));
	}
}
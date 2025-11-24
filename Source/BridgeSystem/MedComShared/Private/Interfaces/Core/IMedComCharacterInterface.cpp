// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Core/IMedComCharacterInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"

UEventDelegateManager* IMedComCharacterInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComCharacterInterface::BroadcastWeaponChanged(const UObject* Character, AActor* NewWeapon, bool bHasWeapon)
{
	if (!Character)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Character);
	if (Manager)
	{
		// Notify about active weapon change
		Manager->NotifyActiveWeaponChanged(NewWeapon);
        
		// Notify about equipment event
		FGameplayTag EventTag = bHasWeapon 
			? FGameplayTag::RequestGameplayTag(TEXT("Character.Event.WeaponEquipped"))
			: FGameplayTag::RequestGameplayTag(TEXT("Character.Event.WeaponUnequipped"));
            
		Manager->NotifyEquipmentEvent(const_cast<UObject*>(Character), EventTag, TEXT(""));
	}
}
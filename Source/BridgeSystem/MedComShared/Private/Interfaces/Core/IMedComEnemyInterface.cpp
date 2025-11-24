// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Core/IMedComEnemyInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComEnemyInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void IMedComEnemyInterface::BroadcastEnemyWeaponChanged(const UObject* Enemy, AActor* NewWeapon)
{
	if (!Enemy)
	{
		return;
	}
    
	UEventDelegateManager* Manager = GetDelegateManagerStatic(Enemy);
	if (Manager)
	{
		// Notify about enemy weapon change
		FGameplayTag EventTag = NewWeapon 
			? FGameplayTag::RequestGameplayTag(TEXT("Enemy.Event.WeaponEquipped"))
			: FGameplayTag::RequestGameplayTag(TEXT("Enemy.Event.WeaponUnequipped"));
            
		FString EventData = NewWeapon ? NewWeapon->GetName() : TEXT("None");
		Manager->NotifyEquipmentEvent(const_cast<UObject*>(Enemy), EventTag, EventData);
	}
}
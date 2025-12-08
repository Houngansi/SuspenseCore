// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/Core/ISuspenseCoreEnemy.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseEnemy::GetDelegateManagerStatic(const UObject* WorldContextObject)
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

void ISuspenseEnemy::BroadcastEnemyWeaponChanged(const UObject* Enemy, AActor* NewWeapon)
{
	if (!Enemy)
	{
		return;
	}
    
	USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Enemy);
	if (Manager)
	{
		// TODO: Migrate to EventBus - old delegate system removed
		// Notify about enemy weapon change
		// FGameplayTag EventTag = NewWeapon
		// 	? FGameplayTag::RequestGameplayTag(TEXT("Enemy.Event.WeaponEquipped"))
		// 	: FGameplayTag::RequestGameplayTag(TEXT("Enemy.Event.WeaponUnequipped"));

		// FString EventData = NewWeapon ? NewWeapon->GetName() : TEXT("None");
		// Manager->NotifyEquipmentEvent(const_cast<UObject*>(Enemy), EventTag, EventData);
	}
}
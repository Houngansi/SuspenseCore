// GA_QuickSlotUse.cpp
// QuickSlot-specific Item Use Ability
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/ItemUse/GA_QuickSlotUse.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseService.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogGA_QuickSlotUse, Log, All);

//==================================================================
// Constructor
//==================================================================

UGA_QuickSlotUse::UGA_QuickSlotUse()
{
	SlotIndex = 0;

	// QuickSlot uses are typically fast but may have duration
	bIsCancellable = true;
	bApplyCooldownOnCancel = false;
}

//==================================================================
// Request Building
//==================================================================

FSuspenseCoreItemUseRequest UGA_QuickSlotUse::BuildItemUseRequest(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData) const
{
	FSuspenseCoreItemUseRequest Request;
	Request.Context = ESuspenseCoreItemUseContext::QuickSlot;
	Request.QuickSlotIndex = SlotIndex;

	if (ActorInfo)
	{
		Request.RequestingActor = ActorInfo->AvatarActor.Get();
	}

	if (const UWorld* World = GetWorld())
	{
		Request.RequestTime = World->GetTimeSeconds();
	}

	// Get item from QuickSlot via QuickSlotProvider
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		UE_LOG(LogGA_QuickSlotUse, Warning, TEXT("BuildItemUseRequest: No avatar actor"));
		return Request;
	}

	// Find QuickSlotProvider component
	TArray<UActorComponent*> Components;
	AvatarActor->GetComponents(USuspenseCoreQuickSlotProvider::StaticClass(), Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<USuspenseCoreQuickSlotProvider>())
		{
			// Get slot data via interface
			FSuspenseCoreQuickSlot SlotData =
				ISuspenseCoreQuickSlotProvider::Execute_GetQuickSlot(Component, SlotIndex);

			if (SlotData.AssignedItemInstanceID.IsValid())
			{
				Request.SourceItem.UniqueInstanceID = SlotData.AssignedItemInstanceID;
				Request.SourceItem.ItemID = SlotData.AssignedItemID;
				Request.SourceSlotIndex = SlotIndex;
				Request.SourceContainerTag = SlotData.SlotTag;

				UE_LOG(LogGA_QuickSlotUse, Verbose,
					TEXT("BuildItemUseRequest: Slot %d has item %s"),
					SlotIndex, *SlotData.AssignedItemID.ToString());

				break;
			}
		}
	}

	if (!Request.IsValid())
	{
		UE_LOG(LogGA_QuickSlotUse, Warning,
			TEXT("BuildItemUseRequest: QuickSlot %d is empty or no provider found"),
			SlotIndex);
	}

	return Request;
}

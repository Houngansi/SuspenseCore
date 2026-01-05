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

	UE_LOG(LogGA_QuickSlotUse, Log, TEXT("BuildItemUseRequest: AvatarActor=%s, SlotIndex=%d"),
		*AvatarActor->GetName(), SlotIndex);

	// Find QuickSlotProvider component - iterate ALL components and check interface
	// Note: GetComponents(UInterface::StaticClass()) doesn't work for UInterfaces
	TArray<UActorComponent*> Components;
	AvatarActor->GetComponents(Components);

	UE_LOG(LogGA_QuickSlotUse, Log, TEXT("BuildItemUseRequest: Found %d components on actor"), Components.Num());

	bool bFoundProvider = false;
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<USuspenseCoreQuickSlotProvider>())
		{
			bFoundProvider = true;
			UE_LOG(LogGA_QuickSlotUse, Log, TEXT("BuildItemUseRequest: Found QuickSlotProvider: %s"),
				*Component->GetName());

			// Get slot data via interface
			FSuspenseCoreQuickSlot SlotData =
				ISuspenseCoreQuickSlotProvider::Execute_GetQuickSlot(Component, SlotIndex);

			UE_LOG(LogGA_QuickSlotUse, Log, TEXT("BuildItemUseRequest: Slot %d - ItemID=%s, InstanceID=%s"),
				SlotIndex,
				*SlotData.AssignedItemID.ToString(),
				SlotData.AssignedItemInstanceID.IsValid() ? *SlotData.AssignedItemInstanceID.ToString() : TEXT("INVALID"));

			if (SlotData.AssignedItemInstanceID.IsValid())
			{
				Request.SourceItem.UniqueInstanceID = SlotData.AssignedItemInstanceID;
				Request.SourceItem.ItemID = SlotData.AssignedItemID;
				Request.SourceSlotIndex = SlotIndex;
				Request.SourceContainerTag = SlotData.SlotTag;

				// Try to get MagazineData if this is a magazine
				// This is CRITICAL for MagazineSwapHandler::ValidateRequest which checks IsMagazine()
				FSuspenseCoreMagazineInstance MagazineData;
				if (ISuspenseCoreQuickSlotProvider::Execute_GetMagazineFromSlot(Component, SlotIndex, MagazineData))
				{
					Request.SourceItem.MagazineData = MagazineData;
					UE_LOG(LogGA_QuickSlotUse, Log,
						TEXT("BuildItemUseRequest: Got MagazineData - MagID=%s, Rounds=%d/%d"),
						*MagazineData.MagazineID.ToString(),
						MagazineData.CurrentRoundCount,
						MagazineData.MaxCapacity);
				}

				UE_LOG(LogGA_QuickSlotUse, Log,
					TEXT("BuildItemUseRequest: SUCCESS - Slot %d has item %s (IsMagazine=%s)"),
					SlotIndex, *SlotData.AssignedItemID.ToString(),
					Request.SourceItem.IsMagazine() ? TEXT("YES") : TEXT("NO"));

				break;
			}
		}
	}

	if (!bFoundProvider)
	{
		UE_LOG(LogGA_QuickSlotUse, Warning, TEXT("BuildItemUseRequest: No component implements ISuspenseCoreQuickSlotProvider!"));
	}

	if (!Request.IsValid())
	{
		UE_LOG(LogGA_QuickSlotUse, Warning,
			TEXT("BuildItemUseRequest: QuickSlot %d is empty or no provider found"),
			SlotIndex);
	}

	return Request;
}

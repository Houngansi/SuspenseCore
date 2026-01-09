// SuspenseCoreMagazineSwapHandler.cpp
// Handler for quick magazine swap via QuickSlot
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreMagazineSwapHandler.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagazineSwapHandler, Log, All);

#define HANDLER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMagazineSwapHandler, Verbosity, TEXT("[MagazineSwap] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreMagazineSwapHandler::USuspenseCoreMagazineSwapHandler()
{
	SwapCooldown = 0.5f;
}

void USuspenseCoreMagazineSwapHandler::Initialize(USuspenseCoreEventBus* InEventBus)
{
	EventBus = InEventBus;

	HANDLER_LOG(Log, TEXT("Initialized with EventBus=%s"),
		InEventBus ? TEXT("Valid") : TEXT("NULL"));
}

//==================================================================
// Handler Identity
//==================================================================

FGameplayTag USuspenseCoreMagazineSwapHandler::GetHandlerTag() const
{
	return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_MagazineSwap;
}

ESuspenseCoreHandlerPriority USuspenseCoreMagazineSwapHandler::GetPriority() const
{
	return ESuspenseCoreHandlerPriority::High; // Higher priority for QuickSlot operations
}

FText USuspenseCoreMagazineSwapHandler::GetDisplayName() const
{
	return FText::FromString(TEXT("Quick Magazine Swap"));
}

//==================================================================
// Supported Types
//==================================================================

FGameplayTagContainer USuspenseCoreMagazineSwapHandler::GetSupportedSourceTags() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Category.Magazine"), false));
	return Tags;
}

TArray<ESuspenseCoreItemUseContext> USuspenseCoreMagazineSwapHandler::GetSupportedContexts() const
{
	return { ESuspenseCoreItemUseContext::QuickSlot };
}

//==================================================================
// Validation
//==================================================================

bool USuspenseCoreMagazineSwapHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
{
	// Must be QuickSlot context
	if (Request.Context != ESuspenseCoreItemUseContext::QuickSlot)
	{
		return false;
	}

	// Must have source item (magazine in QuickSlot)
	if (!Request.SourceItem.IsValid())
	{
		return false;
	}

	// Must have valid QuickSlot index
	if (Request.QuickSlotIndex < 0 || Request.QuickSlotIndex > 3)
	{
		return false;
	}

	return true;
}

bool USuspenseCoreMagazineSwapHandler::ValidateRequest(
	const FSuspenseCoreItemUseRequest& Request,
	FSuspenseCoreItemUseResponse& OutResponse) const
{
	// Check source is valid magazine
	if (!Request.SourceItem.IsValid())
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("QuickSlot is empty")));
		return false;
	}

	// Check that source is a magazine by checking MagazineData validity
	// FSuspenseCoreItemInstance.IsMagazine() returns true if MagazineData is valid
	bool bIsMagazine = Request.SourceItem.IsMagazine();

	if (!bIsMagazine)
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Item in QuickSlot is not a magazine")));
		return false;
	}

	// NOTE: Full caliber validation happens in MagazineComponent::SwapMagazineFromQuickSlot
	// Here we do early validation only if we have the owner actor in the request
	// This prevents unnecessary ability activation for incompatible magazines

	return true;
}

//==================================================================
// Execution
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreMagazineSwapHandler::Execute(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("Execute: Swapping magazine from QuickSlot %d"),
		Request.QuickSlotIndex);

	// Get QuickSlotProvider
	ISuspenseCoreQuickSlotProvider* Provider = GetQuickSlotProvider(OwnerActor);
	if (!Provider)
	{
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_SystemError,
			FText::FromString(TEXT("QuickSlotProvider not found")));
	}

	UObject* ProviderObject = Cast<UObject>(Provider);

	// CRITICAL: Check magazine caliber compatibility BEFORE swap
	// @see TarkovStyle_Ammo_System_Design.md - Caliber validation
	FSuspenseCoreMagazineInstance NewMag;
	if (ISuspenseCoreQuickSlotProvider::Execute_GetMagazineFromSlot(ProviderObject, Request.QuickSlotIndex, NewMag))
	{
		// Find MagazineProvider on weapon for compatibility check
		if (OwnerActor)
		{
			TArray<UActorComponent*> Components;
			OwnerActor->GetComponents(Components);

			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->Implements<USuspenseCoreMagazineProvider>())
				{
					const bool bCompatible = ISuspenseCoreMagazineProvider::Execute_IsMagazineCompatible(Comp, NewMag);
					if (!bCompatible)
					{
						FGameplayTag WeaponCaliber = ISuspenseCoreMagazineProvider::Execute_GetWeaponCaliber(Comp);

						HANDLER_LOG(Warning, TEXT("Execute: Magazine %s NOT compatible with weapon caliber %s"),
							*NewMag.MagazineID.ToString(),
							*WeaponCaliber.ToString());

						return FSuspenseCoreItemUseResponse::Failure(
							Request.RequestID,
							ESuspenseCoreItemUseResult::Failed_IncompatibleItems,
							FText::Format(
								NSLOCTEXT("MagazineSwap", "IncompatibleCaliber", "Magazine caliber does not match weapon ({0})"),
								FText::FromString(WeaponCaliber.ToString())));
					}
					break;
				}
			}
		}
	}

	// Execute swap via QuickSlotProvider interface
	bool bSuccess = ISuspenseCoreQuickSlotProvider::Execute_QuickSwapMagazine(
		ProviderObject,
		Request.QuickSlotIndex,
		false // bEmergencyDrop
	);

	FSuspenseCoreItemUseResponse Response;
	Response.RequestID = Request.RequestID;
	Response.HandlerTag = GetHandlerTag();

	if (bSuccess)
	{
		Response.Result = ESuspenseCoreItemUseResult::Success;
		Response.Cooldown = GetCooldown(Request);
		Response.Progress = 1.0f;

		HANDLER_LOG(Log, TEXT("Execute: Magazine swap successful"));
	}
	else
	{
		Response.Result = ESuspenseCoreItemUseResult::Failed_IncompatibleItems;
		Response.Message = FText::FromString(TEXT("Magazine swap failed - incompatible or no weapon equipped"));

		HANDLER_LOG(Warning, TEXT("Execute: Magazine swap failed"));
	}

	// Publish event
	PublishSwapEvent(Request, Response, OwnerActor);

	return Response;
}

float USuspenseCoreMagazineSwapHandler::GetDuration(const FSuspenseCoreItemUseRequest& Request) const
{
	// Magazine swap is instant (duration handled by reload ability)
	return 0.0f;
}

float USuspenseCoreMagazineSwapHandler::GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
{
	return SwapCooldown;
}

//==================================================================
// Internal Methods
//==================================================================

ISuspenseCoreQuickSlotProvider* USuspenseCoreMagazineSwapHandler::GetQuickSlotProvider(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	// Check actor itself
	if (Actor->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
	{
		return Cast<ISuspenseCoreQuickSlotProvider>(Actor);
	}

	// Check components
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
		{
			return Cast<ISuspenseCoreQuickSlotProvider>(Comp);
		}
	}

	return nullptr;
}

void USuspenseCoreMagazineSwapHandler::PublishSwapEvent(
	const FSuspenseCoreItemUseRequest& Request,
	const FSuspenseCoreItemUseResponse& Response,
	AActor* OwnerActor)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = OwnerActor;
	EventData.Timestamp = FPlatformTime::Seconds();
	EventData.StringPayload.Add(TEXT("RequestID"), Request.RequestID.ToString());
	EventData.StringPayload.Add(TEXT("MagazineID"), Request.SourceItem.ItemID.ToString());
	EventData.IntPayload.Add(TEXT("QuickSlotIndex"), Request.QuickSlotIndex);
	EventData.IntPayload.Add(TEXT("Result"), static_cast<int32>(Response.Result));

	FGameplayTag EventTag = Response.IsSuccess()
		? SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed
		: SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Failed;

	EventBus->Publish(EventTag, EventData);
}

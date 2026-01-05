// SuspenseCoreMagazineSwapHandler.cpp
// Handler for quick magazine swap via QuickSlot
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreMagazineSwapHandler.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"

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

	// Actor validation would check weapon compatibility here
	// For now, we accept all valid magazines

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

// SuspenseCoreAmmoToMagazineHandler.cpp
// Handler for loading ammo into magazines via DragDrop
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreAmmoToMagazineHandler.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmmoToMagazineHandler, Log, All);

#define HANDLER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAmmoToMagazineHandler, Verbosity, TEXT("[AmmoToMagazine] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreAmmoToMagazineHandler::USuspenseCoreAmmoToMagazineHandler()
{
	DefaultCooldown = 0.5f;
}

void USuspenseCoreAmmoToMagazineHandler::Initialize(
	USuspenseCoreDataManager* InDataManager,
	USuspenseCoreEventBus* InEventBus)
{
	DataManager = InDataManager;
	EventBus = InEventBus;

	HANDLER_LOG(Log, TEXT("Initialized with DataManager=%s, EventBus=%s"),
		InDataManager ? TEXT("Valid") : TEXT("NULL"),
		InEventBus ? TEXT("Valid") : TEXT("NULL"));
}

//==================================================================
// Handler Identity
//==================================================================

FGameplayTag USuspenseCoreAmmoToMagazineHandler::GetHandlerTag() const
{
	return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_AmmoToMagazine;
}

ESuspenseCoreHandlerPriority USuspenseCoreAmmoToMagazineHandler::GetPriority() const
{
	return ESuspenseCoreHandlerPriority::Normal;
}

FText USuspenseCoreAmmoToMagazineHandler::GetDisplayName() const
{
	return FText::FromString(TEXT("Load Ammo into Magazine"));
}

//==================================================================
// Supported Types
//==================================================================

FGameplayTagContainer USuspenseCoreAmmoToMagazineHandler::GetSupportedSourceTags() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Ammo"), false));
	return Tags;
}

FGameplayTagContainer USuspenseCoreAmmoToMagazineHandler::GetSupportedTargetTags() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Category.Magazine"), false));
	return Tags;
}

TArray<ESuspenseCoreItemUseContext> USuspenseCoreAmmoToMagazineHandler::GetSupportedContexts() const
{
	return { ESuspenseCoreItemUseContext::DragDrop };
}

//==================================================================
// Validation
//==================================================================

bool USuspenseCoreAmmoToMagazineHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
{
	// Must be DragDrop context
	if (Request.Context != ESuspenseCoreItemUseContext::DragDrop)
	{
		return false;
	}

	// Must have both source and target
	if (!Request.SourceItem.IsValid() || !Request.TargetItem.IsValid())
	{
		return false;
	}

	// Source must be ammo (has Item.Ammo tag)
	// Target must be magazine (has Item.Category.Magazine tag)
	// These checks are done in ValidateRequest for full validation

	return true;
}

bool USuspenseCoreAmmoToMagazineHandler::ValidateRequest(
	const FSuspenseCoreItemUseRequest& Request,
	FSuspenseCoreItemUseResponse& OutResponse) const
{
	// Check source is valid ammo
	if (!Request.SourceItem.IsValid())
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Invalid ammo item")));
		return false;
	}

	// Check target is valid magazine
	if (!Request.TargetItem.IsValid())
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Invalid magazine item")));
		return false;
	}

	// Check ammo quantity
	if (Request.SourceItem.Quantity <= 0)
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_MissingRequirement,
			FText::FromString(TEXT("No ammo available")));
		return false;
	}

	// Get magazine data
	FSuspenseCoreMagazineData MagData;
	if (!GetMagazineData(Request.TargetItem.ItemID, MagData))
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_SystemError,
			FText::FromString(TEXT("Magazine data not found")));
		return false;
	}

	// Get current magazine state from target item
	int32 CurrentRounds = 0;
	if (const int32* RoundCount = Request.TargetItem.IntMetadata.Find(FName("CurrentRounds")))
	{
		CurrentRounds = *RoundCount;
	}

	// Check if magazine is full
	if (CurrentRounds >= MagData.MaxCapacity)
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_TargetFull,
			FText::FromString(TEXT("Magazine is full")));
		return false;
	}

	// Check caliber compatibility
	// Ammo caliber tag should be in source item tags
	FGameplayTag AmmoCaliber;
	for (const FGameplayTag& Tag : Request.SourceItem.ItemTags)
	{
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Item.Ammo"), false)))
		{
			AmmoCaliber = Tag;
			break;
		}
	}

	if (!IsCaliberCompatible(AmmoCaliber, MagData.Caliber))
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_IncompatibleItems,
			FText::FromString(TEXT("Ammo caliber does not match magazine")));
		return false;
	}

	// All checks passed
	return true;
}

//==================================================================
// Execution
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreAmmoToMagazineHandler::Execute(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("Execute: Loading ammo %s into magazine %s"),
		*Request.SourceItem.ItemID.ToString(),
		*Request.TargetItem.ItemID.ToString());

	// Get duration for this operation
	float Duration = GetDuration(Request);

	// Return InProgress response - actual loading happens in OnOperationComplete
	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, Duration);
	Response.HandlerTag = GetHandlerTag();
	Response.Cooldown = GetCooldown(Request);

	// Publish started event
	PublishLoadEvent(Request, Response, OwnerActor);

	return Response;
}

float USuspenseCoreAmmoToMagazineHandler::GetDuration(const FSuspenseCoreItemUseRequest& Request) const
{
	// Get magazine data for load time
	FSuspenseCoreMagazineData MagData;
	if (!GetMagazineData(Request.TargetItem.ItemID, MagData))
	{
		return 0.0f;
	}

	// Get current rounds in magazine
	int32 CurrentRounds = 0;
	if (const int32* RoundCount = Request.TargetItem.IntMetadata.Find(FName("CurrentRounds")))
	{
		CurrentRounds = *RoundCount;
	}

	// Calculate rounds to load
	int32 RoundsToLoad = CalculateRoundsToLoad(
		Request.SourceItem.Quantity,
		CurrentRounds,
		MagData.MaxCapacity);

	if (RoundsToLoad <= 0)
	{
		return 0.0f;
	}

	// Duration = TimePerRound * RoundsToLoad
	float Duration = MagData.LoadTimePerRound * RoundsToLoad;

	HANDLER_LOG(Verbose, TEXT("GetDuration: %d rounds * %.2fs = %.2fs"),
		RoundsToLoad, MagData.LoadTimePerRound, Duration);

	return Duration;
}

float USuspenseCoreAmmoToMagazineHandler::GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
{
	return DefaultCooldown;
}

bool USuspenseCoreAmmoToMagazineHandler::CancelOperation(const FGuid& RequestID)
{
	HANDLER_LOG(Log, TEXT("CancelOperation: %s"), *RequestID.ToString().Left(8));
	// Stateless handler - cancellation is handled by GAS ability
	// No partial loading - if cancelled, nothing is loaded
	return true;
}

FSuspenseCoreItemUseResponse USuspenseCoreAmmoToMagazineHandler::OnOperationComplete(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("OnOperationComplete: Finalizing load for %s"),
		*Request.RequestID.ToString().Left(8));

	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID);
	Response.HandlerTag = GetHandlerTag();
	Response.Progress = 1.0f;

	// Get magazine data
	FSuspenseCoreMagazineData MagData;
	if (!GetMagazineData(Request.TargetItem.ItemID, MagData))
	{
		Response.Result = ESuspenseCoreItemUseResult::Failed_SystemError;
		Response.Message = FText::FromString(TEXT("Magazine data not found during completion"));
		return Response;
	}

	// Calculate rounds loaded
	int32 CurrentRounds = 0;
	if (const int32* RoundCount = Request.TargetItem.IntMetadata.Find(FName("CurrentRounds")))
	{
		CurrentRounds = *RoundCount;
	}

	int32 RoundsLoaded = CalculateRoundsToLoad(
		Request.SourceItem.Quantity,
		CurrentRounds,
		MagData.MaxCapacity);

	// Update modified items
	// Source (ammo): Reduce quantity
	Response.ModifiedSourceItem = Request.SourceItem;
	Response.ModifiedSourceItem.Quantity -= RoundsLoaded;

	// Target (magazine): Add rounds
	Response.ModifiedTargetItem = Request.TargetItem;
	Response.ModifiedTargetItem.IntMetadata.Add(FName("CurrentRounds"), CurrentRounds + RoundsLoaded);

	// Add metadata for UI/logging
	Response.Metadata.Add(TEXT("RoundsLoaded"), FString::FromInt(RoundsLoaded));
	Response.Metadata.Add(TEXT("NewMagazineCount"), FString::FromInt(CurrentRounds + RoundsLoaded));
	Response.Metadata.Add(TEXT("RemainingAmmo"), FString::FromInt(Response.ModifiedSourceItem.Quantity));

	HANDLER_LOG(Log, TEXT("OnOperationComplete: Loaded %d rounds. Magazine now has %d/%d"),
		RoundsLoaded,
		CurrentRounds + RoundsLoaded,
		MagData.MaxCapacity);

	// Publish completion event
	PublishLoadEvent(Request, Response, OwnerActor);

	return Response;
}

//==================================================================
// Internal Methods
//==================================================================

int32 USuspenseCoreAmmoToMagazineHandler::CalculateRoundsToLoad(
	int32 AmmoQuantity,
	int32 CurrentRounds,
	int32 MaxCapacity) const
{
	int32 SpaceAvailable = MaxCapacity - CurrentRounds;
	return FMath::Min(AmmoQuantity, SpaceAvailable);
}

bool USuspenseCoreAmmoToMagazineHandler::GetMagazineData(
	FName MagazineID,
	FSuspenseCoreMagazineData& OutData) const
{
	if (!DataManager.IsValid())
	{
		HANDLER_LOG(Warning, TEXT("GetMagazineData: DataManager not available"));
		return false;
	}

	// Use DataManager to get magazine data
	return DataManager->GetMagazineData(MagazineID, OutData);
}

bool USuspenseCoreAmmoToMagazineHandler::IsCaliberCompatible(
	const FGameplayTag& AmmoCaliber,
	const FGameplayTag& MagazineCaliber) const
{
	// Exact match or ammo is child of magazine caliber
	return AmmoCaliber.MatchesTag(MagazineCaliber);
}

void USuspenseCoreAmmoToMagazineHandler::PublishLoadEvent(
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
	EventData.StringPayload.Add(TEXT("SourceItemID"), Request.SourceItem.ItemID.ToString());
	EventData.StringPayload.Add(TEXT("TargetItemID"), Request.TargetItem.ItemID.ToString());
	EventData.IntPayload.Add(TEXT("Result"), static_cast<int32>(Response.Result));
	EventData.FloatPayload.Add(TEXT("Duration"), Response.Duration);

	if (Response.Metadata.Contains(TEXT("RoundsLoaded")))
	{
		EventData.IntPayload.Add(TEXT("RoundsLoaded"),
			FCString::Atoi(*Response.Metadata[TEXT("RoundsLoaded")]));
	}

	// Determine which event tag to use based on response
	FGameplayTag EventTag;
	if (Response.IsInProgress())
	{
		EventTag = SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Started;
	}
	else if (Response.IsSuccess())
	{
		EventTag = SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed;
	}
	else
	{
		EventTag = SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Failed;
	}

	EventBus->Publish(EventTag, EventData);
}

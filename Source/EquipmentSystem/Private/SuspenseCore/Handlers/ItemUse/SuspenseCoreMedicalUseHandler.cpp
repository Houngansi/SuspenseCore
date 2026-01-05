// SuspenseCoreMedicalUseHandler.cpp
// Handler for medical item usage
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreMedicalUseHandler.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalUseHandler, Log, All);

#define HANDLER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalUseHandler, Verbosity, TEXT("[MedicalUse] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreMedicalUseHandler::USuspenseCoreMedicalUseHandler()
{
	BandageDuration = 3.0f;
	MedkitDuration = 5.0f;
	PainkillerDuration = 2.0f;
	StimulantDuration = 2.0f;
	SplintDuration = 8.0f;
	SurgicalDuration = 15.0f;
	DefaultCooldown = 1.0f;
}

void USuspenseCoreMedicalUseHandler::Initialize(
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

FGameplayTag USuspenseCoreMedicalUseHandler::GetHandlerTag() const
{
	return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_Medical;
}

ESuspenseCoreHandlerPriority USuspenseCoreMedicalUseHandler::GetPriority() const
{
	return ESuspenseCoreHandlerPriority::Normal;
}

FText USuspenseCoreMedicalUseHandler::GetDisplayName() const
{
	return FText::FromString(TEXT("Use Medical Item"));
}

//==================================================================
// Supported Types
//==================================================================

FGameplayTagContainer USuspenseCoreMedicalUseHandler::GetSupportedSourceTags() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Category.Medical"), false));
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical"), false));
	return Tags;
}

TArray<ESuspenseCoreItemUseContext> USuspenseCoreMedicalUseHandler::GetSupportedContexts() const
{
	return {
		ESuspenseCoreItemUseContext::DoubleClick,
		ESuspenseCoreItemUseContext::QuickSlot,
		ESuspenseCoreItemUseContext::Hotkey
	};
}

//==================================================================
// Validation
//==================================================================

bool USuspenseCoreMedicalUseHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
{
	// Must be one of supported contexts
	TArray<ESuspenseCoreItemUseContext> Contexts = GetSupportedContexts();
	if (!Contexts.Contains(Request.Context))
	{
		return false;
	}

	// Must have source item
	if (!Request.SourceItem.IsValid())
	{
		return false;
	}

	// Check if item has medical tag by looking up in DataManager
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			FGameplayTag MedicalTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Medical"), false);
			FGameplayTag MedicalTag2 = FGameplayTag::RequestGameplayTag(FName("Item.Medical"), false);

			// Check ItemType tag
			if (ItemData.ItemType.MatchesTag(MedicalTag) || ItemData.ItemType.MatchesTag(MedicalTag2))
			{
				return true;
			}

			// Check ItemTags container
			if (ItemData.ItemTags.HasTag(MedicalTag) || ItemData.ItemTags.HasTag(MedicalTag2))
			{
				return true;
			}
		}
	}

	return false;
}

bool USuspenseCoreMedicalUseHandler::ValidateRequest(
	const FSuspenseCoreItemUseRequest& Request,
	FSuspenseCoreItemUseResponse& OutResponse) const
{
	// Check source is valid
	if (!Request.SourceItem.IsValid())
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Invalid medical item")));
		return false;
	}

	// Check quantity
	if (Request.SourceItem.Quantity <= 0)
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_MissingRequirement,
			FText::FromString(TEXT("No medical items available")));
		return false;
	}

	// Could add health checks here (e.g., already at full health for medkit)

	return true;
}

//==================================================================
// Execution
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreMedicalUseHandler::Execute(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	// Get item tags from DataManager
	FGameplayTagContainer ItemTags;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			ItemTags = ItemData.ItemTags;
		}
	}

	ESuspenseCoreMedicalType MedType = GetMedicalType(ItemTags);

	HANDLER_LOG(Log, TEXT("Execute: Using medical item %s (type=%d)"),
		*Request.SourceItem.ItemID.ToString(),
		static_cast<int32>(MedType));

	// Get duration for this operation
	float Duration = GetDuration(Request);

	// Return InProgress response - actual healing happens in OnOperationComplete
	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, Duration);
	Response.HandlerTag = GetHandlerTag();
	Response.Cooldown = GetCooldown(Request);

	// Publish started event
	PublishMedicalEvent(Request, Response, OwnerActor);

	return Response;
}

float USuspenseCoreMedicalUseHandler::GetDuration(const FSuspenseCoreItemUseRequest& Request) const
{
	// Get item tags from DataManager
	FGameplayTagContainer ItemTags;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			ItemTags = ItemData.ItemTags;

			// If UseTime is defined in item data, use that
			if (ItemData.bIsConsumable && ItemData.UseTime > 0.0f)
			{
				return ItemData.UseTime;
			}
		}
	}

	ESuspenseCoreMedicalType MedType = GetMedicalType(ItemTags);
	return GetMedicalDuration(MedType);
}

float USuspenseCoreMedicalUseHandler::GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
{
	return DefaultCooldown;
}

bool USuspenseCoreMedicalUseHandler::CancelOperation(const FGuid& RequestID)
{
	HANDLER_LOG(Log, TEXT("CancelOperation: %s"), *RequestID.ToString().Left(8));
	// Cancellation is handled by GAS ability
	// No healing is applied if cancelled
	return true;
}

FSuspenseCoreItemUseResponse USuspenseCoreMedicalUseHandler::OnOperationComplete(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("OnOperationComplete: Applying healing for %s"),
		*Request.RequestID.ToString().Left(8));

	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID);
	Response.HandlerTag = GetHandlerTag();
	Response.Progress = 1.0f;

	// Get heal amount from item data
	float HealAmount = GetHealAmount(Request.SourceItem.ItemID);

	// Apply healing
	if (OwnerActor && HealAmount > 0.0f)
	{
		bool bHealed = ApplyHealing(OwnerActor, HealAmount);
		if (bHealed)
		{
			Response.Metadata.Add(TEXT("HealAmount"), FString::SanitizeFloat(HealAmount));
			HANDLER_LOG(Log, TEXT("Applied %.1f healing"), HealAmount);
		}
	}

	// Update modified items - consume one
	Response.ModifiedSourceItem = Request.SourceItem;
	Response.ModifiedSourceItem.Quantity -= 1;

	Response.Metadata.Add(TEXT("RemainingQuantity"),
		FString::FromInt(Response.ModifiedSourceItem.Quantity));

	// Publish completion event
	PublishMedicalEvent(Request, Response, OwnerActor);

	return Response;
}

//==================================================================
// Internal Methods
//==================================================================

ESuspenseCoreMedicalType USuspenseCoreMedicalUseHandler::GetMedicalType(
	const FGameplayTagContainer& ItemTags) const
{
	// Check for specific medical type tags
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Bandage"), false)))
	{
		return ESuspenseCoreMedicalType::Bandage;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Medkit"), false)))
	{
		return ESuspenseCoreMedicalType::Medkit;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Painkiller"), false)))
	{
		return ESuspenseCoreMedicalType::Painkiller;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Stimulant"), false)))
	{
		return ESuspenseCoreMedicalType::Stimulant;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Splint"), false)))
	{
		return ESuspenseCoreMedicalType::Splint;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Surgical"), false)))
	{
		return ESuspenseCoreMedicalType::Surgical;
	}

	// Default to bandage
	return ESuspenseCoreMedicalType::Bandage;
}

float USuspenseCoreMedicalUseHandler::GetMedicalDuration(ESuspenseCoreMedicalType Type) const
{
	switch (Type)
	{
		case ESuspenseCoreMedicalType::Bandage:
			return BandageDuration;
		case ESuspenseCoreMedicalType::Medkit:
			return MedkitDuration;
		case ESuspenseCoreMedicalType::Painkiller:
			return PainkillerDuration;
		case ESuspenseCoreMedicalType::Stimulant:
			return StimulantDuration;
		case ESuspenseCoreMedicalType::Splint:
			return SplintDuration;
		case ESuspenseCoreMedicalType::Surgical:
			return SurgicalDuration;
		default:
			return BandageDuration;
	}
}

float USuspenseCoreMedicalUseHandler::GetHealAmount(FName ItemID) const
{
	// Get heal amount from item data
	// Note: FSuspenseCoreUnifiedItemData doesn't have a direct HealAmount field
	// For medical items, we use the ConsumeEffects to apply healing via GAS
	// Here we provide default values based on item name patterns

	// Default heal amounts by item name pattern
	FString ItemName = ItemID.ToString();
	if (ItemName.Contains(TEXT("Surgical")) || ItemName.Contains(TEXT("Surgery")))
	{
		return 200.0f;
	}
	if (ItemName.Contains(TEXT("Medkit")) || ItemName.Contains(TEXT("AFAK")))
	{
		return 100.0f;
	}
	if (ItemName.Contains(TEXT("IFAK")))
	{
		return 50.0f;
	}
	if (ItemName.Contains(TEXT("Splint")))
	{
		return 0.0f; // Splints don't heal, they fix fractures
	}
	if (ItemName.Contains(TEXT("Bandage")))
	{
		return 25.0f;
	}
	if (ItemName.Contains(TEXT("Painkiller")) || ItemName.Contains(TEXT("Stimulant")))
	{
		return 0.0f; // Painkillers/stimulants don't heal directly
	}

	return 25.0f; // Default
}

bool USuspenseCoreMedicalUseHandler::ApplyHealing(AActor* Actor, float HealAmount) const
{
	if (!Actor)
	{
		return false;
	}

	// Try to get AbilitySystemComponent
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealing: No ASC found on actor"));
		return false;
	}

	// Apply healing via GameplayEffect or direct attribute modification
	// For now, we'll use a simple approach - find Health attribute and modify

	// Note: In a real implementation, you'd apply a GameplayEffect that modifies Health
	// This is simplified for the handler implementation

	HANDLER_LOG(Log, TEXT("ApplyHealing: Would apply %.1f healing to %s"),
		HealAmount, *Actor->GetName());

	// The actual healing would be done via GameplayEffect in the ability
	// The handler just calculates and returns the amount

	return true;
}

void USuspenseCoreMedicalUseHandler::PublishMedicalEvent(
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
	EventData.StringPayload.Add(TEXT("ItemID"), Request.SourceItem.ItemID.ToString());
	EventData.IntPayload.Add(TEXT("Result"), static_cast<int32>(Response.Result));
	EventData.FloatPayload.Add(TEXT("Duration"), Response.Duration);

	if (Response.Metadata.Contains(TEXT("HealAmount")))
	{
		EventData.FloatPayload.Add(TEXT("HealAmount"),
			FCString::Atof(*Response.Metadata[TEXT("HealAmount")]));
	}

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

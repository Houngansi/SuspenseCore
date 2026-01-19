// SuspenseCoreGrenadeHandler.cpp
// Handler for grenade prepare and throw
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreGrenadeHandler.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrenadeHandler, Log, All);

#define HANDLER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogGrenadeHandler, Verbosity, TEXT("[Grenade] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreGrenadeHandler::USuspenseCoreGrenadeHandler()
{
	PrepareDuration = 0.5f;
	ThrowCooldown = 1.0f;
	DefaultThrowForce = 1500.0f;
}

void USuspenseCoreGrenadeHandler::Initialize(
	USuspenseCoreDataManager* InDataManager,
	USuspenseCoreEventBus* InEventBus)
{
	DataManager = InDataManager;
	EventBus = InEventBus;

	// Subscribe to SpawnRequested events from GrenadeThrowAbility
	if (InEventBus)
	{
		SpawnRequestedHandle = InEventBus->SubscribeNative(
			SuspenseCoreTags::Event::Throwable::SpawnRequested,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGrenadeHandler::OnSpawnRequested),
			ESuspenseCoreEventPriority::High);

		HANDLER_LOG(Log, TEXT("Subscribed to SpawnRequested EventBus events"));
	}

	HANDLER_LOG(Log, TEXT("Initialized with DataManager=%s, EventBus=%s"),
		InDataManager ? TEXT("Valid") : TEXT("NULL"),
		InEventBus ? TEXT("Valid") : TEXT("NULL"));
}

void USuspenseCoreGrenadeHandler::Shutdown()
{
	// Unsubscribe from EventBus
	if (EventBus.IsValid() && SpawnRequestedHandle.IsValid())
	{
		EventBus->Unsubscribe(SpawnRequestedHandle);
		SpawnRequestedHandle.Invalidate();
		HANDLER_LOG(Log, TEXT("Unsubscribed from EventBus"));
	}
}

//==================================================================
// Handler Identity
//==================================================================

FGameplayTag USuspenseCoreGrenadeHandler::GetHandlerTag() const
{
	return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_Grenade;
}

ESuspenseCoreHandlerPriority USuspenseCoreGrenadeHandler::GetPriority() const
{
	return ESuspenseCoreHandlerPriority::High; // Higher priority for combat actions
}

FText USuspenseCoreGrenadeHandler::GetDisplayName() const
{
	return FText::FromString(TEXT("Throw Grenade"));
}

//==================================================================
// Supported Types
//==================================================================

FGameplayTagContainer USuspenseCoreGrenadeHandler::GetSupportedSourceTags() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Category.Grenade"), false));
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Grenade"), false));
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Throwable"), false));
	return Tags;
}

TArray<ESuspenseCoreItemUseContext> USuspenseCoreGrenadeHandler::GetSupportedContexts() const
{
	return {
		ESuspenseCoreItemUseContext::Hotkey,
		ESuspenseCoreItemUseContext::QuickSlot
	};
}

//==================================================================
// Validation
//==================================================================

bool USuspenseCoreGrenadeHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
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

	// Check if item has grenade/throwable tag by looking up in DataManager
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			FGameplayTagContainer SupportedTags = GetSupportedSourceTags();

			// Check ItemType tag
			if (SupportedTags.HasTag(ItemData.ItemType))
			{
				return true;
			}

			// Check bIsThrowable flag
			if (ItemData.bIsThrowable)
			{
				return true;
			}

			// Check ItemTags container
			if (ItemData.ItemTags.HasAny(SupportedTags))
			{
				return true;
			}
		}
	}

	return false;
}

bool USuspenseCoreGrenadeHandler::ValidateRequest(
	const FSuspenseCoreItemUseRequest& Request,
	FSuspenseCoreItemUseResponse& OutResponse) const
{
	// Check source is valid
	if (!Request.SourceItem.IsValid())
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Invalid grenade item")));
		return false;
	}

	// Check quantity
	if (Request.SourceItem.Quantity <= 0)
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_MissingRequirement,
			FText::FromString(TEXT("No grenades available")));
		return false;
	}

	return true;
}

//==================================================================
// Execution
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreGrenadeHandler::Execute(
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
			// Add throwable type tag if present
			if (ItemData.ThrowableType.IsValid())
			{
				ItemTags.AddTag(ItemData.ThrowableType);
			}
		}
	}

	ESuspenseCoreGrenadeType GrenadeType = GetGrenadeType(ItemTags);

	HANDLER_LOG(Log, TEXT("Execute: Activating GrenadeThrowAbility for %s (type=%d)"),
		*Request.SourceItem.ItemID.ToString(),
		static_cast<int32>(GrenadeType));

	// CRITICAL: Trigger GAS GrenadeThrowAbility instead of handling directly
	// The ability handles animation montage, AnimNotify events, and spawning via EventBus
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (ASC)
	{
		// Try to activate GrenadeThrowAbility by tag
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(SuspenseCoreTags::Ability::Throwable::Grenade);

		bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTags);

		if (bActivated)
		{
			HANDLER_LOG(Log, TEXT("GrenadeThrowAbility activated successfully"));

			// Return success with zero duration - ability handles timing internally
			FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, 0.0f);
			Response.HandlerTag = GetHandlerTag();
			Response.Cooldown = GetCooldown(Request);
			Response.Metadata.Add(TEXT("GrenadeType"), FString::FromInt(static_cast<int32>(GrenadeType)));
			Response.Metadata.Add(TEXT("ActivatedViaGAS"), TEXT("true"));

			return Response;
		}
		else
		{
			HANDLER_LOG(Warning, TEXT("Failed to activate GrenadeThrowAbility - ability not granted or blocked"));
		}
	}
	else
	{
		HANDLER_LOG(Warning, TEXT("No AbilitySystemComponent found on %s"), *GetNameSafe(OwnerActor));
	}

	// Fallback: Use legacy direct handling if GAS not available
	HANDLER_LOG(Log, TEXT("Falling back to legacy grenade handling"));

	float Duration = GetDuration(Request);
	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, Duration);
	Response.HandlerTag = GetHandlerTag();
	Response.Cooldown = GetCooldown(Request);
	Response.Metadata.Add(TEXT("GrenadeType"), FString::FromInt(static_cast<int32>(GrenadeType)));
	Response.Metadata.Add(TEXT("ActivatedViaGAS"), TEXT("false"));

	// Publish prepare started event
	PublishGrenadeEvent(Request, Response, OwnerActor);

	return Response;
}

float USuspenseCoreGrenadeHandler::GetDuration(const FSuspenseCoreItemUseRequest& Request) const
{
	// Get item tags from DataManager
	FGameplayTagContainer ItemTags;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			ItemTags = ItemData.ItemTags;
			// Add throwable type tag if present
			if (ItemData.ThrowableType.IsValid())
			{
				ItemTags.AddTag(ItemData.ThrowableType);
			}

			// If UseTime is defined in item data, use that for prepare time
			if (ItemData.bIsConsumable && ItemData.UseTime > 0.0f)
			{
				return ItemData.UseTime;
			}
		}
	}

	ESuspenseCoreGrenadeType GrenadeType = GetGrenadeType(ItemTags);
	return GetPrepareDuration(GrenadeType);
}

float USuspenseCoreGrenadeHandler::GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
{
	return ThrowCooldown;
}

bool USuspenseCoreGrenadeHandler::CancelOperation(const FGuid& RequestID)
{
	HANDLER_LOG(Log, TEXT("CancelOperation: %s - putting pin back"),
		*RequestID.ToString().Left(8));
	// Grenade was not thrown - pin put back
	return true;
}

FSuspenseCoreItemUseResponse USuspenseCoreGrenadeHandler::OnOperationComplete(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("OnOperationComplete: Throwing grenade for %s"),
		*Request.RequestID.ToString().Left(8));

	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID);
	Response.HandlerTag = GetHandlerTag();
	Response.Progress = 1.0f;

	// Throw the grenade
	bool bThrown = ThrowGrenade(Request, OwnerActor);

	if (bThrown)
	{
		// Update modified items - consume one grenade
		Response.ModifiedSourceItem = Request.SourceItem;
		Response.ModifiedSourceItem.Quantity -= 1;

		Response.Metadata.Add(TEXT("RemainingQuantity"),
			FString::FromInt(Response.ModifiedSourceItem.Quantity));

		HANDLER_LOG(Log, TEXT("Grenade thrown successfully. %d remaining"),
			Response.ModifiedSourceItem.Quantity);
	}
	else
	{
		Response.Result = ESuspenseCoreItemUseResult::Failed_SystemError;
		Response.Message = FText::FromString(TEXT("Failed to throw grenade"));

		HANDLER_LOG(Warning, TEXT("Failed to throw grenade"));
	}

	// Publish completion event
	PublishGrenadeEvent(Request, Response, OwnerActor);

	return Response;
}

//==================================================================
// Internal Methods
//==================================================================

ESuspenseCoreGrenadeType USuspenseCoreGrenadeHandler::GetGrenadeType(
	const FGameplayTagContainer& ItemTags) const
{
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Grenade.Smoke"), false)))
	{
		return ESuspenseCoreGrenadeType::Smoke;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Grenade.Flashbang"), false)))
	{
		return ESuspenseCoreGrenadeType::Flashbang;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Grenade.Incendiary"), false)))
	{
		return ESuspenseCoreGrenadeType::Incendiary;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Grenade.Impact"), false)))
	{
		return ESuspenseCoreGrenadeType::Impact;
	}

	// Default to fragmentation
	return ESuspenseCoreGrenadeType::Fragmentation;
}

float USuspenseCoreGrenadeHandler::GetPrepareDuration(ESuspenseCoreGrenadeType Type) const
{
	// All grenades have same prepare time by default
	// Could be customized per type if needed
	return PrepareDuration;
}

bool USuspenseCoreGrenadeHandler::ThrowGrenade(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	if (!OwnerActor)
	{
		return false;
	}

	// Get throw direction from character view
	ACharacter* Character = Cast<ACharacter>(OwnerActor);
	FVector ThrowDirection = OwnerActor->GetActorForwardVector();
	FVector ThrowLocation = OwnerActor->GetActorLocation();

	if (Character)
	{
		// Use camera/view direction for aiming
		FRotator ViewRotation = Character->GetControlRotation();
		ThrowDirection = ViewRotation.Vector();

		// Offset spawn location to hand position
		ThrowLocation += Character->GetActorForwardVector() * 50.0f;
		ThrowLocation += FVector::UpVector * 50.0f;
	}

	// Get grenade actor class from UnifiedItemData
	TSubclassOf<AActor> GrenadeClass = nullptr;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			// Use EquipmentActorClass for throwables
			if (!ItemData.EquipmentActorClass.IsNull())
			{
				GrenadeClass = ItemData.EquipmentActorClass.LoadSynchronous();
			}
		}
	}

	if (!GrenadeClass)
	{
		HANDLER_LOG(Warning, TEXT("ThrowGrenade: No actor class found for %s"),
			*Request.SourceItem.ItemID.ToString());

		// Even without spawning, we consider the grenade "thrown" for gameplay
		// This allows the system to work without actual grenade actors
		return true;
	}

	// Spawn grenade actor
	UWorld* World = OwnerActor->GetWorld();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Grenade = World->SpawnActor<AActor>(GrenadeClass, ThrowLocation, ThrowDirection.Rotation(), SpawnParams);
	if (!Grenade)
	{
		return false;
	}

	// Apply throw force if has physics
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Grenade->GetRootComponent());
	if (PrimComp && PrimComp->IsSimulatingPhysics())
	{
		FVector ThrowVelocity = ThrowDirection * DefaultThrowForce;
		PrimComp->AddImpulse(ThrowVelocity, NAME_None, true);
	}

	HANDLER_LOG(Log, TEXT("Spawned grenade %s at %s"),
		*Grenade->GetName(),
		*ThrowLocation.ToString());

	return true;
}

void USuspenseCoreGrenadeHandler::PublishGrenadeEvent(
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
	EventData.StringPayload.Add(TEXT("GrenadeID"), Request.SourceItem.ItemID.ToString());
	EventData.IntPayload.Add(TEXT("Result"), static_cast<int32>(Response.Result));
	EventData.FloatPayload.Add(TEXT("Duration"), Response.Duration);

	if (Response.Metadata.Contains(TEXT("GrenadeType")))
	{
		EventData.IntPayload.Add(TEXT("GrenadeType"),
			FCString::Atoi(*Response.Metadata[TEXT("GrenadeType")]));
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

//==================================================================
// EventBus Callbacks
//==================================================================

void USuspenseCoreGrenadeHandler::OnSpawnRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract parameters from GrenadeThrowAbility
	AActor* OwnerActor = Cast<AActor>(EventData.Source.Get());
	if (!OwnerActor)
	{
		HANDLER_LOG(Warning, TEXT("OnSpawnRequested: No owner actor in event"));
		return;
	}

	// Get grenade ID
	FName GrenadeID = NAME_None;
	if (const FString* GrenadeIDStr = EventData.StringPayload.Find(TEXT("GrenadeID")))
	{
		GrenadeID = FName(**GrenadeIDStr);
	}

	if (GrenadeID.IsNone())
	{
		HANDLER_LOG(Warning, TEXT("OnSpawnRequested: No GrenadeID in event"));
		return;
	}

	// Get throw parameters
	FVector ThrowLocation = FVector::ZeroVector;
	if (const FVector* Location = EventData.VectorPayload.Find(TEXT("ThrowLocation")))
	{
		ThrowLocation = *Location;
	}
	else
	{
		// Fallback to actor location
		ThrowLocation = OwnerActor->GetActorLocation() + FVector(0, 0, 50);
	}

	FVector ThrowDirection = OwnerActor->GetActorForwardVector();
	if (const FVector* Direction = EventData.VectorPayload.Find(TEXT("ThrowDirection")))
	{
		ThrowDirection = *Direction;
	}

	float ThrowForce = DefaultThrowForce;
	if (const float* Force = EventData.FloatPayload.Find(TEXT("ThrowForce")))
	{
		ThrowForce = *Force;
	}

	float CookTime = 0.0f;
	if (const float* Cook = EventData.FloatPayload.Find(TEXT("CookTime")))
	{
		CookTime = *Cook;
	}

	HANDLER_LOG(Log, TEXT("OnSpawnRequested: Spawning %s, Force=%.0f, CookTime=%.2f"),
		*GrenadeID.ToString(), ThrowForce, CookTime);

	// Spawn the grenade
	ThrowGrenadeFromEvent(OwnerActor, GrenadeID, ThrowLocation, ThrowDirection, ThrowForce, CookTime);
}

bool USuspenseCoreGrenadeHandler::ThrowGrenadeFromEvent(
	AActor* OwnerActor,
	FName GrenadeID,
	const FVector& ThrowLocation,
	const FVector& ThrowDirection,
	float ThrowForce,
	float CookTime)
{
	if (!OwnerActor)
	{
		return false;
	}

	// Get grenade actor class from UnifiedItemData
	TSubclassOf<AActor> GrenadeClass = nullptr;
	float FuseTime = 3.5f; // Default fuse time

	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(GrenadeID, ItemData))
		{
			// Use EquipmentActorClass for throwables
			if (!ItemData.EquipmentActorClass.IsNull())
			{
				GrenadeClass = ItemData.EquipmentActorClass.LoadSynchronous();
			}
		}

		// Get fuse time from ThrowableAttributes if available
		// The grenade actor should handle this, but we can pass it via event
	}

	if (!GrenadeClass)
	{
		HANDLER_LOG(Warning, TEXT("ThrowGrenadeFromEvent: No actor class found for %s"),
			*GrenadeID.ToString());

		// Even without spawning, we consider the grenade "thrown" for gameplay
		return true;
	}

	// Spawn grenade actor
	UWorld* World = OwnerActor->GetWorld();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Grenade = World->SpawnActor<AActor>(GrenadeClass, ThrowLocation, ThrowDirection.Rotation(), SpawnParams);
	if (!Grenade)
	{
		HANDLER_LOG(Warning, TEXT("ThrowGrenadeFromEvent: Failed to spawn grenade actor"));
		return false;
	}

	// Apply throw force if has physics
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Grenade->GetRootComponent());
	if (PrimComp && PrimComp->IsSimulatingPhysics())
	{
		FVector ThrowVelocity = ThrowDirection * ThrowForce;
		PrimComp->AddImpulse(ThrowVelocity, NAME_None, true);
	}

	// If grenade has interface, pass cook time (fuse reduction)
	// The grenade actor should subtract CookTime from its FuseTime
	// This is typically handled by the grenade's BeginPlay or a custom interface

	HANDLER_LOG(Log, TEXT("Spawned grenade %s at %s (CookTime=%.2f reduced from fuse)"),
		*Grenade->GetName(),
		*ThrowLocation.ToString(),
		CookTime);

	return true;
}

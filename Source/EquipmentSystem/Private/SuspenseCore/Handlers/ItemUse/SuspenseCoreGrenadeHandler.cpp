// SuspenseCoreGrenadeHandler.cpp
// Handler for grenade prepare and throw
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreGrenadeHandler.h"
#include "SuspenseCore/Actors/SuspenseCoreGrenadeProjectile.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Abilities/Throwable/SuspenseCoreGrenadeEquipAbility.h"
#include "SuspenseCore/Abilities/Throwable/SuspenseCoreGrenadeThrowAbility.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreActorFactory.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

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
	USuspenseCoreEventBus* InEventBus,
	USuspenseCoreEquipmentServiceLocator* InServiceLocator)
{
	DataManager = InDataManager;
	EventBus = InEventBus;
	ServiceLocator = InServiceLocator;

	// ═══════════════════════════════════════════════════════════════════
	// ACTOR FACTORY SETUP - For object pooling to eliminate microfreeze
	// ═══════════════════════════════════════════════════════════════════
	Tag_ActorFactory = SuspenseCoreEquipmentTags::Service::TAG_Service_ActorFactory;

	if (InServiceLocator)
	{
		if (UObject* FactoryObj = InServiceLocator->TryGetService(Tag_ActorFactory))
		{
			if (FactoryObj->GetClass()->ImplementsInterface(USuspenseCoreActorFactory::StaticClass()))
			{
				CachedActorFactory = static_cast<ISuspenseCoreActorFactory*>(
					FactoryObj->GetInterfaceAddress(USuspenseCoreActorFactory::StaticClass()));

				if (CachedActorFactory)
				{
					HANDLER_LOG(Log, TEXT("ActorFactory acquired for grenade pooling"));

					// Preload grenade classes to avoid microfreeze on first use
					PreloadGrenadeClasses();
				}
			}
		}

		if (!CachedActorFactory)
		{
			HANDLER_LOG(Warning, TEXT("ActorFactory not available - grenades will spawn without pooling (may cause microfreeze)"));
		}
	}

	// Subscribe to EventBus events
	if (InEventBus)
	{
		UE_LOG(LogGrenadeHandler, Warning, TEXT(">>> GrenadeHandler: Subscribing to EventBus events <<<"));

		// Subscribe to SpawnRequested events from GrenadeThrowAbility
		SpawnRequestedHandle = InEventBus->SubscribeNative(
			SuspenseCoreTags::Event::Throwable::SpawnRequested,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGrenadeHandler::OnSpawnRequested),
			ESuspenseCoreEventPriority::High);

		// Subscribe to grenade equipped event (spawn visual)
		FGameplayTag EquippedTag = SuspenseCoreTags::Event::Throwable::Equipped;
		UE_LOG(LogGrenadeHandler, Warning, TEXT("    Subscribing to: %s"), *EquippedTag.ToString());
		EquippedHandle = InEventBus->SubscribeNative(
			EquippedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGrenadeHandler::OnGrenadeEquipped),
			ESuspenseCoreEventPriority::High);

		// Subscribe to grenade unequipped event (destroy visual)
		FGameplayTag UnequippedTag = SuspenseCoreTags::Event::Throwable::Unequipped;
		UE_LOG(LogGrenadeHandler, Warning, TEXT("    Subscribing to: %s"), *UnequippedTag.ToString());
		UnequippedHandle = InEventBus->SubscribeNative(
			UnequippedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGrenadeHandler::OnGrenadeUnequipped),
			ESuspenseCoreEventPriority::High);

		// Subscribe to grenade releasing event (hide visual before throw)
		FGameplayTag ReleasingTag = SuspenseCoreTags::Event::Throwable::Releasing;
		UE_LOG(LogGrenadeHandler, Warning, TEXT("    Subscribing to: %s"), *ReleasingTag.ToString());
		ReleasingHandle = InEventBus->SubscribeNative(
			ReleasingTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGrenadeHandler::OnGrenadeReleasing),
			ESuspenseCoreEventPriority::High);

		UE_LOG(LogGrenadeHandler, Warning, TEXT(">>> GrenadeHandler: Subscriptions complete (Equipped=%s, Unequipped=%s, Releasing=%s) <<<"),
			EquippedHandle.IsValid() ? TEXT("Valid") : TEXT("INVALID"),
			UnequippedHandle.IsValid() ? TEXT("Valid") : TEXT("INVALID"),
			ReleasingHandle.IsValid() ? TEXT("Valid") : TEXT("INVALID"));

		HANDLER_LOG(Log, TEXT("Subscribed to EventBus events (SpawnRequested, Equipped, Unequipped, Releasing)"));
	}
	else
	{
		UE_LOG(LogGrenadeHandler, Error, TEXT(">>> GrenadeHandler: NO EVENTBUS - Cannot subscribe! <<<"));
	}

	HANDLER_LOG(Log, TEXT("Initialized with DataManager=%s, EventBus=%s"),
		InDataManager ? TEXT("Valid") : TEXT("NULL"),
		InEventBus ? TEXT("Valid") : TEXT("NULL"));
}

void USuspenseCoreGrenadeHandler::Shutdown()
{
	// Unsubscribe from EventBus
	if (EventBus.IsValid())
	{
		if (SpawnRequestedHandle.IsValid())
		{
			EventBus->Unsubscribe(SpawnRequestedHandle);
			SpawnRequestedHandle.Invalidate();
		}

		if (EquippedHandle.IsValid())
		{
			EventBus->Unsubscribe(EquippedHandle);
			EquippedHandle.Invalidate();
		}

		if (UnequippedHandle.IsValid())
		{
			EventBus->Unsubscribe(UnequippedHandle);
			UnequippedHandle.Invalidate();
		}

		if (ReleasingHandle.IsValid())
		{
			EventBus->Unsubscribe(ReleasingHandle);
			ReleasingHandle.Invalidate();
		}

		HANDLER_LOG(Log, TEXT("Unsubscribed from EventBus"));
	}

	// Recycle any remaining visual grenades to pool
	for (auto& Pair : VisualGrenades)
	{
		if (AActor* Visual = Pair.Value.Get())
		{
			RecycleGrenadeToPool(Visual);
		}
	}
	VisualGrenades.Empty();

	// Clear ActorFactory reference
	CachedActorFactory = nullptr;
}

//==================================================================
// Pooling Support
//==================================================================

void USuspenseCoreGrenadeHandler::PreloadGrenadeClasses()
{
	if (!CachedActorFactory || !DataManager.IsValid())
	{
		return;
	}

	HANDLER_LOG(Log, TEXT("Preloading grenade classes for pool..."));

	// Get all throwable item IDs from DataManager
	TArray<FName> ThrowableItemIds;

	// Common grenade IDs to preload (can be expanded from SSOT/Settings)
	static const TArray<FName> CommonGrenadeIds = {
		FName("Throwable_F1"),
		FName("Throwable_RGD5"),
		FName("Throwable_M67"),
		FName("Grenade_Frag"),
		FName("Grenade_Smoke"),
		FName("Grenade_Flash")
	};

	for (const FName& GrenadeId : CommonGrenadeIds)
	{
		// Verify item exists in DataManager
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(GrenadeId, ItemData))
		{
			if (!ItemData.EquipmentActorClass.IsNull())
			{
				ThrowableItemIds.Add(GrenadeId);
			}
		}
	}

	// Preload via ActorFactory
	for (const FName& ItemId : ThrowableItemIds)
	{
		if (CachedActorFactory->PreloadActorClass(ItemId))
		{
			HANDLER_LOG(Log, TEXT("  Preloaded: %s"), *ItemId.ToString());
		}
	}

	HANDLER_LOG(Log, TEXT("Preloaded %d grenade classes"), ThrowableItemIds.Num());
}

AActor* USuspenseCoreGrenadeHandler::SpawnGrenadeFromPool(
	TSubclassOf<AActor> GrenadeClass,
	const FTransform& SpawnTransform,
	AActor* Owner)
{
	if (!GrenadeClass)
	{
		return nullptr;
	}

	// ═══════════════════════════════════════════════════════════════════
	// LAZY INIT: Try to get ActorFactory if not cached yet
	// (ActorFactory may not be ready during GrenadeHandler::Initialize)
	// ═══════════════════════════════════════════════════════════════════
	if (!CachedActorFactory && ServiceLocator.IsValid())
	{
		if (UObject* FactoryObj = ServiceLocator->TryGetService(Tag_ActorFactory))
		{
			if (FactoryObj->GetClass()->ImplementsInterface(USuspenseCoreActorFactory::StaticClass()))
			{
				CachedActorFactory = static_cast<ISuspenseCoreActorFactory*>(
					FactoryObj->GetInterfaceAddress(USuspenseCoreActorFactory::StaticClass()));

				if (CachedActorFactory)
				{
					HANDLER_LOG(Log, TEXT("ActorFactory acquired via lazy init"));
				}
			}
		}
	}

	// Try pooled spawn via ActorFactory
	if (CachedActorFactory)
	{
		// First try to get from pool
		if (AActor* PooledActor = CachedActorFactory->GetPooledActor(GrenadeClass))
		{
			// Reset transform
			PooledActor->SetActorTransform(SpawnTransform);
			PooledActor->SetOwner(Owner);

			HANDLER_LOG(Log, TEXT("Got grenade from pool: %s"), *PooledActor->GetName());
			return PooledActor;
		}
	}

	// Fallback: Direct spawn (causes microfreeze on first spawn)
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Cast<APawn>(Owner);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(GrenadeClass, SpawnTransform, SpawnParams);

	if (SpawnedActor)
	{
		HANDLER_LOG(Log, TEXT("Spawned grenade directly (no pool): %s"), *SpawnedActor->GetName());
	}

	return SpawnedActor;
}

void USuspenseCoreGrenadeHandler::RecycleGrenadeToPool(AActor* GrenadeActor)
{
	if (!GrenadeActor)
	{
		return;
	}

	// ═══════════════════════════════════════════════════════════════════
	// LAZY INIT: Try to get ActorFactory if not cached yet
	// ═══════════════════════════════════════════════════════════════════
	if (!CachedActorFactory && ServiceLocator.IsValid())
	{
		if (UObject* FactoryObj = ServiceLocator->TryGetService(Tag_ActorFactory))
		{
			if (FactoryObj->GetClass()->ImplementsInterface(USuspenseCoreActorFactory::StaticClass()))
			{
				CachedActorFactory = static_cast<ISuspenseCoreActorFactory*>(
					FactoryObj->GetInterfaceAddress(USuspenseCoreActorFactory::StaticClass()));

				if (CachedActorFactory)
				{
					HANDLER_LOG(Log, TEXT("ActorFactory acquired via lazy init (recycle)"));
				}
			}
		}
	}

	// Try to recycle via ActorFactory
	if (CachedActorFactory)
	{
		if (CachedActorFactory->RecycleActor(GrenadeActor))
		{
			HANDLER_LOG(Log, TEXT("Recycled grenade to pool: %s"), *GrenadeActor->GetName());
			return;
		}
	}

	// Fallback: Direct destroy
	HANDLER_LOG(Log, TEXT("Destroying grenade (no pool): %s"), *GrenadeActor->GetName());
	GrenadeActor->Destroy();
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
	// Hotkey: Used for throw when grenade is equipped (Fire input routed here)
	// QuickSlot: Used for equipping grenade from QuickSlot
	// Programmatic: Used for AI or script-triggered throws
	return {
		ESuspenseCoreItemUseContext::Hotkey,
		ESuspenseCoreItemUseContext::QuickSlot,
		ESuspenseCoreItemUseContext::Programmatic
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

	// Must have source item with valid ItemID
	if (Request.SourceItem.ItemID.IsNone())
	{
		return false;
	}

	// Primary: Check via DataManager (authoritative SSOT lookup)
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

	// Fallback: Check ItemID naming convention when DataManager unavailable
	// This matches the item database convention: Throwable_F1, Throwable_RGD5, etc.
	FString ItemIDString = Request.SourceItem.ItemID.ToString();
	if (ItemIDString.StartsWith(TEXT("Throwable_")) ||
		ItemIDString.StartsWith(TEXT("Grenade_")) ||
		ItemIDString.Contains(TEXT("Grenade")))
	{
		HANDLER_LOG(Log, TEXT("CanHandle: Matched item '%s' via naming convention fallback"), *ItemIDString);
		return true;
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
	// TARKOV-STYLE TWO-PHASE GRENADE FLOW:
	// Context == QuickSlot → EQUIP grenade (draw animation, change stance)
	// Context == Hotkey → THROW if equipped, or instant throw if not
	// Context == Programmatic → AI/script-triggered throw

	HANDLER_LOG(Log, TEXT("Execute: Context=%d, GrenadeID=%s"),
		static_cast<int32>(Request.Context),
		*Request.SourceItem.ItemID.ToString());

	switch (Request.Context)
	{
		case ESuspenseCoreItemUseContext::QuickSlot:
		{
			// PHASE 1: Equip grenade from QuickSlot
			// Check if already equipped - if so, unequip instead
			if (IsGrenadeEquipped(OwnerActor))
			{
				HANDLER_LOG(Log, TEXT("Grenade already equipped - requesting unequip"));
				// TODO: Request unequip via EventBus or cancel ability
				return FSuspenseCoreItemUseResponse::Failure(
					Request.RequestID,
					ESuspenseCoreItemUseResult::Failed_NotUsable,
					FText::FromString(TEXT("Grenade already equipped")));
			}

			return ExecuteEquip(Request, OwnerActor);
		}

		case ESuspenseCoreItemUseContext::Hotkey:
		{
			// PHASE 2: Throw grenade
			// If State.GrenadeEquipped is present, this is a throw from equipped state
			// If not present but request has grenade, this is a legacy instant throw
			if (IsGrenadeEquipped(OwnerActor))
			{
				HANDLER_LOG(Log, TEXT("Hotkey context - throwing equipped grenade"));
			}
			else
			{
				HANDLER_LOG(Log, TEXT("Hotkey context - instant throw (legacy)"));
			}
			return ExecuteThrow(Request, OwnerActor);
		}

		case ESuspenseCoreItemUseContext::Programmatic:
		{
			// AI/script-triggered throw
			HANDLER_LOG(Log, TEXT("Programmatic context - throwing grenade"));
			return ExecuteThrow(Request, OwnerActor);
		}

		default:
		{
			HANDLER_LOG(Warning, TEXT("Unsupported context: %d"), static_cast<int32>(Request.Context));
			return FSuspenseCoreItemUseResponse::Failure(
				Request.RequestID,
				ESuspenseCoreItemUseResult::Failed_NotUsable,
				FText::FromString(TEXT("Unsupported context")));
		}
	}
}

//==================================================================
// Internal Methods - Ability Activation
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreGrenadeHandler::ExecuteEquip(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("ExecuteEquip: Equipping grenade %s"), *Request.SourceItem.ItemID.ToString());

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("ExecuteEquip: No ASC found on %s"), *GetNameSafe(OwnerActor));
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_SystemError,
			FText::FromString(TEXT("No ability system")));
	}

	// Get grenade type tag from item data
	FGameplayTag GrenadeTypeTag = SuspenseCoreTags::Weapon::Grenade::Frag; // Default
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			if (ItemData.ThrowableType.IsValid())
			{
				GrenadeTypeTag = ItemData.ThrowableType;
			}
		}
	}

	// Use HandleGameplayEvent to pass grenade data to the ability
	// This works with InstancedPerExecution - data is passed via FGameplayEventData
	// GrenadeID will be looked up from QuickSlot component using the slot index
	FGameplayEventData EventData;
	EventData.EventTag = GrenadeTypeTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;
	EventData.EventMagnitude = static_cast<float>(Request.QuickSlotIndex);

	// Add the grenade type tag to InstigatorTags
	EventData.InstigatorTags.AddTag(GrenadeTypeTag);

	HANDLER_LOG(Log, TEXT("Sending GameplayEvent with GrenadeID=%s, Type=%s, Slot=%d"),
		*Request.SourceItem.ItemID.ToString(),
		*GrenadeTypeTag.ToString(),
		Request.QuickSlotIndex);

	// Send event to trigger the ability
	// The ability has Ability.Throwable.Equip tag, so we use that as the event trigger
	FGameplayTag EquipEventTag = SuspenseCoreTags::Ability::Throwable::Equip;
	int32 TriggeredCount = ASC->HandleGameplayEvent(EquipEventTag, &EventData);

	if (TriggeredCount > 0)
	{
		HANDLER_LOG(Log, TEXT("GA_GrenadeEquip triggered via GameplayEvent (%d abilities)"), TriggeredCount);

		FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, 0.0f);
		Response.HandlerTag = GetHandlerTag();
		Response.Cooldown = 0.0f;
		Response.Metadata.Add(TEXT("Phase"), TEXT("Equip"));
		Response.Metadata.Add(TEXT("GrenadeID"), Request.SourceItem.ItemID.ToString());

		return Response;
	}

	// Fallback: Try direct activation with TryActivateAbilitiesByTag
	HANDLER_LOG(Log, TEXT("ExecuteEquip: Event trigger failed, trying direct activation"));

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(SuspenseCoreTags::Ability::Throwable::Equip);

	bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTags);

	if (bActivated)
	{
		HANDLER_LOG(Log, TEXT("GA_GrenadeEquip activated via TryActivateAbilitiesByTag"));

		FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, 0.0f);
		Response.HandlerTag = GetHandlerTag();
		Response.Cooldown = 0.0f;
		Response.Metadata.Add(TEXT("Phase"), TEXT("Equip"));
		Response.Metadata.Add(TEXT("GrenadeID"), Request.SourceItem.ItemID.ToString());

		return Response;
	}
	else
	{
		HANDLER_LOG(Warning, TEXT("Failed to activate GA_GrenadeEquip"));
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Cannot equip grenade")));
	}
}

FSuspenseCoreItemUseResponse USuspenseCoreGrenadeHandler::ExecuteThrow(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	// Get item tags for grenade type
	FGameplayTagContainer ItemTags;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			ItemTags = ItemData.ItemTags;
			if (ItemData.ThrowableType.IsValid())
			{
				ItemTags.AddTag(ItemData.ThrowableType);
			}
		}
	}

	ESuspenseCoreGrenadeType GrenadeType = GetGrenadeType(ItemTags);

	HANDLER_LOG(Log, TEXT("ExecuteThrow: Throwing grenade %s (type=%d)"),
		*Request.SourceItem.ItemID.ToString(),
		static_cast<int32>(GrenadeType));

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("ExecuteThrow: No ASC found on %s"), *GetNameSafe(OwnerActor));
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_SystemError,
			FText::FromString(TEXT("No ability system")));
	}

	// Tarkov-style flow: Set grenade info on the throw ability before activation
	// Find the GrenadeThrowAbility and set grenade info
	FGameplayTag ThrowTag = SuspenseCoreTags::Ability::Throwable::Grenade;
	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(ThrowTag), MatchingSpecs, false);

	for (FGameplayAbilitySpec* Spec : MatchingSpecs)
	{
		if (Spec && Spec->Ability)
		{
			// Get the primary instanced ability (if instanced) or CDO
			USuspenseCoreGrenadeThrowAbility* ThrowAbility = nullptr;

			if (Spec->IsActive())
			{
				// Already active - shouldn't happen, but handle it
				HANDLER_LOG(Warning, TEXT("ExecuteThrow: Ability already active"));
				continue;
			}

			// For InstancedPerActor, get the instanced ability
			if (Spec->GetPrimaryInstance())
			{
				ThrowAbility = Cast<USuspenseCoreGrenadeThrowAbility>(Spec->GetPrimaryInstance());
			}
			else
			{
				// For non-instanced, we need to use the CDO - but this won't persist
				// Instead, we'll pass data via FGameplayEventData
				ThrowAbility = Cast<USuspenseCoreGrenadeThrowAbility>(Spec->Ability);
			}

			if (ThrowAbility)
			{
				// Set grenade info before activation
				ThrowAbility->SetGrenadeInfo(
					Request.SourceItem.ItemID,
					Request.QuickSlotIndex);

				HANDLER_LOG(Log, TEXT("Set grenade info on ThrowAbility: ID=%s, Slot=%d"),
					*Request.SourceItem.ItemID.ToString(),
					Request.QuickSlotIndex);

				// Activate this specific ability spec
				bool bActivated = ASC->TryActivateAbility(Spec->Handle);

				if (bActivated)
				{
					HANDLER_LOG(Log, TEXT("GA_GrenadeThrow activated successfully (Tarkov-style)"));

					FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, 0.0f);
					Response.HandlerTag = GetHandlerTag();
					Response.Cooldown = GetCooldown(Request);
					Response.Metadata.Add(TEXT("Phase"), TEXT("Throw"));
					Response.Metadata.Add(TEXT("GrenadeType"), FString::FromInt(static_cast<int32>(GrenadeType)));
					Response.Metadata.Add(TEXT("ActivatedViaGAS"), TEXT("true"));
					Response.Metadata.Add(TEXT("Flow"), TEXT("Tarkov-style"));

					return Response;
				}
			}
		}
	}

	// Fallback: Try tag-based activation if spec search failed
	HANDLER_LOG(Log, TEXT("ExecuteThrow: Falling back to tag-based activation"));

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(SuspenseCoreTags::Ability::Throwable::Grenade);

	bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTags);

	if (bActivated)
	{
		HANDLER_LOG(Log, TEXT("GA_GrenadeThrow activated successfully (fallback)"));

		FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, 0.0f);
		Response.HandlerTag = GetHandlerTag();
		Response.Cooldown = GetCooldown(Request);
		Response.Metadata.Add(TEXT("Phase"), TEXT("Throw"));
		Response.Metadata.Add(TEXT("GrenadeType"), FString::FromInt(static_cast<int32>(GrenadeType)));
		Response.Metadata.Add(TEXT("ActivatedViaGAS"), TEXT("true"));

		return Response;
	}
	else
	{
		HANDLER_LOG(Warning, TEXT("Failed to activate GA_GrenadeThrow"));
		return FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Cannot throw grenade")));
	}
}

bool USuspenseCoreGrenadeHandler::IsGrenadeEquipped(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return false;
	}

	// Check for State.GrenadeEquipped tag
	FGameplayTag EquippedTag = FGameplayTag::RequestGameplayTag(FName("State.GrenadeEquipped"));
	return ASC->HasMatchingGameplayTag(EquippedTag);
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

	HANDLER_LOG(Log, TEXT("OnSpawnRequested: Spawning %s at Location=(%.1f, %.1f, %.1f), Force=%.0f, CookTime=%.2f"),
		*GrenadeID.ToString(), ThrowLocation.X, ThrowLocation.Y, ThrowLocation.Z, ThrowForce, CookTime);

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

	// ═══════════════════════════════════════════════════════════════════
	// POOLED SPAWN - Eliminates microfreeze on throw
	// ═══════════════════════════════════════════════════════════════════
	FTransform SpawnTransform(ThrowDirection.Rotation(), ThrowLocation);
	AActor* Grenade = SpawnGrenadeFromPool(GrenadeClass, SpawnTransform, OwnerActor);
	if (!Grenade)
	{
		HANDLER_LOG(Warning, TEXT("ThrowGrenadeFromEvent: Failed to spawn grenade from pool"));
		return false;
	}

	// Reset actor state if it came from pool (enable physics/collision for thrown grenade)
	Grenade->SetActorHiddenInGame(false);
	Grenade->SetActorEnableCollision(true);

	// Calculate throw velocity (direction * force)
	FVector ThrowVelocity = ThrowDirection * ThrowForce;

	HANDLER_LOG(Log, TEXT("Grenade class: %s, Velocity: %s"), *Grenade->GetClass()->GetName(), *ThrowVelocity.ToString());

	// Initialize grenade if it's our projectile class (uses ProjectileMovementComponent)
	if (ASuspenseCoreGrenadeProjectile* GrenadeProjectile = Cast<ASuspenseCoreGrenadeProjectile>(Grenade))
	{
		// ═══════════════════════════════════════════════════════════════════
		// REACTIVATE PROJECTILE MOVEMENT (may have been deactivated for visual)
		// ═══════════════════════════════════════════════════════════════════
		if (UProjectileMovementComponent* ProjectileMovement = GrenadeProjectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			if (!ProjectileMovement->IsActive())
			{
				ProjectileMovement->Activate(true);
				HANDLER_LOG(Log, TEXT("Reactivated ProjectileMovement for thrown grenade"));
			}
		}

		// ═══════════════════════════════════════════════════════════════════
		// SSOT INITIALIZATION - Load attributes from DataManager
		// ═══════════════════════════════════════════════════════════════════
		if (DataManager.IsValid())
		{
			FSuspenseCoreThrowableAttributeRow ThrowableAttributes;
			if (DataManager->GetThrowableAttributes(GrenadeID, ThrowableAttributes))
			{
				// Initialize from SSOT first (sets damage, radius, VFX, audio, camera shake)
				GrenadeProjectile->InitializeFromSSOT(ThrowableAttributes);
				HANDLER_LOG(Log, TEXT("Loaded SSOT attributes for %s"), *GrenadeID.ToString());
			}
			else
			{
				HANDLER_LOG(Warning, TEXT("No SSOT attributes found for %s - using Blueprint defaults"), *GrenadeID.ToString());
			}
		}

		// InitializeGrenade sets velocity, arms grenade, reduces fuse by cook time
		GrenadeProjectile->InitializeGrenade(OwnerActor, ThrowVelocity, CookTime, GrenadeID);

		HANDLER_LOG(Warning, TEXT(">>> InitializeGrenade called! Grenade=%s, Armed=%s"),
			*Grenade->GetName(),
			GrenadeProjectile->IsArmed() ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		HANDLER_LOG(Warning, TEXT(">>> Cast to ASuspenseCoreGrenadeProjectile FAILED! Using physics fallback"));

		// Fallback for non-projectile grenades: apply physics impulse
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Grenade->GetRootComponent());
		if (PrimComp)
		{
			// Force enable physics if not already
			if (!PrimComp->IsSimulatingPhysics())
			{
				PrimComp->SetSimulatePhysics(true);
			}
			PrimComp->AddImpulse(ThrowVelocity, NAME_None, true);

			HANDLER_LOG(Log, TEXT("Applied physics impulse to grenade %s: Velocity=%s"),
				*Grenade->GetName(),
				*ThrowVelocity.ToString());
		}
	}

	HANDLER_LOG(Log, TEXT("Spawned grenade %s at %s (CookTime=%.2f reduced from fuse)"),
		*Grenade->GetName(),
		*ThrowLocation.ToString(),
		CookTime);

	return true;
}

//==================================================================
// Visual Grenade Spawn/Destroy
//==================================================================

void USuspenseCoreGrenadeHandler::OnGrenadeEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogGrenadeHandler, Warning, TEXT(">>> OnGrenadeEquipped EVENT RECEIVED <<<"));

	AActor* Character = Cast<AActor>(EventData.Source.Get());
	if (!Character)
	{
		HANDLER_LOG(Warning, TEXT("OnGrenadeEquipped: No character in event"));
		return;
	}

	// Get grenade ID from event
	FName GrenadeID = NAME_None;
	if (const FString* GrenadeIDStr = EventData.StringPayload.Find(TEXT("GrenadeID")))
	{
		GrenadeID = FName(**GrenadeIDStr);
		UE_LOG(LogGrenadeHandler, Warning, TEXT("OnGrenadeEquipped: GrenadeID from event = '%s'"), **GrenadeIDStr);
	}
	else
	{
		UE_LOG(LogGrenadeHandler, Warning, TEXT("OnGrenadeEquipped: No GrenadeID in event payload!"));
	}

	HANDLER_LOG(Log, TEXT("OnGrenadeEquipped: Character=%s, GrenadeID=%s"),
		*Character->GetName(), *GrenadeID.ToString());

	// Spawn visual grenade - also spawn if GrenadeID is "None" string but we have grenade type
	if (!GrenadeID.IsNone() && GrenadeID != FName("None"))
	{
		SpawnVisualGrenade(Character, GrenadeID);
	}
	else
	{
		// Try to get GrenadeType tag and use default ID
		if (const FString* GrenadeTypeStr = EventData.StringPayload.Find(TEXT("GrenadeType")))
		{
			UE_LOG(LogGrenadeHandler, Warning, TEXT("OnGrenadeEquipped: GrenadeID is None, trying GrenadeType: %s"), **GrenadeTypeStr);
			// TODO: Map GrenadeType tag to ItemID via DataManager
		}
		UE_LOG(LogGrenadeHandler, Warning, TEXT("OnGrenadeEquipped: Cannot spawn visual - no valid GrenadeID"));
	}
}

void USuspenseCoreGrenadeHandler::OnGrenadeUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	AActor* Character = Cast<AActor>(EventData.Source.Get());
	if (!Character)
	{
		HANDLER_LOG(Warning, TEXT("OnGrenadeUnequipped: No character in event"));
		return;
	}

	HANDLER_LOG(Log, TEXT("OnGrenadeUnequipped: Character=%s"), *Character->GetName());

	// Destroy visual grenade
	DestroyVisualGrenade(Character);
}

bool USuspenseCoreGrenadeHandler::SpawnVisualGrenade(AActor* Character, FName GrenadeID)
{
	if (!Character)
	{
		return false;
	}

	// Destroy any existing visual grenade for this character
	DestroyVisualGrenade(Character);

	// Get grenade actor class from DataManager
	TSubclassOf<AActor> GrenadeClass = nullptr;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(GrenadeID, ItemData))
		{
			if (!ItemData.EquipmentActorClass.IsNull())
			{
				GrenadeClass = ItemData.EquipmentActorClass.LoadSynchronous();
			}
		}
	}

	if (!GrenadeClass)
	{
		HANDLER_LOG(Warning, TEXT("SpawnVisualGrenade: No actor class found for %s"), *GrenadeID.ToString());
		return false;
	}

	// ═══════════════════════════════════════════════════════════════════
	// POOLED SPAWN - Eliminates microfreeze on first equip
	// ═══════════════════════════════════════════════════════════════════
	FTransform SpawnTransform = FTransform::Identity;
	AActor* VisualGrenade = SpawnGrenadeFromPool(GrenadeClass, SpawnTransform, Character);
	if (!VisualGrenade)
	{
		HANDLER_LOG(Warning, TEXT("SpawnVisualGrenade: Failed to spawn actor from pool"));
		return false;
	}

	// Reset actor state if it came from pool
	VisualGrenade->SetActorHiddenInGame(false);
	VisualGrenade->SetActorEnableCollision(false);  // Visual only, no collision

	// CRITICAL: Disable physics and movement on visual grenade - it's attached to hand
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(VisualGrenade->GetRootComponent());
	if (PrimComp)
	{
		PrimComp->SetSimulatePhysics(false);
		PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// CRITICAL: Disable ProjectileMovementComponent if present (same BP used for thrown grenade)
	// This prevents the visual grenade from moving/falling
	if (ASuspenseCoreGrenadeProjectile* GrenadeProjectile = Cast<ASuspenseCoreGrenadeProjectile>(VisualGrenade))
	{
		// Deactivate projectile movement - visual grenade should stay attached to hand
		if (UProjectileMovementComponent* ProjectileMovement = GrenadeProjectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			ProjectileMovement->Deactivate();
			ProjectileMovement->StopMovementImmediately();
			HANDLER_LOG(Log, TEXT("SpawnVisualGrenade: Deactivated ProjectileMovement on %s"), *VisualGrenade->GetName());
		}
	}

	// ============================================================================
	// ATTACH TO CHARACTER HAND
	// Using the same pattern as EquipmentVisualizationService::AttachActorToCharacter
	// MetaHuman/Modular character: Body component has weapon_r socket
	// ============================================================================

	// Alternative socket names for weapon attachment (prioritized order)
	static const TArray<FName> WeaponSocketAlternatives = {
		FName("weapon_r"),
		FName("GripPoint"),
		FName("RightHandSocket"),
		FName("hand_r"),
		FName("hand_rSocket")
	};

	USkeletalMeshComponent* TargetMesh = nullptr;
	FName FinalSocket = FName("weapon_r");

	// Search for skeletal mesh components
	TArray<USkeletalMeshComponent*> SkelMeshes;
	Character->GetComponents<USkeletalMeshComponent>(SkelMeshes);

	// First pass: Find Body component with socket
	for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
	{
		if (SkelMesh && SkelMesh->GetSkeletalMeshAsset() && SkelMesh->GetName().Contains(TEXT("Body")))
		{
			for (const FName& Socket : WeaponSocketAlternatives)
			{
				if (SkelMesh->DoesSocketExist(Socket))
				{
					TargetMesh = SkelMesh;
					FinalSocket = Socket;
					HANDLER_LOG(Log, TEXT("SpawnVisualGrenade: Found Body with socket '%s': %s"),
						*Socket.ToString(), *SkelMesh->GetName());
					break;
				}
			}
			if (TargetMesh) break;
		}
	}

	// Second pass: Find any mesh with socket
	if (!TargetMesh)
	{
		for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
		{
			if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
			{
				for (const FName& Socket : WeaponSocketAlternatives)
				{
					if (SkelMesh->DoesSocketExist(Socket))
					{
						TargetMesh = SkelMesh;
						FinalSocket = Socket;
						HANDLER_LOG(Log, TEXT("SpawnVisualGrenade: Found mesh with socket '%s': %s"),
							*Socket.ToString(), *SkelMesh->GetName());
						break;
					}
				}
				if (TargetMesh) break;
			}
		}
	}

	// Fallback: Use first skeletal mesh
	if (!TargetMesh && SkelMeshes.Num() > 0)
	{
		for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
		{
			if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
			{
				TargetMesh = SkelMesh;
				HANDLER_LOG(Warning, TEXT("SpawnVisualGrenade: Socket not found, using fallback mesh: %s"),
					*SkelMesh->GetName());
				break;
			}
		}
	}

	if (!TargetMesh)
	{
		HANDLER_LOG(Error, TEXT("SpawnVisualGrenade: No skeletal mesh found on character"));
		VisualGrenade->Destroy();
		return false;
	}

	// Attach grenade to socket
	USceneComponent* GrenadeRoot = VisualGrenade->GetRootComponent();
	if (GrenadeRoot)
	{
		GrenadeRoot->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, FinalSocket);

		HANDLER_LOG(Log, TEXT("SpawnVisualGrenade: SUCCESS - Attached %s to %s at socket %s"),
			*VisualGrenade->GetName(), *TargetMesh->GetName(), *FinalSocket.ToString());
	}

	// Track the visual grenade
	VisualGrenades.Add(Character, VisualGrenade);

	return true;
}

void USuspenseCoreGrenadeHandler::DestroyVisualGrenade(AActor* Character)
{
	if (!Character)
	{
		return;
	}

	TWeakObjectPtr<AActor>* FoundVisual = VisualGrenades.Find(Character);
	if (FoundVisual && FoundVisual->IsValid())
	{
		AActor* Visual = FoundVisual->Get();
		HANDLER_LOG(Log, TEXT("DestroyVisualGrenade: Recycling %s for %s"),
			*Visual->GetName(), *Character->GetName());

		// Detach before recycling
		if (USceneComponent* RootComp = Visual->GetRootComponent())
		{
			RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}

		// Recycle to pool instead of destroying
		RecycleGrenadeToPool(Visual);
	}

	VisualGrenades.Remove(Character);
}

void USuspenseCoreGrenadeHandler::HideVisualGrenade(AActor* Character)
{
	if (!Character)
	{
		return;
	}

	TWeakObjectPtr<AActor>* FoundVisual = VisualGrenades.Find(Character);
	if (FoundVisual && FoundVisual->IsValid())
	{
		AActor* Visual = FoundVisual->Get();
		HANDLER_LOG(Log, TEXT("HideVisualGrenade: Hiding %s for %s (before throw)"),
			*Visual->GetName(), *Character->GetName());

		// Hide instead of destroy - prevents the grenade from falling
		// The actor will be destroyed later when OnGrenadeUnequipped is called
		Visual->SetActorHiddenInGame(true);
		Visual->SetActorEnableCollision(false);
	}
}

void USuspenseCoreGrenadeHandler::OnGrenadeReleasing(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get character from event source (set by BroadcastGrenadeEvent using AvatarActor)
	AActor* Character = Cast<AActor>(EventData.Source.Get());
	if (!Character)
	{
		HANDLER_LOG(Warning, TEXT("OnGrenadeReleasing: No character in event Source"));
		return;
	}

	HANDLER_LOG(Log, TEXT("OnGrenadeReleasing: Hiding visual for %s"), *Character->GetName());

	// Hide the visual grenade immediately so it doesn't fall
	HideVisualGrenade(Character);
}

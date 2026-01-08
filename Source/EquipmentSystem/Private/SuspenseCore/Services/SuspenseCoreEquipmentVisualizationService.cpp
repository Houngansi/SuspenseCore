// EquipmentVisualizationServiceImpl.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentVisualizationService.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreActorFactory.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipment.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Interfaces/Core/ISuspenseCoreCharacter.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"

// Namespace aliases for cleaner code
using namespace SuspenseCoreEquipmentTags;

// ===== Local utilities =====================================================

static int32 LexToInt(const FString& S, int32 Default)
{
	int32 V = Default;
	if (!S.IsEmpty())
	{
		LexFromString(V, *S);
	}
	return V;
}

// ===== IEquipmentService ======================================================

bool USuspenseCoreEquipmentVisualizationService::InitializeService(const FSuspenseCoreServiceInitParams& InitParams)
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT(">>> VisualizationService::InitializeService CALLED"));

	EQUIPMENT_RW_WRITE_LOCK(VisualLock);

	if (IsServiceReady())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("Init skipped: already Ready"));
		return true;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT(">>> VisualizationService: InitializeService STARTED"));

	// ============================================================================
	// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #1: Кэшируем ServiceLocator из InitParams
	// ============================================================================
	// ServiceLocator передается через InitParams.ServiceLocator из EquipmentServiceLocator::InitializeService
	// Это единственное надежное место для получения ServiceLocator для stateless сервиса
	CachedServiceLocator = InitParams.ServiceLocator;

	if (!CachedServiceLocator)
	{
		LifecycleState = ESuspenseCoreServiceLifecycleState::Failed;
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("InitializeService FAILED: ServiceLocator not provided in InitParams!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("ServiceLocator cached: %p"), CachedServiceLocator.Get());

	// Initialize service tag - using native compile-time tags
	VisualizationServiceTag = Service::TAG_Service_Equipment_Visualization;
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("Native tag: %s (Valid: %s)"),
		*VisualizationServiceTag.ToString(),
		VisualizationServiceTag.IsValid() ? TEXT("YES") : TEXT("NO"));

	if (!VisualizationServiceTag.IsValid())
	{
		LifecycleState = ESuspenseCoreServiceLifecycleState::Failed;
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("InitializeService FAILED: Native tag not valid!"));
		return false;
	}

	// Initialize EventBus via GameInstance (ServiceLocator has access to GI)
	// NOTE: ServiceProvider::Get(this) fails because this UObject has no World context
	// Instead, get EventBus through ServiceLocator -> GameInstance -> ServiceProvider
	UGameInstance* GI = CachedServiceLocator->GetGameInstance();
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("GameInstance: %p"), GI);

	if (GI)
	{
		if (USuspenseCoreServiceProvider* Provider = GI->GetSubsystem<USuspenseCoreServiceProvider>())
		{
			EventBus = Provider->GetEventBus();
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("ServiceProvider: %p, EventBus: %p"), Provider, EventBus.Get());
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("ServiceProvider subsystem not found in GameInstance"));
		}
	}

	if (!EventBus)
	{
		LifecycleState = ESuspenseCoreServiceLifecycleState::Failed;
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("InitializeService FAILED: EventBus is null!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("EventBus acquired successfully"));

	LifecycleState = ESuspenseCoreServiceLifecycleState::Initializing;

	// Default config
	MaxUpdateRateHz    = 30.f;
	VisualQualityLevel = 2;     // High
	bEnableBatching    = true;

	CachedUpdateIntervalSec = MaxUpdateRateHz > 0.f ? (1.0 / MaxUpdateRateHz) : 0.0;
	LastProcessTimeSec      = 0.0;

	// Initialize event tags - MUST match what EquipmentInventoryBridge broadcasts
	// Bridge uses: "Equipment.Event.Equipped" and "Equipment.Event.Unequipped"
	// EquipmentUIProvider also subscribes to these same tags
	Tag_OnEquipped     = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Equipped"), false);
	Tag_OnUnequipped   = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Unequipped"), false);
	Tag_OnSlotSwitched = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.SlotSwitched"), false);
	Tag_OnWeaponSlotSwitched = SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched;
	Tag_VisRefreshAll  = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Visual.RefreshAll"), false);

	// Initialize dependency service tags using native tags
	Tag_ActorFactory     = Service::TAG_Service_ActorFactory;
	Tag_AttachmentSystem = Service::TAG_Service_AttachmentSystem;
	Tag_VisualController = Service::TAG_Service_VisualController;
	Tag_EquipmentData    = Service::TAG_Service_Equipment_Data;

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Event tags initialized"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Setting up event handlers..."));

	SetupEventHandlers();

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("Event handlers registered: %d subscriptions"),
		Subscriptions.Num());

	// Apply quality preset to visual controller (if available)
	// ============================================================================
	// ИСПРАВЛЕНИЕ #2: Используем CachedServiceLocator вместо Get(this)
	// ============================================================================
	if (CachedServiceLocator)
	{
		if (UObject* VisualCtl = CachedServiceLocator->TryGetService(Tag_VisualController))
		{
			if (UFunction* Fn = VisualCtl->FindFunction(FName(TEXT("SetVisualQualityLevel"))))
			{
				struct { int32 Level; } Params{ VisualQualityLevel };
				VisualCtl->ProcessEvent(Fn, &Params);
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose,
					TEXT("Applied quality preset to VisualController"));
			}
		}
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("VisService init completed: MaxRate=%.1fHz, Quality=%d"),
		MaxUpdateRateHz, VisualQualityLevel);

	LifecycleState = ESuspenseCoreServiceLifecycleState::Ready;

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("<<< VisualizationService: InitializeService SUCCESS"));
	return true;
}

bool USuspenseCoreEquipmentVisualizationService::ShutdownService(bool /*bForce*/)
{
	EQUIPMENT_RW_WRITE_LOCK(VisualLock);

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT(">>> VisualizationService: ShutdownService STARTED"));

	TeardownEventHandlers();

	// Release all visual instances
	// NOTE: Using Internal version since we already hold VisualLock (fixes deadlock)
	for (auto& Pair : Characters)
	{
		AActor* Character = Pair.Key.Get();
		if (!Character) continue;

		for (auto& SlotPair : Pair.Value.SlotActors)
		{
			ReleaseVisualActorInternal(Character, SlotPair.Key, /*bInstant=*/true);
		}
	}
	Characters.Empty();

	LifecycleState = ESuspenseCoreServiceLifecycleState::Shutdown;

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("<<< VisualizationService: ShutdownService COMPLETED"));
	return true;
}

FGameplayTagContainer USuspenseCoreEquipmentVisualizationService::GetRequiredDependencies() const
{
	// Return empty container - all dependencies are OPTIONAL
	// ActorFactory, AttachmentSystem, VisualController are handled via TryGetService
	// DataService dependency is declared in InitParams.RequiredServices by SystemCoordinator
	// This follows AbilityService pattern which also returns empty dependencies
	FGameplayTagContainer Deps;
	return Deps;
}

bool USuspenseCoreEquipmentVisualizationService::ValidateService(TArray<FText>& OutErrors) const
{
	bool bOk = true;

	if (!VisualizationServiceTag.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("VisualizationServiceTag is invalid")));
		bOk = false;
	}

	if (!EventBus)
	{
		OutErrors.Add(FText::FromString(TEXT("EventBus missing")));
		bOk = false;
	}

	// ============================================================================
	// ИСПРАВЛЕНИЕ: Используем CachedServiceLocator вместо Get(...)
	// ============================================================================
	// Check optional dependencies without failing validation
	if (CachedServiceLocator)
	{
		for (const FGameplayTag& Tag : GetRequiredDependencies())
		{
			if (!CachedServiceLocator->TryGetService(Tag))
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("Optional service not available: %s"), *Tag.ToString());
			}
		}
	}

	return bOk;
}

void USuspenseCoreEquipmentVisualizationService::ResetService()
{
	EQUIPMENT_RW_WRITE_LOCK(VisualLock);

	TeardownEventHandlers();

	for (auto& Pair : Characters)
	{
		AActor* Character = Pair.Key.Get();
		if (!Character) continue;
		for (auto& SlotPair : Pair.Value.SlotActors)
		{
			if (AActor* Visual = SlotPair.Value.Get())
			{
                if (IsValid(Visual)) { Visual->Destroy(); }
			}
		}
	}

	Characters.Empty();
	EventBus = nullptr;
	LastProcessTimeSec = 0.0;
	LifecycleState = ESuspenseCoreServiceLifecycleState::Uninitialized;

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("VisService reset"));
}

FString USuspenseCoreEquipmentVisualizationService::GetServiceStats() const
{
	EQUIPMENT_RW_READ_LOCK(VisualLock);  // Read-only access for stats

	int32 CharCount = 0;
	int32 VisualCount = 0;
	for (const auto& Pair : Characters)
	{
		++CharCount;
		VisualCount += Pair.Value.SlotActors.Num();
	}

	return FString::Printf(TEXT("VisService: Characters=%d, Visuals=%d, Quality=%d, Hz=%.1f"),
		CharCount, VisualCount, VisualQualityLevel, MaxUpdateRateHz);
}

// ===== Event subscriptions ====================================================

void USuspenseCoreEquipmentVisualizationService::SetupEventHandlers()
{
	if (!EventBus)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error, TEXT("SetupEventHandlers: EventBus is null"));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("=== SetupEventHandlers: Subscribing to events ==="));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Tag_OnEquipped: %s (Valid: %s)"),
		*Tag_OnEquipped.ToString(), Tag_OnEquipped.IsValid() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Tag_OnUnequipped: %s (Valid: %s)"),
		*Tag_OnUnequipped.ToString(), Tag_OnUnequipped.IsValid() ? TEXT("YES") : TEXT("NO"));

	if (Tag_OnEquipped.IsValid())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  -> Subscribing to OnEquipped: %s"), *Tag_OnEquipped.ToString());
		Subscriptions.Add(EventBus->SubscribeNative(
			Tag_OnEquipped, this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnEquipped),
			ESuspenseCoreEventPriority::Normal));
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error, TEXT("  -> SKIPPED OnEquipped subscription - tag invalid!"));
	}

	if (Tag_OnUnequipped.IsValid())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  -> Subscribing to OnUnequipped: %s"), *Tag_OnUnequipped.ToString());
		Subscriptions.Add(EventBus->SubscribeNative(
			Tag_OnUnequipped, this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnUnequipped),
			ESuspenseCoreEventPriority::Normal));
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error, TEXT("  -> SKIPPED OnUnequipped subscription - tag invalid!"));
	}

	if (Tag_OnSlotSwitched.IsValid())
	{
		Subscriptions.Add(EventBus->SubscribeNative(
			Tag_OnSlotSwitched, this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnSlotSwitched),
			ESuspenseCoreEventPriority::Normal));
	}

	// WeaponSlotSwitched - triggered by GA_WeaponSwitch ability
	if (Tag_OnWeaponSlotSwitched.IsValid())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  -> Subscribing to WeaponSlotSwitched: %s"), *Tag_OnWeaponSlotSwitched.ToString());
		Subscriptions.Add(EventBus->SubscribeNative(
			Tag_OnWeaponSlotSwitched, this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnWeaponSlotSwitched),
			ESuspenseCoreEventPriority::Normal));
	}

	if (Tag_VisRefreshAll.IsValid())
	{
		Subscriptions.Add(EventBus->SubscribeNative(
			Tag_VisRefreshAll, this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentVisualizationService::OnRefreshAll),
			ESuspenseCoreEventPriority::Normal));
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("=== SetupEventHandlers: Complete ==="));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Total subscriptions: %d"), Subscriptions.Num());
}

void USuspenseCoreEquipmentVisualizationService::TeardownEventHandlers()
{
	if (EventBus)
	{
		for (const FSuspenseCoreSubscriptionHandle& Handle : Subscriptions)
		{
			EventBus->Unsubscribe(Handle);
		}
	}
	Subscriptions.Empty();
}

// ===== Event handlers ============================================================

void USuspenseCoreEquipmentVisualizationService::OnEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT(">>> OnEquipped event received"));

	// Step 1: Rate limiting check
	if (RateLimit())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("OnEquipped rate limited - skipping"));
		return;
	}

	// Step 2: Extract and validate Character
	AActor* Character = EventData.GetObject<AActor>(FName("Target"));
	if (!Character)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: Target is NULL!"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Event has no valid Target actor"));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("OnEquipped: Character = %s (Class: %s)"),
		*Character->GetName(),
		*Character->GetClass()->GetName());

	// Step 3: Parse Slot metadata
	int32 Slot = INDEX_NONE;
	const bool bSlotParsed = TryParseInt(EventData, TEXT("Slot"), Slot);

	if (!bSlotParsed)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: Could not parse 'Slot' metadata"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Metadata['Slot'] = '%s'"),
			*EventData.GetString(FName("Slot")));
		return;
	}

	if (Slot == INDEX_NONE)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: Slot is INDEX_NONE after parsing"));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("OnEquipped: Slot = %d"), Slot);

	// Step 4: Parse ItemID metadata
	const FName ItemID = ParseName(EventData, TEXT("ItemID"));

	if (ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: ItemID is None after parsing"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Metadata['ItemID'] = '%s'"),
			*EventData.GetString(FName("ItemID")));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("OnEquipped: ItemID = %s"), *ItemID.ToString());

	// Step 5: Log all available metadata for diagnostics
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("OnEquipped: All metadata:"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("  Slot       = %s"), *EventData.GetString(FName("Slot")));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("  ItemID     = %s"), *EventData.GetString(FName("ItemID")));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("  InstanceID = %s"), *EventData.GetString(FName("InstanceID")));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("  Quantity   = %s"), *EventData.GetString(FName("Quantity")));

	// Step 6: Extract WeaponAmmoState from EventData (if present)
	// CRITICAL FIX: WeaponAmmoState was being lost during inventory transfers
	// because it was not being extracted from EventData and passed to AcquireVisualActor
	// @see TarkovStyle_Ammo_System_Design.md - WeaponAmmoState persistence
	FSuspenseCoreWeaponAmmoState WeaponAmmoState;
	const FSuspenseCoreWeaponAmmoState* WeaponAmmoStatePtr = nullptr;

	// Check if WeaponAmmoState was transmitted in the event
	const bool bHasWeaponAmmoState = EventData.GetBool(FName(TEXT("WeaponAmmoState_HasMag")));
	if (bHasWeaponAmmoState || EventData.HasKey(FName(TEXT("WeaponAmmoState_MagRounds"))))
	{
		WeaponAmmoState.bHasMagazine = bHasWeaponAmmoState;
		WeaponAmmoState.InsertedMagazine.MagazineID = FName(*EventData.GetString(FName(TEXT("WeaponAmmoState_MagID"))));
		WeaponAmmoState.InsertedMagazine.CurrentRoundCount = EventData.GetInt(FName(TEXT("WeaponAmmoState_MagRounds")));
		WeaponAmmoState.InsertedMagazine.MaxCapacity = EventData.GetInt(FName(TEXT("WeaponAmmoState_MagCapacity")));
		WeaponAmmoState.InsertedMagazine.LoadedAmmoID = FName(*EventData.GetString(FName(TEXT("WeaponAmmoState_MagAmmoID"))));
		WeaponAmmoState.ChamberedRound.AmmoID = FName(*EventData.GetString(FName(TEXT("WeaponAmmoState_ChamberedAmmoID"))));
		WeaponAmmoStatePtr = &WeaponAmmoState;

		UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
			TEXT("OnEquipped: Extracted WeaponAmmoState from event - HasMag=%s, Rounds=%d/%d, Chambered=%s"),
			WeaponAmmoState.bHasMagazine ? TEXT("true") : TEXT("false"),
			WeaponAmmoState.InsertedMagazine.CurrentRoundCount,
			WeaponAmmoState.InsertedMagazine.MaxCapacity,
			WeaponAmmoState.ChamberedRound.AmmoID != NAME_None ? TEXT("true") : TEXT("false"));
	}

	// Step 7: Final validation before calling UpdateVisualForSlot
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("OnEquipped: All validation passed - calling UpdateVisualForSlot"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  Character: %s, Slot: %d, ItemID: %s, HasWeaponAmmoState: %s"),
		*Character->GetName(), Slot, *ItemID.ToString(),
		WeaponAmmoStatePtr ? TEXT("YES") : TEXT("NO"));

	// Step 8: Execute visual update with WeaponAmmoState
	UpdateVisualForSlot(Character, Slot, ItemID, /*bInstant=*/false, WeaponAmmoStatePtr);

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("OnEquipped: UpdateVisualForSlot call completed"));
}

void USuspenseCoreEquipmentVisualizationService::OnUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT(">>> OnUnequipped event received"));

	if (RateLimit())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("OnUnequipped rate limited - skipping"));
		return;
	}

	AActor* Character = EventData.GetObject<AActor>(FName("Target"));
	if (!Character)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("OnUnequipped FAILED: Target is NULL!"));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("OnUnequipped: Character = %s"), *Character->GetName());

	int32 Slot = INDEX_NONE;
	const bool bSlotParsed = TryParseInt(EventData, TEXT("Slot"), Slot);

	if (!bSlotParsed || Slot == INDEX_NONE)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("OnUnequipped FAILED: Could not parse Slot (got %d)"), Slot);
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Metadata['Slot'] = '%s'"),
			*EventData.GetString(FName("Slot")));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("OnUnequipped: Hiding visual for slot %d on %s"),
		Slot, *Character->GetName());

	HideVisualForSlot(Character, Slot, /*bInstant=*/false);

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("OnUnequipped: HideVisualForSlot call completed"));
}

void USuspenseCoreEquipmentVisualizationService::OnSlotSwitched(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (RateLimit()) return;

	AActor* Character = EventData.GetObject<AActor>(FName("Target"));
	if (!Character) return;

	int32 ActiveSlot = INDEX_NONE;
	TryParseInt(EventData, TEXT("ActiveSlot"), ActiveSlot);

	EQUIPMENT_RW_WRITE_LOCK(VisualLock);
	FSuspenseCoreVisCharState& S = Characters.FindOrAdd(Character);
	S.ActiveSlot = ActiveSlot;
}

void USuspenseCoreEquipmentVisualizationService::OnWeaponSlotSwitched(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT(">>> OnWeaponSlotSwitched event received"));

	// Get character from event Target
	AActor* Character = EventData.GetObject<AActor>(FName("Target"));
	if (!Character)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error, TEXT("  FAILED: No Target character found in event!"));
		return;
	}

	int32 PreviousSlot = INDEX_NONE;
	int32 NewSlot = INDEX_NONE;
	TryParseInt(EventData, TEXT("PreviousSlot"), PreviousSlot);
	TryParseInt(EventData, TEXT("NewSlot"), NewSlot);

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Character: %s, PreviousSlot: %d, NewSlot: %d"),
		*Character->GetName(), PreviousSlot, NewSlot);

	// Update active slot tracking
	{
		EQUIPMENT_RW_WRITE_LOCK(VisualLock);
		FSuspenseCoreVisCharState& S = Characters.FindOrAdd(Character);
		S.ActiveSlot = NewSlot;
	}

	// If there was a previous active weapon, re-attach it to storage socket
	if (PreviousSlot != INDEX_NONE)
	{
		AActor* PrevWeaponActor = nullptr;
		{
			EQUIPMENT_RW_READ_LOCK(VisualLock);
			if (const FSuspenseCoreVisCharState* S = Characters.Find(Character))
			{
				if (const TWeakObjectPtr<AActor>* ActorPtr = S->SlotActors.Find(PreviousSlot))
				{
					PrevWeaponActor = ActorPtr->Get();
				}
			}
		}

		if (PrevWeaponActor)
		{
			// Get ItemID from the weapon actor via ISuspenseCoreEquipment interface
			FName PrevItemID = NAME_None;
			if (PrevWeaponActor->GetClass()->ImplementsInterface(USuspenseCoreEquipment::StaticClass()))
			{
				FSuspenseCoreInventoryItemInstance ItemInstance = ISuspenseCoreEquipment::Execute_GetEquippedItemInstance(PrevWeaponActor);
				PrevItemID = ItemInstance.ItemID;
			}

			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Re-attaching previous weapon (slot %d, ItemID: %s) to storage socket"),
				PreviousSlot, *PrevItemID.ToString());
			// Get storage socket for previous slot (now inactive) - NewSlot is active, no lock held
			FName Socket = ResolveAttachSocket(Character, PrevItemID, PreviousSlot, /*bCallerHoldsLock=*/false, /*KnownActiveSlot=*/NewSlot);
			FTransform Offset = ResolveAttachOffset(Character, PrevItemID, PreviousSlot, /*bCallerHoldsLock=*/false, /*KnownActiveSlot=*/NewSlot);
			AttachActorToCharacter(Character, PrevWeaponActor, Socket, Offset);
		}
	}

	// Attach new active weapon to hands
	if (NewSlot != INDEX_NONE)
	{
		AActor* NewWeaponActor = nullptr;
		{
			EQUIPMENT_RW_READ_LOCK(VisualLock);
			if (const FSuspenseCoreVisCharState* S = Characters.Find(Character))
			{
				if (const TWeakObjectPtr<AActor>* ActorPtr = S->SlotActors.Find(NewSlot))
				{
					NewWeaponActor = ActorPtr->Get();
				}
			}
		}

		if (NewWeaponActor)
		{
			// Get ItemID from the weapon actor via ISuspenseCoreEquipment interface
			FName NewItemID = NAME_None;
			if (NewWeaponActor->GetClass()->ImplementsInterface(USuspenseCoreEquipment::StaticClass()))
			{
				FSuspenseCoreInventoryItemInstance ItemInstance = ISuspenseCoreEquipment::Execute_GetEquippedItemInstance(NewWeaponActor);
				NewItemID = ItemInstance.ItemID;
			}

			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Attaching new weapon (slot %d, ItemID: %s) to hands"),
				NewSlot, *NewItemID.ToString());
			// Get active socket (hands) - NewSlot is now active, no lock held
			FName Socket = ResolveAttachSocket(Character, NewItemID, NewSlot, /*bCallerHoldsLock=*/false, /*KnownActiveSlot=*/NewSlot);
			FTransform Offset = ResolveAttachOffset(Character, NewItemID, NewSlot, /*bCallerHoldsLock=*/false, /*KnownActiveSlot=*/NewSlot);
			AttachActorToCharacter(Character, NewWeaponActor, Socket, Offset);

			// Update stance component using weapon archetype tag
			if (USuspenseCoreWeaponStanceComponent* StanceComp = Character->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
			{
				FGameplayTag WeaponType = ISuspenseCoreEquipment::Execute_GetWeaponArchetype(NewWeaponActor);
				if (WeaponType.IsValid())
				{
					StanceComp->SetWeaponStance(WeaponType);
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  Updated stance: %s"), *WeaponType.ToString());
				}
			}
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  No weapon actor found for slot %d"), NewSlot);
		}
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("<<< OnWeaponSlotSwitched complete"));
}

void USuspenseCoreEquipmentVisualizationService::OnRefreshAll(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	AActor* Character = EventData.GetObject<AActor>(FName("Target"));
	if (!Character) return;

	const FString ForceStr = EventData.GetString(FName("Force"));
	const bool bForce = ForceStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	RequestRefresh(Character, bForce);
}

// ===== Public trigger ======================================================

void USuspenseCoreEquipmentVisualizationService::RequestRefresh(AActor* Character, bool bForce)
{
	RefreshAllVisuals(Character, bForce);
}

// ===== High-level operations ==============================================

void USuspenseCoreEquipmentVisualizationService::UpdateVisualForSlot(AActor* Character, int32 SlotIndex, const FName ItemID, bool bInstant, const FSuspenseCoreWeaponAmmoState* InWeaponAmmoState)
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("=== UpdateVisualForSlot START ==="));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  Character: %s"), Character ? *Character->GetName() : TEXT("NULL"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  SlotIndex: %d"), SlotIndex);
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  ItemID: %s"), *ItemID.ToString());
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  bInstant: %s"), bInstant ? TEXT("true") : TEXT("false"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  InWeaponAmmoState: %s"), InWeaponAmmoState ? TEXT("PROVIDED") : TEXT("NULL"));

	if (!Character || ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("UpdateVisualForSlot ABORTED: Invalid parameters"));
		return;
	}

	EQUIPMENT_RW_WRITE_LOCK(VisualLock);

	// 1) Acquire/create visual actor
	// CRITICAL: Pass WeaponAmmoState to preserve ammo during inventory transfers
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("Step 1: Acquiring visual actor for ItemID=%s, Slot=%d"),
		*ItemID.ToString(), SlotIndex);

	AActor* Visual = AcquireVisualActor(Character, ItemID, SlotIndex, InWeaponAmmoState);
	if (!Visual)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("UpdateVisualForSlot FAILED: Could not acquire visual actor!"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  AcquireVisualActor returned nullptr"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Possible causes:"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("    1. ActorFactory service not available"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("    2. Actor class not found for ItemID %s"), *ItemID.ToString());
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("    3. Spawn failed due to collision or other issues"));
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("Step 1 SUCCESS: Visual actor acquired - %s (Class: %s)"),
		*Visual->GetName(), *Visual->GetClass()->GetName());

	// 2) Resolve socket/offset and attach
	// NOTE: We already hold VisualLock, so pass ActiveSlot to avoid nested lock acquisition
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Step 2: Resolving attachment parameters"));

	const FSuspenseCoreVisCharState& CharState = Characters.FindOrAdd(Character);
	const int32 CurrentActiveSlot = CharState.ActiveSlot;
	// NOTE: We already hold VisualLock, pass bCallerHoldsLock=true to avoid deadlock
	const FName Socket      = ResolveAttachSocket(Character, ItemID, SlotIndex, /*bCallerHoldsLock=*/true, /*KnownActiveSlot=*/CurrentActiveSlot);
	const FTransform Offset = ResolveAttachOffset(Character, ItemID, SlotIndex, /*bCallerHoldsLock=*/true, /*KnownActiveSlot=*/CurrentActiveSlot);

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("  Socket: %s, Offset: Loc(%s) Rot(%s)"),
		*Socket.ToString(),
		*Offset.GetLocation().ToString(),
		*Offset.Rotator().ToString());

	if (!AttachActorToCharacter(Character, Visual, Socket, Offset))
	{
		// Failed to attach - release actor
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("UpdateVisualForSlot FAILED: Could not attach actor to character!"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  AttachActorToCharacter returned false"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Possible causes:"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("    1. Character has no SkeletalMeshComponent"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("    2. Socket '%s' doesn't exist on character mesh"), *Socket.ToString());
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("    3. AttachmentSystem service failed"));

		// NOTE: Using Internal version since we already hold VisualLock (fixes deadlock)
		ReleaseVisualActorInternal(Character, SlotIndex, /*bInstant=*/true);
		return;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Step 2 SUCCESS: Actor attached to character"));

	// 3) Apply quality settings
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Verbose, TEXT("Step 3: Applying quality settings"));
	ApplyQualitySettings(Visual);

	// 4) Store in state
	FSuspenseCoreVisCharState& S = Characters.FindOrAdd(Character);
	S.SlotActors.FindOrAdd(SlotIndex) = Visual;

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Step 4: Stored in state map"));

	// ============================================================================
	// ИСПРАВЛЕНИЕ: Используем CachedServiceLocator вместо Get(this)
	// ============================================================================
	// 5) Apply visual profile (via VisualController)
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Step 5: Applying visual profile"));

	if (CachedServiceLocator)
	{
		if (UObject* VisualCtl = CachedServiceLocator->TryGetService(Tag_VisualController))
		{
			if (UFunction* Fn = VisualCtl->FindFunction(FName(TEXT("ApplyVisualProfile"))))
			{
				struct { AActor* Equipment; FGameplayTag Profile; bool bSmooth; bool ReturnValue; }
				Params{ Visual, FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Active")), !bInstant, false };
				VisualCtl->ProcessEvent(Fn, &Params);

				UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
					TEXT("  Visual profile applied via VisualController"));
			}
			else
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("  VisualController doesn't have ApplyVisualProfile method"));
			}
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  VisualController service not available"));
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  CachedServiceLocator is NULL"));
	}

	// ============================================================================
	// Step 6: Update WeaponStanceComponent for animation system (CRITICAL FIX)
	// ============================================================================
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Step 6: Updating WeaponStanceComponent"));

	if (APawn* CharacterPawn = Cast<APawn>(Character))
	{
		if (USuspenseCoreWeaponStanceComponent* StanceComp = CharacterPawn->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
		{
			// Get weapon archetype from DataManager for animation lookup
			FGameplayTag WeaponArchetype;
			if (CachedServiceLocator)
			{
				if (UGameInstance* GI = CachedServiceLocator->GetGameInstance())
				{
					if (USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>())
					{
						FSuspenseCoreUnifiedItemData ItemData;
						if (DataMgr->GetUnifiedItemData(ItemID, ItemData))
						{
							WeaponArchetype = ItemData.WeaponArchetype;
						}
					}
				}
			}

			// Update stance component with weapon type
			if (WeaponArchetype.IsValid())
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("  Setting WeaponStance: %s"), *WeaponArchetype.ToString());
				StanceComp->SetWeaponStance(WeaponArchetype, true);
			}
			else
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("  WeaponArchetype is invalid for ItemID: %s"), *ItemID.ToString());
			}

			// Mark weapon as drawn for active slot (slot 0)
			if (SlotIndex == 0)
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("  Setting WeaponDrawnState: true (active slot)"));
				StanceComp->SetWeaponDrawnState(true);
			}

			// Notify stance of equipment actor
			StanceComp->OnEquipmentChanged(Visual);

			UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
				TEXT("  StanceComponent updated successfully"));
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  Character has no WeaponStanceComponent - animations won't work!"));
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  Character is not a Pawn - cannot update StanceComponent"));
	}

	// ============================================================================
	// Step 7: Set CurrentWeaponActor on character via ISuspenseCoreCharacterInterface
	// This enables ADS camera switching to find the weapon actor
	// ============================================================================
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("Step 7: Setting CurrentWeaponActor on character"));

	if (Character->GetClass()->ImplementsInterface(USuspenseCoreCharacterInterface::StaticClass()))
	{
		ISuspenseCoreCharacterInterface::Execute_SetCurrentWeaponActor(Character, Visual);
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  CurrentWeaponActor SET to: %s"), *Visual->GetName());
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  Character does not implement ISuspenseCoreCharacterInterface - ADS camera won't work!"));
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("=== UpdateVisualForSlot SUCCESS ==="));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  Visual equipment %s now attached to %s at slot %d"),
		*Visual->GetName(), *Character->GetName(), SlotIndex);
}

void USuspenseCoreEquipmentVisualizationService::HideVisualForSlot(AActor* Character, int32 SlotIndex, bool bInstant)
{
	EQUIPMENT_RW_WRITE_LOCK(VisualLock);

	FSuspenseCoreVisCharState* S = Characters.Find(Character);
	if (!S) return;

	TWeakObjectPtr<AActor>* Found = S->SlotActors.Find(SlotIndex);
	if (!Found) return;

	AActor* Visual = Found->Get();
	if (Visual)
	{
		// ============================================================================
		// CRITICAL FIX: Call OnItemInstanceUnequipped BEFORE destroying actor!
		// This ensures AttributeSets are removed from CHARACTER's ASC
		// ============================================================================
		if (Visual->GetClass()->ImplementsInterface(USuspenseCoreEquipment::StaticClass()))
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("HideVisualForSlot: Calling OnItemInstanceUnequipped on %s"), *Visual->GetName());

			// Get the equipped item instance for cleanup
			FSuspenseCoreInventoryItemInstance ItemInstance = ISuspenseCoreEquipment::Execute_GetEquippedItemInstance(Visual);

			// Call unequip methods to trigger AttributeSet cleanup
			ISuspenseCoreEquipment::Execute_OnItemInstanceUnequipped(Visual, ItemInstance);
			ISuspenseCoreEquipment::Execute_OnUnequipped(Visual);

			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("HideVisualForSlot: Cleanup completed for %s"), *Visual->GetName());
		}

		// ============================================================================
		// ИСПРАВЛЕНИЕ: Используем CachedServiceLocator вместо Get(this)
		// ============================================================================
		// Soft disable effects via VisualController
		if (CachedServiceLocator)
		{
			if (UObject* VisualCtl = CachedServiceLocator->TryGetService(Tag_VisualController))
			{
				if (UFunction* Fn = VisualCtl->FindFunction(FName(TEXT("ClearAllEffectsForEquipment"))))
				{
					struct { AActor* Equipment; bool bImmediate; } Params{ Visual, bInstant };
					VisualCtl->ProcessEvent(Fn, &Params);
				}
			}
		}
	}

	// ============================================================================
	// Clear WeaponStanceComponent when unequipping
	// ============================================================================
	if (APawn* CharacterPawn = Cast<APawn>(Character))
	{
		if (USuspenseCoreWeaponStanceComponent* StanceComp = CharacterPawn->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
		{
			// Clear weapon stance when unequipping from active slot
			if (SlotIndex == 0)
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("HideVisualForSlot: Clearing WeaponStance for slot %d"), SlotIndex);
				StanceComp->ClearWeaponStance(bInstant);
				StanceComp->SetWeaponDrawnState(false);
			}
			StanceComp->OnEquipmentChanged(nullptr);
		}
	}

	// ============================================================================
	// Clear CurrentWeaponActor on character when unequipping from active slot
	// ============================================================================
	if (SlotIndex == 0 && Character->GetClass()->ImplementsInterface(USuspenseCoreCharacterInterface::StaticClass()))
	{
		ISuspenseCoreCharacterInterface::Execute_SetCurrentWeaponActor(Character, nullptr);
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("HideVisualForSlot: CurrentWeaponActor cleared"));
	}

	// ============================================================================
	// CRITICAL: Broadcast Visual_Detached event BEFORE destroying actor
	// This notifies UIManager to hide weapon HUD (AmmoCounter, Crosshair)
	// We publish here because we KNOW the SlotIndex - ActorFactory may not!
	// ============================================================================
	if (EventBus)
	{
		using namespace SuspenseCoreEquipmentTags;

		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<USuspenseCoreEquipmentVisualizationService*>(this));
		EventData.SetObject(TEXT("Target"), Visual);
		EventData.SetInt(TEXT("Slot"), SlotIndex);

		// Determine SlotType based on slot index for UI HUD logic
		FString SlotType;
		if (SlotIndex == 0)
		{
			SlotType = TEXT("PrimaryWeapon");
		}
		else if (SlotIndex == 1)
		{
			SlotType = TEXT("SecondaryWeapon");
		}
		else if (SlotIndex == 2)
		{
			SlotType = TEXT("Holster");
		}
		else if (SlotIndex == 3)
		{
			SlotType = TEXT("Scabbard");
		}
		EventData.SetString(TEXT("SlotType"), SlotType);

		EventBus->Publish(Event::TAG_Equipment_Event_Visual_Detached, EventData);

		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("HideVisualForSlot: Published Visual_Detached event - Slot=%d, SlotType=%s"),
			SlotIndex, *SlotType);
	}

	// Return actor to pool/destroy via Factory
	// NOTE: Using Internal version since we already hold VisualLock (fixes deadlock)
	ReleaseVisualActorInternal(Character, SlotIndex, bInstant);
	S->SlotActors.Remove(SlotIndex);
}

void USuspenseCoreEquipmentVisualizationService::RefreshAllVisuals(AActor* Character, bool bForce)
{
	if (!Character) return;

	// Request data layer to resend current state (SuspenseCore EventBus)
	if (EventBus)
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<USuspenseCoreEquipmentVisualizationService*>(this));
		EventData.SetObject(FName("Target"), Character);
		EventData.SetString(FName("Reason"), bForce ? TEXT("ForceRefreshVisual") : TEXT("RefreshVisual"));
		EventBus->Publish(FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Equipment.RequestResend")), EventData);
	}

	// Refresh quality for current visuals
	{
		EQUIPMENT_RW_READ_LOCK(VisualLock);  // Read-only access for quality refresh
		if (FSuspenseCoreVisCharState* S = Characters.Find(Character))
		{
			for (auto& Pair : S->SlotActors)
			{
				if (AActor* Visual = Pair.Value.Get())
				{
					ApplyQualitySettings(Visual);
				}
			}
		}
	}
}

// ===== Integration with presentation via ServiceLocator =========================

AActor* USuspenseCoreEquipmentVisualizationService::AcquireVisualActor(AActor* Character, const FName ItemID, int32 SlotIndex, const FSuspenseCoreWeaponAmmoState* InWeaponAmmoState)
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("=== AcquireVisualActor START ==="));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  Character: %s"), Character ? *Character->GetName() : TEXT("NULL"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  ItemID: %s"), *ItemID.ToString());
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  SlotIndex: %d"), SlotIndex);
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  InWeaponAmmoState: %s"),
		InWeaponAmmoState ? TEXT("PROVIDED") : TEXT("NULL"));

	if (!Character || ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("AcquireVisualActor: Invalid parameters!"));
		return nullptr;
	}

	// ============================================================================
	// ИСПРАВЛЕНИЕ #4: Проверяем кэшированный ServiceLocator
	// ============================================================================
	if (!CachedServiceLocator)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  CRITICAL: CachedServiceLocator is NULL!"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Cannot proceed with actor acquisition."));
		return nullptr;
	}

	// Step 1: Try to get ActorFactory service via cached ServiceLocator
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("Step 1: Attempting to get ActorFactory service"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  Tag_ActorFactory: %s (Valid: %s)"),
		*Tag_ActorFactory.ToString(), Tag_ActorFactory.IsValid() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  ServiceLocator: %p"), CachedServiceLocator.Get());

	if (UObject* FactoryObj = CachedServiceLocator->TryGetService(Tag_ActorFactory))
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
			TEXT("  ActorFactory service found: %s"),
			*FactoryObj->GetClass()->GetName());

		// PREFERRED: via C++ interface ISuspenseCoreActorFactory
		if (FactoryObj->GetClass()->ImplementsInterface(USuspenseCoreActorFactory::StaticClass()))
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
				TEXT("  Using C++ interface ISuspenseCoreActorFactory"));

			if (ISuspenseCoreActorFactory* Factory = static_cast<ISuspenseCoreActorFactory*>(
				FactoryObj->GetInterfaceAddress(USuspenseCoreActorFactory::StaticClass())))
			{
				FEquipmentActorSpawnParams Params;
				Params.SpawnTransform      = Character->GetActorTransform();
				Params.Owner               = Character;
				Params.SlotIndex           = SlotIndex;

				// CRITICAL: Get ACTUAL ItemInstance from EquipmentDataProvider (with WeaponAmmoState!)
				// NOT creating new ItemInstance - that loses saved ammo data!
				bool bGotRealItemInstance = false;

				// Use Tag_EquipmentData (SuspenseCore.Service.Equipment.Data) and get DataProvider via interface
				if (UObject* DataServiceObj = CachedServiceLocator->TryGetService(Tag_EquipmentData))
				{
					// Cast to DataService interface which has GetDataProvider()
					if (ISuspenseCoreEquipmentDataServiceInterface* DataService =
						Cast<ISuspenseCoreEquipmentDataServiceInterface>(DataServiceObj))
					{
						if (ISuspenseCoreEquipmentDataProvider* DataProvider = DataService->GetDataProvider())
						{
							Params.ItemInstance = DataProvider->GetSlotItem(SlotIndex);
							if (Params.ItemInstance.IsValid())
							{
								bGotRealItemInstance = true;
								UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
									TEXT("  ✓ ActorFactory: Got REAL ItemInstance from DataProvider - ID=%s, WeaponAmmoState HasMag=%s, Rounds=%d"),
									*Params.ItemInstance.ItemID.ToString(),
									Params.ItemInstance.WeaponAmmoState.bHasMagazine ? TEXT("true") : TEXT("false"),
									Params.ItemInstance.WeaponAmmoState.InsertedMagazine.CurrentRoundCount);
							}
						}
					}
				}

				// Fallback: use provided WeaponAmmoState or just set ItemID
				if (!bGotRealItemInstance)
				{
					Params.ItemInstance.ItemID = ItemID;

					// CRITICAL FIX: Use WeaponAmmoState from event if provided
					// This preserves ammo state during inventory->equipment transfers
					if (InWeaponAmmoState)
					{
						Params.ItemInstance.WeaponAmmoState = *InWeaponAmmoState;
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
							TEXT("  ✓ ActorFactory: Using WeaponAmmoState from event - HasMag=%s, Rounds=%d/%d"),
							InWeaponAmmoState->bHasMagazine ? TEXT("true") : TEXT("false"),
							InWeaponAmmoState->InsertedMagazine.CurrentRoundCount,
							InWeaponAmmoState->InsertedMagazine.MaxCapacity);
					}
					else
					{
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
							TEXT("  ⚠ ActorFactory: Could not get ItemInstance from DataProvider AND no WeaponAmmoState provided - using ItemID only"));
					}
				}

				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("  Calling Factory->SpawnEquipmentActor..."));

				const FEquipmentActorSpawnResult R = Factory->SpawnEquipmentActor(Params);

				if (R.bSuccess && R.SpawnedActor.IsValid())
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
						TEXT("✓ ActorFactory SUCCESS: Spawned %s"),
						*R.SpawnedActor->GetName());
					return R.SpawnedActor.Get();
				}
				else
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
						TEXT("✗ ActorFactory FAILED: %s"),
						*R.ErrorMessage.ToString());
				}
			}
			else
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
					TEXT("  Failed to get interface address for ISuspenseCoreActorFactory"));
			}
		}

		// Fallback: via ProcessEvent (BP compatibility) - опущено для краткости
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  ❌ ActorFactory service NOT FOUND in ServiceLocator!"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Expected tag: %s"), *Tag_ActorFactory.ToString());
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  Trying direct spawn fallback (Visual.Spawned will NOT be published!)"));
	}

	// Ultimate fallback: direct SpawnActor
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("Step 2: FALLBACK - Direct spawn without factory"));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("  Resolving actor class for ItemID: %s"), *ItemID.ToString());

	TSubclassOf<AActor> Class = ResolveActorClass(ItemID);
	if (!Class)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("✗ ResolveActorClass returned NULL for ItemID: %s"),
			*ItemID.ToString());
		return nullptr;
	}

	// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: используем World от Character
	UWorld* World = Character->GetWorld();
	if (!World)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("✗ Character has no World!"));
		return nullptr;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("  Actor class resolved: %s"), *Class->GetName());
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("  Spawning actor directly..."));

	FActorSpawnParameters SP;
	SP.Owner = Character;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(Class, Character->GetActorTransform(), SP);

	if (SpawnedActor)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("✓ Direct spawn SUCCESS: %s"),
			*SpawnedActor->GetName());

		// CRITICAL FIX: Initialize spawned actor via ISuspenseCoreEquipment interface
		// This was missing in fallback path, causing ScopeCam to not attach to socket!
		if (SpawnedActor->GetClass()->ImplementsInterface(USuspenseCoreEquipment::StaticClass()))
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  Initializing actor via ISuspenseCoreEquipment interface..."));

			// 1) Call OnEquipped with Character as owner
			ISuspenseCoreEquipment::Execute_OnEquipped(SpawnedActor, Character);
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  ✓ OnEquipped called with owner: %s"), *Character->GetName());

			// 2) Call OnItemInstanceEquipped with VALID ItemInstance
			// CRITICAL: Get ACTUAL ItemInstance from EquipmentDataProvider (with WeaponAmmoState!)
			// NOT creating new ItemInstance - that loses saved ammo data!
			FSuspenseCoreInventoryItemInstance ItemInstance;

			// Try to get the real ItemInstance from EquipmentDataProvider via DataService
			bool bGotRealItemInstance = false;
			if (CachedServiceLocator)
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
					TEXT("  DataProvider lookup: ServiceLocator valid, Tag=%s"), *Tag_EquipmentData.ToString());

				// Use Tag_EquipmentData (SuspenseCore.Service.Equipment.Data) and get DataProvider via interface
				if (UObject* DataServiceObj = CachedServiceLocator->TryGetService(Tag_EquipmentData))
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
						TEXT("  DataProvider lookup: Got DataServiceObj=%s"), *DataServiceObj->GetName());

					// Cast to DataService interface which has GetDataProvider()
					if (ISuspenseCoreEquipmentDataServiceInterface* DataService =
						Cast<ISuspenseCoreEquipmentDataServiceInterface>(DataServiceObj))
					{
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
							TEXT("  DataProvider lookup: Cast to ISuspenseCoreEquipmentDataServiceInterface succeeded"));

						if (ISuspenseCoreEquipmentDataProvider* DataProvider = DataService->GetDataProvider())
						{
							UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
								TEXT("  DataProvider lookup: GetDataProvider() returned valid provider"));

							ItemInstance = DataProvider->GetSlotItem(SlotIndex);
							if (ItemInstance.IsValid())
							{
								bGotRealItemInstance = true;
								UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
									TEXT("  ✓ Got REAL ItemInstance from DataProvider - ID=%s, InstanceID=%s, WeaponAmmoState HasMag=%s, Rounds=%d"),
									*ItemInstance.ItemID.ToString(),
									*ItemInstance.InstanceID.ToString(),
									ItemInstance.WeaponAmmoState.bHasMagazine ? TEXT("true") : TEXT("false"),
									ItemInstance.WeaponAmmoState.InsertedMagazine.CurrentRoundCount);
							}
							else
							{
								UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
									TEXT("  DataProvider lookup: GetSlotItem(%d) returned invalid ItemInstance"), SlotIndex);
							}
						}
						else
						{
							UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
								TEXT("  DataProvider lookup: GetDataProvider() returned NULL"));
						}
					}
					else
					{
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
							TEXT("  DataProvider lookup: Cast to ISuspenseCoreEquipmentDataServiceInterface FAILED (obj class: %s)"),
							*DataServiceObj->GetClass()->GetName());
					}
				}
				else
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
						TEXT("  DataProvider lookup: TryGetService(%s) returned NULL"), *Tag_EquipmentData.ToString());
				}
			}
			else
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("  DataProvider lookup: CachedServiceLocator is NULL!"));
			}

			// Fallback: create ItemInstance with WeaponAmmoState from event if provided
			if (!bGotRealItemInstance)
			{
				ItemInstance.ItemID = ItemID;
				ItemInstance.Quantity = 1;
				ItemInstance.InstanceID = FGuid::NewGuid();

				// CRITICAL FIX: Use WeaponAmmoState from event if provided
				// This preserves ammo state during inventory->equipment transfers
				if (InWeaponAmmoState)
				{
					ItemInstance.WeaponAmmoState = *InWeaponAmmoState;
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
						TEXT("  ✓ Using WeaponAmmoState from event - HasMag=%s, Rounds=%d/%d"),
						InWeaponAmmoState->bHasMagazine ? TEXT("true") : TEXT("false"),
						InWeaponAmmoState->InsertedMagazine.CurrentRoundCount,
						InWeaponAmmoState->InsertedMagazine.MaxCapacity);
				}
				else
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
						TEXT("  ⚠ Could not get ItemInstance from DataProvider AND no WeaponAmmoState provided - creating minimal (ammo state will be lost!)"));
				}
			}

			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  Created ItemInstance: ID=%s, Qty=%d, InstanceID=%s, IsValid=%s"),
				*ItemID.ToString(), ItemInstance.Quantity, *ItemInstance.InstanceID.ToString(),
				ItemInstance.IsValid() ? TEXT("YES") : TEXT("NO"));

			ISuspenseCoreEquipment::Execute_OnItemInstanceEquipped(SpawnedActor, ItemInstance);
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  ✓ OnItemInstanceEquipped called for ItemID: %s"), *ItemID.ToString());
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
				TEXT("  Actor does not implement ISuspenseCoreEquipment - skipping initialization"));
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error, TEXT("✗ Direct spawn FAILED!"));
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("=== AcquireVisualActor END ==="));

	return SpawnedActor;
}

void USuspenseCoreEquipmentVisualizationService::ReleaseVisualActor(AActor* Character, int32 SlotIndex, bool bInstant)
{
	if (!Character) return;

	// Acquire lock and delegate to internal (lock-free) implementation
	EQUIPMENT_RW_WRITE_LOCK(VisualLock);
	ReleaseVisualActorInternal(Character, SlotIndex, bInstant);
}

void USuspenseCoreEquipmentVisualizationService::ReleaseVisualActorInternal(AActor* Character, int32 SlotIndex, bool bInstant)
{
	// NOTE: This method MUST be called with VisualLock already held!
	// Called from: HideVisualForSlot, ShutdownService (they hold the lock)

	if (!Character) return;

	FSuspenseCoreVisCharState* S = Characters.Find(Character);
	if (!S) return;

	TWeakObjectPtr<AActor>* Found = S->SlotActors.Find(SlotIndex);
	if (!Found) return;

	AActor* Visual = Found->Get();
	if (!Visual) return;

	// ============================================================================
	// ИСПРАВЛЕНИЕ: Используем CachedServiceLocator вместо Get(this)
	// ============================================================================
	if (CachedServiceLocator)
	{
		if (UObject* FactoryObj = CachedServiceLocator->TryGetService(Tag_ActorFactory))
		{
			// Via interface
			if (FactoryObj->GetClass()->ImplementsInterface(USuspenseCoreActorFactory::StaticClass()))
			{
				if (ISuspenseCoreActorFactory* Factory = static_cast<ISuspenseCoreActorFactory*>(
					FactoryObj->GetInterfaceAddress(USuspenseCoreActorFactory::StaticClass())))
				{
					if (Factory->DestroyEquipmentActor(Visual, bInstant))
					{
						return;
					}
				}
			}

			// Fallback: BP variant
			if (UFunction* Fn = FactoryObj->FindFunction(FName(TEXT("DestroyEquipmentActor"))))
			{
				struct { AActor* Actor; bool bImmediate; bool ReturnValue; } P{ Visual, bInstant, false };
				FactoryObj->ProcessEvent(Fn, &P);
				if (P.ReturnValue)
				{
					return;
				}
			}
		}
	}

	// Failed to destroy via factory - destroy locally
	if (IsValid(Visual))
	{
		Visual->Destroy();
	}
}

bool USuspenseCoreEquipmentVisualizationService::AttachActorToCharacter(AActor* Character, AActor* Visual, const FName Socket, const FTransform& Offset)
{
	if (!Character || !Visual) return false;

	// ============================================================================
	// ИСПРАВЛЕНИЕ: Используем CachedServiceLocator вместо Get(this)
	// ============================================================================
	if (CachedServiceLocator)
	{
		if (UObject* AttachmentSvc = CachedServiceLocator->TryGetService(Tag_AttachmentSystem))
		{
			// Public method in attachment component
			if (UFunction* Fn = AttachmentSvc->FindFunction(FName(TEXT("AttachToCharacter"))))
			{
				struct { AActor* Equipment; AActor* TargetCharacter; FName Socket; FTransform Offset; bool bSmooth; float Blend; bool ReturnValue; }
				Params{ Visual, Character, Socket, Offset, true, 0.2f, false };
				AttachmentSvc->ProcessEvent(Fn, &Params);
				return Params.ReturnValue;
			}
		}
	}

	// ============================================================================
	// CRITICAL FIX: MetaHuman support
	// MetaHuman hierarchy: CharacterMesh0 > Root > Body (sockets here!)
	// The sockets (weapon_r, spine_03, etc.) are on the BODY component itself!
	// ============================================================================

	USkeletalMeshComponent* EquipmentMesh = nullptr;

	// Search for "Body" component by name (MetaHuman hierarchy)
	TArray<USkeletalMeshComponent*> SkelMeshes;
	Character->GetComponents<USkeletalMeshComponent>(SkelMeshes);

	for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
	{
		// Check if this component IS named "Body" (MetaHuman Body component)
		if (SkelMesh->GetName().Contains(TEXT("Body")))
		{
			// Verify socket exists on this mesh
			if (SkelMesh->DoesSocketExist(Socket))
			{
				EquipmentMesh = SkelMesh;
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
					TEXT("AttachActorToCharacter: Found MetaHuman Body component with socket '%s': %s"),
					*Socket.ToString(), *SkelMesh->GetName());
				break;
			}
		}
	}

	// If Body not found, search for any mesh with the required socket
	if (!EquipmentMesh)
	{
		for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
		{
			if (SkelMesh->DoesSocketExist(Socket))
			{
				EquipmentMesh = SkelMesh;
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
					TEXT("AttachActorToCharacter: Found mesh with socket '%s': %s"),
					*Socket.ToString(), *SkelMesh->GetName());
				break;
			}
		}
	}

	// Final fallback: first SkeletalMeshComponent
	if (!EquipmentMesh && SkelMeshes.Num() > 0)
	{
		EquipmentMesh = SkelMeshes[0];
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("AttachActorToCharacter: Socket '%s' not found on any mesh, using fallback: %s"),
			*Socket.ToString(), *EquipmentMesh->GetName());
	}

	if (EquipmentMesh)
	{
		USceneComponent* Root = Visual->GetRootComponent();
		if (!Root) return false;

		Root->AttachToComponent(EquipmentMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, Socket);
		Root->SetRelativeTransform(Offset);

		UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
			TEXT("AttachActorToCharacter: SUCCESS - Attached %s to %s at socket %s"),
			*Visual->GetName(), *EquipmentMesh->GetName(), *Socket.ToString());

		return true;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
		TEXT("AttachActorToCharacter: No SkeletalMeshComponent found on character %s"),
		*Character->GetName());

	return false;
}

void USuspenseCoreEquipmentVisualizationService::ApplyQualitySettings(AActor* Visual) const
{
	if (!Visual) return;

	TArray<UPrimitiveComponent*> Prims;
	Visual->GetComponents<UPrimitiveComponent>(Prims);

	for (UPrimitiveComponent* C : Prims)
	{
		if (!C) continue;

		switch (VisualQualityLevel)
		{
		case 0: // Low
			C->SetCastShadow(false);
			C->SetRenderInMainPass(true);
			break;
		case 1: // Medium
			C->SetCastShadow(true);
			break;
		case 3: // Ultra
			C->SetCastShadow(true);
			C->bCastContactShadow = true;
			break;
		default: // High
			C->SetCastShadow(true);
			break;
		}
	}
}

// ===== Reflection to data/presentation ========================================

TSubclassOf<AActor> USuspenseCoreEquipmentVisualizationService::ResolveActorClass(const FName ItemID) const
{
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("=== ResolveActorClass START ==="));
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning, TEXT("  ItemID: %s"), *ItemID.ToString());

	// ============================================================================
	// ИСПРАВЛЕНИЕ #3: Проверяем кэшированный ServiceLocator
	// ============================================================================
	if (!CachedServiceLocator)
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  CRITICAL: CachedServiceLocator is NULL!"));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  This should never happen if InitializeService() succeeded."));
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  Service may not have been initialized properly."));
		return nullptr;
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
		TEXT("  Using cached ServiceLocator: %p"),
		CachedServiceLocator.Get());

	// Step 1: Try DataService via cached ServiceLocator
	if (UObject* DataSvc = CachedServiceLocator->TryGetService(Tag_EquipmentData))
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Log, TEXT("  DataService found"));

		if (UFunction* Fn = DataSvc->FindFunction(FName(TEXT("GetVisualActorClass"))))
		{
			struct { FName ItemID; TSubclassOf<AActor> ReturnValue; } P{ ItemID, nullptr };
			DataSvc->ProcessEvent(Fn, &P);
			if (P.ReturnValue)
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("✓ DataService->GetVisualActorClass returned: %s"),
					*P.ReturnValue->GetName());
				return P.ReturnValue;
			}
		}
		if (UFunction* Fn2 = DataSvc->FindFunction(FName(TEXT("GetEquipmentActorClass"))))
		{
			struct { FName ItemID; TSubclassOf<AActor> ReturnValue; } P{ ItemID, nullptr };
			DataSvc->ProcessEvent(Fn2, &P);
			if (P.ReturnValue)
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
					TEXT("✓ DataService->GetEquipmentActorClass returned: %s"),
					*P.ReturnValue->GetName());
				return P.ReturnValue;
			}
		}

		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  DataService has no GetVisualActorClass or GetEquipmentActorClass methods"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
			TEXT("  DataService not available in ServiceLocator"));
	}

	// Step 2: Use DataManager (SSOT for all item data)
	UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
		TEXT("Step 2: Using DataManager (SSOT) via ServiceLocator"));

	if (UGameInstance* GI = CachedServiceLocator->GetGameInstance())
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
			TEXT("  GameInstance available via ServiceLocator"));

		if (USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>())
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
				TEXT("  DataManager subsystem found (SSOT)"));

			FSuspenseCoreUnifiedItemData ItemData;
			if (DataMgr->GetUnifiedItemData(ItemID, ItemData))
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
					TEXT("  GetUnifiedItemData succeeded"));

				if (!ItemData.EquipmentActorClass.IsNull())
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
						TEXT("  EquipmentActorClass is set: %s"),
						*ItemData.EquipmentActorClass.ToString());

					// Check if already loaded
					if (ItemData.EquipmentActorClass.IsValid())
					{
						TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.Get();
						if (ActorClass)
						{
							UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
								TEXT("✓ DataManager SUCCESS: Class already loaded - %s"),
								*ActorClass->GetName());
							return ActorClass;
						}
					}

					// Synchronous load
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
						TEXT("  Class not loaded, performing LoadSynchronous..."));
					TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.LoadSynchronous();

					if (ActorClass)
					{
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
							TEXT("✓ DataManager SUCCESS: Class loaded synchronously - %s"),
							*ActorClass->GetName());
						return ActorClass;
					}
					else
					{
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Error, TEXT("✗ LoadSynchronous FAILED!"));
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
							TEXT("  Path: %s"),
							*ItemData.EquipmentActorClass.ToString());
						UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
							TEXT("  This asset may be missing or invalid"));
					}
				}
				else
				{
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
						TEXT("✗ EquipmentActorClass is NULL in ItemData!"));
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
						TEXT("  Item '%s' has no EquipmentActorClass configured"),
						*ItemID.ToString());
					UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
						TEXT("  Solution: Open Item Data Table and set EquipmentActorClass for this item"));
				}
			}
			else
			{
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
					TEXT("✗ GetUnifiedItemData FAILED for ItemID: %s"),
					*ItemID.ToString());
				UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
					TEXT("  Item may not exist in Item Data Table"));
			}
		}
		else
		{
			UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
				TEXT("  DataManager subsystem not found in GameInstance!"));
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
			TEXT("  GameInstance is NULL from ServiceLocator!"));
	}

	UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
		TEXT("=== ResolveActorClass FAILED - Returning nullptr ==="));
	return nullptr;
}

FName USuspenseCoreEquipmentVisualizationService::ResolveAttachSocket(
    AActor* Character,
    const FName ItemID,
    int32 SlotIndex,
    bool bCallerHoldsLock,
    int32 KnownActiveSlot) const
{
    // ============================================================================
    // Читаем сокет из DataTable через DataManager (SSOT)
    // ============================================================================

    // Step 1: Get DataManager to access DataTable
    if (!CachedServiceLocator)
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] CachedServiceLocator is NULL - using fallback"));
        return FName(TEXT("GripPoint"));
    }

    UGameInstance* GI = CachedServiceLocator->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] GameInstance not available - using fallback"));
        return FName(TEXT("GripPoint"));
    }

    USuspenseCoreDataManager* DataManager = GI->GetSubsystem<USuspenseCoreDataManager>();
    if (!DataManager)
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] DataManager not available - using fallback"));
        return FName(TEXT("GripPoint"));
    }

    // Step 2: Load full item data from DataTable (SSOT)
    FSuspenseCoreUnifiedItemData ItemData;
    if (!DataManager->GetUnifiedItemData(ItemID, ItemData))
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
            TEXT("[ResolveAttachSocket] Failed to load ItemData for ItemID: %s"),
            *ItemID.ToString());
        return FName(TEXT("GripPoint"));
    }

    // Step 3: Determine if slot is active
    // bCallerHoldsLock=true means caller already holds VisualLock, use KnownActiveSlot directly
    // bCallerHoldsLock=false means we need to acquire lock and read from state
    bool bIsActiveSlot = false;
    if (bCallerHoldsLock)
    {
        // Caller holds lock, use provided KnownActiveSlot (can be INDEX_NONE if no weapon active)
        bIsActiveSlot = (SlotIndex == KnownActiveSlot);
    }
    else
    {
        EQUIPMENT_RW_READ_LOCK(VisualLock);
        if (const FSuspenseCoreVisCharState* S = Characters.Find(Character))
        {
            bIsActiveSlot = (SlotIndex == S->ActiveSlot);
        }
    }

    // Step 4: Get correct socket from DataTable based on state
    const FName ResolvedSocket = ItemData.GetSocketForState(bIsActiveSlot);

    if (ResolvedSocket.IsNone())
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] Socket is None in DataTable for ItemID: %s, State: %s"),
            *ItemID.ToString(),
            bIsActiveSlot ? TEXT("Active") : TEXT("Inactive"));
        return FName(TEXT("GripPoint"));
    }

    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("[ResolveAttachSocket] ✓ Resolved socket from DataManager (SSOT):"));
    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("  ItemID: %s"), *ItemID.ToString());
    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("  SlotIndex: %d (Active: %s)"), SlotIndex, bIsActiveSlot ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("  Socket: %s"), *ResolvedSocket.ToString());

    return ResolvedSocket;
}

FTransform USuspenseCoreEquipmentVisualizationService::ResolveAttachOffset(
    AActor* Character,
    const FName ItemID,
    int32 SlotIndex,
    bool bCallerHoldsLock,
    int32 KnownActiveSlot) const
{
    // ============================================================================
    // Читаем оффсет из DataTable через DataManager (SSOT)
    // ============================================================================

    // Step 1: Get DataManager to access DataTable
    if (!CachedServiceLocator)
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachOffset] CachedServiceLocator is NULL - using Identity"));
        return FTransform::Identity;
    }

    UGameInstance* GI = CachedServiceLocator->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachOffset] GameInstance not available - using Identity"));
        return FTransform::Identity;
    }

    USuspenseCoreDataManager* DataManager = GI->GetSubsystem<USuspenseCoreDataManager>();
    if (!DataManager)
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Warning,
            TEXT("[ResolveAttachOffset] DataManager not available - using Identity"));
        return FTransform::Identity;
    }

    // Step 2: Load full item data from DataTable (SSOT)
    FSuspenseCoreUnifiedItemData ItemData;
    if (!DataManager->GetUnifiedItemData(ItemID, ItemData))
    {
        UE_LOG(LogSuspenseCoreEquipmentVisualization, Error,
            TEXT("[ResolveAttachOffset] Failed to load ItemData for ItemID: %s"),
            *ItemID.ToString());
        return FTransform::Identity;
    }

    // Step 3: Determine if slot is active
    // bCallerHoldsLock=true means caller already holds VisualLock, use KnownActiveSlot directly
    // bCallerHoldsLock=false means we need to acquire lock and read from state
    bool bIsActiveSlot = false;
    if (bCallerHoldsLock)
    {
        // Caller holds lock, use provided KnownActiveSlot (can be INDEX_NONE if no weapon active)
        bIsActiveSlot = (SlotIndex == KnownActiveSlot);
    }
    else
    {
        EQUIPMENT_RW_READ_LOCK(VisualLock);
        if (const FSuspenseCoreVisCharState* S = Characters.Find(Character))
        {
            bIsActiveSlot = (SlotIndex == S->ActiveSlot);
        }
    }

    // Step 4: Get correct offset from DataTable based on state
    const FTransform ResolvedOffset = ItemData.GetOffsetForState(bIsActiveSlot);

    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("[ResolveAttachOffset] ✓ Resolved offset from DataManager (SSOT):"));
    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("  ItemID: %s"), *ItemID.ToString());
    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("  SlotIndex: %d (Active: %s)"), SlotIndex, bIsActiveSlot ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogSuspenseCoreEquipmentVisualization, Log,
        TEXT("  Offset: Loc(%s) Rot(%s) Scale(%s)"),
        *ResolvedOffset.GetLocation().ToString(),
        *ResolvedOffset.Rotator().ToString(),
        *ResolvedOffset.GetScale3D().ToString());

    return ResolvedOffset;
}

// ===== Rate limiter ===============================================================

bool USuspenseCoreEquipmentVisualizationService::RateLimit() const
{
	if (CachedUpdateIntervalSec <= 0.0) return false;

	const double Now = FPlatformTime::Seconds();
	if ((Now - LastProcessTimeSec) < CachedUpdateIntervalSec) return true;

	const_cast<USuspenseCoreEquipmentVisualizationService*>(this)->LastProcessTimeSec = Now;
	return false;
}

// ===== Event metadata parsing =====================================================

bool USuspenseCoreEquipmentVisualizationService::TryParseInt(const FSuspenseCoreEventData& EventData, const TCHAR* Key, int32& OutValue)
{
	// CRITICAL FIX: First check IntPayload (Bridge uses SetInt for Slot)
	// EventData has separate containers: IntPayload for SetInt(), StringPayload for SetString()
	const FName KeyName(Key);

	// Try IntPayload first (this is where Bridge stores Slot via SetInt)
	const int32 IntValue = EventData.GetInt(KeyName, INDEX_NONE);
	if (IntValue != INDEX_NONE)
	{
		OutValue = IntValue;
		return true;
	}

	// Fallback: try StringPayload and parse (for backward compatibility)
	const FString S = EventData.GetString(KeyName);
	if (!S.IsEmpty())
	{
		OutValue = LexToInt(S, OutValue);
		return true;
	}

	return false;
}

FName USuspenseCoreEquipmentVisualizationService::ParseName(const FSuspenseCoreEventData& EventData, const TCHAR* Key, const FName DefaultValue)
{
	const FString S = EventData.GetString(FName(Key));
	return S.IsEmpty() ? DefaultValue : FName(*S);
}

// EquipmentVisualizationServiceImpl.cpp
// Copyright MedCom

#include "Services/EquipmentVisualizationServiceImpl.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "ItemSystem/MedComItemManager.h"
#include "Services/EquipmentServiceMacros.h"

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

bool UEquipmentVisualizationServiceImpl::InitializeService(const FServiceInitParams& InitParams)
{
	EQUIPMENT_CRITICAL_LOCK(VisualLock);

	if (IsServiceReady())
	{
		UE_LOG(LogEquipmentVisualization, Verbose, TEXT("Init skipped: already Ready"));
		return true;
	}

	UE_LOG(LogEquipmentVisualization, Log, TEXT(">>> VisualizationService: InitializeService STARTED"));

	// ============================================================================
	// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #1: Кэшируем ServiceLocator из InitParams
	// ============================================================================
	// ServiceLocator передается через InitParams.ServiceLocator из EquipmentServiceLocator::InitializeService
	// Это единственное надежное место для получения ServiceLocator для stateless сервиса
	CachedServiceLocator = InitParams.ServiceLocator;
	
	if (!CachedServiceLocator)
	{
		LifecycleState = EServiceLifecycleState::Failed;
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("InitializeService FAILED: ServiceLocator not provided in InitParams!"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  This indicates a problem with service registration or initialization order."));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Make sure this service is registered through UEquipmentServiceLocator."));
		return false;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("ServiceLocator cached successfully from InitParams: %p"), 
		CachedServiceLocator.Get());

	// Initialize service tag
	VisualizationServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Visualization"));
	if (!VisualizationServiceTag.IsValid())
	{
		LifecycleState = EServiceLifecycleState::Failed;
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("InitializeService FAILED: Could not request Service.Equipment.Visualization tag"));
		return false;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("Service tag initialized: %s"), 
		*VisualizationServiceTag.ToString());

	// Initialize EventBus
	EventBus = FEquipmentEventBus::Get();
	if (!EventBus.IsValid())
	{
		LifecycleState = EServiceLifecycleState::Failed;
		UE_LOG(LogEquipmentVisualization, Error, TEXT("InitializeService FAILED: EventBus missing"));
		return false;
	}

	UE_LOG(LogEquipmentVisualization, Log, TEXT("EventBus acquired successfully"));

	LifecycleState = EServiceLifecycleState::Initializing;

	// Default config
	MaxUpdateRateHz    = 30.f;
	VisualQualityLevel = 2;     // High
	bEnableBatching    = true;

	CachedUpdateIntervalSec = MaxUpdateRateHz > 0.f ? (1.0 / MaxUpdateRateHz) : 0.0;
	LastProcessTimeSec      = 0.0;

	// Initialize event tags (non-fatal if not found)
	Tag_OnEquipped          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Equipped"), false);
	Tag_OnUnequipped        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Unequipped"), false);
	Tag_OnSlotSwitched      = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.SlotSwitched"), false);
	Tag_VisRefreshAll       = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Visual.RefreshAll"), false);

	// Initialize dependency service tags
	Tag_ActorFactory     = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"), false);
	Tag_AttachmentSystem = FGameplayTag::RequestGameplayTag(TEXT("Service.AttachmentSystem"), false);
	Tag_VisualController = FGameplayTag::RequestGameplayTag(TEXT("Service.VisualController"), false);
	Tag_EquipmentData    = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"), false);

	UE_LOG(LogEquipmentVisualization, Log, TEXT("Event tags initialized"));
	UE_LOG(LogEquipmentVisualization, Log, TEXT("Setting up event handlers..."));

	SetupEventHandlers();

	UE_LOG(LogEquipmentVisualization, Log, 
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
				UE_LOG(LogEquipmentVisualization, Verbose, 
					TEXT("Applied quality preset to VisualController"));
			}
		}
	}

	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("VisService init completed: MaxRate=%.1fHz, Quality=%d"),
		MaxUpdateRateHz, VisualQualityLevel);

	LifecycleState = EServiceLifecycleState::Ready;
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("<<< VisualizationService: InitializeService SUCCESS"));
	return true;
}

bool UEquipmentVisualizationServiceImpl::ShutdownService(bool /*bForce*/)
{
	EQUIPMENT_CRITICAL_LOCK(VisualLock);

	UE_LOG(LogEquipmentVisualization, Log, TEXT(">>> VisualizationService: ShutdownService STARTED"));

	TeardownEventHandlers();

	// Release all visual instances
	for (auto& Pair : Characters)
	{
		AActor* Character = Pair.Key.Get();
		if (!Character) continue;

		for (auto& SlotPair : Pair.Value.SlotActors)
		{
			ReleaseVisualActor(Character, SlotPair.Key, /*bInstant=*/true);
		}
	}
	Characters.Empty();

	LifecycleState = EServiceLifecycleState::Shutdown;
	
	UE_LOG(LogEquipmentVisualization, Log, TEXT("<<< VisualizationService: ShutdownService COMPLETED"));
	return true;
}

FGameplayTagContainer UEquipmentVisualizationServiceImpl::GetRequiredDependencies() const
{
	FGameplayTagContainer Deps;
	// Optional dependencies - presentation layer services
	Deps.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"), false));
	Deps.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.AttachmentSystem"), false));
	Deps.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.VisualController"), false));
	Deps.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"), false));

	return Deps;
}

bool UEquipmentVisualizationServiceImpl::ValidateService(TArray<FText>& OutErrors) const
{
	bool bOk = true;
	
	if (!VisualizationServiceTag.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("VisualizationServiceTag is invalid")));
		bOk = false;
	}
	
	if (!EventBus.IsValid())
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
				UE_LOG(LogEquipmentVisualization, Warning, TEXT("Optional service not available: %s"), *Tag.ToString());
			}
		}
	}
	
	return bOk;
}

void UEquipmentVisualizationServiceImpl::ResetService()
{
	EQUIPMENT_CRITICAL_LOCK(VisualLock);
	
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
	EventBus.Reset();
	LastProcessTimeSec = 0.0;
	LifecycleState = EServiceLifecycleState::Uninitialized;
	
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("VisService reset"));
}

FString UEquipmentVisualizationServiceImpl::GetServiceStats() const
{
	EQUIPMENT_CRITICAL_LOCK(VisualLock);
	
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

void UEquipmentVisualizationServiceImpl::SetupEventHandlers()
{
	auto Bus = EventBus.Pin();
	check(Bus.IsValid());

	if (Tag_OnEquipped.IsValid())
	{
		Subscriptions.Add(Bus->Subscribe(
			Tag_OnEquipped,
			FEventHandlerDelegate::CreateUObject(this, &UEquipmentVisualizationServiceImpl::OnEquipped),
			EEventPriority::Normal, EEventExecutionContext::GameThread, this));
	}

	if (Tag_OnUnequipped.IsValid())
	{
		Subscriptions.Add(Bus->Subscribe(
			Tag_OnUnequipped,
			FEventHandlerDelegate::CreateUObject(this, &UEquipmentVisualizationServiceImpl::OnUnequipped),
			EEventPriority::Normal, EEventExecutionContext::GameThread, this));
	}

	if (Tag_OnSlotSwitched.IsValid())
	{
		Subscriptions.Add(Bus->Subscribe(
			Tag_OnSlotSwitched,
			FEventHandlerDelegate::CreateUObject(this, &UEquipmentVisualizationServiceImpl::OnSlotSwitched),
			EEventPriority::Normal, EEventExecutionContext::GameThread, this));
	}

	if (Tag_VisRefreshAll.IsValid())
	{
		Subscriptions.Add(Bus->Subscribe(
			Tag_VisRefreshAll,
			FEventHandlerDelegate::CreateUObject(this, &UEquipmentVisualizationServiceImpl::OnRefreshAll),
			EEventPriority::Normal, EEventExecutionContext::GameThread, this));
	}
}

void UEquipmentVisualizationServiceImpl::TeardownEventHandlers()
{
	if (auto Bus = EventBus.Pin())
	{
		for (const auto& H : Subscriptions) { Bus->Unsubscribe(H); }
	}
	Subscriptions.Empty();
}

// ===== Event handlers ============================================================

void UEquipmentVisualizationServiceImpl::OnEquipped(const FEquipmentEventData& E)
{
	UE_LOG(LogEquipmentVisualization, Warning, TEXT(">>> OnEquipped event received"));
	
	// Step 1: Rate limiting check
	if (RateLimit())
	{
		UE_LOG(LogEquipmentVisualization, Verbose, TEXT("OnEquipped rate limited - skipping"));
		return;
	}
	
	// Step 2: Extract and validate Character
	AActor* Character = E.GetTargetAs<AActor>();
	if (!Character)
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("OnEquipped FAILED: Target is NULL!"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Event has no valid Target actor"));
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("OnEquipped: Character = %s (Class: %s)"),
		*Character->GetName(), 
		*Character->GetClass()->GetName());
	
	// Step 3: Parse Slot metadata
	int32 Slot = INDEX_NONE;
	const bool bSlotParsed = TryParseInt(E, TEXT("Slot"), Slot);
	
	if (!bSlotParsed)
	{
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: Could not parse 'Slot' metadata"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Metadata['Slot'] = '%s'"),
			*E.GetMetadata(TEXT("Slot"), TEXT("<empty>")));
		return;
	}
	
	if (Slot == INDEX_NONE)
	{
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: Slot is INDEX_NONE after parsing"));
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, TEXT("OnEquipped: Slot = %d"), Slot);
	
	// Step 4: Parse ItemID metadata
	const FName ItemID = ParseName(E, TEXT("ItemID"));
	
	if (ItemID.IsNone())
	{
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("OnEquipped FAILED: ItemID is None after parsing"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Metadata['ItemID'] = '%s'"),
			*E.GetMetadata(TEXT("ItemID"), TEXT("<empty>")));
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, TEXT("OnEquipped: ItemID = %s"), *ItemID.ToString());
	
	// Step 5: Log all available metadata for diagnostics
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("OnEquipped: All metadata:"));
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("  Slot       = %s"), *E.GetMetadata(TEXT("Slot"), TEXT("<none>")));
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("  ItemID     = %s"), *E.GetMetadata(TEXT("ItemID"), TEXT("<none>")));
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("  InstanceID = %s"), *E.GetMetadata(TEXT("InstanceID"), TEXT("<none>")));
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("  Quantity   = %s"), *E.GetMetadata(TEXT("Quantity"), TEXT("<none>")));
	
	// Step 6: Final validation before calling UpdateVisualForSlot
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("OnEquipped: All validation passed - calling UpdateVisualForSlot"));
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("  Character: %s, Slot: %d, ItemID: %s"),
		*Character->GetName(), Slot, *ItemID.ToString());
	
	// Step 7: Execute visual update
	UpdateVisualForSlot(Character, Slot, ItemID, /*bInstant=*/false);
	
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("OnEquipped: UpdateVisualForSlot call completed"));
}

void UEquipmentVisualizationServiceImpl::OnUnequipped(const FEquipmentEventData& E)
{
	UE_LOG(LogEquipmentVisualization, Warning, TEXT(">>> OnUnequipped event received"));
	
	if (RateLimit())
	{
		UE_LOG(LogEquipmentVisualization, Verbose, TEXT("OnUnequipped rate limited - skipping"));
		return;
	}

	AActor* Character = E.GetTargetAs<AActor>();
	if (!Character)
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("OnUnequipped FAILED: Target is NULL!"));
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("OnUnequipped: Character = %s"), *Character->GetName());

	int32 Slot = INDEX_NONE;
	const bool bSlotParsed = TryParseInt(E, TEXT("Slot"), Slot);
	
	if (!bSlotParsed || Slot == INDEX_NONE)
	{
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("OnUnequipped FAILED: Could not parse Slot (got %d)"), Slot);
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Metadata['Slot'] = '%s'"),
			*E.GetMetadata(TEXT("Slot"), TEXT("<empty>")));
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("OnUnequipped: Hiding visual for slot %d on %s"),
		Slot, *Character->GetName());

	HideVisualForSlot(Character, Slot, /*bInstant=*/false);
	
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("OnUnequipped: HideVisualForSlot call completed"));
}

void UEquipmentVisualizationServiceImpl::OnSlotSwitched(const FEquipmentEventData& E)
{
	if (RateLimit()) return;

	AActor* Character = E.GetTargetAs<AActor>();
	if (!Character) return;

	int32 ActiveSlot = INDEX_NONE;
	TryParseInt(E, TEXT("ActiveSlot"), ActiveSlot);

	EQUIPMENT_CRITICAL_LOCK(VisualLock);
	FVisCharState& S = Characters.FindOrAdd(Character);
	S.ActiveSlot = ActiveSlot;
}

void UEquipmentVisualizationServiceImpl::OnRefreshAll(const FEquipmentEventData& E)
{
	AActor* Character = E.GetTargetAs<AActor>();
	if (!Character) return;

	const bool bForce = E.GetMetadata(TEXT("Force"), TEXT("false")).Equals(TEXT("true"), ESearchCase::IgnoreCase);
	RequestRefresh(Character, bForce);
}

// ===== Public trigger ======================================================

void UEquipmentVisualizationServiceImpl::RequestRefresh(AActor* Character, bool bForce)
{
	RefreshAllVisuals(Character, bForce);
}

// ===== High-level operations ==============================================

void UEquipmentVisualizationServiceImpl::UpdateVisualForSlot(AActor* Character, int32 SlotIndex, const FName ItemID, bool bInstant)
{
	UE_LOG(LogEquipmentVisualization, Warning, 
		TEXT("=== UpdateVisualForSlot START ==="));
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("  Character: %s"), Character ? *Character->GetName() : TEXT("NULL"));
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("  SlotIndex: %d"), SlotIndex);
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("  ItemID: %s"), *ItemID.ToString());
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("  bInstant: %s"), bInstant ? TEXT("true") : TEXT("false"));
	
	if (!Character || ItemID.IsNone())
	{
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("UpdateVisualForSlot ABORTED: Invalid parameters"));
		return;
	}

	EQUIPMENT_CRITICAL_LOCK(VisualLock);

	// 1) Acquire/create visual actor
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("Step 1: Acquiring visual actor for ItemID=%s, Slot=%d"),
		*ItemID.ToString(), SlotIndex);
	
	AActor* Visual = AcquireVisualActor(Character, ItemID, SlotIndex);
	if (!Visual)
	{
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("UpdateVisualForSlot FAILED: Could not acquire visual actor!"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  AcquireVisualActor returned nullptr"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Possible causes:"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("    1. ActorFactory service not available"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("    2. Actor class not found for ItemID %s"), *ItemID.ToString());
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("    3. Spawn failed due to collision or other issues"));
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Log,
		TEXT("Step 1 SUCCESS: Visual actor acquired - %s (Class: %s)"),
		*Visual->GetName(), *Visual->GetClass()->GetName());

	// 2) Resolve socket/offset and attach
	UE_LOG(LogEquipmentVisualization, Log, TEXT("Step 2: Resolving attachment parameters"));
	
	const FName Socket      = ResolveAttachSocket(Character, ItemID, SlotIndex);
	const FTransform Offset = ResolveAttachOffset(Character, ItemID, SlotIndex);
	
	UE_LOG(LogEquipmentVisualization, Log,
		TEXT("  Socket: %s, Offset: Loc(%s) Rot(%s)"),
		*Socket.ToString(),
		*Offset.GetLocation().ToString(),
		*Offset.Rotator().ToString());

	if (!AttachActorToCharacter(Character, Visual, Socket, Offset))
	{
		// Failed to attach - release actor
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("UpdateVisualForSlot FAILED: Could not attach actor to character!"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  AttachActorToCharacter returned false"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Possible causes:"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("    1. Character has no SkeletalMeshComponent"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("    2. Socket '%s' doesn't exist on character mesh"), *Socket.ToString());
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("    3. AttachmentSystem service failed"));
		
		ReleaseVisualActor(Character, SlotIndex, /*bInstant=*/true);
		return;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, TEXT("Step 2 SUCCESS: Actor attached to character"));

	// 3) Apply quality settings
	UE_LOG(LogEquipmentVisualization, Verbose, TEXT("Step 3: Applying quality settings"));
	ApplyQualitySettings(Visual);

	// 4) Store in state
	FVisCharState& S = Characters.FindOrAdd(Character);
	S.SlotActors.FindOrAdd(SlotIndex) = Visual;
	
	UE_LOG(LogEquipmentVisualization, Log, TEXT("Step 4: Stored in state map"));

	// ============================================================================
	// ИСПРАВЛЕНИЕ: Используем CachedServiceLocator вместо Get(this)
	// ============================================================================
	// 5) Apply visual profile (via VisualController)
	UE_LOG(LogEquipmentVisualization, Log, TEXT("Step 5: Applying visual profile"));
	
	if (CachedServiceLocator)
	{
		if (UObject* VisualCtl = CachedServiceLocator->TryGetService(Tag_VisualController))
		{
			if (UFunction* Fn = VisualCtl->FindFunction(FName(TEXT("ApplyVisualProfile"))))
			{
				struct { AActor* Equipment; FGameplayTag Profile; bool bSmooth; bool ReturnValue; }
				Params{ Visual, FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Active")), !bInstant, false };
				VisualCtl->ProcessEvent(Fn, &Params);
				
				UE_LOG(LogEquipmentVisualization, Log,
					TEXT("  Visual profile applied via VisualController"));
			}
			else
			{
				UE_LOG(LogEquipmentVisualization, Warning,
					TEXT("  VisualController doesn't have ApplyVisualProfile method"));
			}
		}
		else
		{
			UE_LOG(LogEquipmentVisualization, Warning,
				TEXT("  VisualController service not available"));
		}
	}
	else
	{
		UE_LOG(LogEquipmentVisualization, Warning,
			TEXT("  CachedServiceLocator is NULL"));
	}
	
	UE_LOG(LogEquipmentVisualization, Warning, 
		TEXT("=== UpdateVisualForSlot SUCCESS ==="));
	UE_LOG(LogEquipmentVisualization, Warning,
		TEXT("  Visual equipment %s now attached to %s at slot %d"),
		*Visual->GetName(), *Character->GetName(), SlotIndex);
}

void UEquipmentVisualizationServiceImpl::HideVisualForSlot(AActor* Character, int32 SlotIndex, bool bInstant)
{
	EQUIPMENT_CRITICAL_LOCK(VisualLock);

	FVisCharState* S = Characters.Find(Character);
	if (!S) return;

	TWeakObjectPtr<AActor>* Found = S->SlotActors.Find(SlotIndex);
	if (!Found) return;

	AActor* Visual = Found->Get();
	if (Visual)
	{
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

	// Return actor to pool/destroy via Factory
	ReleaseVisualActor(Character, SlotIndex, bInstant);
	S->SlotActors.Remove(SlotIndex);
}

void UEquipmentVisualizationServiceImpl::RefreshAllVisuals(AActor* Character, bool bForce)
{
	if (!Character) return;

	// Request data layer to resend current state (ketchup)
	if (auto Bus = EventBus.Pin())
	{
		FEquipmentEventData Req;
		Req.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.RequestResend"));
		Req.Target    = Character;
		Req.AddMetadata(TEXT("Reason"), bForce ? TEXT("ForceRefreshVisual") : TEXT("RefreshVisual"));
		Bus->Broadcast(Req);
	}

	// Refresh quality for current visuals
	{
		EQUIPMENT_CRITICAL_LOCK(VisualLock);
		if (FVisCharState* S = Characters.Find(Character))
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

AActor* UEquipmentVisualizationServiceImpl::AcquireVisualActor(AActor* Character, const FName ItemID, int32 SlotIndex)
{
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("=== AcquireVisualActor START ==="));
	UE_LOG(LogEquipmentVisualization, Warning, 
		TEXT("  Character: %s"), Character ? *Character->GetName() : TEXT("NULL"));
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("  ItemID: %s"), *ItemID.ToString());
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("  SlotIndex: %d"), SlotIndex);
	
	if (!Character || ItemID.IsNone())
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("AcquireVisualActor: Invalid parameters!"));
		return nullptr;
	}

	// ============================================================================
	// ИСПРАВЛЕНИЕ #4: Проверяем кэшированный ServiceLocator
	// ============================================================================
	if (!CachedServiceLocator)
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("  CRITICAL: CachedServiceLocator is NULL!"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Cannot proceed with actor acquisition."));
		return nullptr;
	}

	// Step 1: Try to get ActorFactory service via cached ServiceLocator
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("Step 1: Attempting to get ActorFactory service"));
	
	if (UObject* FactoryObj = CachedServiceLocator->TryGetService(Tag_ActorFactory))
	{
		UE_LOG(LogEquipmentVisualization, Log, 
			TEXT("  ActorFactory service found: %s"), 
			*FactoryObj->GetClass()->GetName());
		
		// PREFERRED: via C++ interface IMedComActorFactory
		if (FactoryObj->GetClass()->ImplementsInterface(UMedComActorFactory::StaticClass()))
		{
			UE_LOG(LogEquipmentVisualization, Log, 
				TEXT("  Using C++ interface IMedComActorFactory"));
			
			if (IMedComActorFactory* Factory = static_cast<IMedComActorFactory*>(
				FactoryObj->GetInterfaceAddress(UMedComActorFactory::StaticClass())))
			{
				FEquipmentActorSpawnParams Params;
				Params.ItemInstance.ItemID = ItemID;
				Params.SpawnTransform      = Character->GetActorTransform();
				Params.Owner               = Character;
				Params.Instigator          = Cast<APawn>(Character);
				Params.bDeferredSpawn      = false;
				Params.bNoCollisionFail    = true;
				Params.CustomParameters.Add(TEXT("SlotIndex"), LexToString(SlotIndex));

				UE_LOG(LogEquipmentVisualization, Warning, 
					TEXT("  Calling Factory->SpawnEquipmentActor..."));
				
				const FEquipmentActorSpawnResult R = Factory->SpawnEquipmentActor(Params);
				
				if (R.bSuccess && R.SpawnedActor)
				{
					UE_LOG(LogEquipmentVisualization, Warning, 
						TEXT("✓ ActorFactory SUCCESS: Spawned %s"), 
						*R.SpawnedActor->GetName());
					return R.SpawnedActor;
				}
				else
				{
					UE_LOG(LogEquipmentVisualization, Error, 
						TEXT("✗ ActorFactory FAILED: %s"), 
						*R.FailureReason.ToString());
				}
			}
			else
			{
				UE_LOG(LogEquipmentVisualization, Error, 
					TEXT("  Failed to get interface address for IMedComActorFactory"));
			}
		}

		// Fallback: via ProcessEvent (BP compatibility) - опущено для краткости
	}
	else
	{
		UE_LOG(LogEquipmentVisualization, Warning, 
			TEXT("  ActorFactory service not registered in ServiceLocator"));
		UE_LOG(LogEquipmentVisualization, Warning, 
			TEXT("  Trying direct spawn fallback..."));
	}

	// Ultimate fallback: direct SpawnActor
	UE_LOG(LogEquipmentVisualization, Warning, 
		TEXT("Step 2: FALLBACK - Direct spawn without factory"));
	UE_LOG(LogEquipmentVisualization, Warning, 
		TEXT("  Resolving actor class for ItemID: %s"), *ItemID.ToString());
	
	TSubclassOf<AActor> Class = ResolveActorClass(ItemID);
	if (!Class)
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("✗ ResolveActorClass returned NULL for ItemID: %s"), 
			*ItemID.ToString());
		return nullptr;
	}
	
	// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: используем World от Character
	UWorld* World = Character->GetWorld();
	if (!World)
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("✗ Character has no World!"));
		return nullptr;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("  Actor class resolved: %s"), *Class->GetName());
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("  Spawning actor directly..."));

	FActorSpawnParameters SP;
	SP.Owner = Character;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AActor* SpawnedActor = World->SpawnActor<AActor>(Class, Character->GetActorTransform(), SP);
	
	if (SpawnedActor)
	{
		UE_LOG(LogEquipmentVisualization, Warning, 
			TEXT("✓ Direct spawn SUCCESS: %s"), 
			*SpawnedActor->GetName());
	}
	else
	{
		UE_LOG(LogEquipmentVisualization, Error, TEXT("✗ Direct spawn FAILED!"));
	}
	
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("=== AcquireVisualActor END ==="));
	
	return SpawnedActor;
}

void UEquipmentVisualizationServiceImpl::ReleaseVisualActor(AActor* Character, int32 SlotIndex, bool bInstant)
{
	if (!Character) return;

	EQUIPMENT_CRITICAL_LOCK(VisualLock);

	FVisCharState* S = Characters.Find(Character);
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
			if (FactoryObj->GetClass()->ImplementsInterface(UMedComActorFactory::StaticClass()))
			{
				if (IMedComActorFactory* Factory = static_cast<IMedComActorFactory*>(
					FactoryObj->GetInterfaceAddress(UMedComActorFactory::StaticClass())))
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

bool UEquipmentVisualizationServiceImpl::AttachActorToCharacter(AActor* Character, AActor* Visual, const FName Socket, const FTransform& Offset)
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

	// Fallback: attach to first SkeletalMeshComponent
	if (USkeletalMeshComponent* Skel = Character->FindComponentByClass<USkeletalMeshComponent>())
	{
		USceneComponent* Root = Visual->GetRootComponent();
		if (!Root) return false;

		Root->AttachToComponent(Skel, FAttachmentTransformRules::SnapToTargetIncludingScale, Socket);
		Root->SetRelativeTransform(Offset);
		return true;
	}

	return false;
}

void UEquipmentVisualizationServiceImpl::ApplyQualitySettings(AActor* Visual) const
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

TSubclassOf<AActor> UEquipmentVisualizationServiceImpl::ResolveActorClass(const FName ItemID) const
{
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("=== ResolveActorClass START ==="));
	UE_LOG(LogEquipmentVisualization, Warning, TEXT("  ItemID: %s"), *ItemID.ToString());
	
	// ============================================================================
	// ИСПРАВЛЕНИЕ #3: Проверяем кэшированный ServiceLocator
	// ============================================================================
	if (!CachedServiceLocator)
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("  CRITICAL: CachedServiceLocator is NULL!"));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  This should never happen if InitializeService() succeeded."));
		UE_LOG(LogEquipmentVisualization, Error,
			TEXT("  Service may not have been initialized properly."));
		return nullptr;
	}
	
	UE_LOG(LogEquipmentVisualization, Log, 
		TEXT("  Using cached ServiceLocator: %p"), 
		CachedServiceLocator.Get());
	
	// Step 1: Try DataService via cached ServiceLocator
	if (UObject* DataSvc = CachedServiceLocator->TryGetService(Tag_EquipmentData))
	{
		UE_LOG(LogEquipmentVisualization, Log, TEXT("  DataService found"));
		
		if (UFunction* Fn = DataSvc->FindFunction(FName(TEXT("GetVisualActorClass"))))
		{
			struct { FName ItemID; TSubclassOf<AActor> ReturnValue; } P{ ItemID, nullptr };
			DataSvc->ProcessEvent(Fn, &P);
			if (P.ReturnValue)
			{
				UE_LOG(LogEquipmentVisualization, Warning, 
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
				UE_LOG(LogEquipmentVisualization, Warning, 
					TEXT("✓ DataService->GetEquipmentActorClass returned: %s"), 
					*P.ReturnValue->GetName());
				return P.ReturnValue;
			}
		}
		
		UE_LOG(LogEquipmentVisualization, Warning, 
			TEXT("  DataService has no GetVisualActorClass or GetEquipmentActorClass methods"));
	}
	else
	{
		UE_LOG(LogEquipmentVisualization, Warning, 
			TEXT("  DataService not available in ServiceLocator"));
	}
	
	// Step 2: Try ItemManager subsystem via cached ServiceLocator->GetGameInstance()
	UE_LOG(LogEquipmentVisualization, Warning, 
		TEXT("Step 2: Trying ItemManager subsystem via ServiceLocator"));
	
	if (UGameInstance* GI = CachedServiceLocator->GetGameInstance())
	{
		UE_LOG(LogEquipmentVisualization, Log, 
			TEXT("  GameInstance available via ServiceLocator"));
		
		if (UMedComItemManager* ItemMgr = GI->GetSubsystem<UMedComItemManager>())
		{
			UE_LOG(LogEquipmentVisualization, Log, 
				TEXT("  ItemManager subsystem found"));
			
			FMedComUnifiedItemData ItemData;
			if (ItemMgr->GetUnifiedItemData(ItemID, ItemData))
			{
				UE_LOG(LogEquipmentVisualization, Log, 
					TEXT("  GetUnifiedItemData succeeded"));
				
				if (!ItemData.EquipmentActorClass.IsNull())
				{
					UE_LOG(LogEquipmentVisualization, Log, 
						TEXT("  EquipmentActorClass is set: %s"), 
						*ItemData.EquipmentActorClass.ToString());
					
					// Check if already loaded
					if (ItemData.EquipmentActorClass.IsValid())
					{
						TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.Get();
						if (ActorClass)
						{
							UE_LOG(LogEquipmentVisualization, Warning, 
								TEXT("✓ ItemManager SUCCESS: Class already loaded - %s"), 
								*ActorClass->GetName());
							return ActorClass;
						}
					}
					
					// Synchronous load
					UE_LOG(LogEquipmentVisualization, Log, 
						TEXT("  Class not loaded, performing LoadSynchronous..."));
					TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.LoadSynchronous();
					
					if (ActorClass)
					{
						UE_LOG(LogEquipmentVisualization, Warning, 
							TEXT("✓ ItemManager SUCCESS: Class loaded synchronously - %s"), 
							*ActorClass->GetName());
						return ActorClass;
					}
					else
					{
						UE_LOG(LogEquipmentVisualization, Error, TEXT("✗ LoadSynchronous FAILED!"));
						UE_LOG(LogEquipmentVisualization, Error, 
							TEXT("  Path: %s"), 
							*ItemData.EquipmentActorClass.ToString());
						UE_LOG(LogEquipmentVisualization, Error, 
							TEXT("  This asset may be missing or invalid"));
					}
				}
				else
				{
					UE_LOG(LogEquipmentVisualization, Error, 
						TEXT("✗ EquipmentActorClass is NULL in ItemData!"));
					UE_LOG(LogEquipmentVisualization, Error, 
						TEXT("  Item '%s' has no EquipmentActorClass configured"), 
						*ItemID.ToString());
					UE_LOG(LogEquipmentVisualization, Error, 
						TEXT("  Solution: Open Item Data Table and set EquipmentActorClass for this item"));
				}
			}
			else
			{
				UE_LOG(LogEquipmentVisualization, Error, 
					TEXT("✗ GetUnifiedItemData FAILED for ItemID: %s"), 
					*ItemID.ToString());
				UE_LOG(LogEquipmentVisualization, Error, 
					TEXT("  Item may not exist in Item Data Table"));
			}
		}
		else
		{
			UE_LOG(LogEquipmentVisualization, Error, 
				TEXT("  ItemManager subsystem not found in GameInstance!"));
		}
	}
	else
	{
		UE_LOG(LogEquipmentVisualization, Error, 
			TEXT("  GameInstance is NULL from ServiceLocator!"));
	}
	
	UE_LOG(LogEquipmentVisualization, Error, 
		TEXT("=== ResolveActorClass FAILED - Returning nullptr ==="));
	return nullptr;
}

FName UEquipmentVisualizationServiceImpl::ResolveAttachSocket(
    AActor* Character, 
    const FName ItemID, 
    int32 SlotIndex) const
{
    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Читаем сокет из DataTable через ItemManager
    // ============================================================================
    
    // Step 1: Get ItemManager to access DataTable
    if (!CachedServiceLocator)
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] CachedServiceLocator is NULL - using fallback"));
        return FName(TEXT("GripPoint"));
    }
    
    UGameInstance* GI = CachedServiceLocator->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] GameInstance not available - using fallback"));
        return FName(TEXT("GripPoint"));
    }
    
    UMedComItemManager* ItemManager = GI->GetSubsystem<UMedComItemManager>();
    if (!ItemManager)
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] ItemManager not available - using fallback"));
        return FName(TEXT("GripPoint"));
    }
    
    // Step 2: Load full item data from DataTable
    FMedComUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
    {
        UE_LOG(LogEquipmentVisualization, Error,
            TEXT("[ResolveAttachSocket] Failed to load ItemData for ItemID: %s"),
            *ItemID.ToString());
        return FName(TEXT("GripPoint"));
    }
    
    // Step 3: Determine if slot is active
    // SlotIndex 0 is typically the active weapon slot
    // For now, assume slot 0 = active, others = inactive
    // TODO: Get actual active slot from equipment component
    const bool bIsActiveSlot = (SlotIndex == 0);
    
    // Step 4: Get correct socket from DataTable based on state
    const FName ResolvedSocket = ItemData.GetSocketForState(bIsActiveSlot);
    
    if (ResolvedSocket.IsNone())
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachSocket] Socket is None in DataTable for ItemID: %s, State: %s"),
            *ItemID.ToString(),
            bIsActiveSlot ? TEXT("Active") : TEXT("Inactive"));
        return FName(TEXT("GripPoint"));
    }
    
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("[ResolveAttachSocket] ✓ Resolved socket from DataTable:"));
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("  ItemID: %s"), *ItemID.ToString());
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("  SlotIndex: %d (Active: %s)"), SlotIndex, bIsActiveSlot ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("  Socket: %s"), *ResolvedSocket.ToString());
    
    return ResolvedSocket;
}

FTransform UEquipmentVisualizationServiceImpl::ResolveAttachOffset(
    AActor* Character, 
    const FName ItemID, 
    int32 SlotIndex) const
{
    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Читаем оффсет из DataTable через ItemManager
    // ============================================================================
    
    // Step 1: Get ItemManager to access DataTable
    if (!CachedServiceLocator)
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachOffset] CachedServiceLocator is NULL - using Identity"));
        return FTransform::Identity;
    }
    
    UGameInstance* GI = CachedServiceLocator->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachOffset] GameInstance not available - using Identity"));
        return FTransform::Identity;
    }
    
    UMedComItemManager* ItemManager = GI->GetSubsystem<UMedComItemManager>();
    if (!ItemManager)
    {
        UE_LOG(LogEquipmentVisualization, Warning,
            TEXT("[ResolveAttachOffset] ItemManager not available - using Identity"));
        return FTransform::Identity;
    }
    
    // Step 2: Load full item data from DataTable
    FMedComUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
    {
        UE_LOG(LogEquipmentVisualization, Error,
            TEXT("[ResolveAttachOffset] Failed to load ItemData for ItemID: %s"),
            *ItemID.ToString());
        return FTransform::Identity;
    }
    
    // Step 3: Determine if slot is active
    const bool bIsActiveSlot = (SlotIndex == 0);
    
    // Step 4: Get correct offset from DataTable based on state
    const FTransform ResolvedOffset = ItemData.GetOffsetForState(bIsActiveSlot);
    
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("[ResolveAttachOffset] ✓ Resolved offset from DataTable:"));
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("  ItemID: %s"), *ItemID.ToString());
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("  SlotIndex: %d (Active: %s)"), SlotIndex, bIsActiveSlot ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogEquipmentVisualization, Log,
        TEXT("  Offset: Loc(%s) Rot(%s) Scale(%s)"),
        *ResolvedOffset.GetLocation().ToString(),
        *ResolvedOffset.Rotator().ToString(),
        *ResolvedOffset.GetScale3D().ToString());
    
    return ResolvedOffset;
}

// ===== Rate limiter ===============================================================

bool UEquipmentVisualizationServiceImpl::RateLimit() const
{
	if (CachedUpdateIntervalSec <= 0.0) return false;

	const double Now = FPlatformTime::Seconds();
	if ((Now - LastProcessTimeSec) < CachedUpdateIntervalSec) return true;

	const_cast<UEquipmentVisualizationServiceImpl*>(this)->LastProcessTimeSec = Now;
	return false;
}

// ===== Event metadata parsing =====================================================

bool UEquipmentVisualizationServiceImpl::TryParseInt(const FEquipmentEventData& E, const TCHAR* Key, int32& OutValue)
{
	const FString S = E.GetMetadata(Key, TEXT(""));
	if (S.IsEmpty()) { return false; }
	OutValue = LexToInt(S, OutValue);
	return true;
}

FName UEquipmentVisualizationServiceImpl::ParseName(const FEquipmentEventData& E, const TCHAR* Key, const FName DefaultValue)
{
	const FString S = E.GetMetadata(Key, TEXT(""));
	return S.IsEmpty() ? DefaultValue : FName(*S);
}
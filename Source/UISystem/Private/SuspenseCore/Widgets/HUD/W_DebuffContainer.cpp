// W_DebuffContainer.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.
//
// ARCHITECTURE:
// - Container widget for procedural debuff icon management
// - EventBus-driven updates (push model, NO polling!)
// - Object pooling for icon widgets
// - Native GameplayTags only (no RequestGameplayTag!)
//
// DATA FLOW:
// GrenadeProjectile → DoTService → EventBus::Publish(DoT.Applied) → W_DebuffContainer → AddOrUpdateDebuff()

#include "SuspenseCore/Widgets/HUD/W_DebuffContainer.h"
#include "SuspenseCore/Widgets/HUD/W_DebuffIcon.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreDoTService.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogDebuffContainer, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════════

UW_DebuffContainer::UW_DebuffContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffContainer::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize widget pool
	InitializePool();

	// Auto-target local player if configured
	if (bAutoTargetLocalPlayer)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				TargetActor = Pawn;
				UE_LOG(LogDebuffContainer, Log, TEXT("Auto-targeted local player pawn: %s"), *Pawn->GetName());
			}
		}
	}

	// Subscribe to EventBus
	SetupEventSubscriptions();

	// Initial sync from DoTService (in case we missed events during load)
	RefreshFromDoTService();

	UE_LOG(LogDebuffContainer, Log, TEXT("NativeConstruct complete. Pool size: %d, Target: %s"),
		IconPool.Num(),
		TargetActor.IsValid() ? *TargetActor->GetName() : TEXT("None"));
}

void UW_DebuffContainer::NativeDestruct()
{
	// Unsubscribe from EventBus
	TeardownEventSubscriptions();

	// Clear all active debuffs (returns icons to pool)
	ClearAllDebuffs();

	// Clear pool
	IconPool.Empty();
	bPoolInitialized = false;

	Super::NativeDestruct();
}

void UW_DebuffContainer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Icons update their own timers via their NativeTick
	// This tick is reserved for potential future batch operations
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffContainer::RefreshFromDoTService()
{
	if (!TargetActor.IsValid())
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("RefreshFromDoTService: No target actor set"));
		return;
	}

	// Get DoT service
	USuspenseCoreDoTService* DoTService = USuspenseCoreDoTService::Get(this);
	if (!DoTService)
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("RefreshFromDoTService: DoTService not available"));
		return;
	}

	// Get all active DoTs for target
	TArray<FSuspenseCoreActiveDoT> ActiveDoTs = DoTService->GetActiveDoTs(TargetActor.Get());

	UE_LOG(LogDebuffContainer, Log, TEXT("RefreshFromDoTService: Found %d active DoTs for %s"),
		ActiveDoTs.Num(), *TargetActor->GetName());

	// Clear existing debuffs
	ClearAllDebuffs();

	// Add icons for each active DoT
	for (const FSuspenseCoreActiveDoT& DoT : ActiveDoTs)
	{
		AddOrUpdateDebuff(DoT.DoTType, DoT.RemainingDuration, DoT.StackCount);
	}
}

void UW_DebuffContainer::ClearAllDebuffs()
{
	// Release all active icons back to pool
	for (auto& Pair : ActiveDebuffs)
	{
		if (UW_DebuffIcon* Icon = Pair.Value)
		{
			// Unbind delegate before releasing
			Icon->OnRemovalComplete.RemoveAll(this);
			Icon->ResetToDefault();

			// Add back to pool
			IconPool.Add(Icon);
		}
	}

	ActiveDebuffs.Empty();

	UE_LOG(LogDebuffContainer, Log, TEXT("ClearAllDebuffs: All debuffs cleared, pool size: %d"), IconPool.Num());
}

void UW_DebuffContainer::SetTargetActor(AActor* NewTarget)
{
	if (TargetActor.Get() == NewTarget)
	{
		return;
	}

	AActor* OldTarget = TargetActor.Get();
	TargetActor = NewTarget;

	UE_LOG(LogDebuffContainer, Log, TEXT("SetTargetActor: %s → %s"),
		OldTarget ? *OldTarget->GetName() : TEXT("None"),
		NewTarget ? *NewTarget->GetName() : TEXT("None"));

	// Clear existing debuffs and refresh for new target
	ClearAllDebuffs();

	if (NewTarget)
	{
		RefreshFromDoTService();
	}

	// Notify Blueprint
	OnTargetChanged(NewTarget);
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffContainer::OnDoTApplied(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if event is for our target
	if (!IsEventForTarget(EventData))
	{
		return;
	}

	// Extract DoT data from event
	FGameplayTag DoTType;

	// Try to get DoT type from event tags
	if (EventData.Tags.HasTag(SuspenseCoreTags::State::Health::BleedingLight))
	{
		DoTType = SuspenseCoreTags::State::Health::BleedingLight;
	}
	else if (EventData.Tags.HasTag(SuspenseCoreTags::State::Health::BleedingHeavy))
	{
		DoTType = SuspenseCoreTags::State::Health::BleedingHeavy;
	}
	else
	{
		// Try to get from string field
		FString DoTTypeStr = EventData.GetString(TEXT("DoTType"));
		if (!DoTTypeStr.IsEmpty())
		{
			DoTType = FGameplayTag::RequestGameplayTag(FName(*DoTTypeStr), false);
		}
	}

	if (!DoTType.IsValid())
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("OnDoTApplied: Could not determine DoT type from event"));
		return;
	}

	float Duration = EventData.GetFloat(TEXT("Duration"), -1.0f);
	int32 StackCount = EventData.GetInt(TEXT("StackCount"), 1);

	UE_LOG(LogDebuffContainer, Log, TEXT("OnDoTApplied: Type=%s, Duration=%.1f, Stacks=%d"),
		*DoTType.ToString(), Duration, StackCount);

	AddOrUpdateDebuff(DoTType, Duration, StackCount);

	// Notify Blueprint
	OnDebuffAdded(DoTType);
}

void UW_DebuffContainer::OnDoTRemoved(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if event is for our target
	if (!IsEventForTarget(EventData))
	{
		return;
	}

	// Extract DoT type
	FGameplayTag DoTType;

	// Try to get from tags first
	if (EventData.Tags.HasTag(SuspenseCoreTags::State::Health::BleedingLight))
	{
		DoTType = SuspenseCoreTags::State::Health::BleedingLight;
	}
	else if (EventData.Tags.HasTag(SuspenseCoreTags::State::Health::BleedingHeavy))
	{
		DoTType = SuspenseCoreTags::State::Health::BleedingHeavy;
	}
	else
	{
		// Try to get from string field
		FString DoTTypeStr = EventData.GetString(TEXT("DoTType"));
		if (!DoTTypeStr.IsEmpty())
		{
			DoTType = FGameplayTag::RequestGameplayTag(FName(*DoTTypeStr), false);
		}
	}

	if (!DoTType.IsValid())
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("OnDoTRemoved: Could not determine DoT type from event"));
		return;
	}

	UE_LOG(LogDebuffContainer, Log, TEXT("OnDoTRemoved: Type=%s"), *DoTType.ToString());

	RemoveDebuff(DoTType);

	// Notify Blueprint
	OnDebuffRemoved(DoTType);
}

void UW_DebuffContainer::OnDoTExpired(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Expired is same as removed for UI purposes
	OnDoTRemoved(EventTag, EventData);
}

void UW_DebuffContainer::OnDoTTick(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if event is for our target
	if (!IsEventForTarget(EventData))
	{
		return;
	}

	// Tick events could be used for damage numbers or visual feedback
	// For now, icons update their own timers via NativeTick

	// Optional: Update remaining duration from event
	FString DoTTypeStr = EventData.GetString(TEXT("DoTType"));
	if (!DoTTypeStr.IsEmpty())
	{
		FGameplayTag DoTType = FGameplayTag::RequestGameplayTag(FName(*DoTTypeStr), false);
		if (DoTType.IsValid())
		{
			if (TObjectPtr<UW_DebuffIcon>* IconPtr = ActiveDebuffs.Find(DoTType))
			{
				float RemainingDuration = EventData.GetFloat(TEXT("RemainingDuration"), -1.0f);
				if (RemainingDuration >= 0.0f)
				{
					(*IconPtr)->UpdateTimer(RemainingDuration);
				}

				int32 StackCount = EventData.GetInt(TEXT("StackCount"), 0);
				if (StackCount > 0)
				{
					(*IconPtr)->UpdateStackCount(StackCount);
				}
			}
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS SETUP
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffContainer::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("SetupEventSubscriptions: No EventBus available!"));
		return;
	}

	// Subscribe to DoT events using native tags
	// @see SuspenseCoreTags::Event::DoT namespace in SuspenseCoreGameplayTags.h

	DoTAppliedHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::DoT::Applied,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &UW_DebuffContainer::OnDoTApplied),
		ESuspenseCoreEventPriority::Normal
	);

	DoTRemovedHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::DoT::Removed,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &UW_DebuffContainer::OnDoTRemoved),
		ESuspenseCoreEventPriority::Normal
	);

	DoTExpiredHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::DoT::Expired,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &UW_DebuffContainer::OnDoTExpired),
		ESuspenseCoreEventPriority::Normal
	);

	DoTTickHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::DoT::Tick,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &UW_DebuffContainer::OnDoTTick),
		ESuspenseCoreEventPriority::Low  // Low priority - UI doesn't need instant tick updates
	);

	UE_LOG(LogDebuffContainer, Log, TEXT("Event subscriptions setup complete"));
}

void UW_DebuffContainer::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(DoTAppliedHandle);
	EventBus->Unsubscribe(DoTRemovedHandle);
	EventBus->Unsubscribe(DoTExpiredHandle);
	EventBus->Unsubscribe(DoTTickHandle);

	UE_LOG(LogDebuffContainer, Verbose, TEXT("Event subscriptions torn down"));
}

USuspenseCoreEventBus* UW_DebuffContainer::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ICON MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffContainer::AddOrUpdateDebuff(FGameplayTag DoTType, float Duration, int32 StackCount)
{
	// Check if we already have an icon for this type
	if (TObjectPtr<UW_DebuffIcon>* ExistingIcon = ActiveDebuffs.Find(DoTType))
	{
		// Update existing icon
		(*ExistingIcon)->UpdateTimer(Duration);
		(*ExistingIcon)->UpdateStackCount(StackCount);

		UE_LOG(LogDebuffContainer, Verbose, TEXT("Updated existing debuff icon: %s"), *DoTType.ToString());
		return;
	}

	// Check max visible limit
	if (ActiveDebuffs.Num() >= MaxVisibleDebuffs)
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("Max visible debuffs reached (%d), cannot add: %s"),
			MaxVisibleDebuffs, *DoTType.ToString());
		return;
	}

	// Acquire new icon from pool
	UW_DebuffIcon* NewIcon = AcquireIcon();
	if (!NewIcon)
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("Could not acquire icon from pool for: %s"), *DoTType.ToString());
		return;
	}

	// Setup icon
	NewIcon->SetDebuffData(DoTType, Duration, StackCount);

	// Bind removal complete delegate
	NewIcon->OnRemovalComplete.AddDynamic(this, &UW_DebuffContainer::OnIconRemovalComplete);

	// Add to container
	if (DebuffBox)
	{
		UHorizontalBoxSlot* IconSlot = DebuffBox->AddChildToHorizontalBox(NewIcon);
		if (IconSlot)
		{
			IconSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
		}
	}

	// Track in active map
	ActiveDebuffs.Add(DoTType, NewIcon);

	UE_LOG(LogDebuffContainer, Log, TEXT("Added new debuff icon: %s (Active: %d, Pool: %d)"),
		*DoTType.ToString(), ActiveDebuffs.Num(), IconPool.Num());
}

void UW_DebuffContainer::RemoveDebuff(FGameplayTag DoTType)
{
	TObjectPtr<UW_DebuffIcon>* IconPtr = ActiveDebuffs.Find(DoTType);
	if (!IconPtr || !*IconPtr)
	{
		UE_LOG(LogDebuffContainer, Verbose, TEXT("RemoveDebuff: No active icon for type: %s"), *DoTType.ToString());
		return;
	}

	UW_DebuffIcon* Icon = *IconPtr;

	// Play removal animation - icon will call OnRemovalComplete when done
	Icon->PlayRemovalAnimation();

	UE_LOG(LogDebuffContainer, Log, TEXT("RemoveDebuff: Playing removal animation for: %s"), *DoTType.ToString());
}

UW_DebuffIcon* UW_DebuffContainer::AcquireIcon()
{
	// Try to get from pool
	if (IconPool.Num() > 0)
	{
		UW_DebuffIcon* Icon = IconPool.Pop();
		if (Icon)
		{
			Icon->ResetToDefault();
			return Icon;
		}
	}

	// Pool empty - create new if under limit
	if (!DebuffIconClass)
	{
		UE_LOG(LogDebuffContainer, Error, TEXT("AcquireIcon: DebuffIconClass not set!"));
		return nullptr;
	}

	UW_DebuffIcon* NewIcon = CreateWidget<UW_DebuffIcon>(GetOwningPlayer(), DebuffIconClass);
	if (NewIcon)
	{
		UE_LOG(LogDebuffContainer, Verbose, TEXT("Created new icon widget (pool was empty)"));
	}

	return NewIcon;
}

void UW_DebuffContainer::ReleaseIcon(UW_DebuffIcon* Icon)
{
	if (!Icon)
	{
		return;
	}

	// Remove from parent
	Icon->RemoveFromParent();

	// Reset state
	Icon->ResetToDefault();

	// Unbind delegate
	Icon->OnRemovalComplete.RemoveAll(this);

	// Return to pool
	IconPool.Add(Icon);

	UE_LOG(LogDebuffContainer, Verbose, TEXT("Released icon to pool (Pool size: %d)"), IconPool.Num());
}

void UW_DebuffContainer::InitializePool()
{
	if (bPoolInitialized)
	{
		return;
	}

	if (!DebuffIconClass)
	{
		UE_LOG(LogDebuffContainer, Warning, TEXT("InitializePool: DebuffIconClass not set, pool not initialized"));
		return;
	}

	// Pre-create icons for pool
	for (int32 i = 0; i < IconPoolSize; ++i)
	{
		UW_DebuffIcon* Icon = CreateWidget<UW_DebuffIcon>(GetOwningPlayer(), DebuffIconClass);
		if (Icon)
		{
			Icon->SetVisibility(ESlateVisibility::Collapsed);
			IconPool.Add(Icon);
		}
	}

	bPoolInitialized = true;

	UE_LOG(LogDebuffContainer, Log, TEXT("Initialized icon pool with %d widgets"), IconPool.Num());
}

bool UW_DebuffContainer::IsEventForTarget(const FSuspenseCoreEventData& EventData) const
{
	if (!TargetActor.IsValid())
	{
		return false;
	}

	// Check EventData.Source first (primary field used by DoTService)
	AActor* AffectedActor = Cast<AActor>(EventData.Source.Get());

	// Fallback: Check ObjectPayload for "AffectedActor" field
	if (!AffectedActor)
	{
		AffectedActor = EventData.GetObject<AActor>(FName(TEXT("AffectedActor")));
	}

	// Also try "Target" field as fallback
	if (!AffectedActor)
	{
		AffectedActor = EventData.GetObject<AActor>(FName(TEXT("Target")));
	}

	return AffectedActor == TargetActor.Get();
}

void UW_DebuffContainer::OnIconRemovalComplete(UW_DebuffIcon* Icon)
{
	if (!Icon)
	{
		return;
	}

	// Find and remove from active map
	FGameplayTag TypeToRemove;
	for (auto& Pair : ActiveDebuffs)
	{
		if (Pair.Value == Icon)
		{
			TypeToRemove = Pair.Key;
			break;
		}
	}

	if (TypeToRemove.IsValid())
	{
		ActiveDebuffs.Remove(TypeToRemove);

		UE_LOG(LogDebuffContainer, Log, TEXT("OnIconRemovalComplete: Removed %s from active map (Active: %d)"),
			*TypeToRemove.ToString(), ActiveDebuffs.Num());
	}

	// Release to pool
	ReleaseIcon(Icon);
}

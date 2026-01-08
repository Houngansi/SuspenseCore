// SuspenseEquipmentVisualizationService.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "HAL/CriticalSection.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentThreadGuard.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentCacheManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Components/Coordination/SuspenseCoreEquipmentEventDispatcher.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentVisualizationTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreEquipmentVisualizationService.generated.h"

/**
 * Lightweight state per character (visible instances on slots)
 */
USTRUCT()
struct FSuspenseCoreVisCharState
{
	GENERATED_BODY()

	UPROPERTY() TMap<int32, TWeakObjectPtr<AActor>> SlotActors;
	UPROPERTY() int32 ActiveSlot = INDEX_NONE;
	UPROPERTY() float LastTickSec = 0.f;
};

/**
 * Orchestrator (facade) for visual layer.
 * SRP: only routing events to presentation systems (Factory/Attachment/Visual),
 * lightweight cache state and rate limiting.
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentVisualizationService final : public UObject, public ISuspenseCoreEquipmentService
{
	GENERATED_BODY()

public:
	// === ISuspenseCoreEquipmentService ===
	virtual bool InitializeService(const FSuspenseCoreServiceInitParams& InitParams) override;
	virtual bool ShutdownService(bool bForce) override;
	virtual ESuspenseCoreServiceLifecycleState GetServiceState() const override { return LifecycleState; }
	virtual bool IsServiceReady() const override { return LifecycleState == ESuspenseCoreServiceLifecycleState::Ready; }
	virtual FGameplayTag GetServiceTag() const override
	{
		// НИКОГДА не зависящим от состояния инстанса образом (работает и на CDO)
		return FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Visualization"));
	}
	virtual FGameplayTagContainer GetRequiredDependencies() const override;
	virtual bool ValidateService(TArray<FText>& OutErrors) const override;
	virtual void ResetService() override;
	virtual FString GetServiceStats() const override;

	// Manual trigger (optional)
	void RequestRefresh(AActor* Character, bool bForce);

private:
	// Config
	UPROPERTY() float MaxUpdateRateHz = 30.f;              // Rate limiter
	UPROPERTY() int32 VisualQualityLevel = 2;              // 0..3
	UPROPERTY() bool bEnableBatching = true;               // Batch notifications

	// Event bus (SuspenseCore architecture)
	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreEventBus> EventBus = nullptr;

	TArray<FSuspenseCoreSubscriptionHandle> Subscriptions;

	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreEquipmentServiceLocator> CachedServiceLocator;

	// Lightweight state
	UPROPERTY() TMap<TWeakObjectPtr<AActor>, FSuspenseCoreVisCharState> Characters;

	// Thread safety - using FRWLock for read-heavy visual queries
	// Lock ordering: VisualizationService (Level 31) - see SuspenseThreadSafetyPolicy.h
	mutable FRWLock VisualLock;

	// Rate limiter
	double CachedUpdateIntervalSec = 0.0;
	double LastProcessTimeSec = 0.0;

	// Service tags
	FGameplayTag VisualizationServiceTag;
	FGameplayTag Tag_OnEquipped;
	FGameplayTag Tag_OnUnequipped;
	FGameplayTag Tag_OnSlotSwitched;
	FGameplayTag Tag_OnWeaponSlotSwitched;  // SuspenseCore.Event.Equipment.WeaponSlotSwitched
	FGameplayTag Tag_VisRefreshAll;

	// Service dependency tags (via Locator)
	FGameplayTag Tag_ActorFactory;       // "Service.ActorFactory"
	FGameplayTag Tag_AttachmentSystem;   // "Service.AttachmentSystem"
	FGameplayTag Tag_VisualController;   // "Service.VisualController"
	FGameplayTag Tag_EquipmentData;      // "Service.Equipment.Data"

	// Lifecycle state
	UPROPERTY() ESuspenseCoreServiceLifecycleState LifecycleState = ESuspenseCoreServiceLifecycleState::Uninitialized;

	// === Internal logic ===
	void SetupEventHandlers();
	void TeardownEventHandlers();

	// Event handlers (SuspenseCore types)
	void OnEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnSlotSwitched(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnWeaponSlotSwitched(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnRefreshAll(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// High-level operations
	// NOTE: WeaponAmmoState is optional and used to preserve ammo state during inventory transfers
	// @see TarkovStyle_Ammo_System_Design.md - WeaponAmmoState persistence
	void UpdateVisualForSlot(AActor* Character, int32 SlotIndex, const FName ItemID, bool bInstant, const FSuspenseCoreWeaponAmmoState* InWeaponAmmoState = nullptr);
	void HideVisualForSlot(AActor* Character, int32 SlotIndex, bool bInstant);
	void RefreshAllVisuals(AActor* Character, bool bForce);

	// Helpers
	bool RateLimit() const;

	// Integration with presentation subsystems via ServiceLocator
	// NOTE: WeaponAmmoState is optional and used to preserve ammo state during inventory transfers
	AActor* AcquireVisualActor(AActor* Character, const FName ItemID, int32 SlotIndex, const FSuspenseCoreWeaponAmmoState* InWeaponAmmoState = nullptr);
	void   ReleaseVisualActor(AActor* Character, int32 SlotIndex, bool bInstant);

	// Internal helper - MUST be called with VisualLock already held (fixes deadlock)
	void   ReleaseVisualActorInternal(AActor* Character, int32 SlotIndex, bool bInstant);

	bool AttachActorToCharacter(AActor* Character, AActor* Visual, const FName Socket, const FTransform& Offset);
	void ApplyQualitySettings(AActor* Visual) const;

	// Reflection to data/presentation services (minimal dependencies)
	TSubclassOf<AActor> ResolveActorClass(const FName ItemID) const;
	// bCallerHoldsLock: set true when caller already holds VisualLock (pass KnownActiveSlot to avoid re-reading state)
	FName               ResolveAttachSocket(AActor* Character, const FName ItemID, int32 SlotIndex, bool bCallerHoldsLock = false, int32 KnownActiveSlot = INDEX_NONE) const;
	FTransform          ResolveAttachOffset(AActor* Character, const FName ItemID, int32 SlotIndex, bool bCallerHoldsLock = false, int32 KnownActiveSlot = INDEX_NONE) const;

	// Event metadata parsing (SuspenseCore types)
	static bool  TryParseInt(const FSuspenseCoreEventData& EventData, const TCHAR* Key, int32& OutValue);
	static FName ParseName(const FSuspenseCoreEventData& EventData, const TCHAR* Key, const FName DefaultValue = NAME_None);
};

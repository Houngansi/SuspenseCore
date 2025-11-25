// SuspenseEquipmentVisualizationService.h
// Copyright MedCom

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"

// Fundamental: service interface + base utilities
#include "Interfaces/Equipment/IEquipmentService.h"
#include "Core/Utils/FEquipmentEventBus.h"
#include "Core/Utils/FSuspenseEquipmentThreadGuard.h"
#include "Core/Utils/FEquipmentCacheManager.h"
#include "Core/Services/EquipmentServiceLocator.h"

// Presentation layer interfaces
#include "Interfaces/Equipment/ISuspenseActorFactory.h"

// Unified macros (logs/locks/metrics) - log categories declared centrally
#include "Services/SuspenseEquipmentServiceMacros.h"

// Item / visualization types (lightweight dependencies)
#include "Types/Equipment/EquipmentTypes.h"
#include "Types/Equipment/EquipmentVisualizationTypes.h"
#include "Types/Inventory/InventoryTypes.h"

#include "SuspenseEquipmentVisualizationService.generated.h"

/**
 * Lightweight state per character (visible instances on slots)
 */
USTRUCT()
struct FVisCharState
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
class EQUIPMENTSYSTEM_API USuspenseEquipmentVisualizationService final : public UObject, public IEquipmentService
{
	GENERATED_BODY()

public:
	// === IEquipmentService ===
	virtual bool InitializeService(const FServiceInitParams& InitParams) override;
	virtual bool ShutdownService(bool bForce) override;
	virtual EServiceLifecycleState GetServiceState() const override { return LifecycleState; }
	virtual bool IsServiceReady() const override { return LifecycleState == EServiceLifecycleState::Ready; }
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

	// Event bus
	TWeakPtr<FEquipmentEventBus> EventBus;
	TArray<FEventSubscriptionHandle> Subscriptions;

	UPROPERTY(Transient)
	TObjectPtr<UEquipmentServiceLocator> CachedServiceLocator;
	
	// Lightweight state
	UPROPERTY() TMap<TWeakObjectPtr<AActor>, FVisCharState> Characters;
	mutable FCriticalSection VisualLock;

	// Rate limiter
	double CachedUpdateIntervalSec = 0.0;
	double LastProcessTimeSec = 0.0;

	// Service tags
	FGameplayTag VisualizationServiceTag;
	FGameplayTag Tag_OnEquipped;
	FGameplayTag Tag_OnUnequipped;
	FGameplayTag Tag_OnSlotSwitched;
	FGameplayTag Tag_VisRefreshAll;

	// Service dependency tags (via Locator)
	FGameplayTag Tag_ActorFactory;       // "Service.ActorFactory"
	FGameplayTag Tag_AttachmentSystem;   // "Service.AttachmentSystem"
	FGameplayTag Tag_VisualController;   // "Service.VisualController"
	FGameplayTag Tag_EquipmentData;      // "Service.Equipment.Data"

	// Lifecycle state
	UPROPERTY() EServiceLifecycleState LifecycleState = EServiceLifecycleState::Uninitialized;

	// === Internal logic ===
	void SetupEventHandlers();
	void TeardownEventHandlers();

	// Event handlers
	void OnEquipped(const FEquipmentEventData& E);
	void OnUnequipped(const FEquipmentEventData& E);
	void OnSlotSwitched(const FEquipmentEventData& E);
	void OnRefreshAll(const FEquipmentEventData& E);

	// High-level operations
	void UpdateVisualForSlot(AActor* Character, int32 SlotIndex, const FName ItemID, bool bInstant);
	void HideVisualForSlot(AActor* Character, int32 SlotIndex, bool bInstant);
	void RefreshAllVisuals(AActor* Character, bool bForce);

	// Helpers
	bool RateLimit() const;

	// Integration with presentation subsystems via ServiceLocator
	AActor* AcquireVisualActor(AActor* Character, const FName ItemID, int32 SlotIndex);
	void   ReleaseVisualActor(AActor* Character, int32 SlotIndex, bool bInstant);

	bool AttachActorToCharacter(AActor* Character, AActor* Visual, const FName Socket, const FTransform& Offset);
	void ApplyQualitySettings(AActor* Visual) const;

	// Reflection to data/presentation services (minimal dependencies)
	TSubclassOf<AActor> ResolveActorClass(const FName ItemID) const;
	FName               ResolveAttachSocket(AActor* Character, const FName ItemID, int32 SlotIndex) const;
	FTransform          ResolveAttachOffset(AActor* Character, const FName ItemID, int32 SlotIndex) const;

	// Event metadata parsing
	static bool  TryParseInt(const FEquipmentEventData& E, const TCHAR* Key, int32& OutValue);
	static FName ParseName(const FEquipmentEventData& E, const TCHAR* Key, const FName DefaultValue = NAME_None);
};
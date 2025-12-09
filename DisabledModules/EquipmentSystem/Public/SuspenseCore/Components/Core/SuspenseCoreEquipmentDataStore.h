// SuspenseCoreEquipmentDataStore.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "SuspenseCore/Types/Transaction/SuspenseCoreTransactionTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentDataStore.generated.h"

// Define logging category for DataStore
DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentDataStore, Log, All);

/**
 * Structure for deferred event dispatch
 * Used to collect events under lock and dispatch after releasing it
 */
struct FSuspenseCorePendingEventData
{
    enum EEventType
    {
        SlotChanged,
        ConfigChanged,
        StoreReset,
        StateChanged,
        EquipmentDelta  // New type for delta events
    };

    EEventType Type;
    int32 SlotIndex;
    FSuspenseCoreInventoryItemInstance ItemData;
    FGameplayTag StateTag;
    FEquipmentDelta DeltaData;  // New field for delta data
};

/**
 * Internal data storage structure
 * Encapsulates all mutable state for thread-safe access
 */
USTRUCT()
struct FSuspenseCoreEquipmentDataStorage
{
    GENERATED_BODY()

    /** Slot configurations */
    UPROPERTY()
    TArray<FEquipmentSlotConfig> SlotConfigurations;

    /** Items in slots */
    UPROPERTY()
    TArray<FSuspenseCoreInventoryItemInstance> SlotItems;

    /** Active weapon slot index */
    UPROPERTY()
    int32 ActiveWeaponSlot = INDEX_NONE;

    /** Current equipment state */
    UPROPERTY()
    FGameplayTag CurrentState;

    /** Data version for change tracking */
    UPROPERTY()
    uint32 DataVersion = 0;

    /** Last modification time */
    UPROPERTY()
    FDateTime LastModified;

    /** Current transaction context (if any) */
    UPROPERTY()
    FGuid ActiveTransactionId;

    FSuspenseCoreEquipmentDataStorage()
    {
        CurrentState = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Idle"));
        LastModified = FDateTime::Now();
    }
};

/**
 * Equipment Data Store Component
 *
 * Philosophy: Pure data storage with no business logic.
 * Acts as a "dumb" container that only stores and retrieves data.
 * All validation and decision-making is handled by external validators.
 *
 * Key Design Principles:
 * - Thread-safe data access through critical sections
 * - Immutable public interface (all getters return copies)
 * - Event-driven change notifications (NEVER under locks)
 * - No business logic, pure data storage
 * - No validation rules or decision making
 * - DIFF-based change tracking for fine-grained updates
 *
 * Critical Threading Rule:
 * Events are NEVER broadcast under DataCriticalSection to prevent deadlocks.
 * We collect event data under lock, then broadcast after releasing it.
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentDataStore : public UActorComponent, public ISuspenseCoreEquipmentDataProvider
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentDataStore();
    virtual ~USuspenseCoreEquipmentDataStore();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    //~ End UActorComponent Interface

    //========================================
    // ISuspenseCoreEquipmentDataProvider Implementation

	// High-level queries required by ISuspenseCoreEquipmentDataProvider
	virtual TArray<int32> FindCompatibleSlots(const FGameplayTag& ItemSlotTag) const override;
	virtual TArray<int32> GetSlotsByType(EEquipmentSlotType SlotType) const override;
	virtual int32 GetFirstEmptySlotOfType(EEquipmentSlotType SlotType) const override;
	virtual float GetTotalEquippedWeight() const override;
	virtual bool MeetsItemRequirements(const FSuspenseCoreInventoryItemInstance& Item, int32 TargetSlotIndex) const override;
	virtual FString GetDebugInfo() const override;
    //========================================

    // Pure Data Access - No Logic
    virtual FSuspenseCoreInventoryItemInstance GetSlotItem(int32 SlotIndex) const override;
    virtual FEquipmentSlotConfig GetSlotConfiguration(int32 SlotIndex) const override;
    virtual TArray<FEquipmentSlotConfig> GetAllSlotConfigurations() const override;
    virtual TMap<int32, FSuspenseCoreInventoryItemInstance> GetAllEquippedItems() const override;
    virtual int32 GetSlotCount() const override;
    virtual bool IsValidSlotIndex(int32 SlotIndex) const override;
    virtual bool IsSlotOccupied(int32 SlotIndex) const override;

    // Data Modification - No Validation
    virtual bool SetSlotItem(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& ItemInstance, bool bNotifyObservers = true) override;
    virtual FSuspenseCoreInventoryItemInstance ClearSlot(int32 SlotIndex, bool bNotifyObservers = true) override;
    virtual bool InitializeSlots(const TArray<FEquipmentSlotConfig>& Configurations) override;

    // State Management
    virtual int32 GetActiveWeaponSlot() const override;
    virtual bool SetActiveWeaponSlot(int32 SlotIndex) override;
    virtual FGameplayTag GetCurrentEquipmentState() const override;
    virtual bool SetEquipmentState(const FGameplayTag& NewState) override;

    // Snapshot Management
    virtual FEquipmentStateSnapshot CreateSnapshot() const override;
    virtual bool RestoreSnapshot(const FEquipmentStateSnapshot& Snapshot) override;
    virtual FEquipmentSlotSnapshot CreateSlotSnapshot(int32 SlotIndex) const override;

    // Events
    virtual FOnSlotDataChanged& OnSlotDataChanged() override { return OnSlotDataChangedDelegate; }
    virtual FOnSlotConfigurationChanged& OnSlotConfigurationChanged() override { return OnSlotConfigurationChangedDelegate; }
    virtual FOnDataStoreReset& OnDataStoreReset() override { return OnDataStoreResetDelegate; }

    /** Get equipment delta event */
    FOnEquipmentDelta& OnEquipmentDelta() { return OnEquipmentDeltaDelegate; }

    //========================================
    // Transaction Support
    //========================================

    /**
     * Set active transaction context
     * @param TransactionId Current transaction ID
     */
    void SetActiveTransaction(const FGuid& TransactionId);

    /**
     * Clear active transaction context
     */
    void ClearActiveTransaction();

    /**
     * Get active transaction ID
     * @return Current transaction or invalid GUID
     */
    FGuid GetActiveTransaction() const;

	/**
	* Clears ActiveTransactionId only if it matches the provided TxnId.
	* Safe for nested transactions.
	*/
	void ClearActiveTransactionIfMatches(const FGuid& TxnId);
	//========================================
	// Transaction Delta Handler
	//========================================

	/**
	 * Handle transaction deltas from TransactionProcessor
	 * Called when transaction is committed/rolled back
	 * Updates internal state and broadcasts events
	 *
	 * @param Deltas - Array of transaction deltas with before/after states
	 */
	void OnTransactionDelta(const TArray<FEquipmentDelta>& Deltas);
    //========================================
    // Additional Public Methods
    //========================================

    //UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
    uint32 GetDataVersion() const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
    FDateTime GetLastModificationTime() const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
    void ResetToDefault();

    UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
    int32 GetMemoryUsage() const;
	/**
		* Get fresh slot configuration directly from LoadoutManager
		* This ensures we always use the most up-to-date configuration
		* @param SlotIndex Index of the slot
		* @return Fresh configuration from LoadoutManager or cached if unavailable
		*/
	UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
	FEquipmentSlotConfig GetFreshSlotConfiguration(int32 SlotIndex) const;

	/**
	 * Refresh all cached slot configurations from LoadoutManager
	 * Call this when you know configurations have changed
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
	void RefreshSlotConfigurations();

	/**
	 * Set the current loadout ID for this data store
	 * Used to fetch correct configuration from LoadoutManager
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|DataStore")
	void SetCurrentLoadoutID(const FName& LoadoutID);
protected:
    /**
     * Internal method to modify data with event collection
     * Collects events under lock, broadcasts after releasing
     */
    bool ModifyDataWithEvents(
        TFunction<bool(FSuspenseCoreEquipmentDataStorage&, TArray<FSuspenseCorePendingEventData>&)> ModificationFunc,
        bool bNotifyObservers = true
    );

    /**
     * Create delta for a change
     */
    FEquipmentDelta CreateDelta(
        const FGameplayTag& ChangeType,
        int32 SlotIndex,
        const FSuspenseCoreInventoryItemInstance& Before,
        const FSuspenseCoreInventoryItemInstance& After,
        const FGameplayTag& Reason
    );

    bool ValidateSlotIndexInternal(int32 SlotIndex, const FString& FunctionName) const;
    FSuspenseCoreEquipmentDataStorage CreateDataSnapshot() const;
    bool ApplyDataSnapshot(const FSuspenseCoreEquipmentDataStorage& Snapshot, bool bNotifyObservers = true);
    void IncrementVersion();
    void LogDataModification(const FString& ModificationType, const FString& Details) const;

    /**
     * Broadcast collected events after releasing lock
     * This method is called OUTSIDE of any critical section
     */
    void BroadcastPendingEvents(const TArray<FSuspenseCorePendingEventData>& PendingEvents);
	/** Current loadout ID being used by this data store */
	UPROPERTY()
	FName CurrentLoadoutID;
private:
    //========================================
    // Core Data Storage
    //========================================

    /** Main data storage */
    UPROPERTY()
    FSuspenseCoreEquipmentDataStorage DataStorage;

    /** Critical section for thread-safe access */
    mutable FCriticalSection DataCriticalSection;

    //========================================
    // Snapshot Management
    //========================================

    /** History of snapshots for undo/redo */
    UPROPERTY()
    TArray<FEquipmentStateSnapshot> SnapshotHistory;

    /** Maximum snapshots to keep */
    static constexpr int32 MaxSnapshotHistory = 10;

    //========================================
    // Event Delegates
    //========================================

    /** Delegate fired when slot data changes */
    FOnSlotDataChanged OnSlotDataChangedDelegate;

    /** Delegate fired when slot configuration changes */
    FOnSlotConfigurationChanged OnSlotConfigurationChangedDelegate;

    /** Delegate fired when data store is reset */
    FOnDataStoreReset OnDataStoreResetDelegate;

    /** Delegate fired for equipment deltas */
    FOnEquipmentDelta OnEquipmentDeltaDelegate;

    //========================================
    // Statistics
    //========================================

    /** Total modifications counter */
    UPROPERTY()
    int32 TotalModifications = 0;

    /** Total deltas generated */
    UPROPERTY()
    int32 TotalDeltasGenerated = 0;

    /** Modification rate tracking */
    UPROPERTY()
    float ModificationRate = 0.0f;

    /** Last rate calculation time */
    float LastRateCalculationTime = 0.0f;

    void UpdateStatistics();

	/** Конвертирует тег состояния в enum (для обратной совместимости снапшотов) */
	EEquipmentState ConvertTagToEquipmentState(const FGameplayTag& StateTag) const;

	/** Конвертирует enum состояния в тег (fallback, если в снапшоте нет тега) */
	FGameplayTag ConvertEquipmentStateToTag(EEquipmentState State) const;
};

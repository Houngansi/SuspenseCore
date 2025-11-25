// Copyright Suspense Team Team

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"

#include "Interfaces/UI/ISuspenseEquipmentUIBridgeWidget.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseEquipmentOperations.h"

#include "Types/UI/EquipmentUITypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/UI/ContainerUITypes.h"

#include "SuspenseEquipmentUIBridge.generated.h"

class USuspenseUIManager;
class UEventDelegateManager;
class USuspenseItemManager;

// NEW: Multicast delegate for direct widget notifications
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentUIDataChanged, const TArray<FEquipmentSlotUIData>&);

/**
 * USuspenseEquipmentUIBridge
 *
 * SIMPLIFIED ARCHITECTURE:
 *  - Directly subscribes to DataStore via OnSlotDataChanged delegate
 *  - Maintains internal cache of slot configs and equipped items
 *  - Builds FEquipmentSlotUIData on demand for widgets
 *  - Provides FOnEquipmentUIDataChanged delegate for direct widget subscriptions
 *  - Handles event coalescing internally via timer (no separate UIConnector needed)
 *  - Routes user commands (equip/unequip) to EventDelegateManager
 * 
 * Key difference from old architecture:
 *  - NO ISuspenseEquipmentUIPort implementation (removed middleware layer)
 *  - NO dependency on UIConnector component
 *  - Direct communication with DataStore and Widgets
 */
UCLASS(BlueprintType)
class UISYSTEM_API USuspenseEquipmentUIBridge
    : public UObject
    , public ISuspenseEquipmentUIBridgeWidget
{
    GENERATED_BODY()

public:
    USuspenseEquipmentUIBridge();

    /** Initialize bridge with owning controller (for UI subsystems lookup) */
    UFUNCTION(BlueprintCallable, Category="UI|Equipment")
    void Initialize(APlayerController* InPC);

    /** Clean shutdown of the bridge */
    UFUNCTION(BlueprintCallable, Category="UI|Equipment")
    void Shutdown();

 // ===== NEW: Direct DataStore Binding =====

 /**
  * Bind directly to DataStore for slot change notifications
  * This replaces the old UIConnector → UIPort → Bridge flow
  * @param DataStore - Equipment data provider to subscribe to (wrapped in TScriptInterface)
  */
 UFUNCTION(BlueprintCallable, Category="UI|Equipment")
 void BindToDataStore(const TScriptInterface<ISuspenseEquipmentDataProvider>& DataStore);

    /**
     * Get cached UI data directly (no conversion needed)
     * Widgets can call this to get current state without triggering refresh
     * @return Array of cached equipment slot UI data
     */
    UFUNCTION(BlueprintCallable, Category="UI|Equipment")
    const TArray<FEquipmentSlotUIData>& GetCachedUIData() const { return CachedUIData; }

    // ===== NEW: Direct Widget Notification Delegate =====
    
    /**
     * Multicast delegate for widgets to subscribe to data changes
     * Replaces EventDelegateManager::NotifyInventoryUIRefreshRequested
     * Provides direct, typed data instead of generic refresh signal
     */
    FOnEquipmentUIDataChanged OnEquipmentUIDataChanged;

    // ===== ISuspenseEquipmentUIBridgeWidget Implementation =====
    virtual void ShowEquipmentUI_Implementation() override;
    virtual void HideEquipmentUI_Implementation() override;
    virtual void ToggleEquipmentUI_Implementation() override;
    virtual bool IsEquipmentUIVisible_Implementation() const override;
    virtual void RefreshEquipmentUI_Implementation() override;
    virtual void OnEquipmentDataChanged_Implementation(const FGameplayTag& ChangeType) override;
    virtual bool IsEquipmentConnected_Implementation() const override;
    virtual bool GetEquipmentSlotsUIData_Implementation(TArray<FEquipmentSlotUIData>& OutSlots) const override;
    virtual bool ProcessEquipmentDrop_Implementation(int32 SlotIndex, const FDragDropUIData& DragData) override;
    virtual bool ProcessUnequipRequest_Implementation(int32 SlotIndex, int32 TargetInventorySlot) override;
    virtual void SetEquipmentInterface_Implementation(const TScriptInterface<ISuspenseEquipmentInterface>& InEquipment) override;
    virtual TScriptInterface<ISuspenseEquipmentInterface> GetEquipmentInterface_Implementation() const override;
    virtual USuspenseItemManager* GetItemManager_Implementation() const override;

    // Static helpers
    static void RegisterBridge(USuspenseEquipmentUIBridge* Bridge);
    static void UnregisterBridge(USuspenseEquipmentUIBridge* Bridge);

    /** Optional: operations interface can be injected */
    UFUNCTION(BlueprintCallable, Category="UI|Equipment")
    void SetOperationsInterface(const TScriptInterface<ISuspenseEquipmentOperations>& InOps) { Operations = InOps; }

protected:
    // ===== Internal Helpers =====
    
    /**
     * Handle slot data change from DataStore (direct subscription)
     * Updates cache incrementally for the changed slot only
     * Schedules coalesced notification to widgets
     */
    void HandleDataStoreSlotChanged(int32 SlotIndex, const FSuspenseInventoryItemInstance& NewItem);
    
    /**
     * Handle full reset from DataStore
     * Rebuilds entire cache from scratch
     */
    void HandleDataStoreReset();
    
    /**
     * Update cached slot data incrementally
     * Called for each slot change, updates only affected slot
     */
    void UpdateCachedSlot(int32 SlotIndex, const FSuspenseInventoryItemInstance& NewItem);
    
    /**
     * Rebuild entire UI cache from DataStore
     * Called on initialization and reset
     */
    void RebuildUICache();
    
    /**
     * Schedule coalesced notification to widgets
     * Uses timer to batch multiple rapid changes into single update
     */
    void ScheduleCoalescedNotification();
    
    /**
     * Execute coalesced notification
     * Broadcasts OnEquipmentUIDataChanged with full cached data
     */
    void CoalesceAndNotify();
    
    /** Convert item instance to UI format */
    bool ConvertItemInstanceToUIData(const FSuspenseInventoryItemInstance& ItemInstance, FItemUIData& OutUIData) const;
    
    /** Resolve operations interface lazily */
    ISuspenseEquipmentOperations* ResolveOperations() const;
    
    /** Show user notification */
    void NotifyUser(const FString& Text, float Time = 2.0f) const;

private:
    // ===== Core Services =====
    UPROPERTY() APlayerController* OwningPlayerController = nullptr;
    UPROPERTY() USuspenseUIManager* UIManager = nullptr;
    UPROPERTY() UEventDelegateManager* EventManager = nullptr;
    UPROPERTY() mutable TWeakObjectPtr<USuspenseItemManager> CachedItemManager;

    // ===== NEW: Direct DataStore Subscription =====
    
    /** Delegate handle for DataStore slot changed subscription */
    FDelegateHandle DataStoreSlotChangedHandle;
    
    /** Delegate handle for DataStore reset subscription */
    FDelegateHandle DataStoreResetHandle;

    // ===== NEW: Internal Coalescing =====
    
    /** Set of pending slot updates for coalescing */
    TSet<int32> PendingSlotUpdates;
    
    /** Timer handle for coalesced notification */
    FTimerHandle CoalesceTimerHandle;
    
    /** Coalescing interval in seconds (default 50ms = 20 updates/sec max) */
    float CoalescingInterval = 0.05f;

    // ===== Cached State =====
    
    /** Cached slot configurations (from DataStore snapshot) */
    UPROPERTY() TArray<FEquipmentSlotConfig> CachedConfigs;
    
    /** NEW: Pre-built UI data cache (ready for widgets, no conversion needed) */
    UPROPERTY() TArray<FEquipmentSlotUIData> CachedUIData;
    
    /** Legacy: Map of slot index to equipped item (for backward compatibility) */
    UPROPERTY() TMap<int32, FSuspenseInventoryItemInstance> CachedItems;
    
    /** Active weapon slot index */
    UPROPERTY() int32 CachedActiveWeaponSlot = INDEX_NONE;
    
    /** Current equipment state tag */
    UPROPERTY() FGameplayTag CachedStateTag;
    
    /** Whether we have received initial snapshot from DataStore */
    UPROPERTY() bool bHasSnapshot = false;

    // ===== Legacy Compatibility =====
    UPROPERTY() TScriptInterface<ISuspenseEquipmentInterface> GameEquipment;
    UPROPERTY() TScriptInterface<ISuspenseEquipmentOperations> Operations;
    UPROPERTY() bool bVisible = false;
};
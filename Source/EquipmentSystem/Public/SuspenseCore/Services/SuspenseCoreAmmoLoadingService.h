// SuspenseCoreAmmoLoadingService.h
// Tarkov-style ammo loading service for magazines
// Copyright Suspense Team. All Rights Reserved.
//
// This service handles loading/unloading ammunition into magazines.
// Supports: Drag&Drop, Quick Load (double-click), Context Menu loading.
//
// ARCHITECTURE:
// - Centralized service for all ammo loading operations
// - EventBus integration for UI feedback
// - Server Authority for all mutations
// - Time-based loading with interruptible operations

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Containers/Ticker.h"  // FTSTicker for service ticking
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAmmoLoadingService.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreDataManager;
struct FSuspenseCoreEventData;

/**
 * Ammo loading operation state
 * Supports full state machine with pause/resume for edge cases:
 * - Magazine moved during loading
 * - Ammo dragged away during loading
 * - Player takes damage (cancellation via GAS)
 */
UENUM(BlueprintType)
enum class ESuspenseCoreAmmoLoadingState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Loading         UMETA(DisplayName = "Loading Ammo"),
    Unloading       UMETA(DisplayName = "Unloading Ammo"),
    Paused          UMETA(DisplayName = "Paused"),
    Cancelled       UMETA(DisplayName = "Cancelled"),
    Completed       UMETA(DisplayName = "Completed")
};

/**
 * Reason for ammo loading cancellation
 * @see TarkovStyle_Ammo_System_Design.md - Edge case handling
 */
UENUM(BlueprintType)
enum class ESuspenseCoreLoadCancelReason : uint8
{
    None                UMETA(DisplayName = "None"),
    UserCancelled       UMETA(DisplayName = "User Cancelled"),
    MagazineMoved       UMETA(DisplayName = "Magazine Moved"),
    MagazineRemoved     UMETA(DisplayName = "Magazine Removed"),
    AmmoDragged         UMETA(DisplayName = "Ammo Dragged Away"),
    AmmoInsufficient    UMETA(DisplayName = "Ammo Insufficient"),
    PlayerDamaged       UMETA(DisplayName = "Player Took Damage"),
    PlayerMoved         UMETA(DisplayName = "Player Moved"),
    InventoryClosed     UMETA(DisplayName = "Inventory Closed"),
    SystemError         UMETA(DisplayName = "System Error")
};

/**
 * Ammo loading request
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreAmmoLoadRequest
{
    GENERATED_BODY()

    /** Magazine instance ID to load into */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AmmoLoading")
    FGuid MagazineInstanceID;

    /** Ammo item ID from inventory */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AmmoLoading")
    FName AmmoID = NAME_None;

    /** Number of rounds to load (0 = max possible) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AmmoLoading")
    int32 RoundsToLoad = 0;

    /** Source inventory slot (for removing ammo) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AmmoLoading")
    int32 SourceInventorySlot = -1;

    /**
     * Source container ID (InventoryComponent ProviderID)
     * Used to identify which inventory to update when loading completes.
     * @see TarkovStyle_Ammo_System_Design.md - EventBus integration
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AmmoLoading")
    FGuid SourceContainerID;

    /** Is this a quick load (double-click) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AmmoLoading")
    bool bIsQuickLoad = false;

    bool IsValid() const
    {
        return MagazineInstanceID.IsValid() && !AmmoID.IsNone();
    }
};

/**
 * Ammo loading result
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreAmmoLoadResult
{
    GENERATED_BODY()

    /** Was operation successful */
    UPROPERTY(BlueprintReadOnly, Category = "AmmoLoading")
    bool bSuccess = false;

    /** Actual rounds loaded/unloaded */
    UPROPERTY(BlueprintReadOnly, Category = "AmmoLoading")
    int32 RoundsProcessed = 0;

    /** Error message if failed */
    UPROPERTY(BlueprintReadOnly, Category = "AmmoLoading")
    FString ErrorMessage;

    /** Time taken (seconds) */
    UPROPERTY(BlueprintReadOnly, Category = "AmmoLoading")
    float Duration = 0.0f;
};

/**
 * Active loading operation tracking
 * Extended with source validation for edge case handling.
 * @see TarkovStyle_Ammo_System_Design.md - Edge case handling
 */
USTRUCT()
struct FSuspenseCoreActiveLoadOperation
{
    GENERATED_BODY()

    UPROPERTY()
    FSuspenseCoreAmmoLoadRequest Request;

    UPROPERTY()
    FSuspenseCoreMagazineInstance TargetMagazine;

    UPROPERTY()
    ESuspenseCoreAmmoLoadingState State = ESuspenseCoreAmmoLoadingState::Idle;

    /** Reason for cancellation (if State == Cancelled) */
    UPROPERTY()
    ESuspenseCoreLoadCancelReason CancelReason = ESuspenseCoreLoadCancelReason::None;

    UPROPERTY()
    float StartTime = 0.0f;

    UPROPERTY()
    float TotalDuration = 0.0f;

    UPROPERTY()
    int32 RoundsProcessed = 0;

    UPROPERTY()
    int32 RoundsRemaining = 0;

    UPROPERTY()
    float TimePerRound = 0.5f;

    /** Accumulated time for per-round processing */
    UPROPERTY()
    float AccumulatedTime = 0.0f;

    //==================================================================
    // Source validation tracking (for edge case detection)
    //==================================================================

    /** Last validated source ammo quantity */
    UPROPERTY()
    int32 LastValidatedAmmoQuantity = 0;

    /** Last validated magazine slot index (-1 for inventory) */
    UPROPERTY()
    int32 LastValidatedMagazineSlot = -1;

    /** Was source validated this tick */
    UPROPERTY()
    bool bSourceValidatedThisTick = false;

    /** Owner actor for validation callbacks */
    UPROPERTY()
    TWeakObjectPtr<AActor> OwnerActor;

    bool IsActive() const
    {
        return State == ESuspenseCoreAmmoLoadingState::Loading ||
               State == ESuspenseCoreAmmoLoadingState::Unloading;
    }

    bool IsPaused() const
    {
        return State == ESuspenseCoreAmmoLoadingState::Paused;
    }

    bool IsCompleted() const
    {
        return State == ESuspenseCoreAmmoLoadingState::Completed;
    }

    bool IsCancelled() const
    {
        return State == ESuspenseCoreAmmoLoadingState::Cancelled;
    }
};

/**
 * Delegate for loading state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnAmmoLoadingStateChanged,
    const FGuid&, MagazineInstanceID,
    ESuspenseCoreAmmoLoadingState, NewState,
    float, Progress
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAmmoLoadingCompleted,
    const FGuid&, MagazineInstanceID,
    const FSuspenseCoreAmmoLoadResult&, Result
);

/**
 * USuspenseCoreAmmoLoadingService
 *
 * Centralized service for Tarkov-style ammunition loading into magazines.
 * Handles time-based loading with proper EventBus integration.
 *
 * FEATURES:
 * - Drag & Drop: Load ammo by dropping onto magazine
 * - Quick Load: Double-click ammo to load into best magazine
 * - Timed Loading: LoadTimePerRound from MagazineData
 * - Interruptible: Can cancel ongoing loading
 * - Caliber Check: Only compatible ammo can be loaded
 *
 * USAGE:
 *   // Request loading via service
 *   FSuspenseCoreAmmoLoadRequest Request;
 *   Request.MagazineInstanceID = MagGuid;
 *   Request.AmmoID = FName("556x45_M855");
 *   Request.RoundsToLoad = 30;
 *
 *   AmmoLoadingService->StartLoading(Request, OwnerActor);
 *
 * @see FSuspenseCoreAmmoLoadRequest
 * @see FSuspenseCoreMagazineData
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreAmmoLoadingService : public UObject, public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    USuspenseCoreAmmoLoadingService();

    //==================================================================
    // ISuspenseCoreEquipmentService Implementation
    //==================================================================

    virtual bool InitializeService(const FSuspenseCoreServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual ESuspenseCoreServiceLifecycleState GetServiceState() const override { return ServiceState; }
    virtual bool IsServiceReady() const override { return ServiceState == ESuspenseCoreServiceLifecycleState::Ready; }
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;

    //==================================================================
    // Legacy Initialization (kept for compatibility)
    //==================================================================

    /**
     * Initialize service with required dependencies
     * @param InEventBus EventBus for publishing events
     * @param InDataManager DataManager for magazine/ammo data
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    void Initialize(USuspenseCoreEventBus* InEventBus, USuspenseCoreDataManager* InDataManager);

    //==================================================================
    // Loading Operations
    //==================================================================

    /**
     * Start loading ammo into magazine
     * @param Request Loading request details
     * @param OwnerActor Actor performing the loading (for authority check)
     * @return true if loading started
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    bool StartLoading(const FSuspenseCoreAmmoLoadRequest& Request, AActor* OwnerActor);

    /**
     * Start unloading ammo from magazine
     * @param MagazineInstanceID Magazine to unload
     * @param RoundsToUnload Number of rounds (0 = all)
     * @param OwnerActor Actor performing the unloading
     * @return true if unloading started
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    bool StartUnloading(const FGuid& MagazineInstanceID, int32 RoundsToUnload, AActor* OwnerActor);

    /**
     * Cancel ongoing loading/unloading operation
     * @param MagazineInstanceID Magazine operation to cancel
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    void CancelOperation(const FGuid& MagazineInstanceID);

    /**
     * Cancel ongoing operation with specific reason
     * @param MagazineInstanceID Magazine operation to cancel
     * @param Reason Reason for cancellation (included in event)
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    void CancelOperationWithReason(const FGuid& MagazineInstanceID, ESuspenseCoreLoadCancelReason Reason);

    /**
     * Cancel all active operations for an actor
     * @param OwnerActor Actor whose operations to cancel
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    void CancelAllOperations(AActor* OwnerActor);

    //==================================================================
    // Pause/Resume Operations (for edge case handling)
    //==================================================================

    /**
     * Pause an active loading operation
     * Called when source validation fails but may recover (e.g., inventory refresh)
     * @param MagazineInstanceID Magazine operation to pause
     * @return true if paused
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    bool PauseOperation(const FGuid& MagazineInstanceID);

    /**
     * Resume a paused loading operation
     * @param MagazineInstanceID Magazine operation to resume
     * @return true if resumed
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    bool ResumeOperation(const FGuid& MagazineInstanceID);

    /**
     * Check if operation is paused
     * @param MagazineInstanceID Magazine instance to check
     * @return true if paused
     */
    UFUNCTION(BlueprintPure, Category = "AmmoLoading")
    bool IsPaused(const FGuid& MagazineInstanceID) const;

    //==================================================================
    // Quick Load (Double-click)
    //==================================================================

    /**
     * Quick load: Find best magazine and load ammo
     * @param AmmoID Type of ammo to load
     * @param AmmoCount Available ammo count
     * @param OwnerActor Actor performing the action
     * @param OutMagazineID Output: Magazine that was selected
     * @return true if quick load started
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    bool QuickLoadAmmo(FName AmmoID, int32 AmmoCount, AActor* OwnerActor, FGuid& OutMagazineID);

    //==================================================================
    // Queries
    //==================================================================

    /**
     * Check if can load ammo into magazine
     * @param MagazineID Magazine DataTable ID
     * @param AmmoID Ammo type to check
     * @return true if compatible
     */
    UFUNCTION(BlueprintPure, Category = "AmmoLoading")
    bool CanLoadAmmo(FName MagazineID, FName AmmoID) const;

    /**
     * Get loading time for operation
     * @param MagazineID Magazine DataTable ID
     * @param RoundCount Number of rounds
     * @return Total loading time in seconds
     */
    UFUNCTION(BlueprintPure, Category = "AmmoLoading")
    float GetLoadingDuration(FName MagazineID, int32 RoundCount) const;

    /**
     * Check if magazine is currently being loaded
     * @param MagazineInstanceID Magazine instance to check
     * @return true if loading in progress
     */
    UFUNCTION(BlueprintPure, Category = "AmmoLoading")
    bool IsLoading(const FGuid& MagazineInstanceID) const;

    /**
     * Get loading progress
     * @param MagazineInstanceID Magazine instance
     * @return Progress 0.0 - 1.0
     */
    UFUNCTION(BlueprintPure, Category = "AmmoLoading")
    float GetLoadingProgress(const FGuid& MagazineInstanceID) const;

    /**
     * Get active operation for magazine
     * @param MagazineInstanceID Magazine instance
     * @param OutOperation Output operation data
     * @return true if active operation exists
     */
    bool GetActiveOperation(const FGuid& MagazineInstanceID, FSuspenseCoreActiveLoadOperation& OutOperation) const;

    //==================================================================
    // Tick (called by owner)
    //==================================================================

    /**
     * Update active operations
     * @param DeltaTime Time since last update
     */
    void Tick(float DeltaTime);

    //==================================================================
    // Events
    //==================================================================

    /** Fired when loading state changes */
    UPROPERTY(BlueprintAssignable, Category = "AmmoLoading|Events")
    FOnAmmoLoadingStateChanged OnLoadingStateChanged;

    /** Fired when loading completes (success or failure) */
    UPROPERTY(BlueprintAssignable, Category = "AmmoLoading|Events")
    FOnAmmoLoadingCompleted OnLoadingCompleted;

    /**
     * Subscribe to EventBus events for ammo loading requests
     * Called automatically during Initialize if EventBus is valid
     */
    void SubscribeToEvents();

    /**
     * Unsubscribe from EventBus events
     */
    void UnsubscribeFromEvents();

protected:
    //==================================================================
    // Internal Methods
    //==================================================================

    /** Handle ammo load request event from UI (drag&drop, etc.) */
    void OnAmmoLoadRequestedEvent(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

    /** Process a single tick of loading operation */
    void ProcessLoadingTick(FSuspenseCoreActiveLoadOperation& Operation, float DeltaTime);

    /** Complete loading operation */
    void CompleteOperation(const FGuid& MagazineInstanceID, bool bSuccess, const FString& ErrorMessage = TEXT(""));

    /** Publish EventBus event for loading */
    void PublishLoadingEvent(const FGameplayTag& EventTag, const FGuid& MagazineInstanceID,
                             const FSuspenseCoreActiveLoadOperation& Operation);

    /** Get magazine data from DataManager */
    bool GetMagazineData(FName MagazineID, FSuspenseCoreMagazineData& OutData) const;

    /** Validate ammo compatibility with magazine */
    bool ValidateAmmoCompatibility(const FSuspenseCoreMagazineData& MagData, FName AmmoID) const;

    //==================================================================
    // Per-Round Source Validation (Edge Case Detection)
    // @see TarkovStyle_Ammo_System_Design.md - Edge case handling
    //==================================================================

    /**
     * Validate source items before processing next round
     * Checks:
     * - Magazine still exists in original location
     * - Ammo still exists at source slot with sufficient quantity
     *
     * @param Operation Active operation to validate
     * @param OutCancelReason Set if validation fails
     * @return true if valid, false if loading should cancel
     */
    bool ValidateSourceBeforeRound(
        FSuspenseCoreActiveLoadOperation& Operation,
        ESuspenseCoreLoadCancelReason& OutCancelReason);

    /**
     * Called when inventory item is moved
     * Checks if any active loading operations are affected
     */
    void OnInventoryItemMoved(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /**
     * Called when inventory item is removed
     * Cancels affected loading operations
     */
    void OnInventoryItemRemoved(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /**
     * Subscribe to inventory events for edge case detection
     */
    void SubscribeToInventoryEvents();

    /**
     * Unsubscribe from inventory events
     */
    void UnsubscribeFromInventoryEvents();

private:
    //==================================================================
    // Dependencies
    //==================================================================

    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreDataManager> DataManager;

    //==================================================================
    // Active Operations
    //==================================================================

    /** Map of active loading operations by magazine instance ID */
    UPROPERTY()
    TMap<FGuid, FSuspenseCoreActiveLoadOperation> ActiveOperations;

    /** Map of magazine instances being managed (cached) */
    UPROPERTY()
    TMap<FGuid, FSuspenseCoreMagazineInstance> ManagedMagazines;

    /** Event subscription handle for load requests */
    FSuspenseCoreSubscriptionHandle LoadRequestedEventHandle;

    /** Event subscription handle for inventory item moved (edge case detection) */
    FSuspenseCoreSubscriptionHandle ItemMovedEventHandle;

    /** Event subscription handle for inventory item removed (edge case detection) */
    FSuspenseCoreSubscriptionHandle ItemRemovedEventHandle;

    /** Ticker handle for processing loading operations */
    FTSTicker::FDelegateHandle TickerHandle;

    /** Service lifecycle state */
    ESuspenseCoreServiceLifecycleState ServiceState = ESuspenseCoreServiceLifecycleState::Uninitialized;

private:
    //==================================================================
    // Ticker Management
    //==================================================================

    /** Start ticking if we have active operations */
    void StartTicking();

    /** Stop ticking when no active operations */
    void StopTicking();

    /** Ticker callback - returns true to continue ticking */
    bool TickerCallback(float DeltaTime);
};

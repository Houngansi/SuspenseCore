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
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Events/SuspenseCoreEventTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAmmoLoadingService.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreDataManager;
struct FSuspenseCoreEventData;

/**
 * Ammo loading operation state
 */
UENUM(BlueprintType)
enum class ESuspenseCoreAmmoLoadingState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Loading         UMETA(DisplayName = "Loading Ammo"),
    Unloading       UMETA(DisplayName = "Unloading Ammo"),
    Cancelled       UMETA(DisplayName = "Cancelled")
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

    bool IsActive() const
    {
        return State == ESuspenseCoreAmmoLoadingState::Loading ||
               State == ESuspenseCoreAmmoLoadingState::Unloading;
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
class EQUIPMENTSYSTEM_API USuspenseCoreAmmoLoadingService : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreAmmoLoadingService();

    //==================================================================
    // Initialization
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
     * Cancel all active operations for an actor
     * @param OwnerActor Actor whose operations to cancel
     */
    UFUNCTION(BlueprintCallable, Category = "AmmoLoading")
    void CancelAllOperations(AActor* OwnerActor);

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
};

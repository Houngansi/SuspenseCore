// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Abilities/ISuspenseAbilityProvider.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseEquipmentComponentBase.generated.h"

// Forward declarations
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class USuspenseEventManager;
class USuspenseItemManager;

// Equipment logging category definition
DECLARE_LOG_CATEGORY_EXTERN(LogMedComEquipment, Log, All);

// Helper macro for equipment logging with context
#define EQUIPMENT_LOG(Verbosity, Format, ...) \
UE_LOG(LogMedComEquipment, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

// Client prediction data structure
USTRUCT()
struct FEquipmentComponentPredictionData
{
    GENERATED_BODY()

    /** Unique prediction key */
    UPROPERTY()
    int32 PredictionKey = 0;

    /** Predicted item instance */
    UPROPERTY()
    FSuspenseInventoryItemInstance PredictedItem;

    /** Time when prediction was made */
    UPROPERTY()
    float PredictionTime = 0.0f;

    /** Whether prediction was confirmed */
    UPROPERTY()
    bool bConfirmed = false;

    FEquipmentComponentPredictionData() = default;

    bool IsExpired(float CurrentTime, float TimeoutSeconds = 2.0f) const
    {
        return (CurrentTime - PredictionTime) > TimeoutSeconds;
    }
};

/**
 * Base class for all equipment-related components
 *
 * ENHANCED VERSION:
 * - Full replication support for all critical states
 * - Client prediction infrastructure
 * - Thread-safe caching with critical sections
 * - Enhanced validation and error handling
 * - No legacy code or placeholders
 */
UCLASS(Abstract)
class EQUIPMENTSYSTEM_API USuspenseEquipmentComponentBase : public UActorComponent, public ISuspenseAbilityProvider
{
    GENERATED_BODY()

public:
    USuspenseEquipmentComponentBase();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Initialize component with owner and ASC
     * @param InOwner The actor that owns this equipment component
     * @param InASC The ability system component for GAS integration
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    virtual void Initialize(AActor* InOwner, UAbilitySystemComponent* InASC);

    /**
     * Initialize component with item instance data
     * @param InOwner The actor that owns this equipment component
     * @param InASC The ability system component for GAS integration
     * @param ItemInstance The item instance to equip
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    virtual void InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance);

    /**
     * Comprehensive resource cleanup
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    virtual void Cleanup();

    /**
     * Update equipment with new item instance
     * @param NewItemInstance New item instance data
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    virtual void UpdateEquippedItem(const FSuspenseInventoryItemInstance& NewItemInstance);

    //================================================
    // Client Prediction Support
    //================================================

    /**
     * Start client prediction for equipment change
     * @param PredictedInstance Item instance being predicted
     * @return Prediction key for tracking
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Prediction")
    int32 StartClientPrediction(const FSuspenseInventoryItemInstance& PredictedInstance);

    /**
     * Confirm or reject client prediction
     * @param PredictionKey Key from StartClientPrediction
     * @param bSuccess Whether prediction was correct
     * @param ActualInstance Actual item instance from server
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Prediction")
    void ConfirmClientPrediction(int32 PredictionKey, bool bSuccess, const FSuspenseInventoryItemInstance& ActualInstance);

    /**
     * Clean up expired predictions
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Prediction")
    void CleanupExpiredPredictions();

    /**
     * Check if we're in prediction mode
     * @return True if any predictions are active
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Prediction")
    bool IsInPredictionMode() const { return ActivePredictions.Num() > 0; }

    //================================================
    // DataTable Integration Methods
    //================================================

    /**
     * Get item manager subsystem (thread-safe)
     * @return Item manager or nullptr if not available
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    USuspenseItemManager* GetItemManager() const;

    /**
     * Get currently equipped item instance
     * @return Current item instance
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    const FSuspenseInventoryItemInstance& GetEquippedItemInstance() const { return EquippedItemInstance; }

    /**
     * Set equipped item instance
     * @param ItemInstance New item instance to equip
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    virtual void SetEquippedItemInstance(const FSuspenseInventoryItemInstance& ItemInstance);

    /**
     * Get equipped item data from DataTable
     * @param OutItemData Output unified item data
     * @return True if data was retrieved successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    bool GetEquippedItemData(FSuspenseUnifiedItemData& OutItemData) const;

    /**
     * Check if item is currently equipped
     * @return True if valid item is equipped
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    bool HasEquippedItem() const { return EquippedItemInstance.IsValid(); }

    /**
     * Get equipped item ID
     * @return Item ID or NAME_None if no item equipped
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    FName GetEquippedItemID() const { return EquippedItemInstance.ItemID; }

    /**
     * Get runtime property from equipped item
     * @param PropertyName Name of the property
     * @param DefaultValue Default value if property not found
     * @return Property value
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    float GetEquippedItemProperty(const FName& PropertyName, float DefaultValue = 0.0f) const;

    /**
     * Set runtime property on equipped item
     * @param PropertyName Name of the property
     * @param Value New value
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    void SetEquippedItemProperty(const FName& PropertyName, float Value);

    //================================================
    // System Access Methods
    //================================================

    /**
     * Cached ability system component access
     * @return The cached ability system component
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|System")
    UAbilitySystemComponent* GetCachedASC() const { return CachedASC; }

    /**
     * Central delegate manager access (thread-safe)
     * @return Pointer to the central delegate manager, or nullptr if unavailable
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|System")
    USuspenseEventManager* GetDelegateManager() const;

    //================================================
    // Enhanced Broadcast Methods
    //================================================

    /** All broadcast methods remain unchanged but now with improved validation */
    void BroadcastItemEquipped(const FSuspenseInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType);
    void BroadcastItemUnequipped(const FSuspenseInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType);
    void BroadcastEquipmentPropertyChanged(const FName& PropertyName, float OldValue, float NewValue);
    void BroadcastEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bInterrupted = false);
    void BroadcastEquipmentEvent(const FGameplayTag& EventTag, const FString& EventData);
    void BroadcastEquipmentUpdated();

    //================================================
    // Weapon-Specific Broadcasts
    //================================================

    void BroadcastAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize);
    void BroadcastWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, const FGameplayTag& FireMode);
    void BroadcastFireModeChanged(const FGameplayTag& NewFireMode, const FText& FireModeDisplayName);
    void BroadcastWeaponReload(bool bStarted, float ReloadDuration = 0.0f);
    void BroadcastWeaponSpreadUpdated(float NewSpread, float MaxSpread);

    //================================================
    // ISuspenseAbilityProvider Implementation
    //================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Abilities")
    UAbilitySystemComponent* GetAbilitySystemComponent() const;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent_Implementation() const override { return CachedASC; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Abilities")
    void InitializeAbilityProvider(UAbilitySystemComponent* InASC);
    virtual void InitializeAbilityProvider_Implementation(UAbilitySystemComponent* InASC) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Abilities")
    FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID);
    virtual FGameplayAbilitySpecHandle GrantAbility_Implementation(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Abilities")
    void RemoveAbility(FGameplayAbilitySpecHandle AbilityHandle);
    virtual void RemoveAbility_Implementation(FGameplayAbilitySpecHandle AbilityHandle) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Abilities")
    FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level);
    virtual FActiveGameplayEffectHandle ApplyEffectToSelf_Implementation(TSubclassOf<UGameplayEffect> EffectClass, float Level) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Abilities")
    void RemoveEffect(FActiveGameplayEffectHandle EffectHandle);
    virtual void RemoveEffect_Implementation(FActiveGameplayEffectHandle EffectHandle) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment")
    bool IsInitialized() const;
    virtual bool IsInitialized_Implementation() const override { return bIsInitialized; }

protected:
    /**
     * Called when equipment is initialized
     */
    virtual void OnEquipmentInitialized();

    /**
     * Called when equipped item changes
     */
    virtual void OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem);

    /**
     * Server-side execution helper
     * @param FuncName Function name for logging purposes
     * @param ServerCode Lambda containing code to execute only on server
     * @return True if the code was executed (i.e., we're on server)
     */
    bool ExecuteOnServer(const FString& FuncName, TFunction<void()> ServerCode);

    /**
     * Validate delegate manager availability
     * @return True if delegate manager is available and valid
     */
    bool ValidateDelegateManager() const;

    /**
     * Thread-safe cache initialization
     */
    void InitializeCoreReferences();

    /**
     * Validate system references
     * @return True if all required systems are available
     */
    bool ValidateSystemReferences() const;

    /**
     * Log event broadcast with context
     * @param EventName Name of the event being broadcast
     * @param bSuccess Whether broadcast was successful
     */
    void LogEventBroadcast(const FString& EventName, bool bSuccess) const;

    //================================================
    // Replication Callbacks
    //================================================

    UFUNCTION()
    void OnRep_EquippedItemInstance(const FSuspenseInventoryItemInstance& OldInstance);

    UFUNCTION()
    void OnRep_ComponentState();

    //================================================
    // Core State
    //================================================

    /** Initialization status flag - replicated for network consistency */
    UPROPERTY(ReplicatedUsing=OnRep_ComponentState)
    bool bIsInitialized;

    /** Currently equipped item instance - replicated for network consistency */
    UPROPERTY()
    FSuspenseInventoryItemInstance EquippedItemInstance;

    /** Component version for compatibility tracking */
    UPROPERTY(Replicated)
    uint8 ComponentVersion;

    /** Cached reference to the ability system component */
    UPROPERTY()
    UAbilitySystemComponent* CachedASC;

    /** Debug counter for tracking component lifecycle events */
    UPROPERTY(Replicated)
    int32 EquipmentCycleCounter;

    /** Counter for broadcast events (debug) */
    UPROPERTY()
    mutable int32 BroadcastEventCounter;

    //================================================
    // Thread-Safe Cache References
    //================================================

    /** Critical section for thread-safe cache access */
    mutable FCriticalSection CacheCriticalSection;

    /** Cached reference to item manager subsystem */
    mutable TWeakObjectPtr<USuspenseItemManager> CachedItemManager;

    /** Cached reference to delegate manager */
    mutable TWeakObjectPtr<USuspenseEventManager> CachedDelegateManager;

    /** Last time caches were validated */
    mutable float LastCacheValidationTime;

    //================================================
    // Client Prediction State
    //================================================

    /** Active predictions waiting for server confirmation */
    UPROPERTY()
    TArray<FEquipmentComponentPredictionData> ActivePredictions;

    /** Counter for generating unique prediction keys */
    UPROPERTY()
    int32 NextPredictionKey;

    /** Maximum number of concurrent predictions allowed */
    static constexpr int32 MaxConcurrentPredictions = 5;

    /** Timeout for predictions in seconds */
    static constexpr float PredictionTimeoutSeconds = 2.0f;
};

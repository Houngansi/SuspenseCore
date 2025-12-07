// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Types/Loadout/SuspenseCoreItemDataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentMeshComponent.generated.h"

// Forward declarations
class UCameraComponent;
class UNiagaraComponent;
class UAudioComponent;
#include "ItemSystem/SuspenseCoreItemManager.h"
class USuspenseCoreEventManager;

/**
 * Visual state data for synchronization
 */
USTRUCT()
struct FSuspenseCoreEquipmentVisualState
{
    GENERATED_BODY()

    /** Current condition visual (0-1) */
    UPROPERTY()
    float ConditionPercent = 1.0f;

    /** Current rarity glow intensity */
    UPROPERTY()
    float RarityGlowIntensity = 0.0f;

    /** Rarity color */
    UPROPERTY()
    FLinearColor RarityColor = FLinearColor::White;

    /** Active visual effects tags */
    UPROPERTY()
    FGameplayTagContainer ActiveEffects;

    /** Custom material parameters */
    UPROPERTY()
    TMap<FName, float> MaterialScalarParams;

    /** Custom material colors */
    UPROPERTY()
    TMap<FName, FLinearColor> MaterialVectorParams;

    /** State version for change detection */
    UPROPERTY()
    uint8 StateVersion = 0;

    FEquipmentVisualState() = default;

    bool operator==(const FEquipmentVisualState& Other) const
    {
        return ConditionPercent == Other.ConditionPercent &&
               RarityGlowIntensity == Other.RarityGlowIntensity &&
               RarityColor == Other.RarityColor &&
               ActiveEffects == Other.ActiveEffects;
    }

    bool operator!=(const FEquipmentVisualState& Other) const
    {
        return !(*this == Other);
    }
};

/**
 * Visual effect prediction data
 */
USTRUCT()
struct FSuspenseCoreVisualEffectPrediction
{
    GENERATED_BODY()

    /** Unique prediction key */
    UPROPERTY()
    int32 PredictionKey = 0;

    /** Effect type being predicted */
    UPROPERTY()
    FGameplayTag EffectType;

    /** Time when effect started */
    UPROPERTY()
    float StartTime = 0.0f;

    /** Expected duration */
    UPROPERTY()
    float Duration = 0.0f;

    /** Component playing the effect */
    UPROPERTY()
    TWeakObjectPtr<USceneComponent> EffectComponent;

    FVisualEffectPrediction() = default;
};

/**
 * Specialized mesh component for equipment visualization
 *
 * ENHANCED VERSION:
 * - Visual state synchronization through parent component
 * - Client prediction for effects and material changes
 * - Thread-safe material instance management
 * - Optimized effect pooling preparation
 * - Full integration with new DataTable architecture
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentMeshComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentMeshComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Initialize mesh from item instance data
     * @param ItemInstance Item instance with runtime data
     * @return True if initialization successful
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    bool InitializeFromItemInstance(const FSuspenseCoreInventoryItemInstance& ItemInstance);

    /**
     * Update visual state based on item properties
     * @param ItemInstance Updated item instance
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    void UpdateVisualState(const FSuspenseCoreInventoryItemInstance& ItemInstance);

    /**
     * Clean up visual components and resources
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    void CleanupVisuals();

    //================================================
    // Visual State Synchronization
    //================================================

 /**
  * Get current visual state for synchronization
  * NOTE: Not Blueprint-callable due to TMap in FEquipmentVisualState
  * @return Current visual state data
  */
 FSuspenseCoreEquipmentVisualState GetVisualState() const { return CurrentVisualState; }

 /**
  * Apply visual state from synchronization
  * NOTE: Not Blueprint-callable due to TMap in FEquipmentVisualState
  * @param NewState State to apply
  * @param bForceUpdate Force update even if state appears unchanged
  */
 void ApplyVisualState(const FSuspenseCoreEquipmentVisualState& NewState, bool bForceUpdate = false);

 /**
  * Check if visual state has changed
  * NOTE: Not Blueprint-callable due to TMap in FEquipmentVisualState
  * @param OtherState State to compare against
  * @return True if states differ
  */
 bool HasVisualStateChanged(const FSuspenseCoreEquipmentVisualState& OtherState) const;

    //================================================
    // Socket and Transform Management
    //================================================

    /**
     * Get socket location with fallback
     * @param SocketName Socket to find
     * @return Socket location or component location if not found
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    FVector GetSocketLocationSafe(const FName& SocketName) const;

    /**
     * Get socket rotation with fallback
     * @param SocketName Socket to find
     * @return Socket rotation or component rotation if not found
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    FRotator GetSocketRotationSafe(const FName& SocketName) const;

    /**
     * Get socket transform with fallback
     * @param SocketName Socket to find
     * @return Socket transform or component transform if not found
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    FTransform GetSocketTransformSafe(const FName& SocketName) const;

    /**
     * Apply additional offset transform
     * @param Offset Transform offset to apply
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Mesh")
    void ApplyOffsetTransform(const FTransform& Offset);

    //================================================
    // Weapon-Specific Features
    //================================================

    /**
     * Setup weapon visual components
     * @param WeaponData Weapon item data
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    void SetupWeaponVisuals(const FSuspenseCoreUnifiedItemData& WeaponData);

    /**
     * Get muzzle socket location
     * @return Muzzle location in world space
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    FVector GetMuzzleLocation() const;

    /**
     * Get muzzle socket direction
     * @return Normalized muzzle forward direction
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    FVector GetMuzzleDirection() const;

    /**
     * Play muzzle flash effect with prediction
     * @return Prediction key for tracking
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    int32 PlayMuzzleFlash();

 /**
  * Setup scope camera with FOV
  * @param FOV Field of view for scope
  * @param bShouldAutoActivate Whether to auto-activate camera
  */
 UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
 void SetupScopeCamera(float FOV, bool bShouldAutoActivate = false);

    /**
     * Activate/deactivate scope camera
     * @param bActivate Whether to activate
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    void SetScopeCameraActive(bool bActivate);

    /**
     * Check if weapon has scope
     * @return True if scope camera exists
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    bool HasScope() const { return ScopeCamera != nullptr; }

    /**
     * Get scope camera component
     * @return Scope camera or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Weapon")
    UCameraComponent* GetScopeCamera() const { return ScopeCamera; }

    //================================================
    // Visual State and Effects
    //================================================

    /**
     * Set equipment condition visual state
     * @param ConditionPercent Condition from 0.0 to 1.0
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void SetConditionVisual(float ConditionPercent);

    /**
     * Set equipment rarity visual state
     * @param RarityTag Rarity tag from item data
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void SetRarityVisual(const FGameplayTag& RarityTag);

    /**
     * Play equipment use effect with prediction
     * @param EffectType Type of effect to play
     * @return Prediction key for tracking
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    int32 PlayEquipmentEffect(const FGameplayTag& EffectType);

    /**
     * Confirm effect prediction
     * @param PredictionKey Key from PlayEquipmentEffect
     * @param bSuccess Whether effect played successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void ConfirmEffectPrediction(int32 PredictionKey, bool bSuccess);

    /**
     * Update material parameters
     * @param ParameterName Parameter to update
     * @param Value New value
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void SetMaterialParameter(const FName& ParameterName, float Value);

    /**
     * Update material color parameter
     * @param ParameterName Parameter to update
     * @param Color New color value
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void SetMaterialColorParameter(const FName& ParameterName, const FLinearColor& Color);

    //================================================
    // Attachment Points
    //================================================

    /**
     * Get attachment socket for modification
     * @param ModificationType Type of modification (sight, grip, etc.)
     * @return Socket name or NAME_None
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    FName GetAttachmentSocket(const FGameplayTag& ModificationType) const;

    /**
     * Check if attachment socket exists
     * @param ModificationType Type of modification
     * @return True if socket exists
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    bool HasAttachmentSocket(const FGameplayTag& ModificationType) const;

    //================================================
    // State Notification
    //================================================

    /**
     * Notify parent component of visual state change
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void NotifyVisualStateChanged();

    /**
     * Request visual state synchronization
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual")
    void RequestStateSync();

protected:
    /**
     * Initialize visual components based on item type
     * @param ItemData Item data from DataTable
     */
    void InitializeVisualComponents(const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Load and apply mesh from item data
     * @param ItemData Item data with mesh reference
     * @return True if mesh loaded successfully
     */
    bool LoadMeshFromItemData(const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Create dynamic material instances
     */
    void CreateDynamicMaterials();

    /**
     * Update all dynamic materials with current state
     */
    void UpdateDynamicMaterials();

    /**
     * Get item manager subsystem
     * @return Item manager or nullptr
     */
    USuspenseCoreItemManager* GetItemManager() const;

    /**
     * Get event delegate manager
     * @return Delegate manager or nullptr
     */
    USuspenseCoreEventManager* GetDelegateManager() const;

    /**
     * Map modification type to socket name
     * @param WeaponData Weapon item data
     * @param ModificationType Modification type tag
     * @return Socket name from weapon data
     */
    FName GetWeaponSocketName(const FSuspenseCoreUnifiedItemData& WeaponData, const FGameplayTag& ModificationType) const;

    /**
     * Play visual effect at location
     * @param EffectType Type of effect
     * @param Location World location
     * @param Rotation World rotation
     * @return Effect component if spawned
     */
    UNiagaraComponent* PlayVisualEffectAtLocation(const FGameplayTag& EffectType, const FVector& Location, const FRotator& Rotation);

    /**
     * Clean up expired effect predictions
     */
    void CleanupExpiredPredictions();

    /**
     * Apply predicted effect locally
     * @param Prediction Effect prediction data
     */
    void ApplyPredictedEffect(const FSuspenseCoreVisualEffectPrediction& Prediction);

    /**
     * Stop predicted effect
     * @param Prediction Effect prediction data
     */
    void StopPredictedEffect(const FSuspenseCoreVisualEffectPrediction& Prediction);

private:
    //================================================
    // Visual Components
    //================================================

    /** Scope camera component for weapons */
    UPROPERTY()
    UCameraComponent* ScopeCamera;

    /** Muzzle flash effect component */
    UPROPERTY()
    UNiagaraComponent* MuzzleFlashComponent;

    /** Audio component for equipment sounds */
    UPROPERTY()
    UAudioComponent* AudioComponent;

    /** Dynamic material instances for customization */
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> DynamicMaterials;

    /** Active effect components for cleanup */
    UPROPERTY()
    TArray<UNiagaraComponent*> ActiveEffectComponents;

    //================================================
    // Configuration
    //================================================

    /** Currently loaded item data */
    UPROPERTY()
    FSuspenseCoreUnifiedItemData CachedItemData;

    /** Current item instance */
    UPROPERTY()
    FSuspenseCoreInventoryItemInstance CurrentItemInstance;

    /** Current visual state */
    UPROPERTY()
    FSuspenseCoreEquipmentVisualState CurrentVisualState;

    /** Previous visual state for change detection */
    UPROPERTY()
    FSuspenseCoreEquipmentVisualState PreviousVisualState;

    /** Additional transform offset */
    UPROPERTY()
    FTransform AdditionalOffset;

    /** Whether visual components are initialized */
    UPROPERTY()
    bool bVisualsInitialized;

    /** Thread-safe access to visual state */
    mutable FCriticalSection VisualStateCriticalSection;

    //================================================
    // Effect Prediction
    //================================================

    /** Active effect predictions */
    UPROPERTY()
    TArray<FSuspenseCoreVisualEffectPrediction> ActivePredictions;

    /** Next prediction key */
    UPROPERTY()
    int32 NextPredictionKey;

    /** Maximum effect components to pool */
    static constexpr int32 MaxPooledEffects = 10;

    /** Pooled effect components for reuse */
    UPROPERTY()
    TArray<UNiagaraComponent*> PooledEffectComponents;

    //================================================
    // Cached References
    //================================================

    /** Cached item manager reference */
    UPROPERTY()
    mutable TWeakObjectPtr<USuspenseCoreItemManager> CachedItemManager;

    /** Cached delegate manager reference */
    UPROPERTY()
    mutable TWeakObjectPtr<USuspenseCoreEventManager> CachedDelegateManager;

    /** Last cache validation time */
    mutable float LastCacheValidationTime;

    //================================================
    // Socket Name Constants
    //================================================

    /** Default muzzle socket name */
    static const FName DefaultMuzzleSocket;

    /** Default scope socket name */
    static const FName DefaultScopeSocket;

    /** Default magazine socket name */
    static const FName DefaultMagazineSocket;

    /** Cache validation interval */
    static constexpr float CacheValidationInterval = 1.0f;
};

// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Interfaces/Weapon/ISuspenseWeaponAnimation.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentAttachmentComponent.generated.h"

// Forward declarations
class USceneComponent;
class USkeletalMeshComponent;
class USuspenseCoreEquipmentMeshComponent;
class USuspenseCoreWeaponStanceComponent;
class UAnimMontage;

/**
 * Attachment state for replication and animation sync
 */
USTRUCT()
struct FSuspenseCoreAttachmentAnimationState
{
    GENERATED_BODY()

    /** Current animation montage being played */
    UPROPERTY()
    UAnimMontage* CurrentMontage = nullptr;

    /** Animation play rate */
    UPROPERTY()
    float PlayRate = 1.0f;

    /** Is animation playing */
    UPROPERTY()
    bool bIsPlaying = false;

    /** Animation start time */
    UPROPERTY()
    float StartTime = 0.0f;

    FSuspenseCoreAttachmentAnimationState() = default;
};

/**
 * Attachment prediction data for client-side responsiveness
 */
USTRUCT()
struct FSuspenseCoreAttachmentPredictionData
{
    GENERATED_BODY()

    /** Unique prediction key */
    UPROPERTY()
    int32 PredictionKey = 0;

    /** Predicted attachment state */
    UPROPERTY()
    bool bPredictedAttached = false;

    /** Predicted socket name */
    UPROPERTY()
    FName PredictedSocketName = NAME_None;

    /** Predicted transform offset */
    UPROPERTY()
    FTransform PredictedOffset = FTransform::Identity;

    /** Target character for attachment */
    UPROPERTY()
    TWeakObjectPtr<AActor> PredictedCharacter;

    /** Whether attachment is in active state */
    UPROPERTY()
    bool bPredictedActive = false;

    /** Weapon type for animation lookup */
    UPROPERTY()
    FGameplayTag WeaponTypeTag;

    /** Time when prediction was made */
    UPROPERTY()
    float PredictionTime = 0.0f;

    FSuspenseCoreAttachmentPredictionData() = default;
};

/**
 * Socket search result with quality score
 */
USTRUCT()
struct FSuspenseCoreSocketSearchResult
{
    GENERATED_BODY()

    /** Socket name found */
    UPROPERTY()
    FName SocketName = NAME_None;

    /** Quality score (higher is better) */
    UPROPERTY()
    int32 QualityScore = 0;

    /** Whether socket exists on mesh */
    UPROPERTY()
    bool bSocketExists = false;

    FSuspenseCoreSocketSearchResult() = default;

    FSuspenseCoreSocketSearchResult(const FName& InSocketName, int32 InScore, bool bExists)
        : SocketName(InSocketName), QualityScore(InScore), bSocketExists(bExists) {}
};

/**
 * Component that handles equipment attachment to characters with animation support
 *
 * ARCHITECTURE:
 * - Integrates with StanceComponent for weapon pose management
 * - Uses AnimationInterface for accessing animation data
 * - Coordinates attachment state with equipment state machine
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentAttachmentComponent : public USuspenseCoreEquipmentComponentBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentAttachmentComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Initialize component with item instance
     * @param InOwner Owner actor
     * @param InASC Ability system component
     * @param ItemInstance Item instance with runtime data
     */
    virtual void InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance) override;

    /**
     * Clean up attachment and resources
     */
    virtual void Cleanup() override;

    /**
     * Update attachment when item changes
     * @param NewItemInstance New item instance
     */
    virtual void UpdateEquippedItem(const FSuspenseInventoryItemInstance& NewItemInstance) override;

    //==================================================================
    // Core Attachment Methods
    //==================================================================

    /**
     * Attach equipment to character using item data
     * CRITICAL: bUseActiveSocket parameter determines which socket to use (active vs unequipped)
     * @param Character Character to attach to
     * @param bUseActiveSocket True if this is the active weapon (use AttachmentSocket), false for inactive (use UnequippedSocket)
     * @param ComponentToAttach Component to attach (if null, will use spawned actor's component)
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    bool AttachToCharacter(AActor* Character, bool bUseActiveSocket, USceneComponent* ComponentToAttach = nullptr);

    /**
     * Detach equipment from current character
     * @param bMaintainWorldTransform Whether to keep world transform after detach
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    void Detach(bool bMaintainWorldTransform = false);

    /**
     * Update attachment position based on active state with animation
     * Moves equipment between active and inactive sockets with proper transitions
     * @param bMakeActive True to move to active socket, false for inactive
     * @param bAnimated Play transition animation
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    void UpdateAttachmentState(bool bMakeActive, bool bAnimated = true);

    //==================================================================
    // Animation Integration
    //==================================================================

    /**
     * Play attachment transition animation
     * @param bToActive Transitioning to active state
     * @param Duration Animation duration override (0 = use default)
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Animation")
    void PlayAttachmentAnimation(bool bToActive, float Duration = 0.0f);

    /**
     * Get animation interface for current weapon
     * @return Animation interface or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Animation")
    TScriptInterface<ISuspenseWeaponAnimation> GetAnimationInterface() const;

    /**
     * Handle animation completion
     */
    UFUNCTION()
    void OnAttachmentAnimationComplete();

    //==================================================================
    // Stance Integration
    //==================================================================

    /**
     * Link to stance component for coordinated updates
     * @param StanceComponent Stance component to link
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Stance")
    void LinkStanceComponent(USuspenseCoreWeaponStanceComponent* StanceComponent);

    /**
     * Notify stance component of attachment state change
     * @param bAttached New attachment state
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Stance")
    void NotifyStanceOfAttachment(bool bAttached);

    /**
     * Get current weapon type tag
     * @return Weapon type gameplay tag
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Stance")
    FGameplayTag GetWeaponTypeTag() const;

    //==================================================================
    // State Queries
    //==================================================================

    /**
     * Get attachment socket name from item data
     * @param bActive Whether to get active or inactive socket
     * @return Socket name for attachment
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    FName GetAttachmentSocketName(bool bActive = true) const;

    /**
     * Get attachment offset from item data
     * @param bActive Whether to get active or inactive offset
     * @return Transform offset for attachment
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    FTransform GetAttachmentOffset(bool bActive = true) const;

    /**
     * Check if equipment is currently attached
     * @return True if attached to a character
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    bool IsAttached() const { return bIsAttached && AttachTarget != nullptr; }

    /**
     * Check if attached in active state
     * @return True if attached to active socket
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    bool IsAttachedActive() const { return bIsAttached && bIsInActiveState; }

    /**
     * Check if currently transitioning between states
     * @return True if animation is playing
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    bool IsTransitioning() const { return AnimationState.bIsPlaying; }

    /**
     * Get current attach target
     * @return Component we're attached to
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    USceneComponent* GetAttachTarget() const { return AttachTarget; }

    /**
     * Get spawned equipment actor
     * @return Spawned actor or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment")
    AActor* GetEquipmentActor() const { return SpawnedEquipmentActor; }

    //==================================================================
    // Client Prediction Methods
    //==================================================================

    /**
     * Start client prediction for attachment
     * @param Character Character to attach to
     * @param bUseActiveSocket Whether attaching to active socket
     * @param SocketName Predicted socket
     * @param Offset Predicted offset
     * @param WeaponType Weapon type for animations
     * @return Prediction key
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment|Prediction")
    int32 PredictAttachment(AActor* Character, bool bUseActiveSocket, const FName& SocketName,
                           const FTransform& Offset, const FGameplayTag& WeaponType);

    /**
     * Start client prediction for detachment
     * @return Prediction key
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment|Prediction")
    int32 PredictDetachment();

    /**
     * Confirm attachment prediction
     * @param PredictionKey Key from prediction
     * @param bSuccess Whether prediction was correct
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment|Prediction")
    void ConfirmAttachmentPrediction(int32 PredictionKey, bool bSuccess);

    //==================================================================
    // Socket Management
    //==================================================================

    /**
     * Get all valid sockets for item type
     * @param ItemData Item data for socket preferences
     * @param TargetMesh Mesh to search on
     * @return Array of valid sockets with scores
     */
    TArray<FSuspenseCoreSocketSearchResult> GetValidSocketsForItem(const FSuspenseUnifiedItemData& ItemData, USkeletalMeshComponent* TargetMesh) const;

    /**
     * Validate socket exists and is appropriate
     * @param SocketName Socket to validate
     * @param TargetMesh Mesh to check on
     * @return True if socket is valid
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attachment|Sockets")
    bool ValidateSocket(const FName& SocketName, USkeletalMeshComponent* TargetMesh) const;

protected:
    /**
     * Called when equipment is initialized
     */
    virtual void OnEquipmentInitialized() override;

    /**
     * Called when equipped item changes
     */
    virtual void OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem) override;

    /**
     * Spawn equipment actor from item data
     * @param ItemData Item data with actor class
     * @return Spawned actor or nullptr
     */
    AActor* SpawnEquipmentActor(const FSuspenseUnifiedItemData& ItemData);

    /**
     * Destroy spawned equipment actor
     */
    void DestroyEquipmentActor();

    /**
     * Find best socket for attachment based on preferences and availability
     * @param TargetMesh Mesh to search for sockets
     * @param ItemData Item data with socket preferences
     * @param bForActive Whether looking for active or inactive socket
     * @return Best available socket name
     */
    FName FindBestAttachmentSocket(USkeletalMeshComponent* TargetMesh, const FSuspenseUnifiedItemData& ItemData, bool bForActive) const;

    /**
     * Get default socket name based on equipment slot type
     * @param SlotType Equipment slot type tag
     * @param bForActive Whether to get active or inactive default
     * @return Default socket name for slot type
     */
    FName GetDefaultSocketForSlot(const FGameplayTag& SlotType, bool bForActive) const;

    /**
     * Apply attachment rules and offset with physics handling
     * @param ComponentToAttach Component being attached
     * @param TargetComponent Target component
     * @param SocketName Socket to attach to
     * @param AttachmentOffset Additional offset
     */
    void ApplyAttachment(USceneComponent* ComponentToAttach, USceneComponent* TargetComponent,
                        const FName& SocketName, const FTransform& AttachmentOffset);

    /**
     * Broadcast attachment event with context
     * @param bAttached True if attached, false if detached
     * @param Character Character involved
     * @param SocketName Socket used
     */
    void BroadcastAttachmentEvent(bool bAttached, AActor* Character, const FName& SocketName);

    /**
     * Get primary mesh component from character
     * @param Character Character to search
     * @return Primary skeletal mesh or nullptr
     */
    USkeletalMeshComponent* GetCharacterMesh(AActor* Character) const;

    /**
     * Update attachment state for replication
     */
    void UpdateReplicatedAttachmentState();

    /**
     * Apply predicted attachment locally
     * @param Prediction Prediction data to apply
     */
    void ApplyPredictedAttachment(const FSuspenseCoreAttachmentPredictionData& Prediction);

    /**
     * Revert predicted attachment
     * @param Prediction Prediction data to revert
     */
    void RevertPredictedAttachment(const FSuspenseCoreAttachmentPredictionData& Prediction);

    /**
     * Clean up expired predictions
     */
    void CleanupExpiredPredictions();

    /**
     * Update animation state
     * @param DeltaTime Frame time
     */
    void UpdateAnimationState(float DeltaTime);

    /**
     * Get weapon type from item data
     * @return Weapon archetype tag
     */
    FGameplayTag GetWeaponArchetypeFromItem() const;

    //==================================================================
    // Replication Callbacks
    //==================================================================

    UFUNCTION()
    void OnRep_AttachmentState();

    UFUNCTION()
    void OnRep_SpawnedEquipmentActor();

    UFUNCTION()
    void OnRep_AnimationState();

    //==================================================================
    // Network RPCs
    //==================================================================

    /**
     * Server RPC for attachment request with animation support
     * @param Character Character to attach to
     * @param bUseActiveSocket Whether to use active socket
     * @param RequestedSocket Requested socket name
     * @param RequestedOffset Requested offset
     * @param WeaponType Weapon type for animations
     * @param PredictionKey Client prediction key
     */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRequestAttachment(AActor* Character, bool bUseActiveSocket, FName RequestedSocket,
                                FTransform RequestedOffset, FGameplayTag WeaponType, int32 PredictionKey);

    /**
     * Server RPC for detachment request
     * @param bMaintainTransform Whether to maintain world transform
     * @param PredictionKey Client prediction key
     */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRequestDetachment(bool bMaintainTransform, int32 PredictionKey);

    /**
     * Client RPC for attachment confirmation
     * @param PredictionKey Prediction to confirm
     * @param bSuccess Whether attachment succeeded
     * @param ActualSocket Actual socket used
     * @param ActualOffset Actual offset applied
     */
    UFUNCTION(Client, Reliable)
    void ClientConfirmAttachment(int32 PredictionKey, bool bSuccess, FName ActualSocket, FTransform ActualOffset);

    /**
     * Multicast RPC for attachment replication with animation
     * @param Actor Actor to attach
     * @param Parent Parent component
     * @param Socket Socket name
     * @param Offset Attachment offset
     * @param WeaponType Weapon type for animation
     * @param bAnimated Play animation
     */
    UFUNCTION(NetMulticast, Reliable)
    void MulticastAttachment(AActor* Actor, USceneComponent* Parent, FName Socket,
                           FTransform Offset, FGameplayTag WeaponType, bool bAnimated);

    /**
     * Multicast RPC for detachment replication
     * @param Actor Actor to detach
     * @param bMaintainTransform Whether to maintain world transform
     */
    UFUNCTION(NetMulticast, Reliable)
    void MulticastDetachment(AActor* Actor, bool bMaintainTransform);

private:
    //==================================================================
    // Attachment State
    //==================================================================

    /** Currently spawned equipment actor */
    UPROPERTY(ReplicatedUsing=OnRep_SpawnedEquipmentActor)
    AActor* SpawnedEquipmentActor;

    /** Target component for attachment */
    UPROPERTY()
    USceneComponent* AttachTarget;

    /** Attachment state bundle for atomic replication */
    UPROPERTY(ReplicatedUsing=OnRep_AttachmentState)
    bool bIsAttached;

    /** Whether attached to active socket (true) or inactive socket (false) */
    UPROPERTY(ReplicatedUsing=OnRep_AttachmentState)
    bool bIsInActiveState;

    /** Socket name currently used */
    UPROPERTY(ReplicatedUsing=OnRep_AttachmentState)
    FName CurrentSocketName;

    /** Current attachment offset */
    UPROPERTY(ReplicatedUsing=OnRep_AttachmentState)
    FTransform CurrentAttachmentOffset;

    /** Current weapon type for animations */
    UPROPERTY(ReplicatedUsing=OnRep_AttachmentState)
    FGameplayTag CurrentWeaponType;

    /** Cached character reference */
    UPROPERTY()
    TWeakObjectPtr<AActor> AttachedCharacter;

    /** Whether we spawned the equipment actor */
    UPROPERTY(Replicated)
    bool bDidSpawnActor;

    /** Component version for compatibility */
    UPROPERTY(Replicated)
    uint8 AttachmentVersion;

    //==================================================================
    // Animation State
    //==================================================================

    /** Current animation state */
    UPROPERTY(ReplicatedUsing=OnRep_AnimationState)
    FSuspenseCoreAttachmentAnimationState AnimationState;

    /** Timer handle for animation completion */
    FTimerHandle AnimationCompletionTimer;

    /** Cached animation interface */
    mutable TScriptInterface<ISuspenseWeaponAnimation> CachedAnimationInterface;

    /** Last animation interface cache time */
    mutable float LastAnimationInterfaceCacheTime;

    //==================================================================
    // Stance Integration
    //==================================================================

    /** Linked stance component */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreWeaponStanceComponent> LinkedStanceComponent;

    /** Auto-find stance component on begin play */
    UPROPERTY(EditDefaultsOnly, Category = "Equipment|Stance")
    bool bAutoLinkStanceComponent = true;

    //==================================================================
    // Client Prediction State
    //==================================================================

    /** Active attachment predictions specific to this component */
    UPROPERTY()
    TArray<FSuspenseCoreAttachmentPredictionData> AttachmentPredictions;

    /** Next prediction key for attachments */
    UPROPERTY()
    int32 NextAttachmentPredictionKey;

    /** Last confirmed attachment state */
    UPROPERTY()
    FSuspenseCoreAttachmentPredictionData LastConfirmedState;

    //==================================================================
    // Performance Optimization
    //==================================================================

    /** Socket cache for performance */
    mutable TMap<FString, FSuspenseCoreSocketSearchResult> SocketCache;

    /** Last socket cache update time */
    mutable float LastSocketCacheTime;

    /** Socket cache lifetime */
    static constexpr float SocketCacheLifetime = 5.0f;

    /** Animation interface cache lifetime */
    static constexpr float AnimationInterfaceCacheLifetime = 1.0f;

    /** Critical section for thread-safe socket operations */
    mutable FCriticalSection SocketCacheCriticalSection;

    /** Socket preference priority lists */
    static const TArray<FName> WeaponSocketPriority;
    static const TArray<FName> ArmorSocketPriority;
    static const TArray<FName> AccessorySocketPriority;
};

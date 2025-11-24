// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/MedComGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "MedComWeaponToggleAbility.generated.h"

// Forward declarations
class IMedComEquipmentInterface;
class IMedComWeaponAnimationInterface;
class UAnimMontage;
class UEventDelegateManager;
struct FInventoryItemInstance;

/**
 * Ability for toggling weapon state (draw/holster) in the same slot
 * 
 * ARCHITECTURE:
 * - Works exclusively through interfaces to maintain module independence
 * - No direct dependencies on Equipment module components
 * - Handles single slot toggle operations
 * - Integrates with animation subsystem for montages
 */
UCLASS()
class MEDCOMGAS_API UMedComWeaponToggleAbility : public UMedComGameplayAbility
{
    GENERATED_BODY()

public:
    UMedComWeaponToggleAbility();

    //==================================================================
    // Configuration
    //==================================================================
    
    /** Allow toggle during reload */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toggle|Settings")
    bool bAllowToggleDuringReload = false;
    
    /** Allow toggle while aiming */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toggle|Settings")
    bool bAllowToggleWhileAiming = false;
    
    /** Play animations for toggle */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toggle|Animation")
    bool bPlayToggleAnimations = true;
    
    /** Animation playback rate multiplier */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toggle|Animation")
    float AnimationPlayRate = 1.0f;
    
    /** Show debug information */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toggle|Debug")
    bool bShowDebugInfo = false;

protected:
    //==================================================================
    // Ability Overrides
    //==================================================================
    
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    //==================================================================
    // Toggle Logic
    //==================================================================
    
    /**
     * Determine which slot to toggle based on input
     * @param TriggerEventData Event data with slot info
     * @return Slot index to toggle or INDEX_NONE
     */
    int32 DetermineToggleSlot(const FGameplayEventData* TriggerEventData) const;
    
    /**
     * Check if weapon is currently drawn through equipment interface
     * @param EquipmentInterface Equipment interface to query
     * @param SlotIndex Slot to check
     * @return True if weapon is drawn
     */
    bool IsWeaponDrawn(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface, int32 SlotIndex) const;
    
    /**
     * Get current equipment state tag
     * @param EquipmentInterface Equipment interface to query
     * @return Current state tag
     */
    FGameplayTag GetCurrentEquipmentState(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface) const;
    
    /**
     * Set equipment state through interface
     * @param EquipmentInterface Equipment interface
     * @param NewState New state tag
     */
    void SetEquipmentState(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface, const FGameplayTag& NewState);
    
    /**
     * Perform weapon draw operation
     * @param SlotIndex Slot to draw
     */
    void PerformDraw(int32 SlotIndex);
    
    /**
     * Perform weapon holster operation
     * @param SlotIndex Slot to holster
     */
    void PerformHolster(int32 SlotIndex);
    
    /**
     * Play draw animation montage
     * @param WeaponType Weapon type for animation
     * @param bFirstDraw Is this first draw
     */
    void PlayDrawAnimation(const FGameplayTag& WeaponType, bool bFirstDraw);
    
    /**
     * Play holster animation montage
     * @param WeaponType Weapon type for animation
     */
    void PlayHolsterAnimation(const FGameplayTag& WeaponType);

    //==================================================================
    // Animation Callbacks
    //==================================================================
    
    UFUNCTION()
    void OnDrawAnimationComplete();
    
    UFUNCTION()
    void OnHolsterAnimationComplete();
    
    UFUNCTION()
    void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
    
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    //==================================================================
    // Helper Methods
    //==================================================================
    
    /**
     * Find equipment interface on actor or PlayerState
     * @return Equipment interface or nullptr
     */
    TScriptInterface<IMedComEquipmentInterface> FindEquipmentInterface() const;
    
    /**
     * Get animation interface from subsystem
     * @return Animation interface or nullptr
     */
    TScriptInterface<IMedComWeaponAnimationInterface> GetAnimationInterface() const;
    
    /**
     * Get weapon type for slot through equipment interface
     * @param EquipmentInterface Equipment interface
     * @param SlotIndex Slot index
     * @return Weapon type tag
     */
    FGameplayTag GetWeaponTypeForSlot(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface, int32 SlotIndex) const;
    
    /**
     * Apply gameplay tags for toggle state
     * @param bApply Apply or remove tags
     * @param bIsDrawingWeapon Is drawing (vs holstering)
     */
    void ApplyToggleTags(bool bApply, bool bIsDrawingWeapon);
    
    /**
     * Send toggle event through delegate manager
     * @param bStarted Event started or completed
     * @param SlotIndex Affected slot
     * @param bIsDrawingWeapon Drawing or holstering
     */
    void SendToggleEvent(bool bStarted, int32 SlotIndex, bool bIsDrawingWeapon) const;
    
    /**
     * Debug logging helper
     * @param Message Log message
     * @param bError Is error message
     */
    void LogToggleDebug(const FString& Message, bool bError = false) const;

    //==================================================================
    // Networking
    //==================================================================
    
    /**
     * Server RPC for toggle request
     * @param SlotIndex Slot to toggle
     * @param bDraw Draw or holster
     * @param PredictionKey Client prediction key
     */
    UFUNCTION(Server, Reliable)
    void ServerRequestToggle(int32 SlotIndex, bool bDraw, int32 PredictionKey);
    
    /**
     * Client RPC for toggle confirmation
     * @param SlotIndex Toggled slot
     * @param bSuccess Operation success
     * @param PredictionKey Prediction key
     */
    UFUNCTION(Client, Reliable)
    void ClientConfirmToggle(int32 SlotIndex, bool bSuccess, int32 PredictionKey);

private:
    //==================================================================
    // State
    //==================================================================
    
    /** Current toggle slot */
    int32 CurrentToggleSlot;
    
    /** Is currently drawing (vs holstering) */
    bool bIsDrawing;
    
    /** Current weapon type being toggled */
    FGameplayTag CurrentWeaponType;
 
    /** Timer handle for animation timeout */
    FTimerHandle AnimationTimeoutHandle;
    
    /** Cached equipment interface */
    TScriptInterface<IMedComEquipmentInterface> CachedEquipmentInterface;
    
    /** Cached animation interface */
    TScriptInterface<IMedComWeaponAnimationInterface> CachedAnimationInterface;
    
    /** Current prediction key */
    int32 CurrentPredictionKey;
    
    /** Current spec handle */
    FGameplayAbilitySpecHandle CurrentSpecHandle;
    
    //==================================================================
    // Gameplay Tags
    //==================================================================
    
    /** Tag applied while toggling */
    FGameplayTag WeaponTogglingTag;
    
    /** Tag for blocking toggle */
    FGameplayTag ToggleBlockTag;
    
    /** Equipment state tags */
    FGameplayTag EquipmentDrawingTag;
    FGameplayTag EquipmentHolsteringTag;
    FGameplayTag EquipmentReadyTag;
    FGameplayTag EquipmentHolsteredTag;
    
    /** Input tags for slots */
    FGameplayTag InputSlot1Tag;
    FGameplayTag InputSlot2Tag;
    FGameplayTag InputSlot3Tag;
    FGameplayTag InputSlot4Tag;
    FGameplayTag InputSlot5Tag;
    
    //==================================================================
    // Delegates (NOTE: Will be moved to EventDelegateManager later)
    //==================================================================
    
    // TODO: Move these delegates to EventDelegateManager in refactor phase
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnToggleStarted, int32 /*SlotIndex*/, bool /*bIsDrawing*/);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnToggleCompleted, int32 /*SlotIndex*/, bool /*bWasDrawn*/);
    
public:
    /** Called when toggle operation starts */
    FOnToggleStarted OnToggleStarted;
    
    /** Called when toggle operation completes */
    FOnToggleCompleted OnToggleCompleted;
};
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GASAbility.h"
#include "GameplayTagContainer.h"
#include "WeaponSwitchAbility.generated.h"

class USuspenseEventManager;
class IMedComEquipmentInterface;
class IMedComWeaponInterface;
struct FAnimationStateData;

/**
 * Weapon switch modes
 */
UENUM(BlueprintType)
enum class EWeaponSwitchMode : uint8
{
    ToSlotIndex     UMETA(DisplayName = "Switch to Specific Slot"),
    NextWeapon      UMETA(DisplayName = "Switch to Next Weapon"),
    PreviousWeapon  UMETA(DisplayName = "Switch to Previous Weapon"),
    QuickSwitch     UMETA(DisplayName = "Quick Switch to Last Weapon"),
    Invalid         UMETA(DisplayName = "Invalid Mode")
};

/**
 * Weapon switch ability with full animation support
 * Works through interfaces to maintain module separation
 */
UCLASS(meta=(ReplicationPolicy=ReplicateYes))
class GAS_API UWeaponSwitchAbility : public UGASAbility
{
    GENERATED_BODY()

public:
    UWeaponSwitchAbility();

    //==================================================================
    // Configuration
    //==================================================================
    
    /** Duration multiplier for holster animations */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    float HolsterDurationMultiplier = 1.0f;
    
    /** Duration multiplier for draw animations */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    float DrawDurationMultiplier = 1.0f;
    
    /** Allow quick switch during reload */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
    bool bAllowSwitchDuringReload = true;
    
    /** Allow switch while firing */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
    bool bAllowSwitchWhileFiring = false;
    
    /** Skip empty slots when cycling */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
    bool bSkipEmptySlots = true;
    
    /** Play animations for weapon switch */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    bool bPlaySwitchAnimations = true;
    
    /** Show debug information */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
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
    // Weapon Switch Logic
    //==================================================================
    
    /** Determine switch mode from input */
    EWeaponSwitchMode DetermineSwitchMode(const FGameplayEventData* TriggerEventData) const;
    
    /** Get target slot based on switch mode */
    int32 GetTargetSlot(EWeaponSwitchMode Mode, int32 InputSlot = -1) const;
    
    /** Perform the weapon switch */
    void PerformWeaponSwitch(int32 TargetSlot);
    
    /** Play holster animation for current weapon */
    void PlayHolsterAnimation();
    
    /** Play draw animation for new weapon */
    void PlayDrawAnimation();
    
    /** Handle animation completion */
    UFUNCTION()
    void OnHolsterAnimationComplete();
    
    UFUNCTION()
    void OnDrawAnimationComplete();

    //==================================================================
    // Networking
    //==================================================================
    
    /** Server RPC for weapon switch */
    UFUNCTION(Server, Reliable)
    void ServerRequestWeaponSwitch(int32 TargetSlot, int32 PredictionKey);
    
    /** Client RPC for switch confirmation */
    UFUNCTION(Client, Reliable)
    void ClientConfirmWeaponSwitch(int32 NewActiveSlot, bool bSuccess, int32 PredictionKey);

    //==================================================================
    // Helper Methods
    //==================================================================
    
    /** Find equipment interface on owner */
    TScriptInterface<IMedComEquipmentInterface> FindEquipmentInterface() const;
    
    /** Get all equipment slots that contain weapons */
    TArray<int32> GetWeaponSlotIndices() const;
    
    /** Get active weapon slot */
    int32 GetActiveWeaponSlot() const;
    
    /** Get next weapon slot index */
    int32 GetNextWeaponSlot(int32 CurrentSlot) const;
    
    /** Get previous weapon slot index */
    int32 GetPreviousWeaponSlot(int32 CurrentSlot) const;
    
    /** Check if slot contains a weapon */
    bool IsWeaponSlot(int32 SlotIndex) const;
    
    /** Get delegate manager */
    USuspenseEventManager* GetDelegateManager() const;
    
    /** Send switch events */
    void SendWeaponSwitchEvent(bool bStarted, int32 FromSlot, int32 ToSlot) const;
    
    /** Check if we can switch weapons */
    bool CanSwitchWeapons() const;
    
    /** Apply gameplay tags during switch */
    void ApplySwitchTags(bool bApply);
    
    /** Debug logging */
    void LogSwitchDebug(const FString& Message, bool bError = false) const;

private:
    //==================================================================
    // State Tracking
    //==================================================================
    
    /** Current switch mode */
    EWeaponSwitchMode CurrentSwitchMode;
    
    /** Target slot we're switching to */
    int32 TargetSlotIndex;
    
    /** Slot we're switching from */
    int32 SourceSlotIndex;
    
    /** Prediction key for networking */
    int32 CurrentPredictionKey;
    
    /** Timer handles for animations */
    FTimerHandle HolsterTimerHandle;
    FTimerHandle DrawTimerHandle;
    
    /** Animation montages being played */
    UPROPERTY()
    class UAnimMontage* CurrentHolsterMontage;
    
    UPROPERTY()
    class UAnimMontage* CurrentDrawMontage;
    
    /** Cached equipment interface */
    TScriptInterface<IMedComEquipmentInterface> CachedEquipmentInterface;
    
    /** Track last active slot for quick switch */
    mutable int32 LastActiveWeaponSlot;
    /** Current ability spec handle */
    FGameplayAbilitySpecHandle CurrentSpecHandle;
    //==================================================================
    // Gameplay Tags
    //==================================================================
    
    /** Tag applied while switching weapons */
    FGameplayTag WeaponSwitchingTag;
    
    /** Tag for blocking abilities during switch */
    FGameplayTag WeaponSwitchBlockTag;
    
    /** Equipment state tags */
    FGameplayTag EquipmentDrawingTag;
    FGameplayTag EquipmentHolsteringTag;
    FGameplayTag EquipmentSwitchingTag;
    
    /** Input tags for different switch modes */
    FGameplayTag InputNextWeaponTag;
    FGameplayTag InputPrevWeaponTag;
    FGameplayTag InputQuickSwitchTag;
    FGameplayTag InputSlot1Tag;
    FGameplayTag InputSlot2Tag;
    FGameplayTag InputSlot3Tag;
    FGameplayTag InputSlot4Tag;
    FGameplayTag InputSlot5Tag;
};
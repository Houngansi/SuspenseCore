// SuspenseCoreGrenadeThrowAbility.h
// Grenade throw ability with animation montage support
// Copyright Suspense Team. All Rights Reserved.
//
// ANIMATION NOTIFY POINTS:
// - "PinPull": Pin removed, grenade armed (can cancel before this)
// - "Ready": Grenade ready in hand (cooking starts)
// - "Release": Grenade thrown, spawn projectile
//
// ARCHITECTURE:
// - Uses ISuspenseCoreQuickSlotProvider interface (BridgeSystem)
// - Integrates with SuspenseCoreGrenadeHandler for spawning
// - No direct dependency on EquipmentSystem

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "SuspenseCoreGrenadeThrowAbility.generated.h"

// Forward declarations
class ISuspenseCoreQuickSlotProvider;
class UAnimMontage;
class USoundBase;
class UCameraShakeBase;

/**
 * Grenade type enum for different throw behaviors
 */
UENUM(BlueprintType)
enum class ESuspenseCoreGrenadeThrowType : uint8
{
    Overhand    UMETA(DisplayName = "Overhand Throw"),    // Standard long throw
    Underhand   UMETA(DisplayName = "Underhand Throw"),   // Short lob
    Roll        UMETA(DisplayName = "Roll")               // Ground roll
};

/**
 * USuspenseCoreGrenadeThrowAbility
 *
 * GAS ability for throwing grenades with full animation support.
 * Handles the complete throw sequence: prepare → cook (optional) → throw.
 *
 * THROW PHASES:
 * 1. Prepare - Pull pin, can be cancelled (grenade not consumed)
 * 2. Cook - Optional hold to reduce fuse time (risky!)
 * 3. Throw - Release to throw, spawns grenade actor
 *
 * BLOCKING TAGS:
 * - State.Firing (can't throw while firing)
 * - State.Reloading (can't throw while reloading)
 * - State.Dead, State.Stunned, State.Disabled
 *
 * @see SuspenseCoreGrenadeHandler - Spawns actual grenade actor
 * @see ConsumablesThrowables_GDD.md - Design document
 */
UCLASS()
class GAS_API USuspenseCoreGrenadeThrowAbility : public USuspenseCoreAbility
{
    GENERATED_BODY()

public:
    USuspenseCoreGrenadeThrowAbility();

    //==================================================================
    // Configuration - Montages
    //==================================================================

    /** Montage for overhand throw (standard long throw) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Montages")
    TObjectPtr<UAnimMontage> OverhandThrowMontage;

    /** Montage for underhand throw (short lob) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Montages")
    TObjectPtr<UAnimMontage> UnderhandThrowMontage;

    /** Montage for roll throw (ground roll) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Montages")
    TObjectPtr<UAnimMontage> RollThrowMontage;

    //==================================================================
    // Configuration - Timing
    //==================================================================

    /** Time to prepare grenade (pull pin) - can cancel before completion */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Timing")
    float PrepareTime = 0.5f;

    /** Maximum cook time before forced throw (safety) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Timing")
    float MaxCookTime = 5.0f;

    /** Cooldown after throwing before next grenade */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Timing")
    float ThrowCooldown = 1.0f;

    //==================================================================
    // Configuration - Physics
    //==================================================================

    /** Base throw force for overhand throw */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Physics")
    float OverhandThrowForce = 1500.0f;

    /** Base throw force for underhand throw */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Physics")
    float UnderhandThrowForce = 800.0f;

    /** Base throw force for roll */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Physics")
    float RollThrowForce = 500.0f;

    /** Upward angle adjustment for overhand throw (degrees) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Physics")
    float OverhandUpAngle = 15.0f;

    //==================================================================
    // Configuration - Sounds
    //==================================================================

    /** Sound when pin is pulled */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Sounds")
    TObjectPtr<USoundBase> PinPullSound;

    /** Sound when grenade is thrown */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Sounds")
    TObjectPtr<USoundBase> ThrowSound;

    /** Sound when throw is cancelled (pin put back) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Sounds")
    TObjectPtr<USoundBase> CancelSound;

    //==================================================================
    // Configuration - Effects
    //==================================================================

    /** GameplayEffect for movement speed reduction while preparing */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Effects")
    TSubclassOf<UGameplayEffect> PrepareSpeedDebuffClass;

    /** Camera shake when throwing grenade (similar to recoil shake in FireAbility) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Effects")
    TSubclassOf<UCameraShakeBase> ThrowCameraShake;

    /** Camera shake scale multiplier (1.0 = full intensity) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Effects")
    float ThrowCameraShakeScale = 0.5f;

    //==================================================================
    // Runtime Accessors
    //==================================================================

    /** Get current throw type */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
    ESuspenseCoreGrenadeThrowType GetThrowType() const { return CurrentThrowType; }

    /** Check if grenade is being prepared */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
    bool IsPreparing() const { return bIsPreparing; }

    /** Check if grenade is being cooked (pin pulled, holding) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
    bool IsCooking() const { return bIsCooking; }

    /** Get cook time elapsed */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
    float GetCookTime() const;

    /** Set throw type (call before activation or during prepare) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Grenade")
    void SetThrowType(ESuspenseCoreGrenadeThrowType NewType);

    /**
     * Set grenade info from GrenadeEquipAbility (Tarkov-style flow)
     * Called by GrenadeHandler when activating throw from equipped state
     *
     * @param InGrenadeID Item ID of the grenade
     * @param InSlotIndex QuickSlot index the grenade came from
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Grenade")
    void SetGrenadeInfo(FName InGrenadeID, int32 InSlotIndex);

    /** Check if grenade info was set externally (Tarkov-style flow) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
    bool HasGrenadeInfoSet() const { return bGrenadeInfoSet; }

protected:
    //==================================================================
    // GameplayAbility Interface
    //==================================================================

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags
    ) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

    virtual void InputReleased(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo
    ) override;

    //==================================================================
    // Animation Notify Handlers
    //==================================================================

    /** Called when "PinPull" notify fires - pin removed, grenade armed */
    UFUNCTION()
    void OnPinPullNotify();

    /** Called when "Ready" notify fires - grenade ready in hand */
    UFUNCTION()
    void OnReadyNotify();

    /** Called when "Release" notify fires - throw the grenade */
    UFUNCTION()
    void OnReleaseNotify();

    /** Called when montage ends */
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    /** Called when montage blend out starts */
    UFUNCTION()
    void OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

    /** Handles all montage AnimNotify events */
    UFUNCTION()
    void OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    //==================================================================
    // Blueprint Events
    //==================================================================

    /** Called when prepare phase starts */
    UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Grenade")
    void OnPrepareStarted();

    /** Called when pin is pulled (grenade armed) */
    UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Grenade")
    void OnGrenadePinPulled();

    /** Called when grenade is thrown */
    UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Grenade")
    void OnGrenadeThrown();

    /** Called when throw is cancelled */
    UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Grenade")
    void OnThrowCancelled();

private:
    //==================================================================
    // Internal Methods
    //==================================================================

    /** Get QuickSlotProvider interface from owner */
    ISuspenseCoreQuickSlotProvider* GetQuickSlotProvider() const;

    /** Find grenade in QuickSlots */
    bool FindGrenadeInQuickSlots(int32& OutSlotIndex, FName& OutGrenadeID) const;

    /** Get montage for current throw type */
    UAnimMontage* GetMontageForThrowType() const;

    /** Get throw force for current throw type */
    float GetThrowForceForType() const;

    /** Start playing throw montage */
    bool PlayThrowMontage();

    /** Stop throw montage */
    void StopThrowMontage();

    /** Actually spawn and throw the grenade */
    bool ExecuteThrow();

    /** Apply prepare effects (speed debuff) */
    void ApplyPrepareEffects();

    /** Remove prepare effects */
    void RemovePrepareEffects();

    /** Play sound at owner location */
    void PlaySound(USoundBase* Sound);

    /** Broadcast grenade events via EventBus */
    void BroadcastGrenadeEvent(FGameplayTag EventTag);

    /** Consume grenade from QuickSlot */
    void ConsumeGrenade();

    //==================================================================
    // Runtime State
    //==================================================================

    /** Current throw type */
    UPROPERTY()
    ESuspenseCoreGrenadeThrowType CurrentThrowType = ESuspenseCoreGrenadeThrowType::Overhand;

    /** Is in prepare phase */
    UPROPERTY()
    bool bIsPreparing = false;

    /** Is cooking (pin pulled, holding) */
    UPROPERTY()
    bool bIsCooking = false;

    /** Has the pin been pulled */
    UPROPERTY()
    bool bPinPulled = false;

    /** Was grenade info set externally (Tarkov-style flow) */
    UPROPERTY()
    bool bGrenadeInfoSet = false;

    /** Time when cooking started */
    UPROPERTY()
    float CookStartTime = 0.0f;

    /** QuickSlot index of current grenade */
    UPROPERTY()
    int32 CurrentGrenadeSlotIndex = -1;

    /** ItemID of current grenade */
    UPROPERTY()
    FName CurrentGrenadeID;

    /** Cached ActorInfo */
    const FGameplayAbilityActorInfo* CachedActorInfo = nullptr;

    /** Cached ability handle */
    FGameplayAbilitySpecHandle CachedSpecHandle;

    /** Cached activation info */
    FGameplayAbilityActivationInfo CachedActivationInfo;

    /** Handle for speed debuff effect */
    FActiveGameplayEffectHandle PrepareSpeedEffectHandle;

    /** Cached AnimInstance for montage notify binding */
    UPROPERTY()
    TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

    /** Cached QuickSlotProvider */
    mutable TWeakObjectPtr<UObject> CachedQuickSlotProvider;
};

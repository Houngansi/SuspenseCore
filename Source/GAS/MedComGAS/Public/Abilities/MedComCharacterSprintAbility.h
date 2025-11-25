// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "MedComCharacterSprintAbility.generated.h"

// Forward declarations
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitAttributeChangeThreshold;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayTag;
struct FGameplayTagContainer;

/**
 * Sprint Ability for GAS characters
 * 
 * Uses two separate GameplayEffects:
 * - SprintBuffEffectClass for speed increase
 * - SprintCostEffectClass for stamina drain
 */
UCLASS()
class MEDCOMGAS_API UMedComCharacterSprintAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UMedComCharacterSprintAbility();

    // ========================================
    // Sprint Configuration
    // ========================================
    
    /** Effect for speed boost during sprint */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Effects")
    TSubclassOf<UGameplayEffect> SprintBuffEffectClass;
    
    /** Effect for stamina cost during sprint */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Effects")
    TSubclassOf<UGameplayEffect> SprintCostEffectClass;

    /** Speed multiplier while sprinting (for UI display, actual value in effect) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Speed", 
        meta = (ClampMin = "1.1", ClampMax = "3.0"))
    float SprintSpeedMultiplier;

    /** Stamina cost per second (for validation, actual drain in effect) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Stamina", 
        meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float StaminaCostPerSecond;

    /** Minimum stamina required to start sprinting */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Stamina", 
        meta = (ClampMin = "0.0"))
    float MinimumStaminaToSprint;

protected:
    // ========================================
    // Core GameplayAbility Methods
    // ========================================
    
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr
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

    virtual void InputPressed(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo
    ) override;

    virtual void InputReleased(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo
    ) override;

private:
    // ========================================
    // Internal State
    // ========================================
    
    /** Handle for active speed buff effect */
    FActiveGameplayEffectHandle SprintBuffEffectHandle;
    
    /** Handle for active stamina cost effect */
    FActiveGameplayEffectHandle SprintCostEffectHandle;
    
    /** Saved activation parameters for use in callbacks */
    FGameplayAbilitySpecHandle CurrentSpecHandle;
    const FGameplayAbilityActorInfo* CurrentActorInfo;
    FGameplayAbilityActivationInfo CurrentActivationInfo;

    // ========================================
    // Event Handlers
    // ========================================
    
    /** Called when sprint button is released */
    UFUNCTION()
    void OnSprintInputReleased(float TimeHeld);

    /** Called when stamina drops below threshold */
    UFUNCTION()
    void OnStaminaBelowThreshold(bool bMatched, float CurrentValue);
};
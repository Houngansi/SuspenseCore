// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "CharacterCrouchAbility.generated.h"

// Forward declarations
class UAbilityTask_WaitInputRelease;
class UAbilitySystemComponent;
class UGameplayEffect;
class USoundBase;
struct FGameplayTag;
struct FGameplayTagContainer;

/**
 * Crouch Ability for MedCom characters
 * 
 * Работает по модели "нажал-держи-отпустил" как Sprint
 * Использует CrouchDebuffEffectClass для снижения скорости и добавления тега
 */
UCLASS()
class GAS_API UCharacterCrouchAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCharacterCrouchAbility();

    // ========================================
    // Crouch Configuration
    // ========================================
    
    /** Эффект дебаффа для крауча (снижение скорости + тег) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crouch|Effects")
    TSubclassOf<UGameplayEffect> CrouchDebuffEffectClass;

    /** Множитель скорости в крауче (для UI, реальное значение в эффекте) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crouch|Speed", 
        meta = (ClampMin = "0.1", ClampMax = "0.9"))
    float CrouchSpeedMultiplier;

    /** Звук начала крауча */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crouch|Audio")
    USoundBase* CrouchStartSound;

    /** Звук окончания крауча */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crouch|Audio")
    USoundBase* CrouchEndSound;

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
    
    /** Handle активного эффекта дебаффа */
    FActiveGameplayEffectHandle CrouchDebuffEffectHandle;
    
    /** Сохранённые параметры активации для коллбэков */
    FGameplayAbilitySpecHandle CurrentSpecHandle;
    const FGameplayAbilityActorInfo* CurrentActorInfo;
    FGameplayAbilityActivationInfo CurrentActivationInfo;

    // ========================================
    // Event Handlers
    // ========================================
    
    /** Вызывается при отпускании кнопки крауча */
    UFUNCTION()
    void OnCrouchInputReleased(float TimeHeld);
};
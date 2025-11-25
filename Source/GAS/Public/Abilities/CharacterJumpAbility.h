// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "CharacterJumpAbility.generated.h"

// Forward declarations
class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayTag;
struct FGameplayTagContainer;

/**
 * Надежная способность прыжка для GAS персонажей
 * 
 * Особенности реализации:
 * - Не полагается на внешние события для завершения
 * - Использует простую логику состояний
 * - Поддерживает контроль высоты прыжка при отпускании кнопки
 * - Автоматически завершается при приземлении или по таймауту
 */
UCLASS()
class GAS_API UCharacterJumpAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCharacterJumpAbility();

    // ========================================
    // Настройки прыжка
    // ========================================
    
    /** Сила прыжка (множитель для JumpZVelocity персонажа) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Mechanics", 
        meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float JumpPowerMultiplier;

    /** Расход стамины за один прыжок */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Stamina", 
        meta = (ClampMin = "0.0", ClampMax = "50.0"))
    float StaminaCostPerJump;

    /** Минимальная стамина для прыжка */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Stamina", 
        meta = (ClampMin = "0.0"))
    float MinimumStaminaToJump;

    /** Класс эффекта для единоразового расхода стамины */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Effects")
    TSubclassOf<UGameplayEffect> JumpStaminaCostEffectClass;

    /** Максимальное время прыжка до принудительного завершения (защита от зависания) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Safety", 
        meta = (ClampMin = "0.5", ClampMax = "10.0"))
    float MaxJumpDuration;

    /** Интервал проверки приземления */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Safety", 
        meta = (ClampMin = "0.05", ClampMax = "0.5"))
    float GroundCheckInterval;

protected:
    // ========================================
    // Основные методы GameplayAbility
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

    virtual void InputReleased(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo
    ) override;

private:
    // ========================================
    // Вспомогательные методы
    // ========================================
    
    /** Проверяет, находится ли персонаж на земле */
    bool IsCharacterGrounded(const FGameplayAbilityActorInfo* ActorInfo) const;
    
    /** Применяет расход стамины через эффект */
    bool ApplyStaminaCost(const FGameplayAbilityActorInfo* ActorInfo);
    
    /** Выполняет прыжок через интерфейс движения */
    void PerformJump(const FGameplayAbilityActorInfo* ActorInfo);
    
    /** Периодически проверяет, приземлился ли персонаж */
    void CheckForLanding();
    
    /** Принудительно завершает способность по таймауту */
    void ForceEndAbility();
    
    // ========================================
    // Внутреннее состояние
    // ========================================
    
    /** Таймер проверки приземления */
    FTimerHandle LandingCheckTimer;
    
    /** Таймер безопасности для принудительного завершения */
    FTimerHandle SafetyTimer;
    
    /** Флаг для предотвращения двойного завершения */
    bool bIsEnding;
};
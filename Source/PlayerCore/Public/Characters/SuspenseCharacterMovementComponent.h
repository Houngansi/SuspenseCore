// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EMedComMovementMode : uint8
{
    None        UMETA(DisplayName = "None"),
    Walking     UMETA(DisplayName = "Walking"),
    Sprinting   UMETA(DisplayName = "Sprinting"),
    Crouching   UMETA(DisplayName = "Crouching"),
    Jumping     UMETA(DisplayName = "Jumping"),
    Falling     UMETA(DisplayName = "Falling"),
    Sliding     UMETA(DisplayName = "Sliding"),
    Swimming    UMETA(DisplayName = "Swimming"),
    Flying      UMETA(DisplayName = "Flying")
};

/**
 * Кастомный компонент движения для персонажей MedCom
 * Синхронизирует скорость движения с GAS AttributeSet
 * Управляет состояниями движения через теги GameplayAbility System
 */
UCLASS()
class SUSPENSECORE_API USuspenseCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    USuspenseCharacterMovementComponent();

    // BEGIN UActorComponent Interface
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    // END UActorComponent Interface

    // BEGIN UCharacterMovementComponent Interface
    virtual bool DoJump(bool bReplayingMoves) override;
    virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
    virtual void Crouch(bool bClientSimulation = false) override;
    virtual void UnCrouch(bool bClientSimulation = false) override;
    // END UCharacterMovementComponent Interface

    /** КРИТИЧНО: Синхронизирует скорость движения с AttributeSet - единственный способ обновления скорости */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Movement")
    void SyncMovementSpeedFromAttributes();

    /** Получить текущий режим движения */
    UFUNCTION(BlueprintPure, Category = "MedCom|Movement")
    EMedComMovementMode GetCurrentMovementMode() const;

    // Состояния движения - только для чтения, управляются через теги GAS
    // Используем уникальные имена чтобы избежать конфликтов с родительским классом
    
    /** Проверить, находится ли персонаж в состоянии спринта (синхронизировано с GAS) */
    UFUNCTION(BlueprintPure, Category = "MedCom|Movement", DisplayName = "Is Sprinting (GAS)")
    bool IsSprintingFromGAS() const { return bIsSprintingGAS; }
    
    /** Проверить, находится ли персонаж в состоянии приседания (синхронизировано с GAS) */
    UFUNCTION(BlueprintPure, Category = "MedCom|Movement", DisplayName = "Is Crouching (GAS)")
    bool IsCrouchingFromGAS() const { return bIsCrouchingGAS; }
    
    /** Проверить, находится ли персонаж в прыжке */
    UFUNCTION(BlueprintPure, Category = "MedCom|Movement")
    bool IsJumping() const { return bIsJumping; }
    
    /** Проверить, находится ли персонаж в воздухе */
    UFUNCTION(BlueprintPure, Category = "MedCom|Movement")
    bool IsInAir() const { return IsFalling(); }
    
    /** Проверить, находится ли персонаж в слайдинге */
    UFUNCTION(BlueprintPure, Category = "MedCom|Movement")
    bool IsSliding() const { return bIsSliding; }

    // Методы для обратной совместимости - используют GAS-синхронизированные значения
    bool IsSprinting() const { return bIsSprintingGAS; }
    bool IsCrouching() const override { return bIsCrouchingGAS; }

    // Sliding mechanics
    void StartSliding();
    void StopSliding();
    bool CanSlide() const;

protected:
    /** Обновляет внутренние флаги состояния на основе тегов GAS */
    void UpdateMovementStateFromTags();
    
    /** Получить ASC владельца (поддерживает ASC на Character или PlayerState) */
    class UAbilitySystemComponent* GetOwnerASC() const;
    
    /** Получить AttributeSet владельца */
    const class UMedComBaseAttributeSet* GetOwnerAttributeSet() const;

    // Внутренние флаги состояния - синхронизируются с тегами GAS
    // Переименованы для ясности что это GAS-синхронизированные значения
    bool bIsSprintingGAS;
    bool bIsCrouchingGAS;
    bool bIsJumping;
    bool bIsSliding;

    // Параметры слайдинга
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Movement|Sliding")
    float MinSlideSpeed = 400.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Movement|Sliding")
    float SlideSpeed = 600.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Movement|Sliding")
    float SlideFriction = 0.1f;
    
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Movement|Sliding")
    float SlideDuration = 1.5f;

private:
    /** Кэшированные теги для оптимизации */
    FGameplayTag SprintingTag;
    FGameplayTag CrouchingTag;
    
    /** Переменные для слайдинга */
    float SlideTimer;
    FVector SlideStartVelocity;
    
    /** Обновление логики слайдинга */
    void UpdateSliding(float DeltaTime);

    /** Счетчик для ограничения частоты логов */
    int32 SyncLogCounter;
};
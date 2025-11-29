// SuspenseCoreMovementAttributeSet.h
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreMovementAttributeSet.generated.h"

class USuspenseCoreAbilitySystemComponent;

// Стандартные макросы GAS для доступа к атрибутам
#define SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USuspenseCoreMovementAttributeSet
 *
 * Детальные атрибуты движения для тактических FPS.
 * Позволяет модифицировать скорости через GameplayEffects.
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 *
 * Особенности:
 * - Раздельные скорости для разных состояний
 * - Множители для временных эффектов (баффы/дебаффы)
 * - Интеграция с CharacterMovementComponent
 * - EventBus уведомления об изменениях
 */
UCLASS()
class GAS_API USuspenseCoreMovementAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USuspenseCoreMovementAttributeSet();

	// ═══════════════════════════════════════════════════════════════════════════
	// BASE MOVEMENT SPEEDS (units per second)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Базовая скорость ходьбы */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_WalkSpeed)
	FGameplayAttributeData WalkSpeed;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, WalkSpeed)

	/** Скорость спринта */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_SprintSpeed)
	FGameplayAttributeData SprintSpeed;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, SprintSpeed)

	/** Скорость в присяде */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_CrouchSpeed)
	FGameplayAttributeData CrouchSpeed;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, CrouchSpeed)

	/** Скорость ползком (prone) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_ProneSpeed)
	FGameplayAttributeData ProneSpeed;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, ProneSpeed)

	/** Скорость в прицеле (ADS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_AimSpeed)
	FGameplayAttributeData AimSpeed;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, AimSpeed)

	/** Скорость движения назад (множитель от базовой) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_BackwardSpeedMultiplier)
	FGameplayAttributeData BackwardSpeedMultiplier;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, BackwardSpeedMultiplier)

	/** Скорость движения боком (множитель от базовой) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Speed", ReplicatedUsing = OnRep_StrafeSpeedMultiplier)
	FGameplayAttributeData StrafeSpeedMultiplier;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, StrafeSpeedMultiplier)

	// ═══════════════════════════════════════════════════════════════════════════
	// JUMP ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Высота прыжка */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Jump", ReplicatedUsing = OnRep_JumpHeight)
	FGameplayAttributeData JumpHeight;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, JumpHeight)

	/** Максимальное количество прыжков (1 = обычный, 2+ = double jump) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Jump", ReplicatedUsing = OnRep_MaxJumpCount)
	FGameplayAttributeData MaxJumpCount;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, MaxJumpCount)

	/** Управление в воздухе (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Jump", ReplicatedUsing = OnRep_AirControl)
	FGameplayAttributeData AirControl;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, AirControl)

	// ═══════════════════════════════════════════════════════════════════════════
	// ROTATION ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Скорость поворота (градусов/сек) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Rotation", ReplicatedUsing = OnRep_TurnRate)
	FGameplayAttributeData TurnRate;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, TurnRate)

	/** Скорость поворота в прицеле (множитель) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Rotation", ReplicatedUsing = OnRep_AimTurnRateMultiplier)
	FGameplayAttributeData AimTurnRateMultiplier;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, AimTurnRateMultiplier)

	// ═══════════════════════════════════════════════════════════════════════════
	// ACCELERATION ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Ускорение на земле */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Acceleration", ReplicatedUsing = OnRep_GroundAcceleration)
	FGameplayAttributeData GroundAcceleration;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, GroundAcceleration)

	/** Торможение на земле */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Acceleration", ReplicatedUsing = OnRep_GroundDeceleration)
	FGameplayAttributeData GroundDeceleration;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, GroundDeceleration)

	/** Ускорение в воздухе */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Acceleration", ReplicatedUsing = OnRep_AirAcceleration)
	FGameplayAttributeData AirAcceleration;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, AirAcceleration)

	// ═══════════════════════════════════════════════════════════════════════════
	// WEIGHT/ENCUMBRANCE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Текущий переносимый вес */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Weight", ReplicatedUsing = OnRep_CurrentWeight)
	FGameplayAttributeData CurrentWeight;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, CurrentWeight)

	/** Максимальный переносимый вес */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Weight", ReplicatedUsing = OnRep_MaxWeight)
	FGameplayAttributeData MaxWeight;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, MaxWeight)

	/** Штраф к скорости за вес (0-1, автоматически вычисляется) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Weight")
	FGameplayAttributeData WeightSpeedPenalty;
	SUSPENSECORE_MOVEMENT_ATTRIBUTE_ACCESSORS(USuspenseCoreMovementAttributeSet, WeightSpeedPenalty)

	// ═══════════════════════════════════════════════════════════════════════════
	// UAttributeSet Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Получить владельца */
	AActor* GetOwningActor() const;

	/** Получить ASC */
	USuspenseCoreAbilitySystemComponent* GetSuspenseCoreASC() const;

	/** Получить эффективную скорость с учётом веса */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	float GetEffectiveWalkSpeed() const;

	/** Получить эффективную скорость спринта с учётом веса */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	float GetEffectiveSprintSpeed() const;

	/** Перегружен ли персонаж? */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	bool IsOverencumbered() const;

	/** Процент загруженности (0-1+) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	float GetEncumbrancePercent() const;

	/** Обновить CharacterMovement скорости */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void ApplySpeedsToCharacter();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// REPLICATION HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	virtual void OnRep_WalkSpeed(const FGameplayAttributeData& OldWalkSpeed);

	UFUNCTION()
	virtual void OnRep_SprintSpeed(const FGameplayAttributeData& OldSprintSpeed);

	UFUNCTION()
	virtual void OnRep_CrouchSpeed(const FGameplayAttributeData& OldCrouchSpeed);

	UFUNCTION()
	virtual void OnRep_ProneSpeed(const FGameplayAttributeData& OldProneSpeed);

	UFUNCTION()
	virtual void OnRep_AimSpeed(const FGameplayAttributeData& OldAimSpeed);

	UFUNCTION()
	virtual void OnRep_BackwardSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_StrafeSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_JumpHeight(const FGameplayAttributeData& OldJumpHeight);

	UFUNCTION()
	virtual void OnRep_MaxJumpCount(const FGameplayAttributeData& OldMaxJumpCount);

	UFUNCTION()
	virtual void OnRep_AirControl(const FGameplayAttributeData& OldAirControl);

	UFUNCTION()
	virtual void OnRep_TurnRate(const FGameplayAttributeData& OldTurnRate);

	UFUNCTION()
	virtual void OnRep_AimTurnRateMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_GroundAcceleration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_GroundDeceleration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_AirAcceleration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_CurrentWeight(const FGameplayAttributeData& OldCurrentWeight);

	UFUNCTION()
	virtual void OnRep_MaxWeight(const FGameplayAttributeData& OldMaxWeight);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Пересчитать штраф от веса */
	void RecalculateWeightPenalty();

	/** Clamp значение атрибута */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value);

	/** Опубликовать изменение скорости */
	void BroadcastSpeedChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue);
};

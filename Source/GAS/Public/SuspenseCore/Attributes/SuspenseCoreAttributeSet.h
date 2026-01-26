// SuspenseCoreAttributeSet.h
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAttributeSet.generated.h"

class USuspenseCoreAbilitySystemComponent;
class USuspenseCoreEventBus;

// Стандартные макросы GAS для доступа к атрибутам
#define SUSPENSECORE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USuspenseCoreAttributeSet
 *
 * Базовый набор атрибутов для SuspenseCore.
 * Автоматически публикует изменения атрибутов через EventBus.
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 * Legacy: UGASAttributeSet (не трогаем)
 *
 * Особенности:
 * - Все атрибуты реплицируются
 * - Изменения публикуются в EventBus
 * - Clamping значений
 * - Критические события (смерть, низкое HP)
 */
UCLASS()
class GAS_API USuspenseCoreAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USuspenseCoreAttributeSet();

	// ═══════════════════════════════════════════════════════════════════════════
	// HEALTH ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Текущее здоровье */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, Health)

	/** Максимальное здоровье */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxHealth)

	/** Регенерация здоровья в секунду */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Health", ReplicatedUsing = OnRep_HealthRegen)
	FGameplayAttributeData HealthRegen;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, HealthRegen)

	// ═══════════════════════════════════════════════════════════════════════════
	// STAMINA ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Текущая стамина */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Stamina", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, Stamina)

	/** Максимальная стамина */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MaxStamina)

	/** Регенерация стамины в секунду */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Stamina", ReplicatedUsing = OnRep_StaminaRegen)
	FGameplayAttributeData StaminaRegen;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, StaminaRegen)

	// ═══════════════════════════════════════════════════════════════════════════
	// COMBAT ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Броня (снижение урона) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Combat", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, Armor)

	/** Сила атаки (множитель урона) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Combat", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, AttackPower)

	// ═══════════════════════════════════════════════════════════════════════════
	// MOVEMENT ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Скорость передвижения (множитель) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement", ReplicatedUsing = OnRep_MovementSpeed)
	FGameplayAttributeData MovementSpeed;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, MovementSpeed)

	// ═══════════════════════════════════════════════════════════════════════════
	// META ATTRIBUTES (не реплицируются, для расчётов)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Входящий урон (мета-атрибут для расчёта) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Meta")
	FGameplayAttributeData IncomingDamage;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, IncomingDamage)

	/** Входящее лечение (мета-атрибут для расчёта) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Meta")
	FGameplayAttributeData IncomingHealing;
	SUSPENSECORE_ATTRIBUTE_ACCESSORS(USuspenseCoreAttributeSet, IncomingHealing)

	// ═══════════════════════════════════════════════════════════════════════════
	// UAttributeSet Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Получить владельца (Actor).
	 */
	AActor* GetOwningActor() const;

	/**
	 * Получить AbilitySystemComponent как SuspenseCore версию.
	 */
	USuspenseCoreAbilitySystemComponent* GetSuspenseCoreASC() const;

	/**
	 * Проверить жив ли владелец.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	bool IsAlive() const;

	/**
	 * Получить процент здоровья (0-1).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetHealthPercent() const;

	/**
	 * Получить процент стамины (0-1).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	float GetStaminaPercent() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// REPLICATION HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	virtual void OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen);

	UFUNCTION()
	virtual void OnRep_Armor(const FGameplayAttributeData& OldArmor);

	UFUNCTION()
	virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Публикует изменение атрибута в EventBus.
	 */
	void BroadcastAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue);

	/**
	 * Обработка смерти.
	 * Публикует событие смерти и вызывает Die() на персонаже.
	 */
	virtual void HandleDeath(AActor* DamageInstigator, AActor* DamageCauser);

	/**
	 * Обработка низкого здоровья.
	 */
	virtual void HandleLowHealth();

	/** Флаг предотвращения двойного вызова смерти */
	bool bIsDead = false;

	/**
	 * Clamp значение атрибута.
	 */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value);

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base walk speed for movement calculations (used with MovementSpeed multiplier) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	float BaseWalkSpeed = 600.0f;

private:
	/** Порог низкого здоровья (процент) */
	static constexpr float LowHealthThreshold = 0.25f;

	/** Было ли опубликовано событие низкого здоровья */
	bool bLowHealthEventPublished = false;

	/** Cached stamina value before PreAttributeChange for accurate broadcast in PostGameplayEffectExecute */
	float CachedPreChangeStamina = 0.0f;
};

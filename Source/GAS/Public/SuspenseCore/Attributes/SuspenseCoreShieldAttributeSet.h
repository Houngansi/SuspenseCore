// SuspenseCoreShieldAttributeSet.h
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreShieldAttributeSet.generated.h"

class USuspenseCoreAbilitySystemComponent;
class USuspenseCoreEventBus;

// Стандартные макросы GAS для доступа к атрибутам
#define SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USuspenseCoreShieldAttributeSet
 *
 * Система щитов для современных FPS (Halo, Apex, Overwatch style).
 * Автоматически публикует изменения через EventBus.
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 *
 * Особенности:
 * - Щит поглощает урон до здоровья
 * - Регенерация после задержки
 * - Разные типы щитов (обычный, перезаряжаемый, разрушаемый)
 * - Интеграция с EventBus для UI уведомлений
 */
UCLASS()
class GAS_API USuspenseCoreShieldAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USuspenseCoreShieldAttributeSet();

	// ═══════════════════════════════════════════════════════════════════════════
	// SHIELD ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Текущий щит */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, Shield)

	/** Максимальный щит */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield", ReplicatedUsing = OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, MaxShield)

	/** Регенерация щита в секунду */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield", ReplicatedUsing = OnRep_ShieldRegen)
	FGameplayAttributeData ShieldRegen;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, ShieldRegen)

	/** Задержка перед началом регенерации (секунды) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield", ReplicatedUsing = OnRep_ShieldRegenDelay)
	FGameplayAttributeData ShieldRegenDelay;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, ShieldRegenDelay)

	// ═══════════════════════════════════════════════════════════════════════════
	// SHIELD BREAK MECHANICS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Кулдаун после полного разрушения щита (секунды) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield|Break", ReplicatedUsing = OnRep_ShieldBreakCooldown)
	FGameplayAttributeData ShieldBreakCooldown;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, ShieldBreakCooldown)

	/** Процент щита при восстановлении после разрушения (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield|Break", ReplicatedUsing = OnRep_ShieldBreakRecoveryPercent)
	FGameplayAttributeData ShieldBreakRecoveryPercent;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, ShieldBreakRecoveryPercent)

	// ═══════════════════════════════════════════════════════════════════════════
	// SHIELD DAMAGE MODIFIERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Снижение урона щитом (0-1, где 1 = 100% поглощение) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield|Damage", ReplicatedUsing = OnRep_ShieldDamageReduction)
	FGameplayAttributeData ShieldDamageReduction;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, ShieldDamageReduction)

	/** Overflow урон в здоровье (0-1, сколько урона пробивает при разрушении) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield|Damage", ReplicatedUsing = OnRep_ShieldOverflowDamage)
	FGameplayAttributeData ShieldOverflowDamage;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, ShieldOverflowDamage)

	// ═══════════════════════════════════════════════════════════════════════════
	// META ATTRIBUTES (для расчётов, не реплицируются)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Входящий урон по щиту (мета-атрибут) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield|Meta")
	FGameplayAttributeData IncomingShieldDamage;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, IncomingShieldDamage)

	/** Входящее восстановление щита (мета-атрибут) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Shield|Meta")
	FGameplayAttributeData IncomingShieldHealing;
	SUSPENSECORE_SHIELD_ATTRIBUTE_ACCESSORS(USuspenseCoreShieldAttributeSet, IncomingShieldHealing)

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

	/** Щит активен? */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Shield")
	bool HasShield() const;

	/** Получить процент щита (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Shield")
	float GetShieldPercent() const;

	/** Щит полностью разрушен? */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Shield")
	bool IsShieldBroken() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// REPLICATION HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);

	UFUNCTION()
	virtual void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);

	UFUNCTION()
	virtual void OnRep_ShieldRegen(const FGameplayAttributeData& OldShieldRegen);

	UFUNCTION()
	virtual void OnRep_ShieldRegenDelay(const FGameplayAttributeData& OldShieldRegenDelay);

	UFUNCTION()
	virtual void OnRep_ShieldBreakCooldown(const FGameplayAttributeData& OldShieldBreakCooldown);

	UFUNCTION()
	virtual void OnRep_ShieldBreakRecoveryPercent(const FGameplayAttributeData& OldShieldBreakRecoveryPercent);

	UFUNCTION()
	virtual void OnRep_ShieldDamageReduction(const FGameplayAttributeData& OldShieldDamageReduction);

	UFUNCTION()
	virtual void OnRep_ShieldOverflowDamage(const FGameplayAttributeData& OldShieldOverflowDamage);

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Публикует изменение щита в EventBus */
	void BroadcastShieldChange(float OldValue, float NewValue);

	/** Обработка разрушения щита */
	virtual void HandleShieldBroken(AActor* DamageInstigator, AActor* DamageCauser);

	/** Обработка низкого щита */
	virtual void HandleLowShield();

	/** Clamp значение атрибута */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value);

private:
	/** Порог низкого щита (процент) */
	static constexpr float LowShieldThreshold = 0.25f;

	/** Было ли опубликовано событие низкого щита */
	bool bLowShieldEventPublished = false;

	/** Флаг разрушенного щита */
	bool bShieldBroken = false;
};

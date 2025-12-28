// SuspenseCoreAbilitySystemComponent.h
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAbilitySystemComponent.generated.h"

class USuspenseCoreEventBus;

/**
 * USuspenseCoreAbilitySystemComponent
 *
 * Новый AbilitySystemComponent с интеграцией EventBus.
 * Все изменения атрибутов публикуются через EventBus.
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 * Legacy: UGASAbilitySystemComponent (не трогаем)
 *
 * Особенности:
 * - Автоматическая публикация событий при изменении атрибутов
 * - Поддержка GameplayTags для идентификации событий
 * - Интеграция с SuspenseCoreEventManager
 */
UCLASS(ClassGroup = "SuspenseCore", meta = (BlueprintSpawnableComponent))
class GAS_API USuspenseCoreAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreAbilitySystemComponent();

	// ═══════════════════════════════════════════════════════════════════════════
	// UAbilitySystemComponent Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Батчинг RPC для сетевой оптимизации
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS INTEGRATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Включить/выключить публикацию событий атрибутов.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	void SetAttributeEventsEnabled(bool bEnabled);

	/**
	 * Публиковать событие при изменении атрибута.
	 * Вызывается автоматически из AttributeSet.
	 */
	void PublishAttributeChangeEvent(
		const FGameplayAttribute& Attribute,
		float OldValue,
		float NewValue
	);

	/**
	 * Публиковать событие при критическом изменении (смерть, низкое здоровье).
	 */
	void PublishCriticalEvent(FGameplayTag EventTag, float CurrentValue, float MaxValue);

	/**
	 * Получить EventBus (кэшируется).
	 */
	USuspenseCoreEventBus* GetEventBus() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// ABILITY HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Выдать способность по классу.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	FGameplayAbilitySpecHandle GiveAbilityOfClass(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level = 1, int32 InputID = -1);

	/**
	 * Забрать все способности определённого класса.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	void RemoveAbilitiesOfClass(TSubclassOf<UGameplayAbility> AbilityClass);

	/**
	 * Проверить есть ли способность по тегу.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	bool HasAbilityWithTag(FGameplayTag AbilityTag) const;

	/**
	 * Попробовать активировать способность по тегу.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag, bool bAllowRemoteActivation = true);

	// ═══════════════════════════════════════════════════════════════════════════
	// EFFECT HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Применить GameplayEffect к себе.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	FActiveGameplayEffectHandle ApplyEffectToSelf(
		TSubclassOf<UGameplayEffect> EffectClass,
		float Level = 1.0f
	);

	/**
	 * Применить GameplayEffect с контекстом.
	 */
	FActiveGameplayEffectHandle ApplyEffectToSelfWithContext(
		TSubclassOf<UGameplayEffect> EffectClass,
		const FGameplayEffectContextHandle& Context,
		float Level = 1.0f
	);

	/**
	 * Удалить все активные эффекты определённого класса.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	void RemoveActiveEffectsOfClass(TSubclassOf<UGameplayEffect> EffectClass);

	// ═══════════════════════════════════════════════════════════════════════════
	// STAMINA REGENERATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Enable/disable stamina regeneration timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	void SetStaminaRegenEnabled(bool bEnabled);

	/**
	 * Check if stamina regeneration is enabled.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|GAS")
	bool IsStaminaRegenEnabled() const { return bStaminaRegenEnabled; }

protected:
	/** Публиковать ли события изменения атрибутов */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Events")
	bool bPublishAttributeEvents = true;

	/** Кэшированный EventBus */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	// ─────────────────────────────────────────────────────────────────────────
	// Stamina Regen Timer
	// ─────────────────────────────────────────────────────────────────────────

	/** Timer handle for stamina regeneration */
	FTimerHandle StaminaRegenTimerHandle;

	/** Is stamina regen enabled */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Stamina")
	bool bStaminaRegenEnabled = true;

	/** Stamina regen tick rate (10 ticks/sec = 0.1s period) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Stamina")
	float StaminaRegenTickRate = 0.1f;

	/** Tags that block stamina regeneration */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Stamina")
	FGameplayTagContainer StaminaRegenBlockTags;

	/** Timer callback for stamina regeneration */
	void OnStaminaRegenTick();

	/** Start stamina regen timer */
	void StartStaminaRegenTimer();

	/** Stop stamina regen timer */
	void StopStaminaRegenTimer();

	/**
	 * Инициализировать подписки на EventBus.
	 */
	virtual void SetupEventBusSubscriptions();

	/**
	 * Отписаться от EventBus.
	 */
	virtual void TeardownEventBusSubscriptions();
};

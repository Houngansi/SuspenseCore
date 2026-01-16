// SuspenseCoreProgressionAttributeSet.h
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreProgressionAttributeSet.generated.h"

class USuspenseCoreAbilitySystemComponent;

// Стандартные макросы GAS для доступа к атрибутам
#define SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USuspenseCoreProgressionAttributeSet
 *
 * Атрибуты прогрессии и MMO механик.
 * Уровень, опыт, репутация, валюта.
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 *
 * Особенности:
 * - Система уровней с гибкой кривой опыта
 * - Репутация для фракций/торговцев
 * - Две валюты (soft/hard currency)
 * - EventBus уведомления о level up
 */
UCLASS()
class GAS_API USuspenseCoreProgressionAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USuspenseCoreProgressionAttributeSet();

	// ═══════════════════════════════════════════════════════════════════════════
	// LEVEL & EXPERIENCE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Текущий уровень игрока */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Level", ReplicatedUsing = OnRep_Level)
	FGameplayAttributeData Level;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, Level)

	/** Максимальный уровень */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Level", ReplicatedUsing = OnRep_MaxLevel)
	FGameplayAttributeData MaxLevel;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, MaxLevel)

	/** Текущий опыт */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Experience", ReplicatedUsing = OnRep_Experience)
	FGameplayAttributeData Experience;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, Experience)

	/** Опыт для следующего уровня */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Experience", ReplicatedUsing = OnRep_ExperienceToNextLevel)
	FGameplayAttributeData ExperienceToNextLevel;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, ExperienceToNextLevel)

	/** Множитель получаемого опыта (бонусы, баффы) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Experience", ReplicatedUsing = OnRep_ExperienceMultiplier)
	FGameplayAttributeData ExperienceMultiplier;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, ExperienceMultiplier)

	// ═══════════════════════════════════════════════════════════════════════════
	// META ATTRIBUTES - Experience Gain
	// ═══════════════════════════════════════════════════════════════════════════

	/** Входящий опыт (мета-атрибут для расчёта) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Meta")
	FGameplayAttributeData IncomingExperience;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, IncomingExperience)

	// ═══════════════════════════════════════════════════════════════════════════
	// REPUTATION (для фракций/торговцев)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Общая репутация (0-100) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Reputation", ReplicatedUsing = OnRep_Reputation)
	FGameplayAttributeData Reputation;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, Reputation)

	/** Множитель влияния на репутацию */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Reputation", ReplicatedUsing = OnRep_ReputationMultiplier)
	FGameplayAttributeData ReputationMultiplier;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, ReputationMultiplier)

	// ═══════════════════════════════════════════════════════════════════════════
	// CURRENCY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Мягкая валюта (gold, credits, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Currency", ReplicatedUsing = OnRep_SoftCurrency)
	FGameplayAttributeData SoftCurrency;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, SoftCurrency)

	/** Твёрдая валюта (premium currency) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Currency", ReplicatedUsing = OnRep_HardCurrency)
	FGameplayAttributeData HardCurrency;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, HardCurrency)

	// ═══════════════════════════════════════════════════════════════════════════
	// SKILL POINTS (для прокачки)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Доступные очки навыков */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Skills", ReplicatedUsing = OnRep_SkillPoints)
	FGameplayAttributeData SkillPoints;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, SkillPoints)

	/** Очки атрибутов (для распределения в stats) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Skills", ReplicatedUsing = OnRep_AttributePoints)
	FGameplayAttributeData AttributePoints;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, AttributePoints)

	// ═══════════════════════════════════════════════════════════════════════════
	// PRESTIGE / SEASONS (для длительной игры)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Уровень престижа (после max level) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Prestige", ReplicatedUsing = OnRep_PrestigeLevel)
	FGameplayAttributeData PrestigeLevel;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, PrestigeLevel)

	/** Сезонный ранг */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Season", ReplicatedUsing = OnRep_SeasonRank)
	FGameplayAttributeData SeasonRank;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, SeasonRank)

	/** Сезонный опыт */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Progression|Season", ReplicatedUsing = OnRep_SeasonExperience)
	FGameplayAttributeData SeasonExperience;
	SUSPENSECORE_PROGRESSION_ATTRIBUTE_ACCESSORS(USuspenseCoreProgressionAttributeSet, SeasonExperience)

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

	/** Получить процент прогресса до следующего уровня (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	float GetLevelProgressPercent() const;

	/** Достигнут ли максимальный уровень? */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	bool IsMaxLevel() const;

	/** Добавить опыт с учётом множителя */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	void AddExperience(float Amount);

	/** Добавить soft currency */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	bool AddSoftCurrency(float Amount);

	/** Потратить soft currency */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	bool SpendSoftCurrency(float Amount);

	/** Достаточно ли soft currency? */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	bool CanAffordSoftCurrency(float Amount) const;

	/** Вычислить опыт для уровня (переопределяемая формула) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Progression")
	virtual float CalculateExperienceForLevel(int32 TargetLevel) const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// REPLICATION HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	virtual void OnRep_Level(const FGameplayAttributeData& OldLevel);

	UFUNCTION()
	virtual void OnRep_MaxLevel(const FGameplayAttributeData& OldMaxLevel);

	UFUNCTION()
	virtual void OnRep_Experience(const FGameplayAttributeData& OldExperience);

	UFUNCTION()
	virtual void OnRep_ExperienceToNextLevel(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_ExperienceMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_Reputation(const FGameplayAttributeData& OldReputation);

	UFUNCTION()
	virtual void OnRep_ReputationMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_SoftCurrency(const FGameplayAttributeData& OldSoftCurrency);

	UFUNCTION()
	virtual void OnRep_HardCurrency(const FGameplayAttributeData& OldHardCurrency);

	UFUNCTION()
	virtual void OnRep_SkillPoints(const FGameplayAttributeData& OldSkillPoints);

	UFUNCTION()
	virtual void OnRep_AttributePoints(const FGameplayAttributeData& OldAttributePoints);

	UFUNCTION()
	virtual void OnRep_PrestigeLevel(const FGameplayAttributeData& OldPrestigeLevel);

	UFUNCTION()
	virtual void OnRep_SeasonRank(const FGameplayAttributeData& OldSeasonRank);

	UFUNCTION()
	virtual void OnRep_SeasonExperience(const FGameplayAttributeData& OldSeasonExperience);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Обработка level up */
	virtual void HandleLevelUp(int32 OldLevel, int32 NewLevel);

	/** Обновить ExperienceToNextLevel */
	void UpdateExperienceToNextLevel();

	/** Clamp значение атрибута */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value);

	/** Опубликовать событие прогрессии с указанным атрибутом */
	void BroadcastProgressionEvent(FGameplayTag EventTag, const FGameplayAttribute& Attribute, float OldValue, float NewValue);

private:
	/** Базовый опыт для уровня 2 */
	static constexpr float BaseExperienceForLevel2 = 100.0f;

	/** Множитель роста опыта за уровень */
	static constexpr float ExperienceGrowthRate = 1.15f;

	/** Skill points за уровень */
	static constexpr float SkillPointsPerLevel = 1.0f;

	/** Attribute points за уровень */
	static constexpr float AttributePointsPerLevel = 3.0f;
};

// SuspenseCoreProgressionAttributeSet.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreProgressionAttributeSet.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

USuspenseCoreProgressionAttributeSet::USuspenseCoreProgressionAttributeSet()
{
	// Level & Experience
	InitLevel(1.0f);
	InitMaxLevel(100.0f);
	InitExperience(0.0f);
	InitExperienceToNextLevel(BaseExperienceForLevel2);
	InitExperienceMultiplier(1.0f);
	InitIncomingExperience(0.0f);

	// Reputation
	InitReputation(50.0f); // Нейтральная репутация
	InitReputationMultiplier(1.0f);

	// Currency
	InitSoftCurrency(0.0f);
	InitHardCurrency(0.0f);

	// Skill Points
	InitSkillPoints(0.0f);
	InitAttributePoints(0.0f);

	// Prestige & Season
	InitPrestigeLevel(0.0f);
	InitSeasonRank(0.0f);
	InitSeasonExperience(0.0f);
}

void USuspenseCoreProgressionAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, Level, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, MaxLevel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, Experience, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, ExperienceToNextLevel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, ExperienceMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, Reputation, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, ReputationMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, SoftCurrency, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, HardCurrency, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, SkillPoints, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, AttributePoints, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, PrestigeLevel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, SeasonRank, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreProgressionAttributeSet, SeasonExperience, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreProgressionAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void USuspenseCoreProgressionAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Обработка входящего опыта
	if (Data.EvaluatedData.Attribute == GetIncomingExperienceAttribute())
	{
		const float LocalXP = GetIncomingExperience();
		SetIncomingExperience(0.0f);

		if (LocalXP > 0.0f)
		{
			AddExperience(LocalXP);
		}
	}
}

AActor* USuspenseCoreProgressionAttributeSet::GetOwningActor() const
{
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		return ASC->GetAvatarActor();
	}
	return nullptr;
}

USuspenseCoreAbilitySystemComponent* USuspenseCoreProgressionAttributeSet::GetSuspenseCoreASC() const
{
	return Cast<USuspenseCoreAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

float USuspenseCoreProgressionAttributeSet::GetLevelProgressPercent() const
{
	const float XPToNext = GetExperienceToNextLevel();
	if (XPToNext > 0.0f)
	{
		return GetExperience() / XPToNext;
	}
	return 1.0f;
}

bool USuspenseCoreProgressionAttributeSet::IsMaxLevel() const
{
	return GetLevel() >= GetMaxLevel();
}

void USuspenseCoreProgressionAttributeSet::AddExperience(float Amount)
{
	if (Amount <= 0.0f || IsMaxLevel())
	{
		return;
	}

	// Применяем множитель
	const float FinalXP = Amount * GetExperienceMultiplier();
	const float OldXP = GetExperience();
	float NewXP = OldXP + FinalXP;
	const int32 OldLevel = FMath::FloorToInt(GetLevel());

	// Проверяем level up
	while (NewXP >= GetExperienceToNextLevel() && !IsMaxLevel())
	{
		NewXP -= GetExperienceToNextLevel();

		const float NewLevel = GetLevel() + 1.0f;
		SetLevel(FMath::Min(NewLevel, GetMaxLevel()));

		// Обновляем требуемый опыт для следующего уровня
		UpdateExperienceToNextLevel();

		// Даём очки навыков и атрибутов
		SetSkillPoints(GetSkillPoints() + SkillPointsPerLevel);
		SetAttributePoints(GetAttributePoints() + AttributePointsPerLevel);
	}

	SetExperience(NewXP);

	// Уведомление о level up
	const int32 NewLevel = FMath::FloorToInt(GetLevel());
	if (NewLevel > OldLevel)
	{
		HandleLevelUp(OldLevel, NewLevel);
	}

	// Broadcast XP change
	BroadcastProgressionEvent(
		FGameplayTag::RequestGameplayTag(FName("Event.Progression.Experience.Changed")),
		OldXP,
		GetExperience()
	);
}

bool USuspenseCoreProgressionAttributeSet::AddSoftCurrency(float Amount)
{
	if (Amount <= 0.0f)
	{
		return false;
	}

	const float OldValue = GetSoftCurrency();
	SetSoftCurrency(OldValue + Amount);

	BroadcastProgressionEvent(
		FGameplayTag::RequestGameplayTag(FName("Event.Progression.Currency.Soft.Changed")),
		OldValue,
		GetSoftCurrency()
	);

	return true;
}

bool USuspenseCoreProgressionAttributeSet::SpendSoftCurrency(float Amount)
{
	if (!CanAffordSoftCurrency(Amount))
	{
		return false;
	}

	const float OldValue = GetSoftCurrency();
	SetSoftCurrency(OldValue - Amount);

	BroadcastProgressionEvent(
		FGameplayTag::RequestGameplayTag(FName("Event.Progression.Currency.Soft.Changed")),
		OldValue,
		GetSoftCurrency()
	);

	return true;
}

bool USuspenseCoreProgressionAttributeSet::CanAffordSoftCurrency(float Amount) const
{
	return GetSoftCurrency() >= Amount;
}

float USuspenseCoreProgressionAttributeSet::CalculateExperienceForLevel(int32 TargetLevel) const
{
	if (TargetLevel <= 1)
	{
		return 0.0f;
	}

	// Экспоненциальная формула: BaseXP * (GrowthRate ^ (Level - 2))
	return BaseExperienceForLevel2 * FMath::Pow(ExperienceGrowthRate, static_cast<float>(TargetLevel - 2));
}

// ═══════════════════════════════════════════════════════════════════════════
// REPLICATION HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreProgressionAttributeSet::OnRep_Level(const FGameplayAttributeData& OldLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, Level, OldLevel);
}

void USuspenseCoreProgressionAttributeSet::OnRep_MaxLevel(const FGameplayAttributeData& OldMaxLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, MaxLevel, OldMaxLevel);
}

void USuspenseCoreProgressionAttributeSet::OnRep_Experience(const FGameplayAttributeData& OldExperience)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, Experience, OldExperience);
}

void USuspenseCoreProgressionAttributeSet::OnRep_ExperienceToNextLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, ExperienceToNextLevel, OldValue);
}

void USuspenseCoreProgressionAttributeSet::OnRep_ExperienceMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, ExperienceMultiplier, OldValue);
}

void USuspenseCoreProgressionAttributeSet::OnRep_Reputation(const FGameplayAttributeData& OldReputation)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, Reputation, OldReputation);
}

void USuspenseCoreProgressionAttributeSet::OnRep_ReputationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, ReputationMultiplier, OldValue);
}

void USuspenseCoreProgressionAttributeSet::OnRep_SoftCurrency(const FGameplayAttributeData& OldSoftCurrency)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, SoftCurrency, OldSoftCurrency);
}

void USuspenseCoreProgressionAttributeSet::OnRep_HardCurrency(const FGameplayAttributeData& OldHardCurrency)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, HardCurrency, OldHardCurrency);
}

void USuspenseCoreProgressionAttributeSet::OnRep_SkillPoints(const FGameplayAttributeData& OldSkillPoints)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, SkillPoints, OldSkillPoints);
}

void USuspenseCoreProgressionAttributeSet::OnRep_AttributePoints(const FGameplayAttributeData& OldAttributePoints)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, AttributePoints, OldAttributePoints);
}

void USuspenseCoreProgressionAttributeSet::OnRep_PrestigeLevel(const FGameplayAttributeData& OldPrestigeLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, PrestigeLevel, OldPrestigeLevel);
}

void USuspenseCoreProgressionAttributeSet::OnRep_SeasonRank(const FGameplayAttributeData& OldSeasonRank)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, SeasonRank, OldSeasonRank);
}

void USuspenseCoreProgressionAttributeSet::OnRep_SeasonExperience(const FGameplayAttributeData& OldSeasonExperience)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreProgressionAttributeSet, SeasonExperience, OldSeasonExperience);
}

// ═══════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreProgressionAttributeSet::HandleLevelUp(int32 OldLevel, int32 NewLevel)
{
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		ASC->PublishCriticalEvent(
			FGameplayTag::RequestGameplayTag(FName("Event.Progression.LevelUp")),
			static_cast<float>(NewLevel),
			GetMaxLevel()
		);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCore: %s leveled up from %d to %d"),
		*GetNameSafe(GetOwningActor()), OldLevel, NewLevel);
}

void USuspenseCoreProgressionAttributeSet::UpdateExperienceToNextLevel()
{
	const int32 CurrentLevel = FMath::FloorToInt(GetLevel());
	const float XPForNext = CalculateExperienceForLevel(CurrentLevel + 1);
	SetExperienceToNextLevel(XPForNext);
}

void USuspenseCoreProgressionAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value)
{
	if (Attribute == GetLevelAttribute())
	{
		Value = FMath::Clamp(Value, 1.0f, GetMaxLevel());
	}
	else if (Attribute == GetMaxLevelAttribute())
	{
		Value = FMath::Max(1.0f, Value);
	}
	else if (Attribute == GetExperienceAttribute() ||
			 Attribute == GetExperienceToNextLevelAttribute() ||
			 Attribute == GetSeasonExperienceAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetExperienceMultiplierAttribute() ||
			 Attribute == GetReputationMultiplierAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetReputationAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 100.0f);
	}
	else if (Attribute == GetSoftCurrencyAttribute() ||
			 Attribute == GetHardCurrencyAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetSkillPointsAttribute() ||
			 Attribute == GetAttributePointsAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetPrestigeLevelAttribute() ||
			 Attribute == GetSeasonRankAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
}

void USuspenseCoreProgressionAttributeSet::BroadcastProgressionEvent(FGameplayTag EventTag, float OldValue, float NewValue)
{
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		// Используем generic attribute change для progression events
		ASC->PublishAttributeChangeEvent(
			GetExperienceAttribute(), // Placeholder
			OldValue,
			NewValue
		);
	}
}

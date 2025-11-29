// SuspenseCoreShieldAttributeSet.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreShieldAttributeSet.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

USuspenseCoreShieldAttributeSet::USuspenseCoreShieldAttributeSet()
{
	// Инициализация значений по умолчанию
	InitShield(0.0f);
	InitMaxShield(100.0f);
	InitShieldRegen(10.0f);
	InitShieldRegenDelay(3.0f);

	InitShieldBreakCooldown(5.0f);
	InitShieldBreakRecoveryPercent(0.25f);

	InitShieldDamageReduction(1.0f); // 100% поглощение по умолчанию
	InitShieldOverflowDamage(0.0f);  // Без overflow по умолчанию

	InitIncomingShieldDamage(0.0f);
	InitIncomingShieldHealing(0.0f);
}

void USuspenseCoreShieldAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, ShieldRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, ShieldRegenDelay, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, ShieldBreakCooldown, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, ShieldBreakRecoveryPercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, ShieldDamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreShieldAttributeSet, ShieldOverflowDamage, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreShieldAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void USuspenseCoreShieldAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Получаем контекст
	AActor* TargetActor = nullptr;
	AActor* SourceActor = nullptr;

	if (Data.Target.AbilityActorInfo.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
	}

	if (Data.EffectSpec.GetContext().GetEffectCauser())
	{
		SourceActor = Data.EffectSpec.GetContext().GetEffectCauser();
	}

	// Обработка входящего урона по щиту
	if (Data.EvaluatedData.Attribute == GetIncomingShieldDamageAttribute())
	{
		const float LocalDamage = GetIncomingShieldDamage();
		SetIncomingShieldDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			const float OldShield = GetShield();
			const float DamageAfterReduction = LocalDamage * GetShieldDamageReduction();
			const float NewShield = FMath::Max(0.0f, OldShield - DamageAfterReduction);

			SetShield(NewShield);
			BroadcastShieldChange(OldShield, NewShield);

			// Проверка на разрушение щита
			if (NewShield <= 0.0f && OldShield > 0.0f)
			{
				bShieldBroken = true;
				HandleShieldBroken(SourceActor, Data.EffectSpec.GetContext().GetEffectCauser());
			}

			// Проверка на низкий щит
			const float ShieldPercent = GetShieldPercent();
			if (ShieldPercent <= LowShieldThreshold && ShieldPercent > 0.0f && !bLowShieldEventPublished)
			{
				bLowShieldEventPublished = true;
				HandleLowShield();
			}
			else if (ShieldPercent > LowShieldThreshold)
			{
				bLowShieldEventPublished = false;
			}
		}
	}

	// Обработка входящего лечения щита
	if (Data.EvaluatedData.Attribute == GetIncomingShieldHealingAttribute())
	{
		const float LocalHealing = GetIncomingShieldHealing();
		SetIncomingShieldHealing(0.0f);

		if (LocalHealing > 0.0f)
		{
			const float OldShield = GetShield();
			const float NewShield = FMath::Min(GetMaxShield(), OldShield + LocalHealing);

			SetShield(NewShield);
			BroadcastShieldChange(OldShield, NewShield);

			// Сброс флага разрушения
			if (NewShield > 0.0f && bShieldBroken)
			{
				bShieldBroken = false;
			}
		}
	}
}

AActor* USuspenseCoreShieldAttributeSet::GetOwningActor() const
{
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		return ASC->GetAvatarActor();
	}
	return nullptr;
}

USuspenseCoreAbilitySystemComponent* USuspenseCoreShieldAttributeSet::GetSuspenseCoreASC() const
{
	return Cast<USuspenseCoreAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

bool USuspenseCoreShieldAttributeSet::HasShield() const
{
	return GetShield() > 0.0f;
}

float USuspenseCoreShieldAttributeSet::GetShieldPercent() const
{
	const float MaxShieldValue = GetMaxShield();
	if (MaxShieldValue > 0.0f)
	{
		return GetShield() / MaxShieldValue;
	}
	return 0.0f;
}

bool USuspenseCoreShieldAttributeSet::IsShieldBroken() const
{
	return bShieldBroken;
}

// ═══════════════════════════════════════════════════════════════════════════
// REPLICATION HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreShieldAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, Shield, OldShield);
}

void USuspenseCoreShieldAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, MaxShield, OldMaxShield);
}

void USuspenseCoreShieldAttributeSet::OnRep_ShieldRegen(const FGameplayAttributeData& OldShieldRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, ShieldRegen, OldShieldRegen);
}

void USuspenseCoreShieldAttributeSet::OnRep_ShieldRegenDelay(const FGameplayAttributeData& OldShieldRegenDelay)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, ShieldRegenDelay, OldShieldRegenDelay);
}

void USuspenseCoreShieldAttributeSet::OnRep_ShieldBreakCooldown(const FGameplayAttributeData& OldShieldBreakCooldown)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, ShieldBreakCooldown, OldShieldBreakCooldown);
}

void USuspenseCoreShieldAttributeSet::OnRep_ShieldBreakRecoveryPercent(const FGameplayAttributeData& OldShieldBreakRecoveryPercent)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, ShieldBreakRecoveryPercent, OldShieldBreakRecoveryPercent);
}

void USuspenseCoreShieldAttributeSet::OnRep_ShieldDamageReduction(const FGameplayAttributeData& OldShieldDamageReduction)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, ShieldDamageReduction, OldShieldDamageReduction);
}

void USuspenseCoreShieldAttributeSet::OnRep_ShieldOverflowDamage(const FGameplayAttributeData& OldShieldOverflowDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreShieldAttributeSet, ShieldOverflowDamage, OldShieldOverflowDamage);
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENTBUS HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreShieldAttributeSet::BroadcastShieldChange(float OldValue, float NewValue)
{
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		ASC->PublishAttributeChangeEvent(
			GetShieldAttribute(),
			OldValue,
			NewValue
		);
	}
}

void USuspenseCoreShieldAttributeSet::HandleShieldBroken(AActor* DamageInstigator, AActor* DamageCauser)
{
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		ASC->PublishCriticalEvent(
			FGameplayTag::RequestGameplayTag(FName("Event.GAS.Shield.Broken")),
			DamageInstigator
		);
	}

	UE_LOG(LogTemp, Warning, TEXT("SuspenseCore: Shield broken on %s"), *GetNameSafe(GetOwningActor()));
}

void USuspenseCoreShieldAttributeSet::HandleLowShield()
{
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		ASC->PublishCriticalEvent(
			FGameplayTag::RequestGameplayTag(FName("Event.GAS.Shield.Low")),
			nullptr
		);
	}
}

void USuspenseCoreShieldAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value)
{
	if (Attribute == GetShieldAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GetMaxShield());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetShieldRegenAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetShieldRegenDelayAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetShieldBreakCooldownAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetShieldBreakRecoveryPercentAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 1.0f);
	}
	else if (Attribute == GetShieldDamageReductionAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 1.0f);
	}
	else if (Attribute == GetShieldOverflowDamageAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 1.0f);
	}
}

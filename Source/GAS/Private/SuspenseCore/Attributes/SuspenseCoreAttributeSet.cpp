// SuspenseCoreAttributeSet.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/SuspenseCoreEventBus.h"
#include "SuspenseCore/SuspenseCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreAttributes, Log, All);

USuspenseCoreAttributeSet::USuspenseCoreAttributeSet()
{
	// Инициализация значений по умолчанию
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitHealthRegen(0.0f);

	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitStaminaRegen(10.0f);

	InitArmor(0.0f);
	InitAttackPower(1.0f);
	InitMovementSpeed(1.0f);

	InitIncomingDamage(0.0f);
	InitIncomingHealing(0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UAttributeSet Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, HealthRegen, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, StaminaRegen, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamping
	ClampAttribute(Attribute, NewValue);
}

void USuspenseCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Получаем контекст
	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	// Обработка IncomingDamage
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float LocalDamage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			// Применяем броню
			const float DamageAfterArmor = FMath::Max(LocalDamage - GetArmor(), 0.0f);

			// Применяем к здоровью
			const float OldHealth = GetHealth();
			const float NewHealth = FMath::Clamp(OldHealth - DamageAfterArmor, 0.0f, GetMaxHealth());
			SetHealth(NewHealth);

			// Публикуем событие урона
			BroadcastAttributeChange(GetHealthAttribute(), OldHealth, NewHealth);

			UE_LOG(LogSuspenseCoreAttributes, Log, TEXT("Damage: %.2f -> %.2f (after armor). Health: %.2f -> %.2f"),
				LocalDamage, DamageAfterArmor, OldHealth, NewHealth);

			// Проверяем смерть
			if (NewHealth <= 0.0f)
			{
				HandleDeath(Instigator, Causer);
			}
			// Проверяем низкое здоровье
			else if (GetHealthPercent() <= LowHealthThreshold && !bLowHealthEventPublished)
			{
				HandleLowHealth();
			}
		}
	}
	// Обработка IncomingHealing
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float LocalHealing = GetIncomingHealing();
		SetIncomingHealing(0.0f);

		if (LocalHealing > 0.0f)
		{
			const float OldHealth = GetHealth();
			const float NewHealth = FMath::Clamp(OldHealth + LocalHealing, 0.0f, GetMaxHealth());
			SetHealth(NewHealth);

			BroadcastAttributeChange(GetHealthAttribute(), OldHealth, NewHealth);

			UE_LOG(LogSuspenseCoreAttributes, Log, TEXT("Healing: %.2f. Health: %.2f -> %.2f"),
				LocalHealing, OldHealth, NewHealth);

			// Сбрасываем флаг низкого здоровья если вылечились
			if (GetHealthPercent() > LowHealthThreshold)
			{
				bLowHealthEventPublished = false;
			}
		}
	}
	// Обработка изменения MovementSpeed
	else if (Data.EvaluatedData.Attribute == GetMovementSpeedAttribute())
	{
		// Обновляем скорость персонажа
		if (ACharacter* Character = Cast<ACharacter>(GetOwningActor()))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				// MovementSpeed - это множитель базовой скорости
				const float BaseSpeed = 600.0f; // Можно вынести в конфиг
				Movement->MaxWalkSpeed = BaseSpeed * GetMovementSpeed();
			}
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

AActor* USuspenseCoreAttributeSet::GetOwningActor() const
{
	return GetOwningAbilitySystemComponent() ? GetOwningAbilitySystemComponent()->GetAvatarActor() : nullptr;
}

USuspenseCoreAbilitySystemComponent* USuspenseCoreAttributeSet::GetSuspenseCoreASC() const
{
	return Cast<USuspenseCoreAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

bool USuspenseCoreAttributeSet::IsAlive() const
{
	return GetHealth() > 0.0f;
}

float USuspenseCoreAttributeSet::GetHealthPercent() const
{
	const float MaxHP = GetMaxHealth();
	return MaxHP > 0.0f ? GetHealth() / MaxHP : 0.0f;
}

float USuspenseCoreAttributeSet::GetStaminaPercent() const
{
	const float MaxST = GetMaxStamina();
	return MaxST > 0.0f ? GetStamina() / MaxST : 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// REPLICATION HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, Health, OldHealth);
	BroadcastAttributeChange(GetHealthAttribute(), OldHealth.GetCurrentValue(), Health.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, MaxHealth, OldMaxHealth);
	BroadcastAttributeChange(GetMaxHealthAttribute(), OldMaxHealth.GetCurrentValue(), MaxHealth.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, HealthRegen, OldHealthRegen);
}

void USuspenseCoreAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, Stamina, OldStamina);
	BroadcastAttributeChange(GetStaminaAttribute(), OldStamina.GetCurrentValue(), Stamina.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, MaxStamina, OldMaxStamina);
	BroadcastAttributeChange(GetMaxStaminaAttribute(), OldMaxStamina.GetCurrentValue(), MaxStamina.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, StaminaRegen, OldStaminaRegen);
}

void USuspenseCoreAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, Armor, OldArmor);
}

void USuspenseCoreAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, AttackPower, OldAttackPower);
}

void USuspenseCoreAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, MovementSpeed, OldMovementSpeed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributeSet::BroadcastAttributeChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue)
{
	// Проверяем что значение реально изменилось
	if (FMath::IsNearlyEqual(OldValue, NewValue))
	{
		return;
	}

	// Получаем ASC и публикуем через него
	if (USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC())
	{
		ASC->PublishAttributeChangeEvent(Attribute, OldValue, NewValue);
	}
}

void USuspenseCoreAttributeSet::HandleDeath(AActor* DamageInstigator, AActor* DamageCauser)
{
	UE_LOG(LogSuspenseCoreAttributes, Log, TEXT("HandleDeath: Owner=%s, Instigator=%s, Causer=%s"),
		*GetNameSafe(GetOwningActor()),
		*GetNameSafe(DamageInstigator),
		*GetNameSafe(DamageCauser));

	if (USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC())
	{
		// Публикуем критическое событие смерти
		ASC->PublishCriticalEvent(
			FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Player.Died"))),
			0.0f,
			GetMaxHealth()
		);
	}
}

void USuspenseCoreAttributeSet::HandleLowHealth()
{
	UE_LOG(LogSuspenseCoreAttributes, Log, TEXT("HandleLowHealth: Owner=%s, Health=%.2f%%"),
		*GetNameSafe(GetOwningActor()),
		GetHealthPercent() * 100.0f);

	bLowHealthEventPublished = true;

	if (USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC())
	{
		ASC->PublishCriticalEvent(
			FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Player.LowHealth"))),
			GetHealth(),
			GetMaxHealth()
		);
	}
}

void USuspenseCoreAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value)
{
	if (Attribute == GetHealthAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		Value = FMath::Max(Value, 1.0f);
	}
	else if (Attribute == GetArmorAttribute())
	{
		Value = FMath::Max(Value, 0.0f);
	}
	else if (Attribute == GetMovementSpeedAttribute())
	{
		Value = FMath::Clamp(Value, 0.1f, 3.0f); // От 10% до 300% базовой скорости
	}
}

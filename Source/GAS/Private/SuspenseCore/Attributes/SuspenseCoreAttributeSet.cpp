// SuspenseCoreAttributeSet.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeDefaults.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreAttributes, Log, All);

USuspenseCoreAttributeSet::USuspenseCoreAttributeSet()
{
	// Initialize with base values from SSOT (SuspenseCoreAttributeDefaults)
	// These will be multiplied by CharacterClassData.AttributeModifiers when class is applied
	using namespace FSuspenseCoreAttributeDefaults;

	InitHealth(BaseMaxHealth);
	InitMaxHealth(BaseMaxHealth);
	InitHealthRegen(BaseHealthRegen);

	InitStamina(BaseMaxStamina);
	InitMaxStamina(BaseMaxStamina);
	InitStaminaRegen(BaseStaminaRegen);

	InitArmor(BaseArmor);
	InitAttackPower(BaseAttackPower);
	InitMovementSpeed(BaseMovementSpeed);

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

	// DEBUG: Log all attribute changes
	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("[AttributeSet] PostGameplayEffectExecute - Attribute: %s, Magnitude: %.2f"),
		*Data.EvaluatedData.Attribute.GetName(), Data.EvaluatedData.Magnitude);

	// Get context
	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	// Process IncomingDamage
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float LocalDamage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			// Apply armor
			const float DamageAfterArmor = FMath::Max(LocalDamage - GetArmor(), 0.0f);

			// Apply to health
			const float OldHealth = GetHealth();
			const float NewHealth = FMath::Clamp(OldHealth - DamageAfterArmor, 0.0f, GetMaxHealth());
			SetHealth(NewHealth);

			// Publish damage event
			BroadcastAttributeChange(GetHealthAttribute(), OldHealth, NewHealth);

			UE_LOG(LogSuspenseCoreAttributes, Log, TEXT("Damage: %.2f -> %.2f (after armor). Health: %.2f -> %.2f"),
				LocalDamage, DamageAfterArmor, OldHealth, NewHealth);

			// Check for death
			if (NewHealth <= 0.0f)
			{
				HandleDeath(Instigator, Causer);
			}
			// Check for low health
			else if (GetHealthPercent() <= LowHealthThreshold && !bLowHealthEventPublished)
			{
				HandleLowHealth();
			}
		}
	}
	// Process IncomingHealing
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

			// Reset low health flag if healed
			if (GetHealthPercent() > LowHealthThreshold)
			{
				bLowHealthEventPublished = false;
			}
		}
	}
	// Process MovementSpeed change
	else if (Data.EvaluatedData.Attribute == GetMovementSpeedAttribute())
	{
		// Update character speed
		if (ACharacter* Character = Cast<ACharacter>(GetOwningActor()))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				// MovementSpeed is a multiplier of base speed (configurable)
				Movement->MaxWalkSpeed = BaseWalkSpeed * GetMovementSpeed();
			}
		}
	}
	// Process Stamina change (from Sprint, Jump, etc.)
	// This broadcasts events for LOCAL changes (replication uses OnRep_Stamina)
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		const float MaxST = GetMaxStamina();
		const float StaminaDelta = Data.EvaluatedData.Magnitude;
		float CurrentStamina = GetStamina(); // Already clamped by PreAttributeChange

		// For positive changes (regen), we need to flatten the base value to prevent
		// modifiers from accumulating beyond MaxStamina.
		// PreAttributeChange clamps the DISPLAYED value but not the underlying base,
		// so periodic regen effects can cause the base to exceed max over time.
		if (StaminaDelta > 0.0f)
		{
			// Always set base value to the clamped current value after regen
			// This prevents infinite stacking of periodic additive modifiers
			const float ClampedValue = FMath::Clamp(CurrentStamina, 0.0f, MaxST);
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(GetStaminaAttribute(), ClampedValue);
			}
			CurrentStamina = ClampedValue;
		}

		// Calculate old value from current and delta for event broadcasting
		const float OldStamina = FMath::Clamp(CurrentStamina - StaminaDelta, 0.0f, MaxST);

		UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("[AttributeSet] STAMINA CHANGE DETECTED! Old: %.2f, New: %.2f, Delta: %.2f"),
			OldStamina, CurrentStamina, StaminaDelta);

		BroadcastAttributeChange(GetStaminaAttribute(), OldStamina, CurrentStamina);
	}
	// Process MaxStamina change
	else if (Data.EvaluatedData.Attribute == GetMaxStaminaAttribute())
	{
		const float NewMaxStamina = GetMaxStamina();
		const float MaxStaminaDelta = Data.EvaluatedData.Magnitude;
		const float OldMaxStamina = NewMaxStamina - MaxStaminaDelta;

		BroadcastAttributeChange(GetMaxStaminaAttribute(), OldMaxStamina, NewMaxStamina);
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
	BroadcastAttributeChange(GetHealthRegenAttribute(), OldHealthRegen.GetCurrentValue(), HealthRegen.GetCurrentValue());
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
	BroadcastAttributeChange(GetStaminaRegenAttribute(), OldStaminaRegen.GetCurrentValue(), StaminaRegen.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, Armor, OldArmor);
	BroadcastAttributeChange(GetArmorAttribute(), OldArmor.GetCurrentValue(), Armor.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, AttackPower, OldAttackPower);
	BroadcastAttributeChange(GetAttackPowerAttribute(), OldAttackPower.GetCurrentValue(), AttackPower.GetCurrentValue());
}

void USuspenseCoreAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreAttributeSet, MovementSpeed, OldMovementSpeed);
	BroadcastAttributeChange(GetMovementSpeedAttribute(), OldMovementSpeed.GetCurrentValue(), MovementSpeed.GetCurrentValue());
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributeSet::BroadcastAttributeChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue)
{
	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("[AttributeSet] BroadcastAttributeChange called - %s: %.2f -> %.2f"),
		*Attribute.GetName(), OldValue, NewValue);

	// Check if value actually changed
	if (FMath::IsNearlyEqual(OldValue, NewValue))
	{
		UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("[AttributeSet] Values are equal, skipping broadcast"));
		return;
	}

	// Get ASC and publish through it
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("[AttributeSet] Publishing via ASC..."));
		ASC->PublishAttributeChangeEvent(Attribute, OldValue, NewValue);
	}
	else
	{
		UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("[AttributeSet] ERROR: SuspenseCoreASC is NULL! Using base ASC: %s"),
			GetOwningAbilitySystemComponent() ? TEXT("YES") : TEXT("NO"));
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
		// Publish critical death event
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
		Value = FMath::Clamp(Value, 0.1f, 3.0f); // 10% to 300% of base speed
	}
}

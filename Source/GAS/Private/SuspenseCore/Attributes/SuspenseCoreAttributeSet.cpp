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
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreAttributes, Log, All);

USuspenseCoreAttributeSet::USuspenseCoreAttributeSet()
{
	// Initialize with base values from SSOT (SuspenseCoreAttributeDefaults)
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

	// Cache old stamina before the change for accurate broadcast in PostGameplayEffectExecute
	if (Attribute == GetStaminaAttribute())
	{
		CachedPreChangeStamina = GetStamina();
	}

	ClampAttribute(Attribute, NewValue);
}

void USuspenseCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

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
			const float DamageAfterArmor = FMath::Max(LocalDamage - GetArmor(), 0.0f);
			const float OldHealth = GetHealth();
			const float NewHealth = FMath::Clamp(OldHealth - DamageAfterArmor, 0.0f, GetMaxHealth());
			SetHealth(NewHealth);

			BroadcastAttributeChange(GetHealthAttribute(), OldHealth, NewHealth);

			if (NewHealth <= 0.0f)
			{
				HandleDeath(Instigator, Causer);
			}
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

			if (GetHealthPercent() > LowHealthThreshold)
			{
				bLowHealthEventPublished = false;
			}
		}
	}
	// Process MovementSpeed change
	else if (Data.EvaluatedData.Attribute == GetMovementSpeedAttribute())
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwningActor()))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->MaxWalkSpeed = BaseWalkSpeed * GetMovementSpeed();
			}
		}
	}
	// Process Stamina change
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		const float MaxST = GetMaxStamina();
		const float StaminaDelta = Data.EvaluatedData.Magnitude;
		float CurrentStamina = GetStamina();

		// For positive changes (regen), flatten base value to prevent accumulation
		// This is CRITICAL: periodic effects add to BASE value, which can exceed max
		// We must reset base to clamped value to prevent base from growing unbounded
		if (StaminaDelta > 0.0f)
		{
			const float ClampedValue = FMath::Clamp(CurrentStamina, 0.0f, MaxST);
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				ASC->SetNumericAttributeBase(GetStaminaAttribute(), ClampedValue);
			}
			CurrentStamina = ClampedValue;
		}

		// Use cached pre-change value for accurate broadcast (set in PreAttributeChange)
		BroadcastAttributeChange(GetStaminaAttribute(), CachedPreChangeStamina, CurrentStamina);
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
	return !bIsDead && GetHealth() > 0.0f;
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
	if (FMath::IsNearlyEqual(OldValue, NewValue))
	{
		return;
	}

	if (USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC())
	{
		ASC->PublishAttributeChangeEvent(Attribute, OldValue, NewValue);
	}
}

void USuspenseCoreAttributeSet::HandleDeath(AActor* DamageInstigator, AActor* DamageCauser)
{
	// Prevent multiple death calls
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// 1. Add State.Dead tag to block abilities
	// ═══════════════════════════════════════════════════════════════════════════════
	FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Dead")), false);
	if (DeadTag.IsValid())
	{
		ASC->AddLooseGameplayTag(DeadTag);
		UE_LOG(LogSuspenseCoreAttributes, Log, TEXT("Added State.Dead tag to %s"), *GetOwningActor()->GetName());
	}

	// ═══════════════════════════════════════════════════════════════════════════════
	// 2. Cancel all active abilities
	// ═══════════════════════════════════════════════════════════════════════════════
	ASC->CancelAllAbilities();

	// ═══════════════════════════════════════════════════════════════════════════════
	// 3. Remove all active gameplay effects (DoTs, buffs, etc.)
	// ═══════════════════════════════════════════════════════════════════════════════
	FGameplayEffectQuery Query;
	ASC->RemoveActiveEffects(Query);

	// ═══════════════════════════════════════════════════════════════════════════════
	// 4. Publish death event via EventBus
	// ═══════════════════════════════════════════════════════════════════════════════
	ASC->PublishCriticalEvent(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Player.Died"))),
		0.0f,
		GetMaxHealth()
	);

	// ═══════════════════════════════════════════════════════════════════════════════
	// 5. Apply death effects to Character
	// ═══════════════════════════════════════════════════════════════════════════════
	AActor* OwningActor = GetOwningActor();
	if (!OwningActor)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OwningActor);
	if (!Character)
	{
		return;
	}

	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("═══════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("  CHARACTER DEATH: %s"), *Character->GetName());
	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("  Killer: %s"), DamageInstigator ? *DamageInstigator->GetName() : TEXT("Unknown"));
	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("  Causer: %s"), DamageCauser ? *DamageCauser->GetName() : TEXT("Unknown"));
	UE_LOG(LogSuspenseCoreAttributes, Warning, TEXT("═══════════════════════════════════════════════════════"));

	// Disable movement
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}

	// Disable collision on capsule
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Enable ragdoll physics on mesh
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetSimulatePhysics(true);
	}

	// Disable input on player controller
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		PC->DisableInput(PC);

		// Optionally show death screen or spectate
		// PC->ChangeState(NAME_Spectating);
	}

	// Detach controller
	Character->DetachFromControllerPendingDestroy();
}

void USuspenseCoreAttributeSet::HandleLowHealth()
{
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
		Value = FMath::Clamp(Value, 0.1f, 3.0f);
	}
}

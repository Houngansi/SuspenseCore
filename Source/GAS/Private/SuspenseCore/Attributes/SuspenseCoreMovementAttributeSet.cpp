// SuspenseCoreMovementAttributeSet.cpp
// SuspenseCore - Clean Architecture GAS Integration
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

USuspenseCoreMovementAttributeSet::USuspenseCoreMovementAttributeSet()
{
	// Базовые скорости (UE5 defaults ~= 600)
	InitWalkSpeed(400.0f);
	InitSprintSpeed(600.0f);
	InitCrouchSpeed(200.0f);
	InitProneSpeed(100.0f);
	InitAimSpeed(250.0f);

	// Множители направления
	InitBackwardSpeedMultiplier(0.7f);
	InitStrafeSpeedMultiplier(0.85f);

	// Прыжок
	InitJumpHeight(420.0f);
	InitMaxJumpCount(1.0f);
	InitAirControl(0.35f);

	// Поворот
	InitTurnRate(180.0f);
	InitAimTurnRateMultiplier(0.6f);

	// Ускорение
	InitGroundAcceleration(2048.0f);
	InitGroundDeceleration(2048.0f);
	InitAirAcceleration(512.0f);

	// Вес
	InitCurrentWeight(0.0f);
	InitMaxWeight(50.0f); // 50 кг базовый лимит
	InitWeightSpeedPenalty(0.0f);
}

void USuspenseCoreMovementAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, WalkSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, SprintSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, CrouchSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, ProneSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, AimSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, BackwardSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, StrafeSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, JumpHeight, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, MaxJumpCount, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, AirControl, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, TurnRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, AimTurnRateMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, GroundAcceleration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, GroundDeceleration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, AirAcceleration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, CurrentWeight, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreMovementAttributeSet, MaxWeight, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreMovementAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void USuspenseCoreMovementAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Пересчитываем штраф от веса при изменении веса
	if (Data.EvaluatedData.Attribute == GetCurrentWeightAttribute() ||
		Data.EvaluatedData.Attribute == GetMaxWeightAttribute())
	{
		RecalculateWeightPenalty();
	}

	// Применяем скорости к CharacterMovement при изменении
	if (Data.EvaluatedData.Attribute == GetWalkSpeedAttribute() ||
		Data.EvaluatedData.Attribute == GetSprintSpeedAttribute() ||
		Data.EvaluatedData.Attribute == GetCrouchSpeedAttribute() ||
		Data.EvaluatedData.Attribute == GetJumpHeightAttribute() ||
		Data.EvaluatedData.Attribute == GetAirControlAttribute())
	{
		ApplySpeedsToCharacter();
	}
}

AActor* USuspenseCoreMovementAttributeSet::GetOwningActor() const
{
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		return ASC->GetAvatarActor();
	}
	return nullptr;
}

USuspenseCoreAbilitySystemComponent* USuspenseCoreMovementAttributeSet::GetSuspenseCoreASC() const
{
	return Cast<USuspenseCoreAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

float USuspenseCoreMovementAttributeSet::GetEffectiveWalkSpeed() const
{
	const float Penalty = GetWeightSpeedPenalty();
	return GetWalkSpeed() * (1.0f - Penalty);
}

float USuspenseCoreMovementAttributeSet::GetEffectiveSprintSpeed() const
{
	const float Penalty = GetWeightSpeedPenalty();
	// Спринт страдает больше от веса
	return GetSprintSpeed() * (1.0f - Penalty * 1.5f);
}

bool USuspenseCoreMovementAttributeSet::IsOverencumbered() const
{
	return GetCurrentWeight() > GetMaxWeight();
}

float USuspenseCoreMovementAttributeSet::GetEncumbrancePercent() const
{
	const float Max = GetMaxWeight();
	if (Max > 0.0f)
	{
		return GetCurrentWeight() / Max;
	}
	return 0.0f;
}

void USuspenseCoreMovementAttributeSet::ApplySpeedsToCharacter()
{
	AActor* Owner = GetOwningActor();
	if (!Owner) return;

	ACharacter* Character = Cast<ACharacter>(Owner);
	if (!Character) return;

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement) return;

	// Применяем эффективные скорости
	Movement->MaxWalkSpeed = GetEffectiveWalkSpeed();
	Movement->MaxWalkSpeedCrouched = GetCrouchSpeed() * (1.0f - GetWeightSpeedPenalty());
	Movement->JumpZVelocity = GetJumpHeight();
	Movement->AirControl = GetAirControl();
	Movement->MaxAcceleration = GetGroundAcceleration();
	Movement->BrakingDecelerationWalking = GetGroundDeceleration();
}

// ═══════════════════════════════════════════════════════════════════════════
// REPLICATION HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreMovementAttributeSet::OnRep_WalkSpeed(const FGameplayAttributeData& OldWalkSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, WalkSpeed, OldWalkSpeed);
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::OnRep_SprintSpeed(const FGameplayAttributeData& OldSprintSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, SprintSpeed, OldSprintSpeed);
}

void USuspenseCoreMovementAttributeSet::OnRep_CrouchSpeed(const FGameplayAttributeData& OldCrouchSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, CrouchSpeed, OldCrouchSpeed);
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::OnRep_ProneSpeed(const FGameplayAttributeData& OldProneSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, ProneSpeed, OldProneSpeed);
}

void USuspenseCoreMovementAttributeSet::OnRep_AimSpeed(const FGameplayAttributeData& OldAimSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, AimSpeed, OldAimSpeed);
}

void USuspenseCoreMovementAttributeSet::OnRep_BackwardSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, BackwardSpeedMultiplier, OldValue);
}

void USuspenseCoreMovementAttributeSet::OnRep_StrafeSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, StrafeSpeedMultiplier, OldValue);
}

void USuspenseCoreMovementAttributeSet::OnRep_JumpHeight(const FGameplayAttributeData& OldJumpHeight)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, JumpHeight, OldJumpHeight);
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::OnRep_MaxJumpCount(const FGameplayAttributeData& OldMaxJumpCount)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, MaxJumpCount, OldMaxJumpCount);
}

void USuspenseCoreMovementAttributeSet::OnRep_AirControl(const FGameplayAttributeData& OldAirControl)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, AirControl, OldAirControl);
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::OnRep_TurnRate(const FGameplayAttributeData& OldTurnRate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, TurnRate, OldTurnRate);
}

void USuspenseCoreMovementAttributeSet::OnRep_AimTurnRateMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, AimTurnRateMultiplier, OldValue);
}

void USuspenseCoreMovementAttributeSet::OnRep_GroundAcceleration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, GroundAcceleration, OldValue);
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::OnRep_GroundDeceleration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, GroundDeceleration, OldValue);
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::OnRep_AirAcceleration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, AirAcceleration, OldValue);
}

void USuspenseCoreMovementAttributeSet::OnRep_CurrentWeight(const FGameplayAttributeData& OldCurrentWeight)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, CurrentWeight, OldCurrentWeight);
	RecalculateWeightPenalty();
}

void USuspenseCoreMovementAttributeSet::OnRep_MaxWeight(const FGameplayAttributeData& OldMaxWeight)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreMovementAttributeSet, MaxWeight, OldMaxWeight);
	RecalculateWeightPenalty();
}

// ═══════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreMovementAttributeSet::RecalculateWeightPenalty()
{
	const float Current = GetCurrentWeight();
	const float Max = GetMaxWeight();

	if (Max <= 0.0f)
	{
		SetWeightSpeedPenalty(0.0f);
		return;
	}

	// Штраф начинается после 50% загрузки
	const float EncumbranceThreshold = 0.5f;
	const float EncumbrancePercent = Current / Max;

	if (EncumbrancePercent <= EncumbranceThreshold)
	{
		SetWeightSpeedPenalty(0.0f);
	}
	else if (EncumbrancePercent >= 1.0f)
	{
		// Максимальный штраф 50% скорости при полной загрузке
		// Сверх лимита - до 80% штрафа
		const float OverWeight = FMath::Min((EncumbrancePercent - 1.0f) * 0.3f, 0.3f);
		SetWeightSpeedPenalty(0.5f + OverWeight);
	}
	else
	{
		// Линейный рост от 0% до 50% штрафа между 50% и 100% загрузки
		const float PenaltyPercent = (EncumbrancePercent - EncumbranceThreshold) / (1.0f - EncumbranceThreshold);
		SetWeightSpeedPenalty(PenaltyPercent * 0.5f);
	}

	// Применяем обновлённые скорости
	ApplySpeedsToCharacter();
}

void USuspenseCoreMovementAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value)
{
	// Скорости
	if (Attribute == GetWalkSpeedAttribute() ||
		Attribute == GetSprintSpeedAttribute() ||
		Attribute == GetCrouchSpeedAttribute() ||
		Attribute == GetProneSpeedAttribute() ||
		Attribute == GetAimSpeedAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	// Множители (0-1)
	else if (Attribute == GetBackwardSpeedMultiplierAttribute() ||
			 Attribute == GetStrafeSpeedMultiplierAttribute() ||
			 Attribute == GetAirControlAttribute() ||
			 Attribute == GetAimTurnRateMultiplierAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 1.0f);
	}
	// Прыжок
	else if (Attribute == GetJumpHeightAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetMaxJumpCountAttribute())
	{
		Value = FMath::Max(1.0f, Value);
	}
	// Ускорение
	else if (Attribute == GetGroundAccelerationAttribute() ||
			 Attribute == GetGroundDecelerationAttribute() ||
			 Attribute == GetAirAccelerationAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	// Вес
	else if (Attribute == GetCurrentWeightAttribute())
	{
		Value = FMath::Max(0.0f, Value);
	}
	else if (Attribute == GetMaxWeightAttribute())
	{
		Value = FMath::Max(1.0f, Value); // Минимум 1 кг лимита
	}
	else if (Attribute == GetWeightSpeedPenaltyAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 0.8f); // Максимум 80% штраф
	}
}

void USuspenseCoreMovementAttributeSet::BroadcastSpeedChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	USuspenseCoreAbilitySystemComponent* ASC = GetSuspenseCoreASC();
	if (ASC)
	{
		ASC->PublishAttributeChangeEvent(Attribute, OldValue, NewValue);
	}
}

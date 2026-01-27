// SuspenseCoreEnemyBehaviorData.cpp
// SuspenseCore - Enemy System
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Types/SuspenseCoreEnemyTypes.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyBehaviorData, Log, All);

USuspenseCoreEnemyBehaviorData::USuspenseCoreEnemyBehaviorData()
{
	// Identity defaults
	BehaviorID = FName(TEXT("DefaultEnemy"));
	DisplayName = FText::FromString(TEXT("Default Enemy"));
	EnemyTypeTag = SuspenseCoreEnemyTags::Type::Scav;
	InitialState = SuspenseCoreEnemyTags::State::Idle;

	// Inline attribute defaults (used when PresetRowName is empty)
	MaxHealth = 100.0f;
	AttackPower = 25.0f;
	Armor = 0.0f;
	WalkSpeed = 200.0f;
	RunSpeed = 500.0f;
	SightRange = 2000.0f;
	HearingRange = 1500.0f;

	// Default FSM states
	FSuspenseCoreEnemyStateConfig IdleConfig;
	IdleConfig.StateTag = SuspenseCoreEnemyTags::State::Idle;
	IdleConfig.StateClass = USuspenseCoreEnemyIdleState::StaticClass();
	States.Add(IdleConfig);

	FSuspenseCoreEnemyStateConfig PatrolConfig;
	PatrolConfig.StateTag = SuspenseCoreEnemyTags::State::Patrol;
	PatrolConfig.StateClass = USuspenseCoreEnemyPatrolState::StaticClass();
	States.Add(PatrolConfig);

	FSuspenseCoreEnemyStateConfig ChaseConfig;
	ChaseConfig.StateTag = SuspenseCoreEnemyTags::State::Chase;
	ChaseConfig.StateClass = USuspenseCoreEnemyChaseState::StaticClass();
	States.Add(ChaseConfig);

	FSuspenseCoreEnemyStateConfig AttackConfig;
	AttackConfig.StateTag = SuspenseCoreEnemyTags::State::Attack;
	AttackConfig.StateClass = USuspenseCoreEnemyAttackState::StaticClass();
	States.Add(AttackConfig);

	FSuspenseCoreEnemyStateConfig DeathConfig;
	DeathConfig.StateTag = SuspenseCoreEnemyTags::State::Death;
	DeathConfig.StateClass = USuspenseCoreEnemyDeathState::StaticClass();
	States.Add(DeathConfig);

	// Default FSM transitions
	FSuspenseCoreEnemyTransitionConfig IdleToPatrol;
	IdleToPatrol.FromState = SuspenseCoreEnemyTags::State::Idle;
	IdleToPatrol.OnEvent = SuspenseCoreEnemyTags::Event::IdleTimeout;
	IdleToPatrol.ToState = SuspenseCoreEnemyTags::State::Patrol;
	Transitions.Add(IdleToPatrol);

	FSuspenseCoreEnemyTransitionConfig IdleToChase;
	IdleToChase.FromState = SuspenseCoreEnemyTags::State::Idle;
	IdleToChase.OnEvent = SuspenseCoreEnemyTags::Event::PlayerDetected;
	IdleToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
	Transitions.Add(IdleToChase);

	FSuspenseCoreEnemyTransitionConfig PatrolToChase;
	PatrolToChase.FromState = SuspenseCoreEnemyTags::State::Patrol;
	PatrolToChase.OnEvent = SuspenseCoreEnemyTags::Event::PlayerDetected;
	PatrolToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
	Transitions.Add(PatrolToChase);

	FSuspenseCoreEnemyTransitionConfig ChaseToAttack;
	ChaseToAttack.FromState = SuspenseCoreEnemyTags::State::Chase;
	ChaseToAttack.OnEvent = SuspenseCoreEnemyTags::Event::TargetInRange;
	ChaseToAttack.ToState = SuspenseCoreEnemyTags::State::Attack;
	Transitions.Add(ChaseToAttack);

	FSuspenseCoreEnemyTransitionConfig ChaseToIdle;
	ChaseToIdle.FromState = SuspenseCoreEnemyTags::State::Chase;
	ChaseToIdle.OnEvent = SuspenseCoreEnemyTags::Event::PlayerLost;
	ChaseToIdle.ToState = SuspenseCoreEnemyTags::State::Idle;
	Transitions.Add(ChaseToIdle);

	FSuspenseCoreEnemyTransitionConfig AttackToChase;
	AttackToChase.FromState = SuspenseCoreEnemyTags::State::Attack;
	AttackToChase.OnEvent = SuspenseCoreEnemyTags::Event::TargetOutOfRange;
	AttackToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
	Transitions.Add(AttackToChase);

	FSuspenseCoreEnemyTransitionConfig AttackToIdle;
	AttackToIdle.FromState = SuspenseCoreEnemyTags::State::Attack;
	AttackToIdle.OnEvent = SuspenseCoreEnemyTags::Event::PlayerLost;
	AttackToIdle.ToState = SuspenseCoreEnemyTags::State::Idle;
	Transitions.Add(AttackToIdle);
}

bool USuspenseCoreEnemyBehaviorData::GetPresetRow(FSuspenseCoreEnemyPresetRow& OutPreset) const
{
	if (PresetRowName.IsNone())
	{
		return false;
	}

	// Try to load from directly referenced DataTable first
	UDataTable* DataTable = nullptr;

	if (!EnemyPresetsDataTable.IsNull())
	{
		DataTable = EnemyPresetsDataTable.LoadSynchronous();
	}

	// Fallback to Project Settings DataTable (SSOT)
	if (!DataTable)
	{
		const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
		if (Settings && !Settings->EnemyPresetsDataTable.IsNull())
		{
			DataTable = Settings->EnemyPresetsDataTable.LoadSynchronous();
		}
	}

	if (!DataTable)
	{
		UE_LOG(LogEnemyBehaviorData, Warning,
			TEXT("BehaviorData '%s': No DataTable found for preset lookup (PresetRowName='%s'). "
				 "Set EnemyPresetsDataTable in this asset or in Project Settings → Game → SuspenseCore."),
			*BehaviorID.ToString(), *PresetRowName.ToString());
		return false;
	}

	// Find the row
	const FSuspenseCoreEnemyPresetRow* FoundRow = DataTable->FindRow<FSuspenseCoreEnemyPresetRow>(
		PresetRowName,
		TEXT("GetPresetRow")
	);

	if (!FoundRow)
	{
		UE_LOG(LogEnemyBehaviorData, Warning,
			TEXT("BehaviorData '%s': Preset row '%s' not found in DataTable"),
			*BehaviorID.ToString(), *PresetRowName.ToString());
		return false;
	}

	OutPreset = *FoundRow;
	return true;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveMaxHealth(int32 Level) const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.GetScaledHealth(Level);
	}
	return MaxHealth;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveArmor(int32 Level) const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.GetScaledArmor(Level);
	}
	return Armor;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveAttackPower(int32 Level) const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.GetScaledAttackPower(Level);
	}
	return AttackPower;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveWalkSpeed() const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.MovementAttributes.WalkSpeed;
	}
	return WalkSpeed;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveRunSpeed() const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.MovementAttributes.RunSpeed;
	}
	return RunSpeed;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveSightRange() const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.PerceptionAttributes.SightRange;
	}
	return SightRange;
}

float USuspenseCoreEnemyBehaviorData::GetEffectiveHearingRange() const
{
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		return Preset.PerceptionAttributes.HearingRange;
	}
	return HearingRange;
}

TArray<TSubclassOf<UGameplayAbility>> USuspenseCoreEnemyBehaviorData::GetAllStartupAbilities() const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;

	// First add preset abilities
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		Result.Append(Preset.StartupAbilities);
	}

	// Then add inline abilities (allowing overrides/additions)
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass && !Result.Contains(AbilityClass))
		{
			Result.Add(AbilityClass);
		}
	}

	return Result;
}

TArray<TSubclassOf<UGameplayEffect>> USuspenseCoreEnemyBehaviorData::GetAllStartupEffects() const
{
	TArray<TSubclassOf<UGameplayEffect>> Result;

	// First add preset effects
	FSuspenseCoreEnemyPresetRow Preset;
	if (GetPresetRow(Preset))
	{
		Result.Append(Preset.StartupEffects);
	}

	// Then add inline effects (allowing overrides/additions)
	for (const TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (EffectClass && !Result.Contains(EffectClass))
		{
			Result.Add(EffectClass);
		}
	}

	return Result;
}

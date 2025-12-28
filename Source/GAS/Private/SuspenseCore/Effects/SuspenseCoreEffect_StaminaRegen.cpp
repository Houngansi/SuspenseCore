// SuspenseCoreEffect_StaminaRegen.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_StaminaRegen.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

USuspenseCoreEffect_StaminaRegen::USuspenseCoreEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// INSTANT duration - applied periodically by ASC timer
	// This fixes the Infinite+Period+Modifiers stacking bug in GAS
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// +1 STA per application (ASC applies this 10x/sec = +10 STA/s)
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FScalableFloat(1.0f);
		Modifiers.Add(Mod);
	}

	// NOTE: Tag blocking is now handled by ASC timer (checks State.Sprinting, State.Dead)

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_StaminaRegen: Instant effect (+1 STA per application)"));
}

// SuspenseCoreMedicalNativeTags.h
// Native GameplayTags for Medical/Healing System
// Copyright Suspense Team. All Rights Reserved.
//
// USAGE:
//   #include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
//   Spec.Data->SetSetByCallerMagnitude(SuspenseCoreMedicalTags::Data::TAG_Data_Medical_InstantHeal, 50.0f);
//
// DO NOT use FGameplayTag::RequestGameplayTag() for these tags.
// Using native tags ensures:
// 1. Compile-time validation (no typos)
// 2. Better performance (no runtime string lookups)
// 3. IDE autocomplete support
//
// RELATED: GE_InstantHeal.h, GE_HealOverTime.h, SuspenseCoreMedicalUseHandler.h

#pragma once

#include "NativeGameplayTags.h"

namespace SuspenseCoreMedicalTags
{
	//==================================================================
	// Data Tags (SetByCaller)
	// Used for GameplayEffect magnitude parameters
	//==================================================================

	namespace Data
	{
		/** Instant heal amount (SetByCaller) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Medical_InstantHeal);

		/** Heal over time per tick (SetByCaller) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Medical_HealPerTick);

		/** HoT duration in seconds (SetByCaller) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Medical_HoTDuration);
	}

	//==================================================================
	// Effect Tags
	// Used for GameplayEffect identification
	//==================================================================

	namespace Effect
	{
		/** Instant heal effect tag */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Effect_Medical_InstantHeal);

		/** Heal over time effect tag */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Effect_Medical_HealOverTime);

		/** Medical item in use (blocking other uses) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Effect_Medical_InProgress);
	}

	//==================================================================
	// Event Tags
	// Published to EventBus during medical operations
	//==================================================================

	namespace Event
	{
		/** Healing applied */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_HealApplied);

		/** Bleeding cured */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_BleedingCured);

		/** Status effect cured */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_StatusCured);

		/** HoT started */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_HoTStarted);

		/** HoT interrupted */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_HoTInterrupted);

		// ══════════════════════════════════════════════════════════════
		// Animation-Driven Flow Events (Tarkov-style)
		// ══════════════════════════════════════════════════════════════

		/** Medical item equipped (draw animation started) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_Equipped);

		/** Medical item unequipped (returned to previous weapon) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_Unequipped);

		/** Medical item ready after equip animation */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_Ready);

		/** Medical use animation started */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_UseStarted);

		/** Medical effect should be applied (AnimNotify "Apply") */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_ApplyEffect);

		/** Medical use animation completed successfully */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_UseComplete);

		/** Medical use was cancelled (damage interrupt) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Medical_UseCancelled);
	}

	//==================================================================
	// State Tags
	// Applied to character during medical effects
	//==================================================================

	namespace State
	{
		/** Heal over time is active */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Medical_Regenerating);

		/** Medical item currently being used */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Medical_UsingItem);

		/** Medical item is equipped and ready for use (Tarkov-style flow) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Medical_Equipped);

		/** Medical item use animation is in progress */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Medical_UsingAnimation);
	}

	//==================================================================
	// Cure Tags
	// What medical items can cure
	// @see FSuspenseCoreConsumableAttributeRow
	//==================================================================

	namespace Cure
	{
		/** Can cure light bleeding */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cure_Bleeding_Light);

		/** Can cure heavy bleeding */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cure_Bleeding_Heavy);

		/** Can cure all bleeding */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cure_Bleeding_All);

		/** Can cure fractures */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cure_Fracture);

		/** Can cure pain */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cure_Pain);
	}

	//==================================================================
	// Ability Tags
	// Used for ability identification and activation
	// @see GA_MedicalEquip, GA_MedicalUse
	//==================================================================

	namespace Ability
	{
		/** Medical equip ability tag (for activation/cancellation) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Medical_Equip);

		/** Medical use ability tag (for activation/cancellation) */
		GAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Medical_Use);
	}
}

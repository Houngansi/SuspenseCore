// SuspenseCoreStatusEffects.h
// GameplayEffect classes for Status Effect System v2.0
// Copyright Suspense Team. All Rights Reserved.
//
// This file contains all GameplayEffect classes for the buff/debuff system.
// These are C++ base classes that can be extended in Blueprints.
//
// @see Documentation/GameDesign/StatusEffect_System_GDD.md
// @see FSuspenseCoreStatusEffectVisualRow (visual data in DataTable)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SuspenseCoreStatusEffects.generated.h"

//========================================================================
// BASE CLASSES
//========================================================================

/**
 * USuspenseCoreEffect_DoT_Base
 *
 * Base class for all Damage-over-Time effects.
 * Subclasses: Bleeding, Burning, Poisoned
 *
 * Common configuration:
 * - Periodic damage via IncomingDamage attribute
 * - SetByCaller magnitudes for damage/duration
 * - EventBus-compatible tags
 */
UCLASS(Abstract, Blueprintable)
class GAS_API USuspenseCoreEffect_DoT_Base : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_DoT_Base();
};


/**
 * USuspenseCoreEffect_Buff_Base
 *
 * Base class for all positive buff effects.
 * Subclasses: Regenerating, Painkiller, Adrenaline, Fortified, Haste
 *
 * Common configuration:
 * - Has Duration (SetByCaller or fixed)
 * - Attribute modifiers (speed, regen, etc.)
 * - No stacking by default (refresh duration)
 */
UCLASS(Abstract, Blueprintable)
class GAS_API USuspenseCoreEffect_Buff_Base : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_Buff_Base();
};


/**
 * USuspenseCoreEffect_Debuff_Base
 *
 * Base class for non-DoT debuffs (Stunned, Suppressed, Fracture).
 *
 * Common configuration:
 * - Has Duration (SetByCaller or fixed)
 * - May have attribute modifiers (speed penalty, aim penalty)
 * - Movement/action restrictions via tags
 */
UCLASS(Abstract, Blueprintable)
class GAS_API USuspenseCoreEffect_Debuff_Base : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEffect_Debuff_Base();
};


//========================================================================
// DEBUFFS - DoT
//========================================================================

/**
 * UGE_Poisoned
 *
 * Poison damage over time with attribute debuffs.
 * - Duration: 30 seconds (SetByCaller)
 * - Damage: 2 HP per 2 seconds
 * - Debuff: -10% movement speed
 * - Cure: Antidote, Augmentin
 *
 * Tag: State.Health.Poisoned
 */
UCLASS(Blueprintable)
class GAS_API UGE_Poisoned : public USuspenseCoreEffect_DoT_Base
{
	GENERATED_BODY()

public:
	UGE_Poisoned();
};


//========================================================================
// DEBUFFS - Combat
//========================================================================

/**
 * UGE_Stunned
 *
 * Complete movement/action disable (flashbang, concussion).
 * - Duration: 2-5 seconds (SetByCaller)
 * - Effect: Cannot move, aim, or fire
 * - Cure: Wait for duration
 *
 * Tag: State.Combat.Stunned
 */
UCLASS(Blueprintable)
class GAS_API UGE_Stunned : public USuspenseCoreEffect_Debuff_Base
{
	GENERATED_BODY()

public:
	UGE_Stunned();
};


/**
 * UGE_Suppressed
 *
 * Aim penalty from nearby enemy fire.
 * - Duration: 3 seconds (refreshes on new suppression)
 * - Effect: -30% aim accuracy, -20% recoil control
 * - Cure: Leave combat area
 *
 * Tag: State.Combat.Suppressed
 */
UCLASS(Blueprintable)
class GAS_API UGE_Suppressed : public USuspenseCoreEffect_Debuff_Base
{
	GENERATED_BODY()

public:
	UGE_Suppressed();
};


//========================================================================
// DEBUFFS - Fractures
//========================================================================

/**
 * UGE_Fracture_Leg
 *
 * Broken leg - severe movement impairment.
 * - Duration: Infinite (requires surgery/splint)
 * - Effect: -40% movement speed, prevents sprinting, causes limp
 * - Cure: Splint, Surgical Kit
 *
 * Tag: State.Health.Fracture.Leg
 */
UCLASS(Blueprintable)
class GAS_API UGE_Fracture_Leg : public USuspenseCoreEffect_Debuff_Base
{
	GENERATED_BODY()

public:
	UGE_Fracture_Leg();
};


/**
 * UGE_Fracture_Arm
 *
 * Broken arm - severe aiming impairment.
 * - Duration: Infinite (requires surgery/splint)
 * - Effect: -50% aim speed, prevents ADS
 * - Cure: Splint, Surgical Kit
 *
 * Tag: State.Health.Fracture.Arm
 */
UCLASS(Blueprintable)
class GAS_API UGE_Fracture_Arm : public USuspenseCoreEffect_Debuff_Base
{
	GENERATED_BODY()

public:
	UGE_Fracture_Arm();
};


//========================================================================
// DEBUFFS - Survival
//========================================================================

/**
 * UGE_Dehydrated
 *
 * Dehydration from lack of water.
 * - Duration: Infinite (until water consumed)
 * - Effect: 1 HP per 5 seconds drain
 * - Cure: Water, Juice, MRE
 *
 * Tag: State.Health.Dehydrated
 */
UCLASS(Blueprintable)
class GAS_API UGE_Dehydrated : public USuspenseCoreEffect_DoT_Base
{
	GENERATED_BODY()

public:
	UGE_Dehydrated();
};


/**
 * UGE_Exhausted
 *
 * Exhaustion from lack of food.
 * - Duration: Infinite (until food consumed)
 * - Effect: Stamina does not regenerate, prevents sprinting
 * - Cure: Food, MRE, Energy Bar
 *
 * Tag: State.Health.Exhausted
 */
UCLASS(Blueprintable)
class GAS_API UGE_Exhausted : public USuspenseCoreEffect_Debuff_Base
{
	GENERATED_BODY()

public:
	UGE_Exhausted();
};


//========================================================================
// BUFFS - Healing
//========================================================================

/**
 * UGE_Regenerating
 *
 * Health regeneration over time (medkit, stimulant).
 * - Duration: 10-30 seconds (SetByCaller)
 * - Effect: +5 HP per second
 * - Source: Medkit, Healing Stimulant
 *
 * Tag: State.Health.Regenerating
 */
UCLASS(Blueprintable)
class GAS_API UGE_Regenerating : public USuspenseCoreEffect_Buff_Base
{
	GENERATED_BODY()

public:
	UGE_Regenerating();
};


//========================================================================
// BUFFS - Combat
//========================================================================

/**
 * UGE_Painkiller
 *
 * Pain suppression - ignore injury effects.
 * - Duration: 60-300 seconds (SetByCaller)
 * - Effect: Can sprint with fractures, ignore limp animation
 * - Source: Painkillers, Morphine
 *
 * Tag: State.Combat.Painkiller
 */
UCLASS(Blueprintable)
class GAS_API UGE_Painkiller : public USuspenseCoreEffect_Buff_Base
{
	GENERATED_BODY()

public:
	UGE_Painkiller();
};


/**
 * UGE_Adrenaline
 *
 * Combat adrenaline rush.
 * - Duration: 30-60 seconds (SetByCaller)
 * - Effect: +15% movement speed, +25% stamina regen
 * - Source: Combat stimulant, natural combat response
 *
 * Tag: State.Combat.Adrenaline
 */
UCLASS(Blueprintable)
class GAS_API UGE_Adrenaline : public USuspenseCoreEffect_Buff_Base
{
	GENERATED_BODY()

public:
	UGE_Adrenaline();
};


/**
 * UGE_Fortified
 *
 * Damage resistance buff.
 * - Duration: 15-30 seconds (SetByCaller)
 * - Effect: +15% damage resistance
 * - Source: Armor ability, defensive stimulant
 *
 * Tag: State.Combat.Fortified
 */
UCLASS(Blueprintable)
class GAS_API UGE_Fortified : public USuspenseCoreEffect_Buff_Base
{
	GENERATED_BODY()

public:
	UGE_Fortified();
};


//========================================================================
// BUFFS - Movement
//========================================================================

/**
 * UGE_Haste
 *
 * Movement speed buff.
 * - Duration: 10-20 seconds (SetByCaller)
 * - Effect: +20% movement speed
 * - Source: Speed stimulant
 *
 * Tag: State.Movement.Haste
 */
UCLASS(Blueprintable)
class GAS_API UGE_Haste : public USuspenseCoreEffect_Buff_Base
{
	GENERATED_BODY()

public:
	UGE_Haste();
};

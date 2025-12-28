// SuspenseCoreAttributeDefaults.h
// SuspenseCore - Base Attribute Constants (SSOT)
// Copyright (c) 2025. All Rights Reserved.

#pragma once

/**
 * FSuspenseCoreAttributeDefaults
 *
 * Single Source of Truth for base attribute values.
 * These values are multiplied by FSuspenseCoreAttributeModifier from CharacterClassData.
 *
 * Example:
 *   Tank class: MaxHealthMultiplier = 1.5
 *   Final MaxHealth = BaseMaxHealth (100) * 1.5 = 150
 *
 * Usage:
 *   #include "SuspenseCore/Attributes/SuspenseCoreAttributeDefaults.h"
 *   float FinalHealth = FSuspenseCoreAttributeDefaults::BaseMaxHealth * ClassData->AttributeModifiers.MaxHealthMultiplier;
 */
namespace FSuspenseCoreAttributeDefaults
{
	// ═══════════════════════════════════════════════════════════════════════════
	// HEALTH & SURVIVABILITY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base max health before class multipliers */
	inline constexpr float BaseMaxHealth = 100.0f;

	/** Base health regeneration per second */
	inline constexpr float BaseHealthRegen = 1.0f;

	/** Base armor value (damage reduction) */
	inline constexpr float BaseArmor = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// SHIELD
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base max shield before class multipliers */
	inline constexpr float BaseMaxShield = 100.0f;

	/** Base shield regeneration per second */
	inline constexpr float BaseShieldRegen = 10.0f;

	/** Base delay before shield starts regenerating (seconds) */
	inline constexpr float BaseShieldRegenDelay = 3.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// STAMINA
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base max stamina before class multipliers */
	inline constexpr float BaseMaxStamina = 100.0f;

	/** Base stamina regeneration per second */
	inline constexpr float BaseStaminaRegen = 10.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// COMBAT
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base attack power multiplier (1.0 = normal damage) */
	inline constexpr float BaseAttackPower = 1.0f;

	/** Base accuracy multiplier (1.0 = normal accuracy) */
	inline constexpr float BaseAccuracy = 1.0f;

	/** Base reload speed multiplier (1.0 = normal speed) */
	inline constexpr float BaseReloadSpeed = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// MOVEMENT (Tarkov-style: slow tactical walk, fast sprint)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base movement speed multiplier (1.0 = normal) */
	inline constexpr float BaseMovementSpeed = 1.0f;

	/**
	 * Base walk speed in cm/s (Tarkov-style: slow tactical walk)
	 * This is the DEFAULT movement speed - player walks slowly
	 */
	inline constexpr float BaseWalkSpeed = 200.0f;

	/**
	 * Sprint speed in cm/s (absolute value, not multiplier)
	 * When sprinting, character moves at this speed regardless of walk speed
	 */
	inline constexpr float BaseSprintSpeed = 600.0f;

	/**
	 * Crouch speed in cm/s (even slower than walk)
	 */
	inline constexpr float BaseCrouchSpeed = 100.0f;

	/** Base jump height in cm */
	inline constexpr float BaseJumpHeight = 420.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// WEIGHT & CARRY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base max weight capacity */
	inline constexpr float BaseMaxWeight = 50.0f;
}

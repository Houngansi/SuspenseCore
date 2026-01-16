// SuspenseCoreSpreadCalculator.cpp
// SuspenseCore - Attribute-Based Spread Calculator Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadProcessor.h"
#include "SuspenseCore/Core/SuspenseCoreUnits.h"
#include "AbilitySystemComponent.h"

float USuspenseCoreSpreadCalculator::CalculateSpreadWithAttributes(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
	const USuspenseCoreAmmoAttributeSet* AmmoAttributes,
	bool bIsAiming,
	float MovementSpeed,
	float RecoilModifier)
{
	// Default spread if no weapon attributes
	constexpr float DefaultHipSpread = 3.0f;
	constexpr float DefaultAimSpread = 1.0f;

	// Get base spread from weapon
	float BaseSpread;
	if (WeaponAttributes)
	{
		if (bIsAiming)
		{
			BaseSpread = WeaponAttributes->GetAimSpread();
		}
		else
		{
			BaseSpread = WeaponAttributes->GetHipFireSpread();
		}
	}
	else
	{
		BaseSpread = bIsAiming ? DefaultAimSpread : DefaultHipSpread;
	}

	// Apply ammo accuracy modifier (lower = more accurate)
	// AccuracyModifier of 1.0 means no change
	// AccuracyModifier of 0.8 means 20% tighter spread
	// AccuracyModifier of 1.2 means 20% wider spread
	if (AmmoAttributes)
	{
		float AccuracyMod = AmmoAttributes->GetAccuracyModifier();
		if (AccuracyMod > 0.0f)
		{
			BaseSpread *= AccuracyMod;
		}
	}

	// Use base processor for state modifiers (movement, etc.)
	return USuspenseCoreSpreadProcessor::CalculateCurrentSpread(
		BaseSpread,
		bIsAiming,
		MovementSpeed,
		RecoilModifier
	);
}

float USuspenseCoreSpreadCalculator::CalculateSpreadFromASC(
	UAbilitySystemComponent* ASC,
	bool bIsAiming,
	float MovementSpeed,
	float RecoilModifier)
{
	if (!ASC)
	{
		// Fallback to default calculation
		return USuspenseCoreSpreadProcessor::CalculateCurrentSpread(
			bIsAiming ? 1.0f : 3.0f,
			bIsAiming,
			MovementSpeed,
			RecoilModifier
		);
	}

	// Get weapon attributes
	const USuspenseCoreWeaponAttributeSet* WeaponAttrs =
		ASC->GetSet<USuspenseCoreWeaponAttributeSet>();

	// Get ammo attributes (may be on same ASC or different one)
	const USuspenseCoreAmmoAttributeSet* AmmoAttrs =
		ASC->GetSet<USuspenseCoreAmmoAttributeSet>();

	return CalculateSpreadWithAttributes(
		WeaponAttrs,
		AmmoAttrs,
		bIsAiming,
		MovementSpeed,
		RecoilModifier
	);
}

float USuspenseCoreSpreadCalculator::CalculateFinalDamage(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
	const USuspenseCoreAmmoAttributeSet* AmmoAttributes,
	float CharacterDamageBonus)
{
	// Start with weapon base damage
	float BaseDamage = 25.0f; // Default
	if (WeaponAttributes)
	{
		BaseDamage = WeaponAttributes->GetBaseDamage();
	}

	// Ammo provides additional damage through its own BaseDamage
	// In realistic systems, ammo is the PRIMARY damage source
	float AmmoDamage = BaseDamage;
	if (AmmoAttributes)
	{
		// Use ammo's base damage if available
		float AmmoBase = AmmoAttributes->GetBaseDamage();
		if (AmmoBase > 0.0f)
		{
			AmmoDamage = AmmoBase;
		}

		// Stopping power adds additional damage
		float StoppingPower = AmmoAttributes->GetStoppingPower();
		AmmoDamage += StoppingPower * 0.1f; // 10% of stopping power as bonus damage
	}

	// Apply character bonus (skills, perks, etc.)
	float FinalDamage = AmmoDamage * (1.0f + CharacterDamageBonus);

	return FinalDamage;
}

float USuspenseCoreSpreadCalculator::CalculateArmorPenetration(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
	const USuspenseCoreAmmoAttributeSet* AmmoAttributes)
{
	float Penetration = 0.0f;

	// Ammo provides primary penetration value
	if (AmmoAttributes)
	{
		Penetration = AmmoAttributes->GetArmorPenetration();
	}

	// Weapon may have penetration bonus (e.g., longer barrel)
	// Currently not in WeaponAttributeSet but could be added

	return Penetration;
}

float USuspenseCoreSpreadCalculator::CalculateEffectiveRange(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
	const USuspenseCoreAmmoAttributeSet* AmmoAttributes)
{
	// NOTE: This function returns METERS (DataTable units)
	// Used for damage falloff calculations, NOT for trace distance
	// For trace distance, use CalculateMaxTraceRange() instead

	float WeaponRange = 400.0f; // Default effective range (meters)
	float AmmoRange = 400.0f;

	if (WeaponAttributes)
	{
		WeaponRange = WeaponAttributes->GetEffectiveRange();
	}

	if (AmmoAttributes)
	{
		AmmoRange = AmmoAttributes->GetEffectiveRange();
	}

	// Effective range is the minimum of both
	// (e.g., pistol can't shoot rifle ammo at rifle ranges)
	return FMath::Min(WeaponRange, AmmoRange);
}

float USuspenseCoreSpreadCalculator::CalculateMaxTraceRange(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
	const USuspenseCoreAmmoAttributeSet* AmmoAttributes)
{
	// =============================================================================
	// MAXIMUM TRACE RANGE CALCULATION
	// =============================================================================
	// Uses MaxRange (maximum bullet travel distance) from weapon attributes.
	// DataTable stores values in METERS, this function converts to UE units.
	//
	// IMPORTANT: This is the correct function for trace endpoint calculation.
	// DO NOT use CalculateEffectiveRange() - that's for damage falloff.
	//
	// Data flow:
	//   JSON: MaxRange = 600 (meters)
	//   → WeaponAttributeSet::MaxRange = 600.0f (meters, as loaded)
	//   → CalculateMaxTraceRange() = 60000.0f (UE units, converted)
	//   → LineTrace endpoint = MuzzleLocation + Direction * 60000
	//
	// @see SuspenseCoreUnits::ConvertRangeToUnits()
	// @see Documentation/GAS/UnitConversionSystem.md
	// =============================================================================

	float MaxRangeMeters = 0.0f;

	// Get weapon's maximum range (primary source)
	if (WeaponAttributes)
	{
		MaxRangeMeters = WeaponAttributes->GetMaxRange();
	}

	// Ammo may have shorter effective range (e.g., subsonic rounds)
	// Take minimum if ammo limits range
	if (AmmoAttributes)
	{
		float AmmoEffectiveRange = AmmoAttributes->GetEffectiveRange();

		// Only limit if ammo has shorter range AND we have weapon data
		// (avoids taking min with zero if weapon attrs missing)
		if (MaxRangeMeters > 0.0f && AmmoEffectiveRange > 0.0f)
		{
			// Ammo effective range can limit max trace range
			// (subsonic rounds don't fly as far as supersonic)
			// But typically MaxRange > AmmoEffectiveRange, so this rarely triggers
			MaxRangeMeters = FMath::Max(MaxRangeMeters, AmmoEffectiveRange);
		}
	}

	// Convert from meters to UE units with validation
	return SuspenseCoreUnits::ConvertRangeToUnits(MaxRangeMeters);
}

float USuspenseCoreSpreadCalculator::CalculateMaxTraceRangeFromWeapon(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes)
{
	// Weapon-only variant - no ammo consideration
	// Useful when ammo attributes aren't loaded yet (e.g., before first shot)

	if (!WeaponAttributes)
	{
		return SuspenseCoreUnits::DefaultTraceRangeUnits;
	}

	float MaxRangeMeters = WeaponAttributes->GetMaxRange();
	return SuspenseCoreUnits::ConvertRangeToUnits(MaxRangeMeters);
}

float USuspenseCoreSpreadCalculator::CalculateRecoil(
	const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
	const USuspenseCoreAmmoAttributeSet* AmmoAttributes,
	bool bIsAiming,
	float ADSRecoilMultiplier)
{
	float BaseRecoil = 1.0f; // Default vertical recoil

	if (WeaponAttributes)
	{
		BaseRecoil = WeaponAttributes->GetVerticalRecoil();
	}

	// Ammo may modify recoil (hotter loads = more recoil)
	if (AmmoAttributes)
	{
		float RecoilMod = AmmoAttributes->GetRecoilModifier();
		if (RecoilMod > 0.0f)
		{
			BaseRecoil *= RecoilMod;
		}
	}

	// ADS reduces recoil
	if (bIsAiming)
	{
		BaseRecoil *= ADSRecoilMultiplier;
	}

	return BaseRecoil;
}

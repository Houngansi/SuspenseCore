// SuspenseCoreSpreadCalculator.cpp
// SuspenseCore - Attribute-Based Spread Calculator Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreSpreadCalculator.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCore/Utils/SuspenseCoreSpreadProcessor.h"
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
	float WeaponRange = 10000.0f; // Default max range
	float AmmoRange = 10000.0f;

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

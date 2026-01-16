// SuspenseCoreSpreadCalculator.h
// SuspenseCore - Attribute-Based Spread Calculator
// Copyright Suspense Team. All Rights Reserved.
//
// GAS module extension for SpreadProcessor that can access WeaponAttributeSet.
// Provides full attribute-based spread calculation including ammo modifiers.
//
// Usage:
//   float Spread = USuspenseCoreSpreadCalculator::CalculateSpreadFromWeapon(
//       WeaponASC, bIsAiming, MovementSpeed, RecoilModifier);

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SuspenseCoreSpreadCalculator.generated.h"

class UAbilitySystemComponent;
class USuspenseCoreWeaponAttributeSet;
class USuspenseCoreAmmoAttributeSet;

/**
 * USuspenseCoreSpreadCalculator
 *
 * GAS module spread calculator that integrates:
 * - Weapon attributes (base spread, aim spread)
 * - Ammo attributes (accuracy modifier)
 * - Character attributes (if applicable)
 *
 * This extends the BridgeSystem's USuspenseCoreSpreadProcessor with
 * full GAS attribute access for Tarkov-style damage calculation:
 *
 * Final Damage = Weapon.BaseDamage * Ammo.DamageModifier * CharacterBonus
 * Final Spread = Weapon.BaseSpread * Ammo.AccuracyModifier * StateMods
 */
UCLASS()
class GAS_API USuspenseCoreSpreadCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Calculate spread using weapon and ammo attributes.
	 * Combines weapon's spread values with ammo's accuracy modifier.
	 *
	 * @param WeaponASC AbilitySystemComponent with weapon attributes
	 * @param AmmoAttributes Optional ammo attribute set for accuracy modifier
	 * @param bIsAiming Is player aiming down sights
	 * @param MovementSpeed Current movement speed
	 * @param RecoilModifier Current recoil multiplier
	 * @return Final spread value in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Spread")
	static float CalculateSpreadWithAttributes(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
		const USuspenseCoreAmmoAttributeSet* AmmoAttributes,
		bool bIsAiming,
		float MovementSpeed,
		float RecoilModifier = 1.0f
	);

	/**
	 * Calculate spread using ASC to find attributes.
	 * Automatically finds WeaponAttributeSet on the ASC.
	 *
	 * @param ASC AbilitySystemComponent with weapon attributes
	 * @param bIsAiming Is player aiming down sights
	 * @param MovementSpeed Current movement speed
	 * @param RecoilModifier Current recoil multiplier
	 * @return Final spread value in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Spread")
	static float CalculateSpreadFromASC(
		UAbilitySystemComponent* ASC,
		bool bIsAiming,
		float MovementSpeed,
		float RecoilModifier = 1.0f
	);

	/**
	 * Calculate damage with all attribute modifiers.
	 * Combines weapon damage + ammo damage + character bonuses.
	 *
	 * Formula: BaseDamage * AmmoDamageMultiplier * (1 + CharacterBonus)
	 *
	 * @param WeaponAttributes Weapon attribute set
	 * @param AmmoAttributes Ammo attribute set
	 * @param CharacterDamageBonus Additional character-based damage bonus (0.0 = none)
	 * @return Final damage value
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
	static float CalculateFinalDamage(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
		const USuspenseCoreAmmoAttributeSet* AmmoAttributes,
		float CharacterDamageBonus = 0.0f
	);

	/**
	 * Calculate armor penetration capability.
	 * Combines weapon and ammo penetration values.
	 *
	 * @param WeaponAttributes Weapon attribute set (may have penetration bonus)
	 * @param AmmoAttributes Ammo attribute set with ArmorPenetration
	 * @return Penetration value (higher = more armor penetration)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
	static float CalculateArmorPenetration(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
		const USuspenseCoreAmmoAttributeSet* AmmoAttributes
	);

	/**
	 * Calculate effective range based on weapon and ammo.
	 * Uses minimum of weapon and ammo effective ranges.
	 * NOTE: Returns value in METERS (for damage falloff calculations).
	 *
	 * @param WeaponAttributes Weapon attribute set
	 * @param AmmoAttributes Ammo attribute set
	 * @return Effective range in METERS (DataTable units)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Range")
	static float CalculateEffectiveRange(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
		const USuspenseCoreAmmoAttributeSet* AmmoAttributes
	);

	/**
	 * Calculate maximum trace range for weapon line traces.
	 * Uses weapon's MaxRange attribute (maximum bullet travel distance).
	 * Automatically converts from meters (DataTable) to UE units (trace).
	 *
	 * This is the PRIMARY function for determining trace endpoint distance.
	 * DO NOT use CalculateEffectiveRange() for traces - that's for damage falloff.
	 *
	 * @param WeaponAttributes Weapon attribute set
	 * @param AmmoAttributes Ammo attribute set (may limit range for certain ammo types)
	 * @return Maximum trace range in UE UNITS (centimeters)
	 *
	 * @see SuspenseCoreUnits::ConvertRangeToUnits() for conversion details
	 * @see FSuspenseCoreWeaponAttributeRow::MaxRange - Source data in meters
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Range")
	static float CalculateMaxTraceRange(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
		const USuspenseCoreAmmoAttributeSet* AmmoAttributes
	);

	/**
	 * Calculate maximum trace range for weapon (weapon-only variant).
	 * Uses when ammo attributes are not available.
	 *
	 * @param WeaponAttributes Weapon attribute set
	 * @return Maximum trace range in UE UNITS (centimeters)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Range")
	static float CalculateMaxTraceRangeFromWeapon(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes
	);

	/**
	 * Calculate recoil magnitude based on weapon and ammo.
	 * Ammo can modify recoil through RecoilModifier.
	 *
	 * @param WeaponAttributes Weapon attribute set
	 * @param AmmoAttributes Ammo attribute set
	 * @param bIsAiming Is player aiming (reduces recoil)
	 * @param ADSRecoilMultiplier ADS recoil reduction (default 0.5)
	 * @return Final vertical recoil value
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	static float CalculateRecoil(
		const USuspenseCoreWeaponAttributeSet* WeaponAttributes,
		const USuspenseCoreAmmoAttributeSet* AmmoAttributes,
		bool bIsAiming,
		float ADSRecoilMultiplier = 0.5f
	);
};

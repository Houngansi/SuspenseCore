// SuspenseCoreGASAttributeRows.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.
//
// DataTable Row Structures for GAS AttributeSets
// These structures are used to import JSON data into DataTables
// and serve as the SINGLE SOURCE OF TRUTH (SSOT) for attribute values.
//
// @see Documentation/Plans/SSOT_AttributeSet_DataTable_Integration.md
// @see Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h
// @see Source/GAS/Public/SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreGASAttributeRows.generated.h"


/**
 * FSuspenseCoreWeaponAttributeRow
 *
 * DataTable row structure for weapon attributes.
 * Maps 1:1 to USuspenseCoreWeaponAttributeSet attributes.
 *
 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreWeaponAttributes.json
 * TARGET DATATABLE: DT_WeaponAttributes
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Import JSON into DataTable via Editor (File → Import)
 * 2. Configure WeaponAttributesDataTable in Project Settings → SuspenseCore
 * 3. DataManager caches rows on Initialize()
 * 4. EquipmentAttributeComponent calls DataManager->GetWeaponAttributes()
 * 5. WeaponAttributeSet->InitializeFromData(Row)
 *
 * @see USuspenseCoreWeaponAttributeSet - 19 GAS attributes
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity (связь с ItemDataTable)
	//========================================================================

	/** Unique weapon identifier - matches FSuspenseCoreUnifiedItemData.ItemID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName WeaponID;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText WeaponName;

	/** Weapon type classification (AssaultRifle, SMG, Pistol, DMR, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponType;

	/** Caliber tag for ammo compatibility */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "Item.Ammo"))
	FGameplayTag Caliber;

	//========================================================================
	// Combat Attributes (1:1 mapping to USuspenseCoreWeaponAttributeSet)
	//========================================================================

	/** Base damage per hit before armor calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "1", ClampMax = "500", ToolTip = "Base damage per hit"))
	float BaseDamage = 42.0f;

	/** Rounds per minute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "1", ClampMax = "1500", ToolTip = "Rate of fire (RPM)"))
	float RateOfFire = 650.0f;

	/** Optimal engagement distance (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "1", ClampMax = "2000", ToolTip = "Effective range in meters"))
	float EffectiveRange = 350.0f;

	/** Maximum projectile travel distance (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "1", ClampMax = "3000", ToolTip = "Maximum range in meters"))
	float MaxRange = 600.0f;

	/** Magazine capacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "1", ClampMax = "200", ToolTip = "Magazine size"))
	float MagazineSize = 30.0f;

	/** Reload time with round in chamber (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "0.1", ClampMax = "10", ToolTip = "Tactical reload time"))
	float TacticalReloadTime = 2.1f;

	/** Full reload time from empty (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
		meta = (ClampMin = "0.1", ClampMax = "15", ToolTip = "Full reload time"))
	float FullReloadTime = 2.8f;

	//========================================================================
	// Accuracy Attributes
	//========================================================================

	/** Minute of Angle - base accuracy (lower = more accurate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0.1", ClampMax = "10", ToolTip = "MOA accuracy"))
	float MOA = 2.9f;

	/** Spread when hip firing (radians) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0.01", ClampMax = "0.5", ToolTip = "Hip fire spread"))
	float HipFireSpread = 0.12f;

	/** Spread when aiming down sights (radians) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0.001", ClampMax = "0.1", ToolTip = "ADS spread"))
	float AimSpread = 0.025f;

	/** Vertical recoil impulse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0", ClampMax = "500", ToolTip = "Vertical recoil"))
	float VerticalRecoil = 145.0f;

	/** Horizontal recoil impulse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0", ClampMax = "500", ToolTip = "Horizontal recoil"))
	float HorizontalRecoil = 280.0f;

	//========================================================================
	// Reliability Attributes
	//========================================================================

	/** Current weapon condition (0-100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Current durability"))
	float Durability = 100.0f;

	/** Maximum durability value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
		meta = (ClampMin = "1", ClampMax = "100", ToolTip = "Maximum durability"))
	float MaxDurability = 100.0f;

	/** Chance of misfire per shot (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Misfire chance"))
	float MisfireChance = 0.001f;

	/** Chance of weapon jam per shot (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reliability",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Jam chance"))
	float JamChance = 0.002f;

	//========================================================================
	// Ergonomics Attributes
	//========================================================================

	/** Overall handling quality (higher = better) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
		meta = (ClampMin = "1", ClampMax = "100", ToolTip = "Ergonomics rating"))
	float Ergonomics = 42.0f;

	/** Time to raise weapon to ADS (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
		meta = (ClampMin = "0.05", ClampMax = "2", ToolTip = "ADS time"))
	float AimDownSightTime = 0.35f;

	/** Weapon weight in kilograms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
		meta = (ClampMin = "0.1", ClampMax = "20", ToolTip = "Weight in kg"))
	float WeaponWeight = 3.4f;

	//========================================================================
	// Fire Modes (metadata, not GAS attributes)
	//========================================================================

	/** Available fire modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireModes",
		meta = (Categories = "Weapon.FireMode"))
	TArray<FGameplayTag> FireModes;

	/** Default fire mode on equip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireModes",
		meta = (Categories = "Weapon.FireMode"))
	FGameplayTag DefaultFireMode;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !WeaponID.IsNone() && BaseDamage > 0.0f;
	}
};


/**
 * FSuspenseCoreAmmoAttributeRow
 *
 * DataTable row structure for ammunition attributes.
 * Maps 1:1 to USuspenseCoreAmmoAttributeSet attributes.
 *
 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreAmmoAttributes.json
 * TARGET DATATABLE: DT_AmmoAttributes
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Import JSON into DataTable via Editor
 * 2. Configure AmmoAttributesDataTable in Project Settings → SuspenseCore
 * 3. DataManager caches rows on Initialize()
 * 4. On reload: DataManager->GetAmmoAttributes(LoadedAmmoID)
 * 5. AmmoAttributeSet->InitializeFromData(Row)
 *
 * TARKOV-STYLE AMMO SYSTEM:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Ammo stored as items in inventory (grid-based)
 * - Magazines are separate items with internal capacity
 * - QuickSlots 1-4 for fast reload access
 * - Different ammo types affect weapon behavior
 *
 * @see USuspenseCoreAmmoAttributeSet - 15 GAS attributes
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAmmoAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity
	//========================================================================

	/** Unique ammo identifier - matches FSuspenseCoreUnifiedItemData.ItemID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName AmmoID;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText AmmoName;

	/** Caliber tag for weapon compatibility */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "Item.Ammo"))
	FGameplayTag Caliber;

	//========================================================================
	// Damage Attributes (1:1 mapping to USuspenseCoreAmmoAttributeSet)
	//========================================================================

	/** Base damage of the round */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ClampMin = "1", ClampMax = "500", ToolTip = "Base damage"))
	float BaseDamage = 42.0f;

	/** Armor penetration value (higher = penetrates more armor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Armor penetration"))
	float ArmorPenetration = 25.0f;

	/** Stopping power multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ClampMin = "0", ClampMax = "2", ToolTip = "Stopping power"))
	float StoppingPower = 0.35f;

	/** Chance for round to fragment on impact (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Fragmentation chance"))
	float FragmentationChance = 0.40f;

	//========================================================================
	// Ballistics Attributes
	//========================================================================

	/** Initial velocity at muzzle (m/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics",
		meta = (ClampMin = "100", ClampMax = "2000", ToolTip = "Muzzle velocity"))
	float MuzzleVelocity = 890.0f;

	/** Air resistance coefficient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics",
		meta = (ClampMin = "0.01", ClampMax = "1", ToolTip = "Drag coefficient"))
	float DragCoefficient = 0.168f;

	/** Bullet mass in grams */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics",
		meta = (ClampMin = "0.1", ClampMax = "100", ToolTip = "Bullet mass (g)"))
	float BulletMass = 3.4f;

	/** Effective engagement range for this ammo (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics",
		meta = (ClampMin = "1", ClampMax = "2000", ToolTip = "Effective range"))
	float EffectiveRange = 350.0f;

	//========================================================================
	// Accuracy Modifiers
	//========================================================================

	/** Accuracy multiplier when using this ammo (1.0 = neutral) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0.5", ClampMax = "1.5", ToolTip = "Accuracy modifier"))
	float AccuracyModifier = 1.0f;

	/** Recoil multiplier when using this ammo (1.0 = neutral) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
		meta = (ClampMin = "0.5", ClampMax = "2", ToolTip = "Recoil modifier"))
	float RecoilModifier = 1.0f;

	//========================================================================
	// Special Effects
	//========================================================================

	/** Chance to ricochet off surfaces (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Ricochet chance"))
	float RicochetChance = 0.30f;

	/** Tracer visibility (0 = not tracer, 1 = full tracer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Tracer visibility"))
	float TracerVisibility = 0.0f;

	/** Additional fire damage on hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Incendiary damage"))
	float IncendiaryDamage = 0.0f;

	//========================================================================
	// Weapon Effects
	//========================================================================

	/** Weapon durability degradation rate multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (ClampMin = "0.5", ClampMax = "3", ToolTip = "Degradation rate"))
	float WeaponDegradationRate = 1.0f;

	/** Misfire chance specific to this ammo type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Misfire chance"))
	float MisfireChance = 0.001f;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !AmmoID.IsNone() && BaseDamage > 0.0f;
	}
};


/**
 * FSuspenseCoreArmorAttributeRow (FUTURE)
 *
 * Placeholder for armor attribute DataTable row.
 * Will be implemented when armor system is extended.
 *
 * @see USuspenseCoreArmorAttributeSet
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreArmorAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName ArmorID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText ArmorName;

	// Armor class (1-6 like Tarkov)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection",
		meta = (ClampMin = "1", ClampMax = "6"))
	int32 ArmorClass = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection")
	float Durability = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection")
	float MaxDurability = 40.0f;

	// Effective durability (material-based multiplier)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection")
	float EffectiveDurability = 1.0f;

	// Movement speed penalty
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
		meta = (ClampMin = "0", ClampMax = "0.5"))
	float SpeedPenalty = 0.1f;

	// Turn speed penalty
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
		meta = (ClampMin = "0", ClampMax = "0.5"))
	float TurnSpeedPenalty = 0.08f;

	// Ergonomics penalty
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ergonomics",
		meta = (ClampMin = "-50", ClampMax = "0"))
	float ErgonomicsPenalty = -8.0f;

	bool IsValid() const
	{
		return !ArmorID.IsNone() && MaxDurability > 0.0f;
	}
};

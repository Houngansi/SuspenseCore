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
	// Recoil Dynamics (Tarkov-Style Convergence)
	// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md
	//========================================================================

	/** Convergence speed - how fast camera returns to aim point (degrees/second)
	 *  Higher value = faster recovery. Affected by Ergonomics.
	 *  Formula: EffectiveSpeed = ConvergenceSpeed * (1 + Ergonomics/100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Convergence",
		meta = (ClampMin = "1.0", ClampMax = "20.0", ToolTip = "Camera return speed (deg/s)"))
	float ConvergenceSpeed = 5.0f;

	/** Delay before convergence starts after shot (seconds)
	 *  During this delay, camera stays at recoil position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Convergence",
		meta = (ClampMin = "0.0", ClampMax = "0.5", ToolTip = "Delay before recovery starts"))
	float ConvergenceDelay = 0.1f;

	/** Horizontal recoil bias: -1.0 (always left) to 1.0 (always right), 0 = random
	 *  Some weapons have tendency to kick in specific direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Pattern",
		meta = (ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "Horizontal bias (-1=left, 1=right)"))
	float RecoilAngleBias = 0.0f;

	/** Recoil pattern predictability: 0.0 (fully random) to 1.0 (learnable pattern)
	 *  Higher values make recoil more consistent and learnable like CS:GO */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Pattern",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Pattern predictability (0=random, 1=fixed)"))
	float RecoilPatternStrength = 0.3f;

	/** Recoil pattern points - defines learnable spray pattern
	 *  Each point is [PitchMultiplier, YawMultiplier] for that shot in sequence
	 *  Pattern loops after all points, scaled by LoopScaleFactor
	 *  Example: [[1.0, 0.0], [0.8, -0.1], [0.7, 0.15]] = up, up-left, up-right */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Pattern",
		meta = (ToolTip = "Pattern points as [Pitch, Yaw] pairs"))
	TArray<FVector2D> RecoilPatternPoints;

	/** Scale factor for pattern on subsequent loops (0.1-1.0)
	 *  0.7 = 70% strength on second loop, 49% on third, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Pattern",
		meta = (ClampMin = "0.1", ClampMax = "1.0", ToolTip = "Pattern scale on loop"))
	float RecoilPatternLoopScale = 0.7f;

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


/**
 * FSuspenseCoreConsumableAttributeRow
 *
 * DataTable row structure for consumable/medical item attributes.
 * Used for Tarkov-style healing system with limb damage, bleeding, fractures.
 *
 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreConsumableAttributes.json
 * TARGET DATATABLE: DT_ConsumableAttributes
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Import JSON into DataTable via Editor (File → Import)
 * 2. Configure ConsumableAttributesDataTable in Project Settings → SuspenseCore
 * 3. DataManager caches rows on Initialize()
 * 4. On use: DataManager->GetConsumableAttributes(ConsumableID)
 *
 * TARKOV-STYLE MEDICAL SYSTEM:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Medical items are inventory objects with use limits
 * - Different items heal different status effects
 * - Healing takes time and can be interrupted
 * - Some items have side effects (hydration/energy cost)
 *
 * @see USuspenseCoreSettings::ConsumableAttributesDataTable
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreConsumableAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity
	//========================================================================

	/** Unique consumable identifier - matches FSuspenseCoreUnifiedItemData.ItemID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName ConsumableID;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText ConsumableName;

	/** Consumable type classification (Medkit, Bandage, Painkiller, Stimulant, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "Item.Consumable"))
	FGameplayTag ConsumableType;

	//========================================================================
	// Healing Attributes
	//========================================================================

	/** Total health points restored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
		meta = (ClampMin = "0", ClampMax = "5000", ToolTip = "Total HP restored"))
	float HealAmount = 0.0f;

	/** Health points restored per second during use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
		meta = (ClampMin = "0", ClampMax = "500", ToolTip = "HP per second"))
	float HealRate = 0.0f;

	/** Time to use the item in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
		meta = (ClampMin = "0.1", ClampMax = "30", ToolTip = "Use time in seconds"))
	float UseTime = 3.0f;

	/** Number of uses before item is depleted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
		meta = (ClampMin = "1", ClampMax = "100", ToolTip = "Max uses"))
	int32 MaxUses = 1;

	//========================================================================
	// Status Effect Healing
	//========================================================================

	/** Can stop heavy bleeding (requires tourniquet/surgery) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusEffects")
	bool bCanHealHeavyBleed = false;

	/** Can stop light bleeding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusEffects")
	bool bCanHealLightBleed = false;

	/** Can fix bone fractures */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StatusEffects")
	bool bCanHealFracture = false;

	//========================================================================
	// Special Effects
	//========================================================================

	/** Duration of painkiller effect in seconds (0 = no painkiller) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialEffects",
		meta = (ClampMin = "0", ClampMax = "600", ToolTip = "Painkiller duration"))
	float PainkillerDuration = 0.0f;

	/** Stamina points restored immediately */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialEffects",
		meta = (ClampMin = "-100", ClampMax = "100", ToolTip = "Stamina restored"))
	float StaminaRestore = 0.0f;

	/** Hydration cost (negative = drains hydration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialEffects",
		meta = (ClampMin = "-100", ClampMax = "100", ToolTip = "Hydration change"))
	float HydrationCost = 0.0f;

	/** Energy cost (negative = drains energy) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialEffects",
		meta = (ClampMin = "-100", ClampMax = "100", ToolTip = "Energy change"))
	float EnergyCost = 0.0f;

	//========================================================================
	// Effect Tags
	//========================================================================

	/** Gameplay effect tags applied by this consumable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
		meta = (Categories = "Consumable.Effect"))
	FGameplayTagContainer EffectTags;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !ConsumableID.IsNone();
	}

	/** Check if this consumable provides healing */
	bool ProvidesHealing() const
	{
		return HealAmount > 0.0f;
	}

	/** Check if this consumable can treat any status effects */
	bool CanTreatStatusEffects() const
	{
		return bCanHealHeavyBleed || bCanHealLightBleed || bCanHealFracture;
	}
};


/**
 * FSuspenseCoreThrowableAttributeRow
 *
 * DataTable row structure for throwable/grenade attributes.
 * Covers frag grenades, smoke, flashbangs, incendiaries, and VOG rounds.
 *
 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreThrowableAttributes.json
 * TARGET DATATABLE: DT_ThrowableAttributes
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Import JSON into DataTable via Editor (File → Import)
 * 2. Configure ThrowableAttributesDataTable in Project Settings → SuspenseCore
 * 3. DataManager caches rows on Initialize()
 * 4. On throw: DataManager->GetThrowableAttributes(ThrowableID)
 *
 * GRENADE TYPES:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Frag: ExplosionDamage + FragmentCount/Damage
 * - Smoke: SmokeDuration + SmokeRadius
 * - Flash: StunDuration + BlindDuration
 * - Incendiary: IncendiaryDamage + IncendiaryDuration
 * - VOG: Launched grenades (FuseTime = 0, ThrowForce = 0)
 *
 * @see USuspenseCoreSettings::ThrowableAttributesDataTable
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreThrowableAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity
	//========================================================================

	/** Unique throwable identifier - matches FSuspenseCoreUnifiedItemData.ItemID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName ThrowableID;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText ThrowableName;

	/** Throwable type classification (Frag, Smoke, Flash, Incendiary, VOG) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "Item.Throwable"))
	FGameplayTag ThrowableType;

	//========================================================================
	// Explosion Attributes
	//========================================================================

	/** Base explosion damage at center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion",
		meta = (ClampMin = "0", ClampMax = "500", ToolTip = "Explosion damage"))
	float ExplosionDamage = 0.0f;

	/** Maximum explosion radius in meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion",
		meta = (ClampMin = "0", ClampMax = "50", ToolTip = "Explosion radius"))
	float ExplosionRadius = 0.0f;

	/** Inner radius for full damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion",
		meta = (ClampMin = "0", ClampMax = "20", ToolTip = "Full damage radius"))
	float InnerRadius = 0.0f;

	/** Damage falloff multiplier (0-1, lower = steeper falloff) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Damage falloff curve"))
	float DamageFalloff = 0.8f;

	//========================================================================
	// Fragmentation Attributes
	//========================================================================

	/** Number of fragments generated on explosion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fragmentation",
		meta = (ClampMin = "0", ClampMax = "1000", ToolTip = "Fragment count"))
	int32 FragmentCount = 0;

	/** Damage per fragment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fragmentation",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Fragment damage"))
	float FragmentDamage = 0.0f;

	/** Fragment spread angle in degrees (360 = full sphere) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fragmentation",
		meta = (ClampMin = "0", ClampMax = "360", ToolTip = "Fragment spread"))
	float FragmentSpread = 360.0f;

	/** Armor penetration value for fragments */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fragmentation",
		meta = (ClampMin = "0", ClampMax = "50", ToolTip = "Fragment armor pen"))
	float ArmorPenetration = 0.0f;

	//========================================================================
	// Throw Physics
	//========================================================================

	/** Time until detonation after pin pull (0 = impact/launcher) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics",
		meta = (ClampMin = "0", ClampMax = "10", ToolTip = "Fuse time in seconds"))
	float FuseTime = 3.5f;

	/** Initial throw velocity (0 = launcher round) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics",
		meta = (ClampMin = "0", ClampMax = "3000", ToolTip = "Throw force"))
	float ThrowForce = 1200.0f;

	/** Default throw arc angle in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics",
		meta = (ClampMin = "0", ClampMax = "90", ToolTip = "Throw arc angle"))
	float ThrowArc = 45.0f;

	/** Number of times grenade can bounce before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics",
		meta = (ClampMin = "0", ClampMax = "10", ToolTip = "Bounce count"))
	int32 BounceCount = 2;

	/** Bounce energy retention (0-1, lower = more energy lost) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Bounce friction"))
	float BounceFriction = 0.5f;

	//========================================================================
	// Special Effects (Stun/Flash)
	//========================================================================

	/** Duration of stun/disorientation effect in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialEffects",
		meta = (ClampMin = "0", ClampMax = "30", ToolTip = "Stun duration"))
	float StunDuration = 0.0f;

	/** Duration of blindness effect in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialEffects",
		meta = (ClampMin = "0", ClampMax = "30", ToolTip = "Blind duration"))
	float BlindDuration = 0.0f;

	//========================================================================
	// Smoke Attributes
	//========================================================================

	/** Duration of smoke screen in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke",
		meta = (ClampMin = "0", ClampMax = "120", ToolTip = "Smoke duration"))
	float SmokeDuration = 0.0f;

	/** Radius of smoke coverage in meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke",
		meta = (ClampMin = "0", ClampMax = "30", ToolTip = "Smoke radius"))
	float SmokeRadius = 0.0f;

	//========================================================================
	// Incendiary Attributes
	//========================================================================

	/** Damage per tick from fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Incendiary",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Fire damage"))
	float IncendiaryDamage = 0.0f;

	/** Duration of fire effect in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Incendiary",
		meta = (ClampMin = "0", ClampMax = "60", ToolTip = "Fire duration"))
	float IncendiaryDuration = 0.0f;

	//========================================================================
	// Visual Effects (VFX)
	//========================================================================

	/** Explosion particle effect (Niagara) - Primary */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<class UNiagaraSystem> ExplosionEffect;

	/** Explosion particle effect (Cascade) - Legacy fallback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<class UParticleSystem> ExplosionEffectLegacy;

	/** Smoke effect (Niagara) - For smoke grenades */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<class UNiagaraSystem> SmokeEffect;

	/** Smoke effect (Cascade) - Legacy fallback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<class UParticleSystem> SmokeEffectLegacy;

	/** Trail effect during flight (Niagara) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<class UNiagaraSystem> TrailEffect;

	//========================================================================
	// Audio
	//========================================================================

	/** Explosion sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> ExplosionSound;

	/** Pin pull sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> PinPullSound;

	/** Bounce sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> BounceSound;

	//========================================================================
	// Camera Shake
	//========================================================================

	/** Camera shake class on explosion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraShake")
	TSoftClassPtr<class UCameraShakeBase> ExplosionCameraShake;

	/** Camera shake radius (0 = use ExplosionRadius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraShake",
		meta = (ClampMin = "0", ClampMax = "5000", ToolTip = "Shake radius"))
	float CameraShakeRadius = 0.0f;

	/** Camera shake intensity multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraShake",
		meta = (ClampMin = "0", ClampMax = "2", ToolTip = "Shake intensity"))
	float CameraShakeIntensity = 1.0f;

	//========================================================================
	// Damage System
	//========================================================================

	/** Whether grenade damages the thrower (self-damage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ToolTip = "Enable self-damage from own grenade"))
	bool bDamageSelf = true;

	/** GameplayEffect class for applying damage via GAS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ToolTip = "Damage effect class (uses default if not set)"))
	TSoftClassPtr<class UGameplayEffect> DamageEffectClass;

	/** GameplayEffect class for flashbang stun effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ToolTip = "Flashbang stun effect class"))
	TSoftClassPtr<class UGameplayEffect> FlashbangEffectClass;

	/** GameplayEffect class for incendiary burn effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage",
		meta = (ToolTip = "Incendiary burn effect class"))
	TSoftClassPtr<class UGameplayEffect> IncendiaryEffectClass;

	//========================================================================
	// DoT Effects (Bleeding/Burning) - Data-Driven
	// @see Documentation/GAS/GrenadeDoT_DesignDocument.md
	//========================================================================

	/** GameplayEffect for light bleeding (shrapnel wounds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ToolTip = "Light bleeding effect (bandage can heal)"))
	TSoftClassPtr<class UGameplayEffect> BleedingLightEffectClass;

	/** GameplayEffect for heavy bleeding (deep shrapnel wounds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ToolTip = "Heavy bleeding effect (requires medkit/surgery)"))
	TSoftClassPtr<class UGameplayEffect> BleedingHeavyEffectClass;

	/** Damage per tick for bleeding effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0", ClampMax = "20", ToolTip = "Bleed damage per tick"))
	float BleedDamagePerTick = 5.0f;

	/** Tick interval for bleeding (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0.1", ClampMax = "5", ToolTip = "Bleed tick interval"))
	float BleedTickInterval = 1.0f;

	/** Armor damage per tick for burning (armor bypass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0", ClampMax = "20", ToolTip = "Burn armor damage per tick"))
	float BurnArmorDamagePerTick = 3.0f;

	/** Health damage per tick for burning (direct) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0", ClampMax = "20", ToolTip = "Burn health damage per tick"))
	float BurnHealthDamagePerTick = 8.0f;

	/** Tick interval for burning (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0.1", ClampMax = "2", ToolTip = "Burn tick interval"))
	float BurnTickInterval = 0.5f;

	/** Minimum armor to block shrapnel (0 = unarmored bleeds only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Armor threshold for bleeding"))
	float ArmorThresholdForBleeding = 0.0f;

	/** Fragment hits required for heavy bleeding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "1", ClampMax = "20", ToolTip = "Hits for heavy bleed"))
	int32 FragmentHitsForHeavyBleed = 5;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !ThrowableID.IsNone();
	}

	/** Check if this is a fragmentation grenade */
	bool IsFragGrenade() const
	{
		return FragmentCount > 0 && FragmentDamage > 0.0f;
	}

	/** Check if this is a smoke grenade */
	bool IsSmokeGrenade() const
	{
		return SmokeDuration > 0.0f;
	}

	/** Check if this is a flashbang */
	bool IsFlashbang() const
	{
		return BlindDuration > 0.0f || StunDuration > 0.0f;
	}

	/** Check if this is an incendiary */
	bool IsIncendiary() const
	{
		return IncendiaryDamage > 0.0f;
	}

	/** Check if this is a launcher round (VOG) */
	bool IsLauncherRound() const
	{
		return FuseTime <= 0.0f && ThrowForce <= 0.0f;
	}

	/** Check if has explosion VFX (either Niagara or Cascade) */
	bool HasExplosionEffect() const
	{
		return !ExplosionEffect.IsNull() || !ExplosionEffectLegacy.IsNull();
	}

	/** Check if has smoke VFX */
	bool HasSmokeEffect() const
	{
		return !SmokeEffect.IsNull() || !SmokeEffectLegacy.IsNull();
	}

	/** Get effective camera shake radius (uses ExplosionRadius if not set) */
	float GetEffectiveCameraShakeRadius() const
	{
		return CameraShakeRadius > 0.0f ? CameraShakeRadius : ExplosionRadius * 100.0f; // Convert meters to cm
	}
};


/**
 * FSuspenseCoreAttachmentAttributeRow
 *
 * DataTable row structure for weapon attachment/modification attributes.
 * Used for muzzle devices (suppressors, compensators), stocks, grips, handguards, optics.
 * Modifiers are MULTIPLICATIVE (0.85 = -15% recoil, 1.1 = +10% spread).
 *
 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreAttachmentAttributes.json
 * TARGET DATATABLE: DT_AttachmentAttributes
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Import JSON into DataTable via Editor (File → Import)
 * 2. Configure AttachmentAttributesDataTable in Project Settings → SuspenseCore
 * 3. DataManager caches rows on Initialize()
 * 4. On weapon equip: DataManager->GetAttachmentAttributes(AttachmentID)
 *
 * TARKOV-STYLE ATTACHMENT SYSTEM:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Attachments are items that can be installed on weapon slots
 * - Multiple modifiers stack multiplicatively for recoil
 * - Ergonomics bonuses stack additively
 * - Suppressors affect sound and muzzle flash
 * - Stocks and grips primarily affect recoil and ergonomics
 *
 * @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Section 5.2
 * @see USuspenseCoreSettings::AttachmentAttributesDataTable
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAttachmentAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity
	//========================================================================

	/** Unique attachment identifier - matches FSuspenseCoreUnifiedItemData.ItemID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName AttachmentID;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	/** Attachment slot type (Muzzle, Stock, Grip, Sight, Handguard, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "Equipment.Slot"))
	FGameplayTag SlotType;

	//========================================================================
	// Stat Modifiers (Multiplicative, 1.0 = no change)
	// Values < 1.0 reduce, > 1.0 increase
	// Final = Base × Modifier1 × Modifier2 × ...
	//========================================================================

	/** Recoil modifier: 0.75 = -25% recoil, 1.2 = +20% recoil
	 *  Muzzle brakes: 0.75-0.85, Stocks: 0.85-0.95, Grips: 0.94-0.98 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Combat",
		meta = (ClampMin = "0.5", ClampMax = "1.5", ToolTip = "Recoil multiplier (0.75 = -25%)"))
	float RecoilModifier = 1.0f;

	/** Accuracy modifier: 0.9 = -10% spread (more accurate), 1.1 = +10% spread
	 *  Long barrels improve accuracy, short barrels reduce it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Combat",
		meta = (ClampMin = "0.5", ClampMax = "1.5", ToolTip = "Accuracy multiplier (0.9 = -10% spread)"))
	float AccuracyModifier = 1.0f;

	/** Muzzle velocity modifier: affects bullet speed and effective range
	 *  Suppressors typically: 0.95-1.05, Long barrels: 1.05-1.15 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Combat",
		meta = (ClampMin = "0.8", ClampMax = "1.2", ToolTip = "Velocity multiplier"))
	float VelocityModifier = 1.0f;

	//========================================================================
	// Stat Additions (Additive)
	// Final = Base + Bonus1 + Bonus2 + ...
	//========================================================================

	/** Ergonomics bonus: +5 = adds 5 ergonomics points, -8 = reduces by 8
	 *  Affects ADS speed, convergence speed. Range: -30 to +30 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Ergonomics",
		meta = (ClampMin = "-30", ClampMax = "30", ToolTip = "Ergonomics bonus (additive)"))
	float ErgonomicsBonus = 0.0f;

	/** Weight in kilograms - affects total weapon weight and movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers|Physical",
		meta = (ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Weight in kg"))
	float Weight = 0.1f;

	//========================================================================
	// Special Effects
	//========================================================================

	/** Suppresses weapon sound (for suppressors) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	bool bSuppressesSound = false;

	/** Sound reduction percentage when suppressed (0.0-1.0)
	 *  0.0 = no reduction, 1.0 = completely silent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bSuppressesSound",
				ToolTip = "Sound reduction (0.7 = 70% quieter)"))
	float SoundReduction = 0.0f;

	/** Hides muzzle flash (for flash hiders and suppressors) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	bool bHidesMuzzleFlash = false;

	/** Muzzle flash reduction percentage (0.0-1.0)
	 *  Flash hiders: 0.8-1.0, Suppressors: 0.9-1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bHidesMuzzleFlash",
				ToolTip = "Flash reduction (0.9 = 90% hidden)"))
	float FlashReduction = 0.0f;

	//========================================================================
	// Compatibility
	//========================================================================

	/** Weapon types this attachment works with (e.g., Weapon.Type.AssaultRifle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility",
		meta = (Categories = "Weapon.Type"))
	FGameplayTagContainer CompatibleWeaponTypes;

	/** Specific weapon IDs this works with (if empty, uses CompatibleWeaponTypes)
	 *  Use for weapon-specific attachments like AK-specific stocks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	TArray<FName> CompatibleWeaponIDs;

	//========================================================================
	// Visuals (References)
	//========================================================================

	/** Static mesh path for world display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> AttachmentMesh;

	/** Icon texture path for inventory UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<UTexture2D> Icon;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !AttachmentID.IsNone();
	}

	/** Check if this attachment is compatible with weapon type */
	bool IsCompatibleWithWeaponType(const FGameplayTag& WeaponType) const
	{
		return CompatibleWeaponTypes.IsEmpty() || CompatibleWeaponTypes.HasTag(WeaponType);
	}

	/** Check if this attachment is compatible with specific weapon */
	bool IsCompatibleWithWeapon(const FName& WeaponID, const FGameplayTag& WeaponType) const
	{
		// Check specific weapon IDs first
		if (CompatibleWeaponIDs.Num() > 0)
		{
			return CompatibleWeaponIDs.Contains(WeaponID);
		}
		// Fall back to weapon type check
		return IsCompatibleWithWeaponType(WeaponType);
	}

	/** Check if this attachment affects recoil */
	bool AffectsRecoil() const
	{
		return !FMath::IsNearlyEqual(RecoilModifier, 1.0f);
	}

	/** Check if this is a suppressor */
	bool IsSuppressor() const
	{
		return bSuppressesSound;
	}

	/** Check if this is a muzzle device */
	bool IsMuzzleDevice() const
	{
		return SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Slot.Muzzle")));
	}
};


/**
 * ESuspenseCoreStatusEffectCategory
 *
 * Classification for status effects (buffs vs debuffs)
 */
UENUM(BlueprintType)
enum class ESuspenseCoreStatusEffectCategory : uint8
{
	/** Negative effect (damage, slow, etc.) */
	Debuff UMETA(DisplayName = "Debuff"),

	/** Positive effect (heal, speed boost, etc.) */
	Buff UMETA(DisplayName = "Buff"),

	/** Neutral effect (marker, reveal, etc.) */
	Neutral UMETA(DisplayName = "Neutral")
};


/**
 * ESuspenseCoreStackBehavior
 *
 * How stacks of the same effect are handled
 */
UENUM(BlueprintType)
enum class ESuspenseCoreStackBehavior : uint8
{
	/** Each stack adds to total effect (e.g., bleeding stacks = more damage) */
	Additive UMETA(DisplayName = "Additive"),

	/** Only the strongest stack applies */
	StrongestWins UMETA(DisplayName = "Strongest Wins"),

	/** Duration refreshes on new application */
	RefreshDuration UMETA(DisplayName = "Refresh Duration"),

	/** Duration extends on new application */
	ExtendDuration UMETA(DisplayName = "Extend Duration"),

	/** Cannot stack - new application is ignored */
	NoStack UMETA(DisplayName = "No Stack")
};


/**
 * FSuspenseCoreStatusEffectVisualRow
 *
 * SIMPLIFIED DataTable row structure for status effect VISUAL data only.
 * Gameplay data (duration, damage, stacking) is managed by GameplayEffect assets.
 *
 * This structure follows the GDD v2.0 architecture:
 * - GameplayEffect Assets → Gameplay logic (duration, damage, stacking)
 * - DataTable (this) → Visual/UI data only (icon, VFX, audio, cure items)
 *
 * JSON SOURCE: Content/Data/StatusEffects/SuspenseCoreStatusEffectVisuals.json
 * TARGET DATATABLE: DT_StatusEffectVisuals
 *
 * @see Documentation/GameDesign/StatusEffect_System_GDD.md
 * @see USuspenseCoreDoTService
 * @see UW_DebuffIcon, UW_DebuffContainer
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreStatusEffectVisualRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity & GAS Link
	//========================================================================

	/** Unique effect identifier (RowName in DataTable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName EffectID;

	/** Effect type tag - MUST match tag granted by GameplayEffect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "State"))
	FGameplayTag EffectTypeTag;

	/** Reference to GameplayEffect asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	TSoftClassPtr<class UGameplayEffect> GameplayEffectClass;

	//========================================================================
	// UI Display
	//========================================================================

	/** Localized display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FText DisplayName;

	/** Short description for tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FText Description;

	/** Buff, Debuff, or Neutral */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	ESuspenseCoreStatusEffectCategory Category = ESuspenseCoreStatusEffectCategory::Debuff;

	/** Sort priority (higher = shown first) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI",
		meta = (ClampMin = "0", ClampMax = "100"))
	int32 DisplayPriority = 50;

	//========================================================================
	// Visual - Icon
	//========================================================================

	/** Icon texture for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Icon tint (normal state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor IconTint = FLinearColor::White;

	/** Icon tint (critical/urgent state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor CriticalIconTint = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	//========================================================================
	// Visual - VFX
	//========================================================================

	/** Niagara effect on character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<class UNiagaraSystem> CharacterVFX;

	/** VFX attachment socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	FName VFXAttachSocket = NAME_None;

	//========================================================================
	// Audio
	//========================================================================

	/** Sound on effect application */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> ApplicationSound;

	/** Sound on effect removal/cure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> RemovalSound;

	//========================================================================
	// Cure System
	//========================================================================

	/** Item IDs that can cure this effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	TArray<FName> CureItemIDs;

	/** Can be cured by basic bandage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	bool bCuredByBandage = false;

	/** Can be cured by medkit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	bool bCuredByMedkit = false;

	/** Requires surgical kit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	bool bRequiresSurgery = false;

	//========================================================================
	// Animation Flags
	//========================================================================

	/** Prevents sprinting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bPreventsSprinting = false;

	/** Prevents ADS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bPreventsADS = false;

	/** Causes limping animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bCausesLimp = false;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !EffectID.IsNone() && EffectTypeTag.IsValid();
	}

	/** Check if this is a debuff */
	bool IsDebuff() const
	{
		return Category == ESuspenseCoreStatusEffectCategory::Debuff;
	}

	/** Check if this is a buff */
	bool IsBuff() const
	{
		return Category == ESuspenseCoreStatusEffectCategory::Buff;
	}

	/** Check if this effect has visual icon */
	bool HasIcon() const
	{
		return !Icon.IsNull();
	}

	/** Check if this effect has character VFX */
	bool HasCharacterVFX() const
	{
		return !CharacterVFX.IsNull();
	}

	/** Check if any item can cure this effect */
	bool CanBeCured() const
	{
		return CureItemIDs.Num() > 0 || bCuredByBandage || bCuredByMedkit;
	}

	/** Check if specific item can cure this effect */
	bool CanBeCuredByItem(FName ItemID) const
	{
		return CureItemIDs.Contains(ItemID);
	}
};


/**
 * FSuspenseCoreStatusEffectModifier
 *
 * Single attribute modifier applied by a status effect
 *
 * @deprecated Use GameplayEffect Modifiers instead.
 * This struct is kept for backward compatibility with old FSuspenseCoreStatusEffectAttributeRow.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreStatusEffectModifier
{
	GENERATED_BODY()

	/** Attribute tag to modify (e.g., Attribute.Health, Attribute.MoveSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier",
		meta = (Categories = "Attribute"))
	FGameplayTag AttributeTag;

	/** Flat value to add/subtract */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	float FlatModifier = 0.0f;

	/** Percentage multiplier (0.8 = -20%, 1.2 = +20%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier",
		meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float PercentModifier = 1.0f;
};


/**
 * FSuspenseCoreStatusEffectAttributeRow
 *
 * @deprecated This struct is deprecated. Use FSuspenseCoreStatusEffectVisualRow instead.
 *
 * The new architecture separates gameplay data (GameplayEffect assets) from
 * visual data (FSuspenseCoreStatusEffectVisualRow). Duration, damage, stacking
 * policies should be configured in GameplayEffect Blueprint assets.
 *
 * @see FSuspenseCoreStatusEffectVisualRow - New simplified structure for visuals
 * @see Documentation/GameDesign/StatusEffect_System_GDD.md - Architecture documentation
 *
 * MIGRATION GUIDE:
 * ═══════════════════════════════════════════════════════════════════════════
 * OLD: FSuspenseCoreStatusEffectAttributeRow (43+ fields)
 *   - Contains Duration, DamagePerTick, StackBehavior, etc.
 *
 * NEW: FSuspenseCoreStatusEffectVisualRow (18 fields) + GameplayEffect Assets
 *   - Visual data only: Icon, VFX, Audio, CureItems
 *   - Gameplay data moved to GE Blueprint assets
 *
 * JSON SOURCE (OLD): Content/Data/StatusEffects/SuspenseCoreStatusEffects.json
 * JSON SOURCE (NEW): Content/Data/StatusEffects/SuspenseCoreStatusEffectVisuals.json
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreStatusEffectAttributeRow : public FTableRowBase
{
	GENERATED_BODY()

	//========================================================================
	// Identity
	//========================================================================

	/** Unique status effect identifier (e.g., "BleedingLight", "BurningFire") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName EffectID;

	/** Localized display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	/** Short description for tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText Description;

	/** Effect type tag (State.Health.Bleeding.Light, State.Combat.Suppressed, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (Categories = "State"))
	FGameplayTag EffectTypeTag;

	/** Category: Buff, Debuff, or Neutral */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	ESuspenseCoreStatusEffectCategory Category = ESuspenseCoreStatusEffectCategory::Debuff;

	/** Priority for UI sorting (higher = shown first, 0-100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity",
		meta = (ClampMin = "0", ClampMax = "100"))
	int32 DisplayPriority = 50;

	//========================================================================
	// Duration & Stacking
	//========================================================================

	/** Default duration in seconds (-1 = infinite, must be healed/cured) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration",
		meta = (ClampMin = "-1", ToolTip = "-1 for infinite (bleeding), positive for timed"))
	float DefaultDuration = 10.0f;

	/** Is this an infinite-duration effect (requires healing to remove) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
	bool bIsInfinite = false;

	/** Maximum stack count (1 = no stacking) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration",
		meta = (ClampMin = "1", ClampMax = "99"))
	int32 MaxStacks = 1;

	/** How new applications of this effect are handled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
	ESuspenseCoreStackBehavior StackBehavior = ESuspenseCoreStackBehavior::RefreshDuration;

	//========================================================================
	// Damage Over Time (DoT)
	//========================================================================

	/** Damage dealt per tick (0 = no DoT) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Damage per tick (0 = no DoT)"))
	float DamagePerTick = 0.0f;

	/** Time between damage ticks in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0.1", ClampMax = "5.0", EditCondition = "DamagePerTick > 0"))
	float TickInterval = 1.0f;

	/** Damage type tag for resistance calculations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (Categories = "Damage.Type"))
	FGameplayTag DamageTypeTag;

	/** Does damage bypass armor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT")
	bool bBypassArmor = false;

	/** Per-stack damage multiplier (for additive stacking) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DoT",
		meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "DamagePerTick > 0"))
	float StackDamageMultiplier = 1.0f;

	//========================================================================
	// Healing Over Time (HoT)
	//========================================================================

	/** Health restored per tick (0 = no HoT) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoT",
		meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Healing per tick (0 = no HoT)"))
	float HealPerTick = 0.0f;

	/** Time between heal ticks in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoT",
		meta = (ClampMin = "0.1", ClampMax = "5.0", EditCondition = "HealPerTick > 0"))
	float HealTickInterval = 1.0f;

	//========================================================================
	// Attribute Modifiers
	//========================================================================

	/** Attribute modifiers applied while effect is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifiers")
	TArray<FSuspenseCoreStatusEffectModifier> AttributeModifiers;

	//========================================================================
	// Cure/Removal Requirements
	//========================================================================

	/** Items that can cure this effect (by ItemID) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	TArray<FName> CureItemIDs;

	/** Effect tags that can cure/remove this effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure",
		meta = (Categories = "Effect.Cure"))
	FGameplayTagContainer CureEffectTags;

	/** Can this effect be removed by basic bandage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	bool bCuredByBandage = false;

	/** Can this effect be removed by medkit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	bool bCuredByMedkit = false;

	/** Can this effect be removed by surgery */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cure")
	bool bRequiresSurgery = false;

	//========================================================================
	// Visual - Icons
	//========================================================================

	/** Icon texture for UI display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Icon")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Icon tint color (normal state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Icon")
	FLinearColor IconTint = FLinearColor::White;

	/** Icon tint color (critical/low health state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Icon")
	FLinearColor CriticalIconTint = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	/** Icon size multiplier (1.0 = default 48x48) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Icon",
		meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float IconScale = 1.0f;

	//========================================================================
	// Visual - VFX
	//========================================================================

	/** Niagara particle effect applied to character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|VFX")
	TSoftObjectPtr<class UNiagaraSystem> CharacterVFX;

	/** Legacy Cascade particle effect (fallback) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|VFX")
	TSoftObjectPtr<class UParticleSystem> CharacterVFXLegacy;

	/** VFX attachment socket on character mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|VFX")
	FName VFXAttachSocket = NAME_None;

	/** Screen overlay material (for effects like burning screen edge) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|VFX")
	TSoftObjectPtr<class UMaterialInterface> ScreenOverlayMaterial;

	/** Post-process material (for effects like poison haze) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|VFX")
	TSoftObjectPtr<class UMaterialInterface> PostProcessMaterial;

	//========================================================================
	// Audio
	//========================================================================

	/** Sound played when effect is applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> ApplicationSound;

	/** Sound played on each tick (looping ambient for continuous effects) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> TickSound;

	/** Sound played when effect is removed/cured */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> RemovalSound;

	/** Ambient sound loop while effect is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> AmbientLoop;

	//========================================================================
	// Animation
	//========================================================================

	/** Animation montage to play on application */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TSoftObjectPtr<class UAnimMontage> ApplicationMontage;

	/** Does this effect prevent sprinting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bPreventsSprinting = false;

	/** Does this effect prevent ADS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bPreventsADS = false;

	/** Does this effect cause limping animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bCausesLimp = false;

	//========================================================================
	// GameplayEffect Integration
	//========================================================================

	/** GameplayEffect class to apply (for GAS integration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TSoftClassPtr<class UGameplayEffect> GameplayEffectClass;

	/** Additional tags granted while effect is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTagContainer GrantedTags;

	/** Tags that block this effect from being applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTagContainer BlockedByTags;

	/** Tags required for this effect to be applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTagContainer RequiredTags;

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if row has valid data */
	bool IsValid() const
	{
		return !EffectID.IsNone() && EffectTypeTag.IsValid();
	}

	/** Check if this is a debuff */
	bool IsDebuff() const
	{
		return Category == ESuspenseCoreStatusEffectCategory::Debuff;
	}

	/** Check if this is a buff */
	bool IsBuff() const
	{
		return Category == ESuspenseCoreStatusEffectCategory::Buff;
	}

	/** Check if this effect deals damage over time */
	bool IsDoT() const
	{
		return DamagePerTick > 0.0f;
	}

	/** Check if this effect heals over time */
	bool IsHoT() const
	{
		return HealPerTick > 0.0f;
	}

	/** Check if this effect is infinite duration (requires cure) */
	bool RequiresCure() const
	{
		return bIsInfinite || DefaultDuration < 0.0f;
	}

	/** Check if this effect modifies attributes */
	bool HasAttributeModifiers() const
	{
		return AttributeModifiers.Num() > 0;
	}

	/** Check if this effect has visual icon */
	bool HasIcon() const
	{
		return !Icon.IsNull();
	}

	/** Check if this effect has character VFX */
	bool HasCharacterVFX() const
	{
		return !CharacterVFX.IsNull() || !CharacterVFXLegacy.IsNull();
	}

	/** Get total damage for given stack count */
	float GetTotalDamagePerTick(int32 Stacks) const
	{
		if (StackBehavior == ESuspenseCoreStackBehavior::Additive)
		{
			return DamagePerTick * Stacks * StackDamageMultiplier;
		}
		return DamagePerTick;
	}
};

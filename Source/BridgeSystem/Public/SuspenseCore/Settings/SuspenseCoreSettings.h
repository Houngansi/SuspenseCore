// SuspenseCoreSettings.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreSettings.generated.h"

class UDataTable;
class UDataAsset;

/**
 * USuspenseCoreSettings
 *
 * Central configuration for the SuspenseCore system.
 * Accessible via Project Settings → Game → SuspenseCore
 *
 * ARCHITECTURE NOTES:
 * ═══════════════════════════════════════════════════════════════════════════
 * This is the SINGLE SOURCE OF TRUTH for all SuspenseCore data configuration.
 * All DataTables, DataAssets, and system settings are configured here.
 *
 * DO NOT configure data in:
 * - GameInstance blueprints (legacy pattern)
 * - Individual actor defaults
 * - Hard-coded paths
 *
 * BEST PRACTICES:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. All new data configurations should be added to this class
 * 2. Use soft references (TSoftObjectPtr) for assets to avoid load-time dependencies
 * 3. Group related settings into categories
 * 4. Provide sensible defaults where possible
 * 5. Add validation in USuspenseCoreDataManager::Initialize()
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * C++:
 *   const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
 *   if (Settings && Settings->ItemDataTable.IsValid())
 *   {
 *       UDataTable* DT = Settings->ItemDataTable.LoadSynchronous();
 *   }
 *
 * Blueprint:
 *   Use "Get SuspenseCore Settings" node
 *
 * @see USuspenseCoreDataManager - Loads and caches data from these settings
 * @see USuspenseCoreEventManager - EventBus integration
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="SuspenseCore"))
class BRIDGESYSTEM_API USuspenseCoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USuspenseCoreSettings();

	//========================================================================
	// Static Access
	//========================================================================

	/** Get the singleton settings instance */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Settings", meta = (DisplayName = "Get SuspenseCore Settings"))
	static const USuspenseCoreSettings* Get();

	/** Get mutable settings (editor only) */
	static USuspenseCoreSettings* GetMutable();

	//========================================================================
	// UDeveloperSettings Interface
	//========================================================================

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("SuspenseCore"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	//========================================================================
	// Item System Configuration
	//========================================================================

	/**
	 * DataTable containing all item definitions
	 * Row Structure: FSuspenseCoreUnifiedItemData (or FSuspenseCoreItemData)
	 *
	 * This is the master item database for the entire game.
	 * All pickups, inventory systems, and equipment reference items from this table.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Item System",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Master item DataTable. Row Structure: FSuspenseCoreUnifiedItemData"))
	TSoftObjectPtr<UDataTable> ItemDataTable;

	/** Validate items on startup (recommended for development) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Item System")
	bool bValidateItemsOnStartup = true;

	/** Block startup if critical items fail validation */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Item System",
		meta = (EditCondition = "bValidateItemsOnStartup"))
	bool bStrictItemValidation = false;

	/** Log detailed item operations */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Item System")
	bool bLogItemOperations = true;

	//========================================================================
	// GAS Attributes Configuration (SSOT)
	//========================================================================

	/**
	 * DataTable containing weapon attribute definitions
	 * Row Structure: FSuspenseCoreWeaponAttributeRow
	 *
	 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreWeaponAttributes.json
	 * Maps 1:1 to USuspenseCoreWeaponAttributeSet (19 attributes)
	 *
	 * @see SuspenseCoreGASAttributeRows.h
	 * @see Documentation/Plans/SSOT_AttributeSet_DataTable_Integration.md
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Weapon attributes DataTable. Row Structure: FSuspenseCoreWeaponAttributeRow"))
	TSoftObjectPtr<UDataTable> WeaponAttributesDataTable;

	/**
	 * DataTable containing ammo attribute definitions
	 * Row Structure: FSuspenseCoreAmmoAttributeRow
	 *
	 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreAmmoAttributes.json
	 * Maps 1:1 to USuspenseCoreAmmoAttributeSet (15 attributes)
	 *
	 * TARKOV-STYLE: Different ammo types affect weapon behavior,
	 * damage, penetration, and reliability.
	 *
	 * @see SuspenseCoreGASAttributeRows.h
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Ammo attributes DataTable. Row Structure: FSuspenseCoreAmmoAttributeRow"))
	TSoftObjectPtr<UDataTable> AmmoAttributesDataTable;

	/**
	 * DataTable containing armor attribute definitions (FUTURE)
	 * Row Structure: FSuspenseCoreArmorAttributeRow
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Armor attributes DataTable. Row Structure: FSuspenseCoreArmorAttributeRow"))
	TSoftObjectPtr<UDataTable> ArmorAttributesDataTable;

	/**
	 * DataTable containing consumable attribute definitions
	 * Row Structure: FSuspenseCoreConsumableAttributeRow
	 *
	 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreConsumableAttributes.json
	 * Maps 1:1 to medical/consumable items (Tarkov-style healing system)
	 *
	 * @see SuspenseCoreGASAttributeRows.h
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Consumable attributes DataTable. Row Structure: FSuspenseCoreConsumableAttributeRow"))
	TSoftObjectPtr<UDataTable> ConsumableAttributesDataTable;

	/**
	 * DataTable containing throwable attribute definitions
	 * Row Structure: FSuspenseCoreThrowableAttributeRow
	 *
	 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreThrowableAttributes.json
	 * Maps 1:1 to grenades/throwables (Frag, Smoke, Flash, Incendiary)
	 *
	 * @see SuspenseCoreGASAttributeRows.h
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Throwable attributes DataTable. Row Structure: FSuspenseCoreThrowableAttributeRow"))
	TSoftObjectPtr<UDataTable> ThrowableAttributesDataTable;

	/**
	 * DataTable containing attachment/modification attribute definitions
	 * Row Structure: FSuspenseCoreAttachmentAttributeRow
	 *
	 * JSON SOURCE: Content/Data/ItemDatabase/SuspenseCoreAttachmentAttributes.json
	 * Used for muzzle devices, stocks, grips, sights, handguards.
	 * Modifiers are multiplicative for recoil (0.85 = -15%), additive for ergonomics.
	 *
	 * @see SuspenseCoreGASAttributeRows.h
	 * @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Section 5.2
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Attachment attributes DataTable. Row Structure: FSuspenseCoreAttachmentAttributeRow"))
	TSoftObjectPtr<UDataTable> AttachmentAttributesDataTable;

	/**
	 * DataTable containing ALL status effect definitions (buffs AND debuffs)
	 * Row Structure: FSuspenseCoreStatusEffectAttributeRow
	 *
	 * JSON SOURCE: Content/Data/StatusEffects/SuspenseCoreStatusEffects.json
	 * SINGLE SOURCE OF TRUTH for all gameplay status effects.
	 *
	 * EFFECT TYPES:
	 * - Debuffs: Bleeding, Burning, Poisoned, Stunned, Slowed, Suppressed
	 * - Buffs: Regenerating, Painkiller, Adrenaline, Fortified
	 *
	 * UI WIDGETS: W_DebuffIcon, W_DebuffContainer read visual data from here.
	 *
	 * @see SuspenseCoreGASAttributeRows.h
	 * @see Documentation/Plans/StatusEffect_SSOT_System.md
	 * @see Documentation/Plans/DebuffWidget_System_Plan.md
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Status effects DataTable. Row Structure: FSuspenseCoreStatusEffectAttributeRow"))
	TSoftObjectPtr<UDataTable> StatusEffectAttributesDataTable;

	/**
	 * DataTable containing status effect VISUAL definitions
	 * Row Structure: FSuspenseCoreStatusEffectVisualRow
	 *
	 * JSON SOURCE: Content/Data/StatusEffects/SuspenseCoreStatusEffectVisuals.json
	 * Contains icons, colors, VFX references for UI display.
	 *
	 * SEPARATE TABLE from attributes because:
	 * - Attributes = gameplay mechanics (damage, duration, stacks)
	 * - Visuals = UI presentation (icons, colors, VFX)
	 *
	 * @see W_DebuffIcon, W_DebuffContainer
	 * @see FSuspenseCoreStatusEffectVisualRow
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Status effect visuals DataTable. Row Structure: FSuspenseCoreStatusEffectVisualRow"))
	TSoftObjectPtr<UDataTable> StatusEffectVisualsDataTable;

	/** Use SSOT DataTable attributes instead of legacy fields */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes",
		meta = (ToolTip = "Enable new SSOT attribute system. Disable for legacy fallback."))
	bool bUseSSOTAttributes = true;

	/** Validate attribute DataTables on startup */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "GAS Attributes")
	bool bValidateAttributesOnStartup = true;

	//========================================================================
	// Enemy System Configuration
	//========================================================================

	/**
	 * DataTable containing enemy preset definitions
	 * Row Structure: FSuspenseCoreEnemyPresetRow
	 *
	 * JSON SOURCE: Content/Data/EnemyDatabase/SuspenseCoreEnemyPresets.json
	 * SSOT for all enemy configurations - stats, perception, combat behavior.
	 *
	 * Referenced by SuspenseCoreEnemyBehaviorData via PresetRowName.
	 *
	 * @see SuspenseCoreEnemyTypes.h
	 * @see SuspenseCoreEnemyBehaviorData
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy System",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Enemy presets DataTable. Row Structure: FSuspenseCoreEnemyPresetRow"))
	TSoftObjectPtr<UDataTable> EnemyPresetsDataTable;

	/** Validate enemy presets on startup */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy System")
	bool bValidateEnemyPresetsOnStartup = true;

	/** Enable verbose enemy AI logging */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy System")
	bool bLogEnemyAIOperations = false;

	//========================================================================
	// Magazine System Configuration (Tarkov-Style)
	//========================================================================

	/**
	 * DataTable containing magazine definitions
	 * Row Structure: FSuspenseCoreMagazineData
	 *
	 * Tarkov-style magazines with:
	 * - Physical magazine items in inventory
	 * - Individual round tracking
	 * - Caliber/weapon compatibility
	 * - Reload time modifiers
	 *
	 * @see SuspenseCoreMagazineTypes.h
	 * @see Documentation/Plans/TarkovStyle_Ammo_System_Design.md
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Magazine System",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Magazine DataTable. Row Structure: FSuspenseCoreMagazineData"))
	TSoftObjectPtr<UDataTable> MagazineDataTable;

	/** Enable Tarkov-style magazine system (physical magazines, not just ammo counters) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Magazine System",
		meta = (ToolTip = "Enable physical magazine tracking. Disable for simplified ammo counter."))
	bool bUseTarkovMagazineSystem = true;

	//========================================================================
	// Character System Configuration
	//========================================================================

	/**
	 * DataAsset containing character class definitions
	 * Used for character selection, spawning, and class-specific logic
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character System",
		meta = (AllowedClasses = "/Script/Engine.DataAsset",
				ToolTip = "Character classes DataAsset"))
	TSoftObjectPtr<UDataAsset> CharacterClassesDataAsset;

	/**
	 * Default character class tag for new players
	 * Must match a valid entry in CharacterClassesDataAsset
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character System")
	FGameplayTag DefaultCharacterClass;

	//========================================================================
	// Loadout System Configuration (Future)
	//========================================================================

	/**
	 * DataTable containing loadout configurations
	 * Row Structure: FLoadoutConfiguration
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loadout System",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Loadout configurations DataTable"))
	TSoftObjectPtr<UDataTable> LoadoutDataTable;

	/** Default loadout ID for new players */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loadout System")
	FName DefaultLoadoutID = TEXT("Default_Soldier");

	//========================================================================
	// Equipment Slot Configuration
	//========================================================================

	/**
	 * DataAsset containing equipment slot presets.
	 * If not set, uses programmatic defaults with Native Tags.
	 *
	 * Create via: Content Browser → Miscellaneous → Data Asset → USuspenseCoreEquipmentSlotPresets
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loadout System",
		meta = (AllowedClasses = "/Script/BridgeSystem.SuspenseCoreEquipmentSlotPresets",
				ToolTip = "Equipment slot presets DataAsset (optional - fallback to defaults if not set)"))
	TSoftObjectPtr<UDataAsset> EquipmentSlotPresetsAsset;

	//========================================================================
	// Animation System Configuration (Future)
	//========================================================================

	/**
	 * DataTable containing weapon animation states
	 * Row Structure: FSuspenseCoreAnimationData
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Animation System",
		meta = (AllowedClasses = "/Script/Engine.DataTable",
				ToolTip = "Weapon animations DataTable"))
	TSoftObjectPtr<UDataTable> WeaponAnimationsTable;

	//========================================================================
	// Camera Shake Configuration
	//========================================================================

	/**
	 * DataAsset containing camera shake presets for all shake types.
	 * If not set, uses programmatic defaults in shake classes.
	 *
	 * Create via: Content Browser → Miscellaneous → Data Asset → USuspenseCoreCameraShakeDataAsset
	 *
	 * @see USuspenseCoreCameraShakeDataAsset
	 * @see USuspenseCoreCameraShakeComponent
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera Shake",
		meta = (AllowedClasses = "/Script/PlayerCore.SuspenseCoreCameraShakeDataAsset",
				ToolTip = "Camera shake presets DataAsset (optional - fallback to code defaults if not set)"))
	TSoftObjectPtr<UDataAsset> CameraShakePresetsAsset;

	/** Use layered camera shake manager (AAA-style priority blending) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera Shake",
		meta = (ToolTip = "Enable priority-based shake blending for AAA-quality camera feedback"))
	bool bUseLayeredCameraShakes = false;

	/** Enable Perlin Noise oscillators for organic shake feel (Battlefield/CoD style) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera Shake",
		meta = (ToolTip = "Use Perlin Noise instead of Sin waves for more organic camera shake"))
	bool bUsePerlinNoiseShakes = true;

	//========================================================================
	// EventBus Configuration
	//========================================================================

	/** Enable verbose EventBus logging */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "EventBus")
	bool bLogEventBusOperations = false;

	/** Maximum event history size for debugging */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "EventBus",
		meta = (ClampMin = "0", ClampMax = "1000"))
	int32 MaxEventHistorySize = 100;

	//========================================================================
	// Debug Configuration
	//========================================================================

	/** Enable on-screen debug messages */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bEnableOnScreenDebug = true;

	/** Duration for on-screen debug messages */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (ClampMin = "0.1", ClampMax = "30.0", EditCondition = "bEnableOnScreenDebug"))
	float DebugMessageDuration = 5.0f;

	//========================================================================
	// Validation
	//========================================================================

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Validate all configured assets */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Settings")
	bool ValidateConfiguration(TArray<FString>& OutErrors) const;
#endif
};


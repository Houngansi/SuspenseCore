// SuspenseCoreEquipmentSlotPresets.h
// SuspenseCore - Equipment Slot Presets DataAsset
// Copyright Suspense Team. All Rights Reserved.
//
// DataAsset for defining equipment slot configurations.
// Allows designers to configure slots without code changes.
//
// Usage:
//   1. Create DataAsset in Editor: Content Browser → Miscellaneous → Data Asset
//   2. Select USuspenseCoreEquipmentSlotPresets
//   3. Configure SlotPresets array
//   4. Assign to SuspenseCoreSettings→EquipmentSlotPresetsAsset

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCoreEquipmentSlotPresets.generated.h"

/**
 * DataAsset containing preset equipment slot configurations.
 *
 * This asset allows designers to define equipment slots without code changes.
 * If no DataAsset is configured, the system falls back to programmatic defaults
 * using Native Tags.
 *
 * SINGLE SOURCE OF TRUTH for equipment slot definitions.
 */
UCLASS(BlueprintType, Const)
class BRIDGESYSTEM_API USuspenseCoreEquipmentSlotPresets : public UDataAsset
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentSlotPresets();

	/**
	 * Array of equipment slot configurations.
	 * Each entry defines a named equipment slot (weapon, armor, storage, etc.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slots", meta = (TitleProperty = "DisplayName"))
	TArray<FEquipmentSlotConfig> SlotPresets;

	/**
	 * Get slot preset by slot type
	 * @param SlotType The equipment slot type to find
	 * @param OutConfig Output config if found
	 * @return True if found, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment")
	bool GetPresetByType(EEquipmentSlotType SlotType, FEquipmentSlotConfig& OutConfig) const;

	/**
	 * Get slot preset by gameplay tag
	 * @param SlotTag The slot's gameplay tag (Equipment.Slot.*)
	 * @param OutConfig Output config if found
	 * @return True if found, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment")
	bool GetPresetByTag(const FGameplayTag& SlotTag, FEquipmentSlotConfig& OutConfig) const;

	/**
	 * Get all slot presets
	 * @return Copy of all slot configurations
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment")
	TArray<FEquipmentSlotConfig> GetAllPresets() const { return SlotPresets; }

	/**
	 * Get slot count
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment")
	int32 GetSlotCount() const { return SlotPresets.Num(); }

	/**
	 * Validate all presets
	 * @return True if all presets are valid
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment")
	bool ValidatePresets() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//========================================
	// Static Factory Methods
	//========================================

	/**
	 * Create default slot presets using Native Tags.
	 * Called as fallback when no DataAsset is configured.
	 * @return Array of default slot configurations
	 */
	static TArray<FEquipmentSlotConfig> CreateDefaultPresets();

private:
	/** Internal helper to create a single slot config with Native Tags */
	static FEquipmentSlotConfig CreateSlotPreset(
		EEquipmentSlotType SlotType,
		const FGameplayTag& SlotTag,
		const FName& AttachmentSocket,
		const FGameplayTagContainer& AllowedTypes
	);
};

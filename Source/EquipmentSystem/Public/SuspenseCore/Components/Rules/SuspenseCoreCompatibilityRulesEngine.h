// SuspenseCoreCompatibilityRulesEngine.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Rules/SuspenseCoreRulesTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreSlotValidator.h"
#include "SuspenseCoreCompatibilityRulesEngine.generated.h"

// Forward declarations
class USuspenseCoreEquipmentSlotValidator;
class ISuspenseCoreItemDataProvider;

// Log category declared in SuspenseCompatibilityRulesEngine.h

/**
 * Compatibility rules engine (prod path).
 * - Uses SlotValidator as base prefilter (no duplication of checks).
 * - No world access; data comes via DI (item data provider) and default equipment provider.
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreCompatibilityRulesEngine : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreCompatibilityRulesEngine();

	//------------- Public API -------------
	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FSuspenseCoreRuleCheckResult CheckItemCompatibility(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FSuspenseCoreRuleCheckResult CheckTypeCompatibility(
		const FGameplayTag& ItemType,
		const FEquipmentSlotConfig& SlotConfig) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FSuspenseCoreAggregatedRuleResult EvaluateCompatibilityRules(
		const FSuspenseCoreRuleContext& Context) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	TArray<int32> FindCompatibleSlots(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		const TArray<FEquipmentSlotConfig>& AvailableSlots) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	float GetCompatibilityScore(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig) const;

	//------------- DI -------------
	/** Provide SlotValidator for base checks (no duplication). */
	void SetSlotValidator(USuspenseCoreEquipmentSlotValidator* InValidator);

	/** Provide authoritative item data provider (no world access). */
	void SetItemDataProvider(TSharedPtr<ISuspenseCoreItemDataProvider> InItemProvider);

	/** Optional default equipment data provider (used when Context does not supply one). */
	void SetDefaultEquipmentDataProvider(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InProvider);

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	void ClearCache() {}  // stateless

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	void ResetStatistics() {}  // stateless

protected:
	//------------- Internal helpers -------------
	bool GetItemData(FName ItemID, struct FSuspenseCoreUnifiedItemData& OutData) const;

	/** Convert SlotValidator result to rules-format (severity mapping). */
	static FSuspenseCoreRuleCheckResult Convert(const FSuspenseCoreSlotValidationResult& R);

private:
	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreEquipmentSlotValidator> SlotValidator = nullptr;

	TSharedPtr<ISuspenseCoreItemDataProvider> ItemProvider;
	TScriptInterface<ISuspenseCoreEquipmentDataProvider> DefaultEquipProvider;
};

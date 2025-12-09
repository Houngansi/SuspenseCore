// SuspenseCoreCompatibilityRulesEngine.h © MedCom Team

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
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
	FSuspenseRuleCheckResult CheckItemCompatibility(
		const FSuspenseInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FSuspenseRuleCheckResult CheckTypeCompatibility(
		const FGameplayTag& ItemType,
		const FEquipmentSlotConfig& SlotConfig) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FSuspenseAggregatedRuleResult EvaluateCompatibilityRules(
		const FSuspenseRuleContext& Context) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	TArray<int32> FindCompatibleSlots(
		const FSuspenseInventoryItemInstance& ItemInstance,
		const TArray<FEquipmentSlotConfig>& AvailableSlots) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	float GetCompatibilityScore(
		const FSuspenseInventoryItemInstance& ItemInstance,
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
	bool GetItemData(FName ItemID, struct FSuspenseUnifiedItemData& OutData) const;

	/** Convert SlotValidator result to rules-format (severity mapping). */
	static FSuspenseRuleCheckResult Convert(const FSlotValidationResult& R);

private:
	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreEquipmentSlotValidator> SlotValidator = nullptr;

	TSharedPtr<ISuspenseCoreItemDataProvider> ItemProvider;
	TScriptInterface<ISuspenseCoreEquipmentDataProvider> DefaultEquipProvider;
};

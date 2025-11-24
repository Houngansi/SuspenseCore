// MedComCompatibilityRulesEngine.h © MedCom Team

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Types/Rules/MedComRulesTypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Equipment/EquipmentTypes.h"
#include "Interfaces/Equipment/IMedComEquipmentDataProvider.h"

class UMedComEquipmentSlotValidator;
class IMedComItemDataProvider;

#include "MedComCompatibilityRulesEngine.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCompatibilityRules, Log, All);

/**
 * Compatibility rules engine (prod path).
 * - Uses SlotValidator as base prefilter (no duplication of checks).
 * - No world access; data comes via DI (item data provider) and default equipment provider.
 */
UCLASS(BlueprintType)
class MEDCOMEQUIPMENT_API UMedComCompatibilityRulesEngine : public UObject
{
	GENERATED_BODY()

public:
	UMedComCompatibilityRulesEngine();

	//------------- Public API -------------
	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FMedComRuleCheckResult CheckItemCompatibility(
		const FInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FMedComRuleCheckResult CheckTypeCompatibility(
		const FGameplayTag& ItemType,
		const FEquipmentSlotConfig& SlotConfig) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	FMedComAggregatedRuleResult EvaluateCompatibilityRules(
		const FMedComRuleContext& Context) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	TArray<int32> FindCompatibleSlots(
		const FInventoryItemInstance& ItemInstance,
		const TArray<FEquipmentSlotConfig>& AvailableSlots) const;

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	float GetCompatibilityScore(
		const FInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig) const;

	//------------- DI -------------
	/** Provide SlotValidator for base checks (no duplication). */
	void SetSlotValidator(UMedComEquipmentSlotValidator* InValidator);

	/** Provide authoritative item data provider (no world access). */
	void SetItemDataProvider(TSharedPtr<IMedComItemDataProvider> InItemProvider);

	/** Optional default equipment data provider (used when Context does not supply one). */
	void SetDefaultEquipmentDataProvider(TScriptInterface<IMedComEquipmentDataProvider> InProvider);

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	void ClearCache() {}  // stateless

	UFUNCTION(BlueprintCallable, Category="Compatibility")
	void ResetStatistics() {}  // stateless

protected:
	//------------- Internal helpers -------------
	bool GetItemData(FName ItemID, struct FMedComUnifiedItemData& OutData) const;

	/** Convert SlotValidator result to rules-format (severity mapping). */
	static FMedComRuleCheckResult Convert(const FSlotValidationResult& R);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMedComEquipmentSlotValidator> SlotValidator = nullptr;

	TSharedPtr<IMedComItemDataProvider> ItemProvider;
	TScriptInterface<IMedComEquipmentDataProvider> DefaultEquipProvider;
};

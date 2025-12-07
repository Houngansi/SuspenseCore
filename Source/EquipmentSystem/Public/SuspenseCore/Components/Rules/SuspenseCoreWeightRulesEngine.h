// SuspenseCoreWeightRulesEngine.h © MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Types/Rules/SuspenseCoreRulesTypes.h"
#include "Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreWeightRulesEngine.generated.h"

/**
 * Rules configuration for weight/encumbrance evaluation.
 */
USTRUCT(BlueprintType)
struct FMedComWeightConfig
{
    GENERATED_BODY()

    /** Base carrying capacity without attributes applied (kg units expected). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight")
    float BaseCarryCapacity = 20.0f;

    /** Additional capacity per point of Strength attribute. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight")
    float CapacityPerStrength = 2.0f;

    /** Tag-based weight multipliers (e.g., Item.Armor.* => 1.05). Not applied unless tags are provided. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(Categories="Item"))
    TMap<FGameplayTag, float> WeightModifiers;

    /** Whether overweight above capacity is allowed (movement penalties apply elsewhere). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight")
    bool bAllowOverweight = true;

    /** Max overweight ratio allowed if overweight is permitted (e.g., 1.25 = 125% of capacity). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(EditCondition="bAllowOverweight"))
    float MaxOverweightRatio = 1.25f;

    /** Overweight severity thresholds (0..1 of capacity). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight")
    float EncumberedThreshold = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight")
    float OverweightThreshold = 1.0f;

    /** Slots excluded from weight calculations (e.g., cosmetic). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(Categories="Equipment.Slot"))
    FGameplayTagContainer ExcludedSlots;
};

/**
 * Weight/Encumbrance rules engine.
 * SRP: read-only computations on snapshots; never mutates inventories/state.
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreWeightRulesEngine : public UObject
{
    GENERATED_BODY()
public:
    USuspenseCoreWeightRulesEngine();

    /** Set configuration (copy); thread-safe snapshot usage. */
    void Initialize(const FMedComWeightConfig& InConfig);

    //==================== Top-level evaluation ====================

    /** Evaluate incoming item vs capacity; returns aggregated rule result (strict on hard limit). */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    FSuspenseCoreAggregatedRuleResult EvaluateWeightRules(const FSuspenseCoreRuleContext& Context) const;

    /** Check simple capacity gate. */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    FSuspenseCoreRuleCheckResult CheckWeightLimit(float CurrentWeight, float AdditionalWeight, float MaxCapacity) const;

    /** Encumbrance evaluation for UX (ratio/tag). */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    FSuspenseCoreRuleCheckResult CheckEncumbrance(const AActor* Character, float TotalWeight) const;

    //==================== Capacity / weights ====================

    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    float CalculateWeightCapacity(const AActor* Character) const;

    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    float CalculateEncumbranceLevel(float TotalWeight, float Capacity) const;

    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    FGameplayTag GetEncumbranceTag(float Ratio) const;

    /** Weight for a single item: uses runtime "Weight" property; non-negative. */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    float CalculateItemWeight(const FSuspenseCoreInventoryItemInstance& Item) const;

    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    float CalculateTotalWeight(const TArray<FSuspenseCoreInventoryItemInstance>& Items) const;

    /** Optional tag-based modifiers. Tags must be supplied by caller; engine never fetches item data. */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    float ApplyWeightModifiers(float BaseWeight, const FGameplayTagContainer& ItemTags) const;

    //==================== Analytics ====================

    /** Split total by top-level item categories (tag roots). If no tags provided, all in Item.Unknown. */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    TMap<FGameplayTag, float> AnalyzeWeightDistribution(
        const TArray<FSuspenseCoreInventoryItemInstance>& Items,
        const TArray<FGameplayTagContainer>& OptionalItemTags /* parallel to Items, may be empty */) const;

    /** Indices of heaviest N items. */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    TArray<int32> FindHeaviestItems(const TArray<FSuspenseCoreInventoryItemInstance>& Items, int32 TopN = 3) const;
    //==================== Cache and Statistics ====================

    /** Clear any cached data (stateless engine - no-op) */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    void ClearCache();

    /** Reset internal statistics (stateless engine - no-op) */
    UFUNCTION(BlueprintCallable, Category="Weight Rules")
    void ResetStatistics();

protected:
    float GetCharacterStrength(const AActor* Character) const;
    float GetItemRuntimeWeight(const FSuspenseCoreInventoryItemInstance& Item) const;

private:
    /** Immutable configuration snapshot (updated via Initialize). */
    FMedComWeightConfig Configuration;
};

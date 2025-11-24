// MedComWeightRulesEngine.cpp © MedCom Team. All Rights Reserved.

#include "Components/Rules/MedComWeightRulesEngine.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagAssetInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeightRules, Log, All);

//==================== ctor/init ====================

UMedComWeightRulesEngine::UMedComWeightRulesEngine()
{
    // Configuration has sensible defaults
}

void UMedComWeightRulesEngine::Initialize(const FMedComWeightConfig& InConfig)
{
    Configuration = InConfig;
    UE_LOG(LogWeightRules, Log, TEXT("WeightRulesEngine initialized: BaseCapacity=%.2f, CapacityPerStrength=%.2f"), 
        Configuration.BaseCarryCapacity, Configuration.CapacityPerStrength);
}

//==================== top-level evaluation ====================

FMedComAggregatedRuleResult UMedComWeightRulesEngine::EvaluateWeightRules(const FMedComRuleContext& Context) const
{
    FMedComAggregatedRuleResult Agg;

    // Calculate capacity from character's strength (ASC-based)
    const float Capacity = CalculateWeightCapacity(Context.Character);

    // Current carried weight from shadow snapshot (Context.CurrentItems already computed by Coordinator)
    const float CurrentWeight = (Context.CurrentItems.Num() > 0)
        ? CalculateTotalWeight(Context.CurrentItems)
        : 0.0f;

    // Weight of incoming item (if any)
    const float AdditionalWeight = Context.ItemInstance.IsValid()
        ? CalculateItemWeight(Context.ItemInstance)
        : 0.0f;

    // Hard capacity gate check first
    {
        const FMedComRuleCheckResult R = CheckWeightLimit(CurrentWeight, AdditionalWeight, Capacity);
        Agg.AddResult(R);
        if (!R.bPassed && (R.Severity == EMedComRuleSeverity::Error || R.Severity == EMedComRuleSeverity::Critical))
        {
            return Agg; // Critical failure - stop processing
        }
    }

    // Soft encumbrance check for UX/metrics (informational)
    {
        const float TotalWeight = CurrentWeight + AdditionalWeight;
        const FMedComRuleCheckResult R = CheckEncumbrance(Context.Character, TotalWeight);
        Agg.AddResult(R);
    }

    return Agg;
}

FMedComRuleCheckResult UMedComWeightRulesEngine::CheckWeightLimit(float CurrentWeight, float AdditionalWeight, float MaxCapacity) const
{
    const float NewTotal = CurrentWeight + AdditionalWeight;

    if (NewTotal <= MaxCapacity)
    {
        FMedComRuleCheckResult OK = FMedComRuleCheckResult::Success(
            FText::Format(NSLOCTEXT("WeightRules", "WithinCapacity", "Weight within capacity: {0}/{1} kg"),
                FText::AsNumber(FMath::RoundToFloat(NewTotal * 10.0f) / 10.0f),
                FText::AsNumber(FMath::RoundToFloat(MaxCapacity * 10.0f) / 10.0f)));
        OK.RuleType = EMedComRuleType::Weight;
        OK.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Weight.Capacity.OK"));
        OK.Context.Add(TEXT("Current"), FString::SanitizeFloat(CurrentWeight));
        OK.Context.Add(TEXT("Additional"), FString::SanitizeFloat(AdditionalWeight));
        OK.Context.Add(TEXT("Capacity"), FString::SanitizeFloat(MaxCapacity));
        return OK;
    }

    // Over capacity - check if overweight is allowed
    const float MaxAllowedWeight = MaxCapacity * FMath::Max(1.0f, Configuration.MaxOverweightRatio);
    
    if (!Configuration.bAllowOverweight || NewTotal > MaxAllowedWeight)
    {
        FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("WeightRules", "OverCapacity", "Exceeds carry capacity: {0}/{1} kg"),
                FText::AsNumber(FMath::RoundToFloat(NewTotal * 10.0f) / 10.0f),
                FText::AsNumber(FMath::RoundToFloat(MaxCapacity * 10.0f) / 10.0f)),
            EMedComRuleSeverity::Error);
        R.RuleType = EMedComRuleType::Weight;
        R.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Weight.Capacity.Exceeded"));
        R.Context.Add(TEXT("TotalWeight"), FString::SanitizeFloat(NewTotal));
        R.Context.Add(TEXT("Capacity"), FString::SanitizeFloat(MaxCapacity));
        R.Context.Add(TEXT("MaxAllowed"), FString::SanitizeFloat(MaxAllowedWeight));
        return R;
    }

    // Allowed overweight - pass but with warning
    FMedComRuleCheckResult W = FMedComRuleCheckResult::Success(
        FText::Format(NSLOCTEXT("WeightRules", "OverweightAllowed", "Overweight but allowed: {0}/{1} kg"),
            FText::AsNumber(FMath::RoundToFloat(NewTotal * 10.0f) / 10.0f),
            FText::AsNumber(FMath::RoundToFloat(MaxCapacity * 10.0f) / 10.0f)));
    W.RuleType = EMedComRuleType::Weight;
    W.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Weight.Capacity.Overweight"));
    W.Severity = EMedComRuleSeverity::Warning;
    W.bCanOverride = true;
    W.Context.Add(TEXT("TotalWeight"), FString::SanitizeFloat(NewTotal));
    W.Context.Add(TEXT("Capacity"), FString::SanitizeFloat(MaxCapacity));
    return W;
}

FMedComRuleCheckResult UMedComWeightRulesEngine::CheckEncumbrance(const AActor* Character, float TotalWeight) const
{
    const float Capacity = CalculateWeightCapacity(Character);
    const float Ratio = CalculateEncumbranceLevel(TotalWeight, Capacity);
    const FGameplayTag EncumbranceTag = GetEncumbranceTag(Ratio);

    EMedComRuleSeverity Severity = EMedComRuleSeverity::Info;
    if (Ratio >= Configuration.OverweightThreshold)
    {
        Severity = EMedComRuleSeverity::Warning;
    }
    else if (Ratio >= Configuration.EncumberedThreshold)
    {
        Severity = EMedComRuleSeverity::Info;
    }

    FMedComRuleCheckResult R = FMedComRuleCheckResult::Success(
        FText::Format(NSLOCTEXT("WeightRules", "EncumbranceInfo", "Encumbrance level: {0}% ({1})"),
            FText::AsNumber(FMath::RoundToInt(Ratio * 100.0f)),
            FText::FromString(EncumbranceTag.ToString())));
    R.RuleType = EMedComRuleType::Weight;
    R.RuleTag = EncumbranceTag;
    R.Severity = Severity;
    R.Context.Add(TEXT("EncumbranceRatio"), FString::SanitizeFloat(Ratio));
    R.Context.Add(TEXT("EncumbranceTag"), EncumbranceTag.ToString());
    R.Context.Add(TEXT("TotalWeight"), FString::SanitizeFloat(TotalWeight));
    R.Context.Add(TEXT("Capacity"), FString::SanitizeFloat(Capacity));
    return R;
}

//==================== capacity / weights ====================

float UMedComWeightRulesEngine::CalculateWeightCapacity(const AActor* Character) const
{
    const float Strength = GetCharacterStrength(Character);
    const float Capacity = FMath::Max(0.0f, Configuration.BaseCarryCapacity + Strength * Configuration.CapacityPerStrength);
    return Capacity;
}

float UMedComWeightRulesEngine::CalculateEncumbranceLevel(float TotalWeight, float Capacity) const
{
    if (Capacity <= 0.0f)
    {
        return (TotalWeight > 0.0f) ? 2.0f : 0.0f; // Max encumbrance if no capacity but has weight
    }
    return FMath::Clamp(TotalWeight / Capacity, 0.0f, 2.0f); // Allow slight >1 for overweight display
}

FGameplayTag UMedComWeightRulesEngine::GetEncumbranceTag(float Ratio) const
{
    if (Ratio >= Configuration.OverweightThreshold)
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbrance.Overweight"));
    }
    if (Ratio >= Configuration.EncumberedThreshold)
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbrance.Encumbered"));
    }
    return FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbrance.Normal"));
}

float UMedComWeightRulesEngine::CalculateItemWeight(const FInventoryItemInstance& Item) const
{
    // Weight is read directly from runtime property on item instance
    // No external data sources - engine doesn't fetch item data from world/managers
    const float BaseWeight = GetItemRuntimeWeight(Item);
    
    // Apply quantity multiplier
    const int32 Quantity = FMath::Max(1, Item.Quantity);
    
    // Note: Weight modifiers are applied externally via ApplyWeightModifiers() if tags are provided
    // This engine doesn't fetch item tags from external sources
    return FMath::Max(0.0f, BaseWeight * Quantity);
}

float UMedComWeightRulesEngine::CalculateTotalWeight(const TArray<FInventoryItemInstance>& Items) const
{
    float Sum = 0.0f;
    for (const FInventoryItemInstance& Item : Items)
    {
        Sum += CalculateItemWeight(Item);
    }
    return FMath::Max(0.0f, Sum);
}

float UMedComWeightRulesEngine::ApplyWeightModifiers(float BaseWeight, const FGameplayTagContainer& ItemTags) const
{
    // Early exit if no tags or no modifiers configured
    if (ItemTags.IsEmpty() || Configuration.WeightModifiers.Num() == 0)
    {
        return BaseWeight;
    }

    float ModifiedWeight = BaseWeight;
    
    // Apply all matching tag-based modifiers
    for (const TPair<FGameplayTag, float>& Pair : Configuration.WeightModifiers)
    {
        const FGameplayTag& ModifierTag = Pair.Key;
        const float Multiplier = Pair.Value;
        
        if (ModifierTag.IsValid() && ItemTags.HasTag(ModifierTag))
        {
            ModifiedWeight *= FMath::Max(0.0f, Multiplier);
            
            UE_LOG(LogWeightRules, Verbose, TEXT("Applied weight modifier %s: %.2f -> %.2f"),
                *ModifierTag.ToString(), BaseWeight, ModifiedWeight);
        }
    }
    
    return FMath::Max(0.0f, ModifiedWeight);
}

//==================== analytics ====================

TMap<FGameplayTag, float> UMedComWeightRulesEngine::AnalyzeWeightDistribution(
    const TArray<FInventoryItemInstance>& Items,
    const TArray<FGameplayTagContainer>& OptionalItemTags) const
{
    TMap<FGameplayTag, float> Distribution;

    const bool bHaveTags = OptionalItemTags.Num() == Items.Num() && Items.Num() > 0;

    for (int32 i = 0; i < Items.Num(); ++i)
    {
        const float ItemWeight = CalculateItemWeight(Items[i]);

        // Determine bucket based on item tags (if provided)
        FGameplayTag Bucket = FGameplayTag::RequestGameplayTag(TEXT("Item.Unknown"));
        if (bHaveTags && !OptionalItemTags[i].IsEmpty())
        {
            // Use the first Item.* tag as category bucket
            for (const FGameplayTag& Tag : OptionalItemTags[i])
            {
                if (Tag.ToString().StartsWith(TEXT("Item.")))
                {
                    Bucket = Tag;
                    break;
                }
            }
        }

        // Accumulate weight in bucket
        float& BucketWeight = Distribution.FindOrAdd(Bucket, 0.0f);
        BucketWeight += ItemWeight;
    }

    return Distribution;
}

TArray<int32> UMedComWeightRulesEngine::FindHeaviestItems(const TArray<FInventoryItemInstance>& Items, int32 TopN) const
{
    struct FIndexedWeight { int32 Index; float Weight; };
    
    TArray<FIndexedWeight> IndexedWeights;
    IndexedWeights.Reserve(Items.Num());
    
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        IndexedWeights.Add({i, CalculateItemWeight(Items[i])});
    }
    
    // Sort by weight descending
    IndexedWeights.Sort([](const FIndexedWeight& A, const FIndexedWeight& B)
    {
        return A.Weight > B.Weight;
    });
    
    // Extract top N indices
    TopN = FMath::Clamp(TopN, 0, IndexedWeights.Num());
    TArray<int32> Result;
    Result.Reserve(TopN);
    
    for (int32 i = 0; i < TopN; ++i)
    {
        Result.Add(IndexedWeights[i].Index);
    }
    
    return Result;
}

//==================== cache and statistics (required by coordinator) ====================

void UMedComWeightRulesEngine::ClearCache()
{
    // Weight engine is stateless - no cache to clear
    UE_LOG(LogWeightRules, Log, TEXT("Cache cleared (no cache maintained)"));
}

void UMedComWeightRulesEngine::ResetStatistics()
{
    // Weight engine doesn't maintain internal statistics beyond what Coordinator tracks
    UE_LOG(LogWeightRules, Log, TEXT("Statistics reset (no internal statistics maintained)"));
}

//==================== data access helpers (ASC-only, no world access) ====================

float UMedComWeightRulesEngine::GetCharacterStrength(const AActor* Character) const
{
    const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character);
    if (!ASI) { return 0.0f; }
    const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC) { return 0.0f; }

    // Try common strength attribute names
    static const FName StrengthNames[] = { 
        FName(TEXT("Strength")), 
        FName(TEXT("STR")),
        FName(TEXT("Str"))
    };

    const TArray<UAttributeSet*>& Sets = ASC->GetSpawnedAttributes();
    
    for (const FName& Name : StrengthNames)
    {
        for (const UAttributeSet* Set : Sets)
        {
            if (!Set) { continue; }
            
            if (FProperty* Prop = Set->GetClass()->FindPropertyByName(Name))
            {
                if (const FFloatProperty* FP = CastField<FFloatProperty>(Prop))
                {
                    const void* Ptr = FP->ContainerPtrToValuePtr<void>(Set);
                    const float* Val = static_cast<const float*>(Ptr);
                    if (Val) { return *Val; }
                }
                if (const FIntProperty* IP = CastField<FIntProperty>(Prop))
                {
                    const void* Ptr = IP->ContainerPtrToValuePtr<void>(Set);
                    const int32* Val = static_cast<const int32*>(Ptr);
                    if (Val) { return static_cast<float>(*Val); }
                }
            }
        }
    }
    
    // No strength attribute found - return 0 (base capacity will still apply)
    return 0.0f;
}

float UMedComWeightRulesEngine::GetItemRuntimeWeight(const FInventoryItemInstance& Item) const
{
    // Read weight directly from runtime property - no external data fetching
    const float Weight = Item.GetRuntimeProperty(TEXT("Weight"), 0.0f);
    return FMath::Max(0.0f, Weight);
}
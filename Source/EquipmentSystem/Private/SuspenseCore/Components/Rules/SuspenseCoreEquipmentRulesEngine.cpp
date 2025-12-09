// SuspenseCoreEquipmentRulesEngine.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Rules/SuspenseCoreEquipmentRulesEngine.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/IConsoleManager.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogEquipmentRules, Log, All);

// Console variable for dev fallback control
static TAutoConsoleVariable<int32> CVarSuspenseCoreUseMonolith(
    TEXT("suspensecore.rules.use_monolith"),
    0,
    TEXT("Enable monolithic rules engine for development/debugging.\n")
    TEXT("0: Disabled (production path through coordinator)\n")
    TEXT("1: Enabled (dev fallback mode)"),
    ECVF_Default
);

USuspenseCoreEquipmentRulesEngine::USuspenseCoreEquipmentRulesEngine()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Initialize configuration
    WeightConfig.BaseWeightLimit = 100.0f;
    WeightConfig.WeightPerStrength = 5.0f;
    MaxEvaluationDepth = 10;
    bEnableCaching = true;
    CacheDuration = 5.0f;
    bEnableDetailedLogging = false;
    MaxViolationHistory = 1000;
    bDevFallbackEnabled = false;  // Disabled by default for production

    // Initialize state
    bIsInitialized = false;
    EngineVersion = 1;
    CurrentEvaluationDepth = 0;
    LastUpdateTime = FDateTime::Now();

    // Setup default encumbrance thresholds
    WeightConfig.EncumbranceThresholds.Add(0.5f, FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbered.Light")));
    WeightConfig.EncumbranceThresholds.Add(0.75f, FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbered.Medium")));
    WeightConfig.EncumbranceThresholds.Add(1.0f, FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbered.Heavy")));
    WeightConfig.EncumbranceThresholds.Add(1.25f, FGameplayTag::RequestGameplayTag(TEXT("Status.Encumbered.Overloaded")));
}

USuspenseCoreEquipmentRulesEngine::~USuspenseCoreEquipmentRulesEngine()
{
    // Cleanup handled by UE4
}

void USuspenseCoreEquipmentRulesEngine::BeginPlay()
{
    Super::BeginPlay();

    // Only register default rules if dev mode is enabled
    if (ShouldUseDevFallback())
    {
        RegisterDefaultRules();
        UE_LOG(LogEquipmentRules, Log, TEXT("Equipment Rules Engine (DEV FALLBACK) initialized with %d rules"),
            RegisteredRules.Num());
    }
    else
    {
        UE_LOG(LogEquipmentRules, Log, TEXT("Equipment Rules Engine (PRODUCTION DISABLED) - use USuspenseCoreRulesCoordinator"));
    }
}

void USuspenseCoreEquipmentRulesEngine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear all data
    RegisteredRules.Empty();
    EnabledRules.Empty();
    RulePriorities.Empty();
    RuleDependencies.Empty();
    ViolationHistory.Empty();
    RuleStats.Empty();
    ResultCache.Empty();
    CacheTimestamps.Empty();

    Super::EndPlay(EndPlayReason);
}

//========================================
// Dev Fallback Control
//========================================

bool USuspenseCoreEquipmentRulesEngine::ShouldUseDevFallback() const
{
    return bDevFallbackEnabled && CVarSuspenseCoreUseMonolith.GetValueOnGameThread() != 0;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CreateDisabledResult(const FString& MethodName) const
{
    FRuleEvaluationResult Result;
    Result.bPassed = true;  // Don't block operations in production
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Dev.Monolith.Disabled"));
    Result.FailureReason = FText::FromString(FString::Printf(
        TEXT("Monolithic rules engine is disabled. Use USuspenseCoreRulesCoordinator for production validation. (Method: %s)"),
        *MethodName));
    Result.ConfidenceScore = 1.0f;
    Result.Details.Add(TEXT("Production path: Use USuspenseCoreRulesCoordinator"));
    Result.Details.Add(FString::Printf(TEXT("Enable with: suspensecore.rules.use_monolith 1 or bDevFallbackEnabled=true")));

    return Result;
}

void USuspenseCoreEquipmentRulesEngine::SetDevFallbackEnabled(bool bEnabled)
{
    bDevFallbackEnabled = bEnabled;
    UE_LOG(LogEquipmentRules, Log, TEXT("Dev fallback mode %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

bool USuspenseCoreEquipmentRulesEngine::IsDevFallbackEnabled() const
{
    return ShouldUseDevFallback();
}

//========================================
// ISuspenseCoreEquipmentRules Implementation (DEV FALLBACK)
//========================================

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::EvaluateRules(const FEquipmentOperationRequest& Operation) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("EvaluateRules"));
    }

    FScopeLock Lock(&RuleCriticalSection);
    const double StartTime = FPlatformTime::Seconds();

    FRuleExecutionContext Context;
    Context.Character = Operation.Instigator.Get();
    Context.Operation = Operation;
    Context.Timestamp = StartTime;

    if (DataProvider.GetInterface())
    {
        Context.CurrentState = DataProvider->CreateSnapshot();
    }

    TArray<FEquipmentRule> ApplicableRules;
    for (const auto& Pair : RegisteredRules)
    {
        if (EnabledRules.Contains(Pair.Key))
        {
            ApplicableRules.Add(Pair.Value);
        }
    }
    ApplicableRules = PrioritizeRules(ApplicableRules);

    FRuleEvaluationResult Overall;
    Overall.bPassed = true;
    Overall.ConfidenceScore = 1.0f;

    for (const FEquipmentRule& Rule : ApplicableRules)
    {
        const FRuleEvaluationResult RuleResult = ExecuteRule(Rule, Context);

        if (!RuleResult.bPassed)
        {
            if (Rule.bIsStrict)
            {
                Overall = RuleResult;
                Overall.RuleType = Rule.RuleTag;

                FRuleViolation V;
                V.ViolatedRule     = Rule;
                V.EvaluationResult = RuleResult;
                V.ViolationTime    = FDateTime::Now();
                V.Context          = FString::Printf(TEXT("Operation: %s"),
                                        *UEnum::GetValueAsString(Operation.OperationType));
                V.Severity         = 10;
                RecordViolation(V); // теперь const
                break;
            }
            else
            {
                Overall.ConfidenceScore *= 0.8f;
                Overall.Details.Add(FString::Printf(TEXT("Warning: %s"),
                                        *RuleResult.FailureReason.ToString()));
            }
        }

        const float Ms = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);
        UpdateStatistics(Rule.RuleTag, RuleResult.bPassed, Ms);
    }

    return Overall;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::EvaluateRulesWithContext(
    const FEquipmentOperationRequest& Operation,
    const FSuspenseCoreRuleContext& Context) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("EvaluateRulesWithContext"));
    }

    // For dev fallback, convert context and delegate to regular evaluation
    // This is simplified - in practice would use the provided context more directly
    return EvaluateRules(Operation);
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckItemCompatibility(
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    const FSuspenseCoreEquipmentSlotConfig& SlotConfig) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckItemCompatibility"));
    }

    FRuleEvaluationResult R;
    R.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility"));

    FSuspenseCoreUnifiedItemData ItemData;
    if (!GetItemData(ItemInstance.ItemID, ItemData))
    {
        R.bPassed = false;
        R.FailureReason   = FText::FromString(TEXT("Item data not found"));
        R.ConfidenceScore = 0.0f;
        return R;
    }

    if (!ItemData.bIsEquippable)
    {
        R.bPassed = false;
        R.FailureReason   = FText::FromString(FString::Printf(TEXT("%s is not equippable"),
                                    *ItemData.DisplayName.ToString()));
        R.ConfidenceScore = 0.0f;
        return R;
    }

    if (!ItemData.EquipmentSlot.MatchesTag(SlotConfig.SlotTag))
    {
        R.bPassed = false;
        R.FailureReason   = FText::FromString(FString::Printf(
                                TEXT("%s cannot be equipped in %s slot"),
                                *ItemData.DisplayName.ToString(),
                                *SlotConfig.DisplayName.ToString()));
        R.ConfidenceScore = 0.0f;
        R.Details.Add(FString::Printf(TEXT("Item slot: %s"), *ItemData.EquipmentSlot.ToString()));
        R.Details.Add(FString::Printf(TEXT("Required slot: %s"), *SlotConfig.SlotTag.ToString()));
        return R;
    }

    if (SlotConfig.AllowedItemTypes.Num() > 0)
    {
        bool bAllowed = false;
        for (const FGameplayTag& AllowedType : SlotConfig.AllowedItemTypes)
        {
            if (ItemData.ItemType.MatchesTag(AllowedType))
            {
                bAllowed = true;
                break;
            }
        }

        if (!bAllowed)
        {
            R.bPassed = false;
            R.FailureReason   = FText::FromString(FString::Printf(
                                    TEXT("Item type %s not allowed in this slot"),
                                    *ItemData.ItemType.ToString()));
            R.ConfidenceScore = 0.0f;
            return R;
        }
    }

    const float Durability = ItemInstance.GetRuntimeProperty(TEXT("Durability"), 100.0f);
    if (Durability <= 0.0f)
    {
        R.bPassed = false;
        R.FailureReason   = FText::FromString(TEXT("Item is broken and cannot be equipped"));
        R.ConfidenceScore = 0.0f;
        R.Details.Add(FString::Printf(TEXT("Durability: %.1f"), Durability));
        return R;
    }
    else if (Durability < 20.0f)
    {
        R.bPassed = true;
        R.ConfidenceScore = 0.5f;
        R.Details.Add(FString::Printf(TEXT("Warning: Item durability low (%.1f)"), Durability));
    }
    else
    {
        R.bPassed = true;
        R.ConfidenceScore = 1.0f;
    }

    return R;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckCharacterRequirements(
    const AActor* Character,
    const FSuspenseCoreInventoryItemInstance& ItemInstance) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckCharacterRequirements"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.CharacterRequirements"));

    if (!Character)
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("No character specified"));
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    // Get item requirements
    FCharacterRequirements Requirements = GetItemRequirements(ItemInstance);

    // Check character meets requirements
    Result = CheckCharacterMeetsRequirements(Character, Requirements);

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckWeightLimit(
    float CurrentWeight,
    float AdditionalWeight) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckWeightLimit"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.WeightLimit"));

    // Calculate weight capacity
    float WeightCapacity = WeightConfig.BaseWeightLimit;

    if (AActor* Owner = GetOwner())
    {
        WeightCapacity = CalculateWeightCapacity(Owner);
    }

    float TotalWeight = CurrentWeight + AdditionalWeight;
    float WeightRatio = TotalWeight / WeightCapacity;

    if (WeightRatio > 1.25f)
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("Would exceed maximum weight capacity"));
        Result.ConfidenceScore = 0.0f;
        Result.Details.Add(FString::Printf(TEXT("Current: %.1f"), CurrentWeight));
        Result.Details.Add(FString::Printf(TEXT("Additional: %.1f"), AdditionalWeight));
        Result.Details.Add(FString::Printf(TEXT("Capacity: %.1f"), WeightCapacity));
        Result.Details.Add(FString::Printf(TEXT("Total: %.1f (%.0f%%)"), TotalWeight, WeightRatio * 100.0f));
    }
    else if (WeightRatio > 1.0f)
    {
        Result.bPassed = true;
        Result.ConfidenceScore = 0.3f;
        Result.Details.Add(FString::Printf(TEXT("Warning: Overloaded (%.0f%% capacity)"), WeightRatio * 100.0f));
    }
    else if (WeightRatio > 0.75f)
    {
        Result.bPassed = true;
        Result.ConfidenceScore = 0.6f;
        Result.Details.Add(FString::Printf(TEXT("Heavy load (%.0f%% capacity)"), WeightRatio * 100.0f));
    }
    else
    {
        Result.bPassed = true;
        Result.ConfidenceScore = 1.0f;
        Result.Details.Add(FString::Printf(TEXT("Weight OK (%.0f%% capacity)"), WeightRatio * 100.0f));
    }

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckConflictingEquipment(
    const TArray<FSuspenseCoreInventoryItemInstance>& ExistingItems,
    const FSuspenseCoreInventoryItemInstance& NewItem) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckConflictingEquipment"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.ConflictDetection"));
    Result.bPassed = true;
    Result.ConfidenceScore = 1.0f;

    // Get new item data from provider
    FSuspenseCoreUnifiedItemData NewItemData;
    if (!GetItemData(NewItem.ItemID, NewItemData))
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("New item data not found"));
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    // Check for conflicts with existing items
    for (const FSuspenseCoreInventoryItemInstance& ExistingItem : ExistingItems)
    {
        if (!ExistingItem.IsValid())
        {
            continue;
        }

        FSuspenseCoreUnifiedItemData ExistingItemData;
        if (!GetItemData(ExistingItem.ItemID, ExistingItemData))
        {
            continue;
        }

        // Check for mutually exclusive tags
        if (NewItemData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Exclusive"))))
        {
            if (ExistingItemData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Exclusive"))))
            {
                Result.bPassed = false;
                Result.FailureReason = FText::FromString(FString::Printf(
                    TEXT("%s conflicts with %s (both exclusive items)"),
                    *NewItemData.DisplayName.ToString(),
                    *ExistingItemData.DisplayName.ToString()));
                Result.ConfidenceScore = 0.0f;
                Result.Details.Add(TEXT("Conflict type: Mutual exclusivity"));
                return Result;
            }
        }

        // Check for specific incompatibilities
        FGameplayTagContainer IncompatibleTags;
        IncompatibleTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Incompatible.Heavy")));
        IncompatibleTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Incompatible.Light")));

        if (NewItemData.ItemTags.HasAny(IncompatibleTags) &&
            ExistingItemData.ItemTags.HasAny(IncompatibleTags))
        {
            FGameplayTagContainer Intersection = NewItemData.ItemTags.Filter(ExistingItemData.ItemTags);
            if (Intersection.Num() == 0)
            {
                Result.bPassed = false;
                Result.FailureReason = FText::FromString(FString::Printf(
                    TEXT("%s is incompatible with %s"),
                    *NewItemData.DisplayName.ToString(),
                    *ExistingItemData.DisplayName.ToString()));
                Result.ConfidenceScore = 0.0f;
                Result.Details.Add(TEXT("Conflict type: Tag incompatibility"));
                return Result;
            }
        }
    }

    return Result;
}

TArray<FEquipmentRule> USuspenseCoreEquipmentRulesEngine::GetActiveRules() const
{
    if (!ShouldUseDevFallback())
    {
        return TArray<FEquipmentRule>(); // Empty array for production
    }

    FScopeLock Lock(&RuleCriticalSection);

    TArray<FEquipmentRule> ActiveRules;

    for (const auto& RulePair : RegisteredRules)
    {
        if (EnabledRules.Contains(RulePair.Key))
        {
            ActiveRules.Add(RulePair.Value);
        }
    }

    return PrioritizeRules(ActiveRules);
}

bool USuspenseCoreEquipmentRulesEngine::RegisterRule(const FEquipmentRule& Rule)
{
    if (!ShouldUseDevFallback())
    {
        UE_LOG(LogEquipmentRules, Warning, TEXT("RegisterRule called on disabled monolith engine"));
        return false;
    }

    FScopeLock Lock(&RuleCriticalSection);

    if (!Rule.RuleTag.IsValid())
    {
        UE_LOG(LogEquipmentRules, Warning, TEXT("RegisterRule: Invalid rule tag"));
        return false;
    }

    RegisteredRules.Add(Rule.RuleTag, Rule);
    EnabledRules.Add(Rule.RuleTag);
    RulePriorities.Add(Rule.RuleTag, Rule.Priority);

    // Initialize statistics
    if (!RuleStats.Contains(Rule.RuleTag))
    {
        FRuleStatistics Stats;
        Stats.LastEvaluationTime = FDateTime::Now();
        RuleStats.Add(Rule.RuleTag, Stats);
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogEquipmentRules, Log, TEXT("Registered rule: %s (Priority: %d)"),
            *Rule.RuleTag.ToString(), Rule.Priority);
    }

    return true;
}

bool USuspenseCoreEquipmentRulesEngine::UnregisterRule(const FGameplayTag& RuleTag)
{
    if (!ShouldUseDevFallback())
    {
        return false;
    }

    FScopeLock Lock(&RuleCriticalSection);

    if (RegisteredRules.Remove(RuleTag) > 0)
    {
        EnabledRules.Remove(RuleTag);
        RulePriorities.Remove(RuleTag);
        RuleDependencies.Remove(RuleTag);

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogEquipmentRules, Log, TEXT("Unregistered rule: %s"), *RuleTag.ToString());
        }

        return true;
    }

    return false;
}

bool USuspenseCoreEquipmentRulesEngine::SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled)
{
    if (!ShouldUseDevFallback())
    {
        return false;
    }

    FScopeLock Lock(&RuleCriticalSection);

    if (!RegisteredRules.Contains(RuleTag))
    {
        return false;
    }

    if (bEnabled)
    {
        EnabledRules.Add(RuleTag);
    }
    else
    {
        EnabledRules.Remove(RuleTag);
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogEquipmentRules, Log, TEXT("Rule %s %s"),
            *RuleTag.ToString(), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    }

    return true;
}

FString USuspenseCoreEquipmentRulesEngine::GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const
{
    if (!ShouldUseDevFallback())
    {
        return TEXT("Monolithic Rules Engine Compliance Report\n")
               TEXT("==========================================\n\n")
               TEXT("Status: DISABLED (Production path uses USuspenseCoreRulesCoordinator)\n\n")
               TEXT("To enable dev fallback mode:\n")
               TEXT("  - Set suspensecore.rules.use_monolith 1\n")
               TEXT("  - Or enable bDevFallbackEnabled in component properties\n\n")
               TEXT("This is a development tool only. Production validation\n")
               TEXT("is handled by the specialized rules coordinator.\n");
    }

    FString Report = TEXT("Equipment Compliance Report (DEV FALLBACK)\n");
    Report += TEXT("============================================\n\n");

    // Базовая информация о состоянии системы
    Report += FString::Printf(TEXT("Timestamp: %s\n"), *FDateTime::Now().ToString());
    Report += FString::Printf(TEXT("Active Rules: %d\n"), EnabledRules.Num());

    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: переход на новую структуру снапшота
    // Старый код использовал CurrentState.SlotConfigurations и CurrentState.SlotItems
    // Новый код использует CurrentState.SlotSnapshots - единую структуру
    Report += FString::Printf(TEXT("Total Slots: %d\n"), CurrentState.SlotSnapshots.Num());

    // Подсчет занятых слотов через новую структуру
    int32 OccupiedSlots = 0;
    for (const FEquipmentSlotSnapshot& SlotSnapshot : CurrentState.SlotSnapshots)
    {
        if (SlotSnapshot.ItemInstance.IsValid())
        {
            OccupiedSlots++;
        }
    }
    Report += FString::Printf(TEXT("Occupied Slots: %d\n\n"), OccupiedSlots);

    // Оценка соответствия правилам
    Report += TEXT("Rule Compliance:\n");
    Report += TEXT("----------------\n");

    FRuleExecutionContext Context;
    Context.Character = GetOwner();
    Context.CurrentState = CurrentState;
    Context.Timestamp = FPlatformTime::Seconds();

    int32 PassedRules = 0;
    int32 FailedRules = 0;

    // Проверяем каждое активное правило
    for (const auto& RulePair : RegisteredRules)
    {
        if (!EnabledRules.Contains(RulePair.Key))
        {
            continue;  // Пропускаем отключенные правила
        }

        FRuleEvaluationResult Result = ExecuteRule(RulePair.Value, Context);

        if (Result.bPassed)
        {
            PassedRules++;
            Report += FString::Printf(TEXT("[PASS] %s\n"), *RulePair.Value.Description.ToString());
        }
        else
        {
            FailedRules++;
            Report += FString::Printf(TEXT("[FAIL] %s - %s\n"),
                *RulePair.Value.Description.ToString(),
                *Result.FailureReason.ToString());
        }
    }

    // Статистика соответствия
    const float ComplianceRate = (PassedRules > 0)
        ? ((float)PassedRules / (PassedRules + FailedRules) * 100.0f)
        : 0.0f;
    Report += FString::Printf(TEXT("\nCompliance Rate: %.1f%%\n"), ComplianceRate);

    // Анализ веса - извлекаем предметы из новой структуры снапшота
    TArray<FSuspenseCoreInventoryItemInstance> AllEquippedItems;
    for (const FEquipmentSlotSnapshot& SlotSnapshot : CurrentState.SlotSnapshots)
    {
        if (SlotSnapshot.ItemInstance.IsValid())
        {
            AllEquippedItems.Add(SlotSnapshot.ItemInstance);
        }
    }

    float TotalWeight = CalculateTotalWeight(AllEquippedItems);
    float WeightCapacity = CalculateWeightCapacity(GetOwner());

    Report += TEXT("\nWeight Status:\n");
    Report += FString::Printf(TEXT("  Current: %.1f kg\n"), TotalWeight);
    Report += FString::Printf(TEXT("  Capacity: %.1f kg\n"), WeightCapacity);
    Report += FString::Printf(TEXT("  Usage: %.0f%%\n"),
        (WeightCapacity > 0.0f) ? (TotalWeight / WeightCapacity * 100.0f) : 0.0f);

    // История нарушений
    if (ViolationHistory.Num() > 0)
    {
        const int32 MaxViolationsToShow = 10;
        Report += FString::Printf(TEXT("\nRecent Violations: %d\n"),
            FMath::Min(MaxViolationsToShow, ViolationHistory.Num()));

        int32 Count = 0;
        // Показываем самые свежие нарушения
        for (int32 i = ViolationHistory.Num() - 1; i >= 0 && Count < MaxViolationsToShow; i--, Count++)
        {
            const FRuleViolation& Violation = ViolationHistory[i];
            Report += FString::Printf(TEXT("  - %s: %s\n"),
                *Violation.ViolatedRule.RuleTag.ToString(),
                *Violation.EvaluationResult.FailureReason.ToString());
        }
    }

    // Напоминание о режиме разработки
    Report += TEXT("\n[DEV FALLBACK MODE ACTIVE - Use USuspenseCoreRulesCoordinator for production]\n");

    return Report;
}

//========================================
// Extended Rule Management
//========================================

bool USuspenseCoreEquipmentRulesEngine::Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    if (!ShouldUseDevFallback())
    {
        UE_LOG(LogEquipmentRules, Warning, TEXT("Initialize called on disabled monolith engine"));
        return false;
    }

    if (!InDataProvider.GetInterface())
    {
        UE_LOG(LogEquipmentRules, Error, TEXT("Initialize: Invalid data provider"));
        return false;
    }

    DataProvider = InDataProvider;
    bIsInitialized = true;

    UE_LOG(LogEquipmentRules, Log, TEXT("Rules engine (DEV FALLBACK) initialized with data provider"));

    return true;
}

int32 USuspenseCoreEquipmentRulesEngine::LoadRulesFromDataTable(UDataTable* RulesTable)
{
    if (!ShouldUseDevFallback() || !RulesTable)
    {
        return 0;
    }

    int32 LoadedCount = 0;

    // Assuming data table has FSuspenseCoreEquipmentRule rows
    const TMap<FName, uint8*>& RowMap = RulesTable->GetRowMap();

    for (const auto& Row : RowMap)
    {
        const FEquipmentRule* RuleData = reinterpret_cast<const FEquipmentRule*>(Row.Value);
        if (RuleData && RegisterRule(*RuleData))
        {
            LoadedCount++;
        }
    }

    UE_LOG(LogEquipmentRules, Log, TEXT("Loaded %d rules from data table (DEV FALLBACK)"), LoadedCount);

    return LoadedCount;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::EvaluateSpecificRule(
    const FGameplayTag& RuleTag,
    const FRuleExecutionContext& Context) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("EvaluateSpecificRule"));
    }

    const FEquipmentRule* Rule = RegisteredRules.Find(RuleTag);
    if (!Rule)
    {
        FRuleEvaluationResult Result;
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("Rule not found"));
        Result.RuleType = RuleTag;
        return Result;
    }

    return ExecuteRule(*Rule, Context);
}

TArray<FRuleEvaluationResult> USuspenseCoreEquipmentRulesEngine::BatchEvaluateRules(
    const TArray<FGameplayTag>& RuleTags,
    const FRuleExecutionContext& Context) const
{
    TArray<FRuleEvaluationResult> Results;

    if (!ShouldUseDevFallback())
    {
        // Return disabled results for all tags
        for (const FGameplayTag& RuleTag : RuleTags)
        {
            Results.Add(CreateDisabledResult(TEXT("BatchEvaluateRules")));
        }
        return Results;
    }

    for (const FGameplayTag& RuleTag : RuleTags)
    {
        Results.Add(EvaluateSpecificRule(RuleTag, Context));
    }

    return Results;
}

void USuspenseCoreEquipmentRulesEngine::ClearAllRules()
{
    if (!ShouldUseDevFallback())
    {
        return;
    }

    FScopeLock Lock(&RuleCriticalSection);

    RegisteredRules.Empty();
    EnabledRules.Empty();
    RulePriorities.Empty();
    RuleDependencies.Empty();

    UE_LOG(LogEquipmentRules, Log, TEXT("All rules cleared (DEV FALLBACK)"));
}

void USuspenseCoreEquipmentRulesEngine::ResetStatistics()
{
    if (!ShouldUseDevFallback())
    {
        return;
    }

    FScopeLock Lock(&RuleCriticalSection);

    RuleStats.Empty();
    ViolationHistory.Empty();

    UE_LOG(LogEquipmentRules, Log, TEXT("Statistics reset (DEV FALLBACK)"));
}

//========================================
// Helper Methods
//========================================

bool USuspenseCoreEquipmentRulesEngine::GetItemData(FName ItemID, FSuspenseCoreUnifiedItemData& OutItemData) const
{
    if (!bIsInitialized || !DataProvider.GetObject())
    {
        UE_LOG(LogEquipmentRules, Warning, TEXT("GetItemData: engine not initialized or provider missing"));
        return false;
    }

    UObject* ProviderObj = DataProvider.GetObject();
    static const FName FuncName(TEXT("GetUnifiedItemData"));
    if (UFunction* Func = ProviderObj->FindFunction(FuncName))
    {
        struct
        {
            FName ItemID;
            FSuspenseCoreUnifiedItemData OutData;
            bool ReturnValue;
        } Params{ ItemID, FSuspenseCoreUnifiedItemData(), false };

        ProviderObj->ProcessEvent(Func, &Params);
        if (Params.ReturnValue)
        {
            OutItemData = MoveTemp(Params.OutData);
            return true;
        }
    }

    UE_LOG(LogEquipmentRules, Warning, TEXT("GetItemData: provider has no GetUnifiedItemData for %s"), *ItemID.ToString());
    return false;
}

bool USuspenseCoreEquipmentRulesEngine::EvaluateExpression(const FString& Expression, const FRuleExecutionContext& Context) const
{
    if (!ShouldUseDevFallback())
    {
        return true;
    }

    if (Expression.IsEmpty())
    {
        return true;
    }

    if (Expression.Contains(TEXT("LEVEL")))
    {
        int32 RequiredLevel = 1;
        if (FParse::Value(*Expression, TEXT("LEVEL>="), RequiredLevel))
        {
            const int32 CharLevel = GetCharacterLevel(Context.Character);
            return CharLevel >= RequiredLevel;
        }
    }

    if (Expression.Contains(TEXT("WEIGHT")))
    {
        float MaxWeight = 100.0f;
        if (FParse::Value(*Expression, TEXT("WEIGHT<="), MaxWeight))
        {
            TArray<FSuspenseCoreInventoryItemInstance> All;
            for (const FEquipmentSlotSnapshot& S : Context.CurrentState.SlotSnapshots)
            {
                if (S.ItemInstance.IsValid())
                {
                    All.Add(S.ItemInstance);
                }
            }
            const float Current = CalculateTotalWeight(All);
            return Current <= MaxWeight;
        }
    }

    return true;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::ExecuteRule(
    const FEquipmentRule& Rule,
    const FRuleExecutionContext& Context) const
{
    FRuleEvaluationResult Result;
    Result.RuleType = Rule.RuleTag;

    if (!ShouldUseDevFallback())
    {
        Result.bPassed = true;
        Result.ConfidenceScore = 1.0f;
        Result.FailureReason = FText::FromString(TEXT("Dev fallback disabled"));
        return Result;
    }

    // Check cache
    if (bEnableCaching)
    {
        if (GetCachedResult(Rule.RuleTag, Result))
        {
            return Result;
        }
    }

    // Check preconditions
    if (!CheckPreconditions(Rule, Context))
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("Preconditions not met"));
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    // Evaluate expression
    bool bExpressionResult = EvaluateExpression(Rule.RuleExpression, Context);

    Result.bPassed = bExpressionResult;
    Result.ConfidenceScore = bExpressionResult ? 1.0f : 0.0f;

    if (!bExpressionResult)
    {
        Result.FailureReason = Rule.Description.IsEmpty() ?
            FText::FromString(FString::Printf(TEXT("Rule %s failed"), *Rule.RuleTag.ToString())) :
            Rule.Description;
    }

    // Cache result
    if (bEnableCaching)
    {
        CacheRuleResult(Rule.RuleTag, Result);
    }

    return Result;
}

bool USuspenseCoreEquipmentRulesEngine::CheckPreconditions(
    const FEquipmentRule& Rule,
    const FRuleExecutionContext& Context) const
{
    // Check dependencies
    if (const FGameplayTagContainer* Dependencies = RuleDependencies.Find(Rule.RuleTag))
    {
        TArray<FGameplayTag> DepArray;
        Dependencies->GetGameplayTagArray(DepArray);

        for (const FGameplayTag& DependencyTag : DepArray)
        {
            const FRuleEvaluationResult DependencyResult = EvaluateSpecificRule(DependencyTag, Context);
            if (!DependencyResult.bPassed)
            {
                return false;
            }
        }
    }

    return true;
}

TArray<FEquipmentRule> USuspenseCoreEquipmentRulesEngine::PrioritizeRules(const TArray<FEquipmentRule>& Rules) const
{
    TArray<FEquipmentRule> SortedRules = Rules;

    SortedRules.Sort([](const FEquipmentRule& A, const FEquipmentRule& B)
    {
        return A.Priority > B.Priority;
    });

    return SortedRules;
}

void USuspenseCoreEquipmentRulesEngine::CacheRuleResult(const FGameplayTag& RuleTag, const FRuleEvaluationResult& Result) const
{
    if (!bEnableCaching)
    {
        return;
    }

    uint32 CacheKey = GetTypeHash(RuleTag);
    float CurrentTime = FPlatformTime::Seconds();

    ResultCache.Add(CacheKey, Result);
    CacheTimestamps.Add(CacheKey, CurrentTime);

    // Cleanup old cache entries
    if (ResultCache.Num() > 100)
    {
        TArray<uint32> KeysToRemove;
        for (const auto& TimePair : CacheTimestamps)
        {
            if (CurrentTime - TimePair.Value > CacheDuration)
            {
                KeysToRemove.Add(TimePair.Key);
            }
        }

        for (uint32 Key : KeysToRemove)
        {
            ResultCache.Remove(Key);
            CacheTimestamps.Remove(Key);
        }
    }
}

bool USuspenseCoreEquipmentRulesEngine::GetCachedResult(const FGameplayTag& RuleTag, FRuleEvaluationResult& OutResult) const
{
    if (!bEnableCaching)
    {
        return false;
    }

    uint32 CacheKey = GetTypeHash(RuleTag);
    const FRuleEvaluationResult* CachedResult = ResultCache.Find(CacheKey);

    if (CachedResult)
    {
        float CurrentTime = FPlatformTime::Seconds();
        const float* CacheTime = CacheTimestamps.Find(CacheKey);

        if (CacheTime && (CurrentTime - *CacheTime) < CacheDuration)
        {
            OutResult = *CachedResult;
            return true;
        }
    }

    return false;
}

int32 USuspenseCoreEquipmentRulesEngine::GetCharacterLevel(const AActor* Character) const
{
    if (!Character)
    {
        return 1;
    }

    // Try to get from ability system
    if (Character->Implements<UAbilitySystemInterface>())
    {
        if (UAbilitySystemComponent* ASC = Cast<IAbilitySystemInterface>(Character)->GetAbilitySystemComponent())
        {
            // Would need to query attribute for level
            // This is placeholder - actual implementation would query the level attribute
            return 1;
        }
    }

    return 1;
}

TMap<FName, float> USuspenseCoreEquipmentRulesEngine::GetCharacterAttributes(const AActor* Character) const
{
    TMap<FName, float> Attributes;

    if (!Character)
    {
        return Attributes;
    }

    // Get from ability system
    if (Character->Implements<UAbilitySystemInterface>())
    {
        if (UAbilitySystemComponent* ASC = Cast<IAbilitySystemInterface>(Character)->GetAbilitySystemComponent())
        {
            // Would iterate through attribute sets
            // This is placeholder - actual implementation would query all attributes
            Attributes.Add(TEXT("Strength"), 10.0f);
            Attributes.Add(TEXT("Dexterity"), 10.0f);
            Attributes.Add(TEXT("Intelligence"), 10.0f);
        }
    }

    return Attributes;
}

FGameplayTagContainer USuspenseCoreEquipmentRulesEngine::GetCharacterTags(const AActor* Character) const
{
    FGameplayTagContainer Tags;

    if (!Character)
    {
        return Tags;
    }

    if (Character->Implements<UAbilitySystemInterface>())
    {
        if (UAbilitySystemComponent* ASC = Cast<IAbilitySystemInterface>(Character)->GetAbilitySystemComponent())
        {
            ASC->GetOwnedGameplayTags(Tags);
        }
    }

    return Tags;
}

float USuspenseCoreEquipmentRulesEngine::CalculateTotalWeight(const TArray<FSuspenseCoreInventoryItemInstance>& Items) const
{
    float TotalWeight = 0.0f;

    for (const FSuspenseCoreInventoryItemInstance& Item : Items)
    {
        if (Item.IsValid())
        {
            FSuspenseCoreUnifiedItemData ItemData;
            if (GetItemData(Item.ItemID, ItemData))
            {
                TotalWeight += ItemData.Weight * Item.Quantity;
            }
        }
    }

    return TotalWeight;
}

void USuspenseCoreEquipmentRulesEngine::RecordViolation(const FRuleViolation& Violation) const
{
    if (!ShouldUseDevFallback())
    {
        return;
    }

    ViolationHistory.Add(Violation);

    if (ViolationHistory.Num() > MaxViolationHistory)
    {
        ViolationHistory.RemoveAt(0, ViolationHistory.Num() - MaxViolationHistory);
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogEquipmentRules, Warning, TEXT("Rule violation: %s - %s"),
            *Violation.ViolatedRule.RuleTag.ToString(),
            *Violation.EvaluationResult.FailureReason.ToString());
    }
}

void USuspenseCoreEquipmentRulesEngine::UpdateStatistics(const FGameplayTag& RuleTag, bool bPassed, float EvaluationTime) const
{
    if (!ShouldUseDevFallback())
    {
        return;
    }

    FRuleStatistics& Stats = RuleStats.FindOrAdd(RuleTag);

    Stats.TotalEvaluations++;
    if (bPassed)
    {
        Stats.PassedEvaluations++;
    }
    else
    {
        Stats.FailedEvaluations++;
    }

    // Update average evaluation time
    float TotalTime = Stats.AverageEvaluationTime * (Stats.TotalEvaluations - 1) + EvaluationTime;
    Stats.AverageEvaluationTime = TotalTime / Stats.TotalEvaluations;

    Stats.LastEvaluationTime = FDateTime::Now();
}

void USuspenseCoreEquipmentRulesEngine::RegisterDefaultRules()
{
    if (!ShouldUseDevFallback())
    {
        return;
    }

    // Register weight limit rule
    FEquipmentRule WeightRule;
    WeightRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.System.WeightLimit"));
    WeightRule.RuleExpression = TEXT("WEIGHT<=MAX_WEIGHT");
    WeightRule.Priority = 100;
    WeightRule.bIsStrict = true;
    WeightRule.Description = FText::FromString(TEXT("Weight must not exceed capacity"));
    RegisterRule(WeightRule);

    // Register durability rule
    FEquipmentRule DurabilityRule;
    DurabilityRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.System.Durability"));
    DurabilityRule.RuleExpression = TEXT("DURABILITY>0");
    DurabilityRule.Priority = 90;
    DurabilityRule.bIsStrict = true;
    DurabilityRule.Description = FText::FromString(TEXT("Cannot equip broken items"));
    RegisterRule(DurabilityRule);

    // Register level requirement rule
    FEquipmentRule LevelRule;
    LevelRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.System.LevelRequirement"));
    LevelRule.RuleExpression = TEXT("LEVEL>=REQUIRED_LEVEL");
    LevelRule.Priority = 80;
    LevelRule.bIsStrict = true;
    LevelRule.Description = FText::FromString(TEXT("Character must meet level requirements"));
    RegisterRule(LevelRule);
}

FCharacterRequirements USuspenseCoreEquipmentRulesEngine::GetItemRequirements(const FSuspenseCoreInventoryItemInstance& ItemInstance) const
{
    FCharacterRequirements Req;

    if (!ShouldUseDevFallback())
    {
        return Req;
    }

    FSuspenseCoreUnifiedItemData Data;
    if (!GetItemData(ItemInstance.ItemID, Data))
    {
        return Req;
    }

    // Пример: требование по атрибутам от архетипа оружия
    if (Data.bIsWeapon)
    {
        const FGameplayTag Heavy = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Type.Heavy"));
        const FGameplayTag Light = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Type.Light"));

        if (Data.WeaponArchetype.MatchesTag(Heavy))
        {
            Req.RequiredAttributes.Add(TEXT("Strength"), 15.0f);
        }
        else if (Data.WeaponArchetype.MatchesTag(Light))
        {
            Req.RequiredAttributes.Add(TEXT("Dexterity"), 12.0f);
        }
    }

    return Req;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckCharacterMeetsRequirements(
    const AActor* Character,
    const FCharacterRequirements& Requirements) const
{
    FRuleEvaluationResult Result;
    Result.bPassed = true;
    Result.ConfidenceScore = 1.0f;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.CharacterRequirements"));

    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckCharacterMeetsRequirements"));
    }

    // Check level
    if (Requirements.RequiredLevel > 0)
    {
        int32 CharLevel = GetCharacterLevel(Character);
        if (CharLevel < Requirements.RequiredLevel)
        {
            Result.bPassed = false;
            Result.FailureReason = FText::FromString(FString::Printf(
                TEXT("Requires level %d (current: %d)"),
                Requirements.RequiredLevel, CharLevel));
            Result.ConfidenceScore = 0.0f;
            return Result;
        }
    }

    // Check attributes
    TMap<FName, float> CharAttributes = GetCharacterAttributes(Character);
    for (const auto& ReqAttr : Requirements.RequiredAttributes)
    {
        const float* CharValue = CharAttributes.Find(ReqAttr.Key);
        if (!CharValue || *CharValue < ReqAttr.Value)
        {
            Result.bPassed = false;
            Result.FailureReason = FText::FromString(FString::Printf(
                TEXT("Requires %s %.0f (current: %.0f)"),
                *ReqAttr.Key.ToString(), ReqAttr.Value,
                CharValue ? *CharValue : 0.0f));
            Result.ConfidenceScore = 0.0f;
            return Result;
        }
    }

    // Check tags
    if (Requirements.RequiredTags.Num() > 0)
    {
        FGameplayTagContainer CharTags = GetCharacterTags(Character);
        if (!CharTags.HasAll(Requirements.RequiredTags))
        {
            Result.bPassed = false;
            Result.FailureReason = FText::FromString(TEXT("Character lacks required abilities or traits"));
            Result.ConfidenceScore = 0.0f;

            // Add missing tags to details
            for (const FGameplayTag& ReqTag : Requirements.RequiredTags)
            {
                if (!CharTags.HasTag(ReqTag))
                {
                    Result.Details.Add(FString::Printf(TEXT("Missing: %s"), *ReqTag.ToString()));
                }
            }
            return Result;
        }
    }

    return Result;
}

float USuspenseCoreEquipmentRulesEngine::CalculateWeightCapacity(const AActor* Character) const
{
    float Capacity = WeightConfig.BaseWeightLimit;

    if (!Character)
    {
        return Capacity;
    }

    // Get strength attribute
    TMap<FName, float> Attributes = GetCharacterAttributes(Character);
    const float* Strength = Attributes.Find(TEXT("Strength"));

    if (Strength)
    {
        Capacity += (*Strength * WeightConfig.WeightPerStrength);
    }

    return Capacity;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckItemDurability(const FSuspenseCoreInventoryItemInstance& ItemInstance) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckItemDurability"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.ItemDurability"));
    Result.bPassed = true;
    Result.ConfidenceScore = 1.0f;

    float Durability = ItemInstance.GetRuntimeProperty(TEXT("Durability"), 100.0f);

    if (Durability <= 0.0f)
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("Item is broken and cannot be used"));
        Result.ConfidenceScore = 0.0f;
    }
    else if (Durability < 25.0f)
    {
        Result.ConfidenceScore = 0.5f;
        Result.Details.Add(FString::Printf(TEXT("Warning: Low durability (%.1f%%)"), Durability));
    }

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckAmmoCompatibility(
    const FSuspenseCoreInventoryItemInstance& WeaponInstance,
    const FSuspenseCoreInventoryItemInstance& AmmoInstance) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckAmmoCompatibility"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.AmmoCompatibility"));
    Result.bPassed = true;
    Result.ConfidenceScore = 1.0f;

    FSuspenseCoreUnifiedItemData WeaponData, AmmoData;
    if (!GetItemData(WeaponInstance.ItemID, WeaponData) || !GetItemData(AmmoInstance.ItemID, AmmoData))
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("Cannot retrieve item data"));
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    // Check if weapon can use this ammo type
    if (!WeaponData.bIsWeapon || !AmmoData.bIsAmmo)
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(TEXT("Invalid weapon/ammo combination"));
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    if (WeaponData.AmmoType != AmmoData.AmmoCaliber)
    {
        Result.bPassed = false;
        Result.FailureReason = FText::Format(
            NSLOCTEXT("EquipmentRules", "IncompatibleAmmo", "Weapon requires {0}, ammo is {1}"),
            FText::FromString(WeaponData.AmmoType.ToString()),
            FText::FromString(AmmoData.AmmoCaliber.ToString())
        );
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::CheckModificationCompatibility(
    const FSuspenseCoreInventoryItemInstance& BaseItem,
    const FSuspenseCoreInventoryItemInstance& Modification) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("CheckModificationCompatibility"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.ModificationCompatibility"));
    Result.bPassed = true;
    Result.ConfidenceScore = 1.0f;

    // Placeholder implementation - in real game would check attachment points, compatibility tags, etc.
    Result.FailureReason = FText::FromString(TEXT("Modification compatibility check passed"));

    return Result;
}

FRuleEvaluationResult USuspenseCoreEquipmentRulesEngine::ValidateLoadout(const TArray<FSuspenseCoreInventoryItemInstance>& LoadoutItems) const
{
    if (!ShouldUseDevFallback())
    {
        return CreateDisabledResult(TEXT("ValidateLoadout"));
    }

    FRuleEvaluationResult Result;
    Result.RuleType = FGameplayTag::RequestGameplayTag(TEXT("Rule.LoadoutValidation"));
    Result.bPassed = true;
    Result.ConfidenceScore = 1.0f;

    // Check total weight
    float TotalWeight = CalculateTotalWeight(LoadoutItems);
    float WeightCapacity = CalculateWeightCapacity(GetOwner());

    if (TotalWeight > WeightCapacity * 1.1f) // 10% tolerance
    {
        Result.bPassed = false;
        Result.FailureReason = FText::FromString(FString::Printf(
            TEXT("Loadout too heavy: %.1f kg (capacity: %.1f kg)"),
            TotalWeight, WeightCapacity));
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    // Check for conflicts between items
    for (int32 i = 0; i < LoadoutItems.Num(); i++)
    {
        for (int32 j = i + 1; j < LoadoutItems.Num(); j++)
        {
            FRuleEvaluationResult ConflictCheck = CheckConflictingEquipment({LoadoutItems[i]}, LoadoutItems[j]);
            if (!ConflictCheck.bPassed)
            {
                Result.bPassed = false;
                Result.FailureReason = ConflictCheck.FailureReason;
                Result.ConfidenceScore = 0.0f;
                return Result;
            }
        }
    }

    Result.FailureReason = FText::FromString(TEXT("Loadout validation passed"));
    return Result;
}

TArray<FRuleViolation> USuspenseCoreEquipmentRulesEngine::FindItemConflicts(
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    const TArray<FSuspenseCoreInventoryItemInstance>& CurrentItems) const
{
    TArray<FRuleViolation> Conflicts;

    if (!ShouldUseDevFallback())
    {
        return Conflicts;
    }

    // This would contain more sophisticated conflict detection
    // For now, return basic conflicts based on CheckConflictingEquipment
    FRuleEvaluationResult ConflictResult = CheckConflictingEquipment(CurrentItems, ItemInstance);

    if (!ConflictResult.bPassed)
    {
        FRuleViolation Violation;
        Violation.EvaluationResult = ConflictResult;
        Violation.ViolationTime = FDateTime::Now();
        Violation.Context = TEXT("Item conflict detection");
        Violation.Severity = 7;
        Conflicts.Add(Violation);
    }

    return Conflicts;
}

bool USuspenseCoreEquipmentRulesEngine::ResolveConflicts(const TArray<FRuleViolation>& Conflicts, int32 ResolutionStrategy)
{
    if (!ShouldUseDevFallback())
    {
        return false;
    }

    // Placeholder implementation
    // 0 = Remove conflicting, 1 = Abort, 2 = Force
    switch (ResolutionStrategy)
    {
        case 0:
            UE_LOG(LogEquipmentRules, Log, TEXT("Resolving %d conflicts by removing conflicting items"), Conflicts.Num());
            return true;
        case 1:
            UE_LOG(LogEquipmentRules, Log, TEXT("Aborting operation due to %d conflicts"), Conflicts.Num());
            return false;
        case 2:
            UE_LOG(LogEquipmentRules, Log, TEXT("Forcing operation despite %d conflicts"), Conflicts.Num());
            return true;
        default:
            return false;
    }
}

FRuleStatistics USuspenseCoreEquipmentRulesEngine::GetRuleStatistics(const FGameplayTag& RuleTag) const
{
    if (!ShouldUseDevFallback())
    {
        return FRuleStatistics();
    }

    const FRuleStatistics* Stats = RuleStats.Find(RuleTag);
    return Stats ? *Stats : FRuleStatistics();
}

TArray<FRuleViolation> USuspenseCoreEquipmentRulesEngine::GetViolationHistory(int32 MaxCount) const
{
    if (!ShouldUseDevFallback())
    {
        return TArray<FRuleViolation>();
    }

    TArray<FRuleViolation> Result = ViolationHistory;

    if (Result.Num() > MaxCount)
    {
        Result.SetNum(MaxCount);
    }

    return Result;
}

FString USuspenseCoreEquipmentRulesEngine::ExportRulesToJSON() const
{
    if (!ShouldUseDevFallback())
    {
        return TEXT("{\"error\":\"Monolithic rules engine disabled\"}");
    }

    // Placeholder implementation
    return TEXT("{\"rules\":[],\"status\":\"dev_fallback_active\"}");
}

int32 USuspenseCoreEquipmentRulesEngine::ImportRulesFromJSON(const FString& JSONString)
{
    if (!ShouldUseDevFallback())
    {
        return 0;
    }

    // Placeholder implementation
    return 0;
}

FString USuspenseCoreEquipmentRulesEngine::GetDebugInfo() const
{
    return FString::Printf(TEXT("Monolithic Rules Engine Debug:\n"
                               "Dev Fallback Enabled: %s\n"
                               "CVar Value: %d\n"
                               "Registered Rules: %d\n"
                               "Enabled Rules: %d\n"
                               "Violations: %d\n"
                               "Cache Entries: %d\n"
                               "Version: %d\n"
                               "Status: %s"),
                          ShouldUseDevFallback() ? TEXT("true") : TEXT("false"),
                          CVarSuspenseCoreUseMonolith.GetValueOnGameThread(),
                          RegisteredRules.Num(),
                          EnabledRules.Num(),
                          ViolationHistory.Num(),
                          ResultCache.Num(),
                          EngineVersion,
                          ShouldUseDevFallback() ? TEXT("ACTIVE") : TEXT("DISABLED - Use USuspenseCoreRulesCoordinator"));
}

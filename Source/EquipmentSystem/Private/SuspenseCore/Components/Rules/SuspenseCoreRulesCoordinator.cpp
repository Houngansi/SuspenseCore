// SuspenseCoreRulesCoordinator.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Rules/SuspenseCoreRulesCoordinator.h"
#include "SuspenseCore/Components/Rules/SuspenseCoreWeightRulesEngine.h"
#include "SuspenseCore/Components/Rules/SuspenseCoreRequirementRulesEngine.h"
#include "SuspenseCore/Components/Rules/SuspenseCoreConflictRulesEngine.h"
#include "SuspenseCore/Components/Rules/SuspenseCoreCompatibilityRulesEngine.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogRulesCoordinator, Log, All);

USuspenseCoreRulesCoordinator::USuspenseCoreRulesCoordinator()
{
    InitializationTime = FDateTime::Now();
}

// ============================================================================
// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Initialize теперь работает БЕЗ обязательного DataProvider
// ============================================================================
bool USuspenseCoreRulesCoordinator::Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    // ✅ DataProvider теперь опционален - сохраняем даже если nullptr
    DataProvider = InDataProvider;

    if (DataProvider.GetInterface())
    {
        UE_LOG(LogRulesCoordinator, Log,
            TEXT("Initialize: DataProvider provided - coordinator will use it for fallback operations"));
    }
    else
    {
        UE_LOG(LogRulesCoordinator, Warning,
            TEXT("Initialize: DataProvider is null - coordinator will work in STATELESS mode"));
        UE_LOG(LogRulesCoordinator, Warning,
            TEXT("  All equipment data must be provided through FSuspenseCoreRuleContext"));
    }

    // Создаем движки независимо от наличия DataProvider
    CreateSpecializedEngines();

    UE_LOG(LogRulesCoordinator, Log,
        TEXT("Rules Coordinator initialized successfully:"));
    UE_LOG(LogRulesCoordinator, Log,
        TEXT("  - Mode: %s"), DataProvider.GetInterface() ? TEXT("STATEFUL") : TEXT("STATELESS"));
    UE_LOG(LogRulesCoordinator, Log,
        TEXT("  - Specialized engines: 4"));
    UE_LOG(LogRulesCoordinator, Log,
        TEXT("  - Total registered engines: %d"), RegisteredEngines.Num());

    // ✅ Всегда возвращаем SUCCESS - coordinator может работать без DataProvider
    return true;
}

void USuspenseCoreRulesCoordinator::CreateSpecializedEngines()
{
    // ============================================================================
    // 1) Compatibility Engine (Critical Priority)
    // ============================================================================
    CompatibilityEngine = NewObject<USuspenseCoreCompatibilityRulesEngine>(this, TEXT("CompatibilityRulesEngine"));
    if (CompatibilityEngine)
    {
        // ✅ ИЗМЕНЕНИЕ: Передаем DataProvider, только если он доступен
        if (DataProvider.GetInterface())
        {
            CompatibilityEngine->SetDefaultEquipmentDataProvider(DataProvider);
            UE_LOG(LogRulesCoordinator, Verbose,
                TEXT("Compatibility engine initialized WITH DataProvider"));
        }
        else
        {
            UE_LOG(LogRulesCoordinator, Verbose,
                TEXT("Compatibility engine initialized WITHOUT DataProvider (stateless)"));
        }

        FRuleEngineRegistration R;
        R.EngineType = FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility"));
        R.Engine     = CompatibilityEngine;
        R.Priority   = ERuleExecutionPriority::Critical;
        R.bEnabled   = true;
        RegisteredEngines.Add(R.EngineType, R);

        UE_LOG(LogRulesCoordinator, Log, TEXT("Created Compatibility engine with Critical priority"));
    }

    // ============================================================================
    // 2) Requirement Engine (High Priority) - НЕ требует DataProvider
    // ============================================================================
    RequirementEngine = NewObject<USuspenseCoreRequirementRulesEngine>(this, TEXT("RequirementRulesEngine"));
    if (RequirementEngine)
    {
        // Требования читают Character/ASC из контекста - провайдер не нужен
        FRuleEngineRegistration R;
        R.EngineType = FGameplayTag::RequestGameplayTag(TEXT("Rule.Requirement"));
        R.Engine     = RequirementEngine;
        R.Priority   = ERuleExecutionPriority::High;
        R.bEnabled   = true;
        RegisteredEngines.Add(R.EngineType, R);

        UE_LOG(LogRulesCoordinator, Log, TEXT("Created Requirement engine with High priority"));
    }

    // ============================================================================
    // 3) Weight Engine (Normal Priority) - НЕ требует DataProvider
    // ============================================================================
    WeightEngine = NewObject<USuspenseCoreWeightRulesEngine>(this, TEXT("WeightRulesEngine"));
    if (WeightEngine)
    {
        FSuspenseCoreWeightConfig Cfg;
        Cfg.BaseCarryCapacity    = 40.0f;
        Cfg.CapacityPerStrength  = 2.0f;
        Cfg.EncumberedThreshold  = 0.75f;
        Cfg.OverweightThreshold  = 1.0f;
        Cfg.bAllowOverweight     = true;
        Cfg.MaxOverweightRatio   = 1.5f;

        Cfg.WeightModifiers.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Heavy")),  1.25f);
        Cfg.WeightModifiers.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Light")),  0.85f);
        Cfg.WeightModifiers.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Heavy")), 1.15f);
        Cfg.WeightModifiers.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Consumable")),   0.90f);

        Cfg.ExcludedSlots.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Cosmetic")));
        Cfg.ExcludedSlots.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Badge")));

        WeightEngine->Initialize(Cfg);
        ExcludedSlotsCache = Cfg.ExcludedSlots;

        FRuleEngineRegistration R;
        R.EngineType = FGameplayTag::RequestGameplayTag(TEXT("Rule.Weight"));
        R.Engine     = WeightEngine;
        R.Priority   = ERuleExecutionPriority::Normal;
        R.bEnabled   = true;
        RegisteredEngines.Add(R.EngineType, R);

        UE_LOG(LogRulesCoordinator, Log, TEXT("Created Weight engine with Normal priority"));
    }

    // ============================================================================
    // 4) Conflict Engine (Low Priority)
    // ============================================================================
    ConflictEngine = NewObject<USuspenseCoreConflictRulesEngine>(this, TEXT("ConflictRulesEngine"));
    if (ConflictEngine)
    {
        // ✅ ИЗМЕНЕНИЕ: Передаем DataProvider, только если он доступен
        if (DataProvider.GetInterface())
        {
            ConflictEngine->Initialize(DataProvider);
            UE_LOG(LogRulesCoordinator, Verbose,
                TEXT("Conflict engine initialized WITH DataProvider"));
        }
        else
        {
            // Инициализируем с пустым интерфейсом - движок должен уметь работать без DataProvider
            ConflictEngine->Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider>());
            UE_LOG(LogRulesCoordinator, Verbose,
                TEXT("Conflict engine initialized WITHOUT DataProvider (stateless)"));
        }

        // Базовые примеры правил конфликтов
        ConflictEngine->RegisterMutualExclusion(
            FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Heavy")),
            FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Light")));

        TArray<FName> AltynArmorSet{ TEXT("Altyn_Helmet"), TEXT("Altyn_Faceshield") };
        ConflictEngine->RegisterItemSet(FGameplayTag::RequestGameplayTag(TEXT("Set.Armor.Altyn")), AltynArmorSet, 2);

        TArray<FName> FortArmorSet{ TEXT("6B43_6A_Zabralo"), TEXT("6B47_Ratnik") };
        ConflictEngine->RegisterItemSet(FGameplayTag::RequestGameplayTag(TEXT("Set.Armor.Fort")), FortArmorSet, 2);

        FRuleEngineRegistration R;
        R.EngineType = FGameplayTag::RequestGameplayTag(TEXT("Rule.Conflict"));
        R.Engine     = ConflictEngine;
        R.Priority   = ERuleExecutionPriority::Low;
        R.bEnabled   = true;
        RegisteredEngines.Add(R.EngineType, R);

        UE_LOG(LogRulesCoordinator, Log, TEXT("Created Conflict engine with Low priority"));
    }
}

FEquipmentStateSnapshot USuspenseCoreRulesCoordinator::BuildShadowSnapshotFromContext(
    const FSuspenseCoreRuleContext& Context) const
{
    FEquipmentStateSnapshot Snapshot;

    // Priority 1: Build snapshot from CurrentItems in context
    // This is the data that ValidationService passed through context
    if (Context.CurrentItems.Num() > 0)
    {
        Snapshot.SlotSnapshots.Reserve(Context.CurrentItems.Num());

        // Create slot snapshots from current items
        for (int32 i = 0; i < Context.CurrentItems.Num(); ++i)
        {
            const FSuspenseCoreInventoryItemInstance& Item = Context.CurrentItems[i];

            FEquipmentSlotSnapshot SlotSnapshot;
            SlotSnapshot.SlotIndex = i; // Use index as slot number
            SlotSnapshot.ItemInstance = Item;

            // Create minimal slot configuration
            // Note: SlotSnapshot.Configuration doesn't have SlotIndex field, removed
            const FName SlotTagName = FName(*FString::Printf(TEXT("Equipment.Slot.%d"), i));
            SlotSnapshot.Configuration.SlotTag = FGameplayTag::RequestGameplayTag(SlotTagName);

            Snapshot.SlotSnapshots.Add(SlotSnapshot);
        }

        UE_LOG(LogRulesCoordinator, Verbose,
            TEXT("BuildShadowSnapshotFromContext: Built snapshot from context (%d items)"),
            Context.CurrentItems.Num());
    }
    // Priority 2: Fallback to DataProvider if context is empty
    else if (DataProvider.GetInterface())
    {
        Snapshot = DataProvider->CreateSnapshot();

        UE_LOG(LogRulesCoordinator, Verbose,
            TEXT("BuildShadowSnapshotFromContext: Built snapshot from DataProvider (fallback)"));
    }
    // Priority 3: Empty snapshot for first-time equip
    else
    {
        UE_LOG(LogRulesCoordinator, Verbose,
            TEXT("BuildShadowSnapshotFromContext: Empty snapshot (no context items, no DataProvider)"));
    }

    return Snapshot;
}

// ============================================================================
// Helper: Apply single operation to shadow snapshot
// ============================================================================
static void ApplyOperationToSnapshot(const FEquipmentOperationRequest& Op, FEquipmentStateSnapshot& Snapshot)
{
    switch (Op.OperationType)
    {
    case EEquipmentOperationType::Equip:
        for (FEquipmentSlotSnapshot& S : Snapshot.SlotSnapshots)
        {
            if (S.SlotIndex == Op.TargetSlotIndex)
            {
                S.ItemInstance = Op.ItemInstance;
                break;
            }
        }
        break;

    case EEquipmentOperationType::Move:
        if (Op.SourceSlotIndex != INDEX_NONE && Op.TargetSlotIndex != INDEX_NONE)
        {
            FSuspenseCoreInventoryItemInstance MovingItem;
            for (FEquipmentSlotSnapshot& S : Snapshot.SlotSnapshots)
            {
                if (S.SlotIndex == Op.SourceSlotIndex)
                {
                    MovingItem = S.ItemInstance;
                    S.ItemInstance = FSuspenseCoreInventoryItemInstance();
                    break;
                }
            }
            for (FEquipmentSlotSnapshot& S : Snapshot.SlotSnapshots)
            {
                if (S.SlotIndex == Op.TargetSlotIndex)
                {
                    S.ItemInstance = MovingItem;
                    break;
                }
            }
        }
        break;

    case EEquipmentOperationType::Swap:
        if (Op.SourceSlotIndex != INDEX_NONE && Op.TargetSlotIndex != INDEX_NONE)
        {
            FSuspenseCoreInventoryItemInstance TempItem;
            FEquipmentSlotSnapshot* SourceSlot = nullptr;
            FEquipmentSlotSnapshot* TargetSlot = nullptr;

            for (FEquipmentSlotSnapshot& S : Snapshot.SlotSnapshots)
            {
                if (S.SlotIndex == Op.SourceSlotIndex)
                {
                    SourceSlot = &S;
                }
                else if (S.SlotIndex == Op.TargetSlotIndex)
                {
                    TargetSlot = &S;
                }
            }

            if (SourceSlot && TargetSlot)
            {
                TempItem = SourceSlot->ItemInstance;
                SourceSlot->ItemInstance = TargetSlot->ItemInstance;
                TargetSlot->ItemInstance = TempItem;
            }
        }
        break;

    case EEquipmentOperationType::Unequip:
        for (FEquipmentSlotSnapshot& S : Snapshot.SlotSnapshots)
        {
            if (S.SlotIndex == Op.TargetSlotIndex)
            {
                S.ItemInstance = FSuspenseCoreInventoryItemInstance();
                break;
            }
        }
        break;

    default:
        break;
    }
}

// ============================================================================
// Helper: Convert snapshot to items with slot filtering
// ============================================================================
static void SnapshotToItemsFiltered(const FEquipmentStateSnapshot& Snapshot,
                                    const FGameplayTagContainer& ExcludedSlots,
                                    TArray<FSuspenseCoreInventoryItemInstance>& OutItems)
{
    OutItems.Reset();
    OutItems.Reserve(Snapshot.SlotSnapshots.Num());

    for (const FEquipmentSlotSnapshot& S : Snapshot.SlotSnapshots)
    {
        const FGameplayTag SlotTag = S.Configuration.SlotTag;
        if (!S.ItemInstance.IsValid()) continue;

        if (!ExcludedSlots.IsEmpty() && ExcludedSlots.HasTag(SlotTag)) continue;

        OutItems.Add(S.ItemInstance);
    }
}

// ============================================================================
// ИСПРАВЛЕНО: EvaluateRules теперь работает без обязательного DataProvider
// ============================================================================
FRuleEvaluationResult USuspenseCoreRulesCoordinator::EvaluateRules(const FEquipmentOperationRequest& Operation) const
{
    // Строим минимальный контекст
    FSuspenseCoreRuleContext Context;
    Context.Character       = Operation.Instigator.Get();
    Context.ItemInstance    = Operation.ItemInstance;
    Context.TargetSlotIndex = Operation.TargetSlotIndex;
    Context.bForceOperation = Operation.bForceOperation;

    // ✅ ИСПРАВЛЕНИЕ: Получаем текущие предметы из DataProvider, только если он доступен
    // Иначе работаем с пустым списком (для первой экипировки)
    if (DataProvider.GetInterface())
    {
        TMap<int32, FSuspenseCoreInventoryItemInstance> EquippedMap = DataProvider->GetAllEquippedItems();
        Context.CurrentItems.Reserve(EquippedMap.Num());
        for (const auto& Pair : EquippedMap)
        {
            Context.CurrentItems.Add(Pair.Value);
        }
    }

    return EvaluateRulesWithContext(Operation, Context);
}

void USuspenseCoreRulesCoordinator::RecordEngineMetrics(const FGameplayTag& EngineType, double DurationMs) const
{
    FScopeLock Lock(&RulesLock);

    EngineExecCount.FindOrAdd(EngineType) += 1;
    EngineExecTimeMs.FindOrAdd(EngineType) += DurationMs;
}

// ============================================================================
// ОСНОВНОЙ МЕТОД ВАЛИДАЦИИ С ПОЛНЫМ PIPELINE
// ============================================================================
FRuleEvaluationResult USuspenseCoreRulesCoordinator::EvaluateRulesWithContext(
    const FEquipmentOperationRequest& Operation,
    const FSuspenseCoreRuleContext& Context) const
{
    TArray<FSuspenseCoreRuleCheckResult> AllResults;

    const double EvaluationStartTime = FPlatformTime::Seconds();

    {
        FScopeLock Lock(&RulesLock);
        TotalEvaluations += 1;
    }

    // ✅ КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Используем новый метод построения snapshot из контекста
    FEquipmentStateSnapshot ShadowSnapshot = BuildShadowSnapshotFromContext(Context);

    // Применяем планируемую операцию к теневому снапшоту
    ApplyOperationToSnapshot(Operation, ShadowSnapshot);

    // Подготавливаем локальный контекст с отфильтрованными данными
    FSuspenseCoreRuleContext LocalContext = Context;
    SnapshotToItemsFiltered(ShadowSnapshot, ExcludedSlotsCache, LocalContext.CurrentItems);

    // Определяем pipeline в фиксированном порядке: Critical -> High -> Normal -> Low
    struct FPipelineEntry
    {
        UObject* Engine;
        FGameplayTag EngineType;
        ERuleExecutionPriority Priority;
    };

    TArray<FPipelineEntry> Pipeline;
    Pipeline.Reserve(4);

    if (CompatibilityEngine)
    {
        Pipeline.Add({ CompatibilityEngine, FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility")), ERuleExecutionPriority::Critical });
    }
    if (RequirementEngine)
    {
        Pipeline.Add({ RequirementEngine, FGameplayTag::RequestGameplayTag(TEXT("Rule.Requirement")), ERuleExecutionPriority::High });
    }
    if (WeightEngine)
    {
        Pipeline.Add({ WeightEngine, FGameplayTag::RequestGameplayTag(TEXT("Rule.Weight")), ERuleExecutionPriority::Normal });
    }
    if (ConflictEngine)
    {
        Pipeline.Add({ ConflictEngine, FGameplayTag::RequestGameplayTag(TEXT("Rule.Conflict")), ERuleExecutionPriority::Low });
    }

    // Выполняем pipeline с возможностью досрочного завершения
    for (const FPipelineEntry& Entry : Pipeline)
    {
        FSuspenseCoreAggregatedRuleResult EngineResult;

        const double EngineStartTime = FPlatformTime::Seconds();

        if (Entry.Engine == CompatibilityEngine)
        {
            EngineResult = CompatibilityEngine->EvaluateCompatibilityRules(LocalContext);
        }
        else if (Entry.Engine == RequirementEngine)
        {
            EngineResult = RequirementEngine->EvaluateRequirementRules(LocalContext);
        }
        else if (Entry.Engine == WeightEngine)
        {
            // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Предотвращаем двойной подсчет веса
            FSuspenseCoreRuleContext WeightContext = LocalContext;
            WeightContext.ItemInstance = FSuspenseCoreInventoryItemInstance();
            EngineResult = WeightEngine->EvaluateWeightRules(WeightContext);
        }
        else if (Entry.Engine == ConflictEngine)
        {
            // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Используем перегрузку с реальными слотами
            EngineResult = ConflictEngine->EvaluateConflictRulesWithSlots(LocalContext, ShadowSnapshot.SlotSnapshots);
        }

        const double EngineEndTime = FPlatformTime::Seconds();
        const double EngineDurationMs = (EngineEndTime - EngineStartTime) * 1000.0;

        RecordEngineMetrics(Entry.EngineType, EngineDurationMs);

        AllResults.Append(EngineResult.Results);

        // Досрочное завершение при критических ошибках
        if (EngineResult.HasCriticalIssues() &&
            (Entry.Priority == ERuleExecutionPriority::Critical || Entry.Priority == ERuleExecutionPriority::High))
        {
            UE_LOG(LogRulesCoordinator, Warning, TEXT("Critical failure in %s engine - terminating rule pipeline early"),
                *Entry.EngineType.ToString());
            break;
        }
    }

    const double EvaluationEndTime = FPlatformTime::Seconds();
    const double TotalEvaluationMs = (EvaluationEndTime - EvaluationStartTime) * 1000.0;

    {
        FScopeLock Lock(&RulesLock);
        AccumulatedEvalMs += TotalEvaluationMs;
        LastExecutionTime = FDateTime::Now();
    }

    return ConvertToLegacyResult(AllResults);
}

// ============================================================================
// Остальные методы интерфейса ISuspenseCoreEquipmentRules
// ============================================================================

FRuleEvaluationResult USuspenseCoreRulesCoordinator::CheckItemCompatibility(
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    const FSuspenseCoreEquipmentSlotConfig& SlotConfig) const
{
    if (CompatibilityEngine)
    {
        return ConvertSingleResult(CompatibilityEngine->CheckItemCompatibility(ItemInstance, SlotConfig));
    }

    FRuleEvaluationResult Compatible;
    Compatible.bPassed = true;
    Compatible.ConfidenceScore = 0.5f;
    Compatible.FailureReason = FText::FromString(TEXT("No compatibility engine - assuming compatible"));
    return Compatible;
}

FRuleEvaluationResult USuspenseCoreRulesCoordinator::CheckCharacterRequirements(
    const AActor* Character,
    const FSuspenseCoreInventoryItemInstance& ItemInstance) const
{
    if (RequirementEngine)
    {
        FSuspenseCoreItemRequirements Requirements;

        Requirements.RequiredLevel = FMath::RoundToInt(ItemInstance.GetRuntimeProperty(TEXT("RequiredLevel"), 0.0f));

        const FString ItemName = ItemInstance.ItemID.ToString();
        if (ItemName.Contains(TEXT("Sniper")) || ItemName.Contains(TEXT("DMR")))
        {
            Requirements.RequiredLevel = FMath::Max(Requirements.RequiredLevel, 10);
            Requirements.RequiredClass = FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Marksman"));
        }
        else if (ItemName.Contains(TEXT("Heavy")) || ItemName.Contains(TEXT("LMG")))
        {
            Requirements.RequiredLevel = FMath::Max(Requirements.RequiredLevel, 5);
            Requirements.RequiredClass = FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Heavy"));
        }

        const float ItemWeight = ItemInstance.GetRuntimeProperty(TEXT("Weight"), 0.0f);
        if (ItemWeight > 8.0f)
        {
            FSuspenseCoreAttributeRequirement StrengthReq;
            StrengthReq.AttributeName = TEXT("Strength");
            StrengthReq.RequiredValue = 12.0f + (ItemWeight - 8.0f) * 0.5f;
            StrengthReq.ComparisonOp = ESuspenseCoreComparisonOp::GreaterOrEqual;
            StrengthReq.DisplayName = FText::FromString(TEXT("Strength requirement"));
            Requirements.AttributeRequirements.Add(StrengthReq);
        }

        return ConvertAggregatedResult(RequirementEngine->CheckAllRequirements(Character, Requirements));
    }

    FRuleEvaluationResult RequirementsMet;
    RequirementsMet.bPassed = true;
    RequirementsMet.ConfidenceScore = 0.5f;
    RequirementsMet.FailureReason = FText::FromString(TEXT("No requirement engine - assuming requirements met"));
    return RequirementsMet;
}

FRuleEvaluationResult USuspenseCoreRulesCoordinator::CheckWeightLimit(
    float CurrentWeight,
    float AdditionalWeight) const
{
    if (WeightEngine)
    {
        const float Capacity = WeightEngine->CalculateWeightCapacity(nullptr);
        return ConvertSingleResult(WeightEngine->CheckWeightLimit(CurrentWeight, AdditionalWeight, Capacity));
    }

    const float TotalWeight = CurrentWeight + AdditionalWeight;
    const float DefaultCapacity = 40.0f;

    FRuleEvaluationResult WeightCheck;
    WeightCheck.bPassed = (TotalWeight <= DefaultCapacity);
    WeightCheck.ConfidenceScore = 0.3f;
    WeightCheck.FailureReason = WeightCheck.bPassed
        ? FText::FromString(TEXT("Weight within default capacity"))
        : FText::FromString(TEXT("Exceeds default weight capacity"));
    return WeightCheck;
}

FRuleEvaluationResult USuspenseCoreRulesCoordinator::CheckConflictingEquipment(
    const TArray<FSuspenseCoreInventoryItemInstance>& ExistingItems,
    const FSuspenseCoreInventoryItemInstance& NewItem) const
{
    if (ConflictEngine)
    {
        return ConvertSingleResult(ConflictEngine->CheckItemConflicts(NewItem, ExistingItems));
    }

    FRuleEvaluationResult NoConflicts;
    NoConflicts.bPassed = true;
    NoConflicts.ConfidenceScore = 0.5f;
    NoConflicts.FailureReason = FText::FromString(TEXT("No conflict engine - assuming no conflicts"));
    return NoConflicts;
}

TArray<FEquipmentRule> USuspenseCoreRulesCoordinator::GetActiveRules() const
{
    FScopeLock Lock(&RulesLock);

    TArray<FEquipmentRule> ActiveRules = GlobalRules;

    ActiveRules.RemoveAll([this](const FEquipmentRule& Rule)
    {
        return DisabledRules.Contains(Rule.RuleTag);
    });

    return ActiveRules;
}

bool USuspenseCoreRulesCoordinator::RegisterRule(const FEquipmentRule& Rule)
{
    FScopeLock Lock(&RulesLock);
    GlobalRules.Add(Rule);
    UE_LOG(LogRulesCoordinator, Log, TEXT("Registered global rule: %s"), *Rule.RuleTag.ToString());
    return true;
}

bool USuspenseCoreRulesCoordinator::UnregisterRule(const FGameplayTag& RuleTag)
{
    FScopeLock Lock(&RulesLock);
    const int32 RemovedCount = GlobalRules.RemoveAll([&RuleTag](const FEquipmentRule& Rule)
    {
        return Rule.RuleTag == RuleTag;
    });

    if (RemovedCount > 0)
    {
        DisabledRules.Remove(RuleTag);
        UE_LOG(LogRulesCoordinator, Log, TEXT("Unregistered %d instances of rule: %s"), RemovedCount, *RuleTag.ToString());
        return true;
    }
    return false;
}

bool USuspenseCoreRulesCoordinator::SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled)
{
    FScopeLock Lock(&RulesLock);

    if (bEnabled)
    {
        DisabledRules.Remove(RuleTag);
    }
    else
    {
        DisabledRules.Add(RuleTag);
    }

    UE_LOG(LogRulesCoordinator, Log, TEXT("Rule %s: %s"),
        *RuleTag.ToString(), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    return true;
}

void USuspenseCoreRulesCoordinator::ClearRuleCache()
{
    if (WeightEngine)        { WeightEngine->ClearCache(); }
    if (RequirementEngine)   { RequirementEngine->ClearCache(); }
    if (ConflictEngine)      { ConflictEngine->ClearCache(); }
    if (CompatibilityEngine) { CompatibilityEngine->ClearCache(); }

    UE_LOG(LogRulesCoordinator, Log, TEXT("Cleared all engine caches"));
}

void USuspenseCoreRulesCoordinator::ResetStatistics()
{
    FScopeLock Lock(&RulesLock);

    TotalEvaluations = 0;
    AccumulatedEvalMs = 0.0;
    EngineExecCount.Empty();
    EngineExecTimeMs.Empty();

    if (WeightEngine)        { WeightEngine->ResetStatistics(); }
    if (RequirementEngine)   { RequirementEngine->ResetStatistics(); }
    if (ConflictEngine)      { ConflictEngine->ResetStatistics(); }
    if (CompatibilityEngine) { CompatibilityEngine->ResetStatistics(); }

    UE_LOG(LogRulesCoordinator, Log, TEXT("Reset all statistics"));
}

FString USuspenseCoreRulesCoordinator::GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const
{
    FString Report = TEXT("=== Equipment Rules Compliance Report ===\n\n");

    Report += FString::Printf(TEXT("Generated: %s\n"), *FDateTime::Now().ToString());
    Report += FString::Printf(TEXT("Initialized: %s\n"), *InitializationTime.ToString());
    Report += FString::Printf(TEXT("Registered Engines: %d\n"), RegisteredEngines.Num());
    Report += FString::Printf(TEXT("Global Rules: %d\n"), GlobalRules.Num());
    Report += FString::Printf(TEXT("Disabled Rules: %d\n"), DisabledRules.Num());

    int64 SafeTotalEvaluations;
    double SafeAccumulatedMs;
    {
        FScopeLock Lock(&RulesLock);
        SafeTotalEvaluations = TotalEvaluations;
        SafeAccumulatedMs = AccumulatedEvalMs;
        Report += FString::Printf(TEXT("Last Execution: %s\n"), LastExecutionTime.IsSet() ? *LastExecutionTime.GetValue().ToString() : TEXT("Never"));
    }

    Report += FString::Printf(TEXT("Total Evaluations: %lld\n"), SafeTotalEvaluations);
    Report += FString::Printf(TEXT("Total Evaluation Time: %.2fms\n\n"), SafeAccumulatedMs);

    Report += TEXT("Engine Performance:\n");
    Report += TEXT("-------------------\n");
    for (const auto& EnginePair : RegisteredEngines)
    {
        const FGameplayTag& EngineType = EnginePair.Key;
        const FRuleEngineRegistration& Registration = EnginePair.Value;

        int64 ExecCount;
        double ExecTime;
        {
            FScopeLock Lock(&RulesLock);
            ExecCount = EngineExecCount.FindRef(EngineType);
            ExecTime = EngineExecTimeMs.FindRef(EngineType);
        }

        const double AvgTime = (ExecCount > 0) ? (ExecTime / ExecCount) : 0.0;

        Report += FString::Printf(TEXT("  %s: %s (Priority: %s)\n"),
            *EngineType.ToString(),
            Registration.bEnabled ? TEXT("✓ Enabled") : TEXT("✗ Disabled"),
            *UEnum::GetValueAsString(Registration.Priority));

        if (ExecCount > 0)
        {
            Report += FString::Printf(TEXT("    Executions: %lld, Total Time: %.2fms, Avg Time: %.2fms\n"),
                ExecCount, ExecTime, AvgTime);
        }
        Report += TEXT("\n");
    }

    Report += TEXT("Slot Compliance Analysis:\n");
    Report += TEXT("------------------------\n");

    int32 CompliantSlots = 0;
    int32 NonCompliantSlots = 0;
    int32 EmptySlots = 0;

    for (const FEquipmentSlotSnapshot& SlotSnapshot : CurrentState.SlotSnapshots)
    {
        if (SlotSnapshot.ItemInstance.IsValid())
        {
            FEquipmentOperationRequest TestOperation;
            TestOperation.OperationType   = EEquipmentOperationType::Equip;
            TestOperation.ItemInstance    = SlotSnapshot.ItemInstance;
            TestOperation.TargetSlotIndex = SlotSnapshot.SlotIndex;
            TestOperation.Instigator      = nullptr;
            TestOperation.bForceOperation = false;

            const FRuleEvaluationResult ComplianceResult = EvaluateRules(TestOperation);

            if (ComplianceResult.bPassed)
            {
                CompliantSlots++;
                Report += FString::Printf(TEXT("  Slot %d (%s): ✓ Compliant [%s]\n"),
                    SlotSnapshot.SlotIndex,
                    *SlotSnapshot.Configuration.SlotTag.ToString(),
                    *SlotSnapshot.ItemInstance.ItemID.ToString());
            }
            else
            {
                NonCompliantSlots++;
                Report += FString::Printf(TEXT("  Slot %d (%s): ✗ Non-compliant [%s]\n"),
                    SlotSnapshot.SlotIndex,
                    *SlotSnapshot.Configuration.SlotTag.ToString(),
                    *SlotSnapshot.ItemInstance.ItemID.ToString());
                Report += FString::Printf(TEXT("    Issue: %s\n"),
                    *ComplianceResult.FailureReason.ToString());
            }
        }
        else
        {
            EmptySlots++;
            Report += FString::Printf(TEXT("  Slot %d (%s): - Empty\n"),
                SlotSnapshot.SlotIndex,
                *SlotSnapshot.Configuration.SlotTag.ToString());
        }
    }

    Report += TEXT("\nWeight Analysis:\n");
    Report += TEXT("---------------\n");

    float TotalWeight = 0.0f;
    if (WeightEngine)
    {
        TArray<FSuspenseCoreInventoryItemInstance> FilteredItems;
        SnapshotToItemsFiltered(CurrentState, ExcludedSlotsCache, FilteredItems);
        TotalWeight = WeightEngine->CalculateTotalWeight(FilteredItems);

        const float MaxCapacity = WeightEngine->CalculateWeightCapacity(nullptr);
        const float EncumbranceRatio = (MaxCapacity > 0.0f) ? (TotalWeight / MaxCapacity) : 0.0f;
        const FGameplayTag EncumbranceTag = WeightEngine->GetEncumbranceTag(EncumbranceRatio);

        Report += FString::Printf(TEXT("  Current Weight: %.2f kg (excluding cosmetic slots)\n"), TotalWeight);
        Report += FString::Printf(TEXT("  Base Capacity: %.2f kg\n"), MaxCapacity);
        Report += FString::Printf(TEXT("  Utilization: %.1f%%\n"), EncumbranceRatio * 100.0f);
        Report += FString::Printf(TEXT("  Status: %s\n"), *EncumbranceTag.ToString());
        Report += FString::Printf(TEXT("  Excluded Slots: %s\n"), *ExcludedSlotsCache.ToStringSimple());

        TArray<FGameplayTagContainer> EmptyTags;
        const TMap<FGameplayTag, float> Distribution = WeightEngine->AnalyzeWeightDistribution(FilteredItems, EmptyTags);

        if (Distribution.Num() > 0)
        {
            Report += TEXT("  Distribution:\n");
            for (const auto& CategoryPair : Distribution)
            {
                const float CategoryWeight = CategoryPair.Value;
                const float CategoryPercent = (TotalWeight > 0.0f) ? (CategoryWeight / TotalWeight * 100.0f) : 0.0f;
                Report += FString::Printf(TEXT("    %s: %.2f kg (%.1f%%)\n"),
                    *CategoryPair.Key.ToString(), CategoryWeight, CategoryPercent);
            }
        }
    }
    else
    {
        Report += TEXT("  Weight engine not available\n");
    }

    Report += TEXT("\nSummary:\n");
    Report += TEXT("--------\n");
    Report += FString::Printf(TEXT("  Total Slots: %d\n"), CurrentState.SlotSnapshots.Num());
    Report += FString::Printf(TEXT("  Compliant: %d\n"), CompliantSlots);
    Report += FString::Printf(TEXT("  Non-compliant: %d\n"), NonCompliantSlots);
    Report += FString::Printf(TEXT("  Empty: %d\n"), EmptySlots);

    const float ComplianceRate = (CurrentState.SlotSnapshots.Num() > 0)
        ? (static_cast<float>(CompliantSlots) / static_cast<float>(CurrentState.SlotSnapshots.Num()) * 100.0f)
        : 100.0f;
    Report += FString::Printf(TEXT("  Compliance Rate: %.1f%%\n"), ComplianceRate);

    return Report;
}

bool USuspenseCoreRulesCoordinator::RegisterRuleEngine(
    const FGameplayTag& EngineType,
    UObject* Engine,
    ERuleExecutionPriority Priority)
{
    if (!Engine || !EngineType.IsValid())
    {
        UE_LOG(LogRulesCoordinator, Warning, TEXT("Cannot register rule engine: invalid engine or type"));
        return false;
    }

    FScopeLock Lock(&RulesLock);

    FRuleEngineRegistration Registration;
    Registration.EngineType = EngineType;
    Registration.Engine     = Engine;
    Registration.Priority   = Priority;
    Registration.bEnabled   = true;

    RegisteredEngines.Add(EngineType, Registration);

    UE_LOG(LogRulesCoordinator, Log, TEXT("Registered external rule engine: %s (Priority: %s)"),
        *EngineType.ToString(), *UEnum::GetValueAsString(Priority));
    return true;
}

bool USuspenseCoreRulesCoordinator::UnregisterRuleEngine(const FGameplayTag& EngineType)
{
    FScopeLock Lock(&RulesLock);

    if (RegisteredEngines.Remove(EngineType) > 0)
    {
        UE_LOG(LogRulesCoordinator, Log, TEXT("Unregistered rule engine: %s"), *EngineType.ToString());
        return true;
    }
    return false;
}

bool USuspenseCoreRulesCoordinator::SetEngineEnabled(const FGameplayTag& EngineType, bool bEnabled)
{
    FScopeLock Lock(&RulesLock);

    FRuleEngineRegistration* Registration = RegisteredEngines.Find(EngineType);
    if (Registration)
    {
        Registration->bEnabled = bEnabled;
        UE_LOG(LogRulesCoordinator, Log, TEXT("Engine %s: %s"),
            *EngineType.ToString(), bEnabled ? TEXT("enabled") : TEXT("disabled"));
        return true;
    }
    return false;
}

TArray<FRuleEngineRegistration> USuspenseCoreRulesCoordinator::GetRegisteredEngines() const
{
    FScopeLock Lock(&RulesLock);

    TArray<FRuleEngineRegistration> Result;
    RegisteredEngines.GenerateValueArray(Result);

    Result.Sort([](const FRuleEngineRegistration& A, const FRuleEngineRegistration& B)
    {
        return static_cast<int32>(A.Priority) < static_cast<int32>(B.Priority);
    });

    return Result;
}

TArray<FRuleEngineRegistration> USuspenseCoreRulesCoordinator::GetSortedEngines() const
{
    return GetRegisteredEngines();
}

TMap<FString, FString> USuspenseCoreRulesCoordinator::GetExecutionStatistics() const
{
    TMap<FString, FString> Stats;

    FScopeLock Lock(&RulesLock);

    Stats.Add(TEXT("TotalEvaluations"), FString::Printf(TEXT("%lld"), TotalEvaluations));
    Stats.Add(TEXT("TotalTimeMs"), FString::Printf(TEXT("%.2f"), AccumulatedEvalMs));
    Stats.Add(TEXT("AverageTimeMs"), TotalEvaluations > 0
        ? FString::Printf(TEXT("%.2f"), AccumulatedEvalMs / TotalEvaluations)
        : TEXT("0.0"));

    Stats.Add(TEXT("RegisteredEngines"), FString::Printf(TEXT("%d"), RegisteredEngines.Num()));
    Stats.Add(TEXT("GlobalRules"), FString::Printf(TEXT("%d"), GlobalRules.Num()));
    Stats.Add(TEXT("DisabledRules"), FString::Printf(TEXT("%d"), DisabledRules.Num()));

    if (LastExecutionTime.IsSet())
    {
        Stats.Add(TEXT("LastExecution"), LastExecutionTime.GetValue().ToString());
    }

    return Stats;
}

FString USuspenseCoreRulesCoordinator::GetPipelineHealth() const
{
    FString Health = TEXT("Rules Pipeline Health Check:\n");

    const bool bHasDataProvider = DataProvider.GetInterface() != nullptr;
    const bool bHasCompatibility = CompatibilityEngine != nullptr;
    const bool bHasRequirement = RequirementEngine != nullptr;
    const bool bHasWeight = WeightEngine != nullptr;
    const bool bHasConflict = ConflictEngine != nullptr;

    Health += FString::Printf(TEXT("Data Provider: %s\n"), bHasDataProvider ? TEXT("✓ OK") : TEXT("⚠ OPTIONAL"));
    Health += FString::Printf(TEXT("Compatibility Engine: %s\n"), bHasCompatibility ? TEXT("✓ OK") : TEXT("✗ MISSING"));
    Health += FString::Printf(TEXT("Requirement Engine: %s\n"), bHasRequirement ? TEXT("✓ OK") : TEXT("✗ MISSING"));
    Health += FString::Printf(TEXT("Weight Engine: %s\n"), bHasWeight ? TEXT("✓ OK") : TEXT("✗ MISSING"));
    Health += FString::Printf(TEXT("Conflict Engine: %s\n"), bHasConflict ? TEXT("✓ OK") : TEXT("✗ MISSING"));

    const bool bHealthy = bHasCompatibility && bHasRequirement && bHasWeight && bHasConflict;
    Health += FString::Printf(TEXT("\nOverall Status: %s"), bHealthy ? TEXT("✓ HEALTHY") : TEXT("✗ DEGRADED"));

    return Health;
}

FRuleEvaluationResult USuspenseCoreRulesCoordinator::ConvertToLegacyResult(
    const TArray<FSuspenseCoreRuleCheckResult>& NewResults) const
{
    FRuleEvaluationResult LegacyResult;
    LegacyResult.bPassed = true;
    LegacyResult.ConfidenceScore = 1.0f;

    for (const FSuspenseCoreRuleCheckResult& Result : NewResults)
    {
        if (!Result.bPassed)
        {
            LegacyResult.bPassed = false;

            if (Result.Severity == ESuspenseCoreRuleSeverity::Critical && LegacyResult.FailureReason.IsEmpty())
            {
                LegacyResult.FailureReason = Result.Message;
                LegacyResult.RuleType = Result.RuleTag;
            }
            else if (Result.Severity == ESuspenseCoreRuleSeverity::Error && LegacyResult.FailureReason.IsEmpty())
            {
                LegacyResult.FailureReason = Result.Message;
                LegacyResult.RuleType = Result.RuleTag;
            }
        }

        LegacyResult.ConfidenceScore *= Result.ConfidenceScore;

        if (!Result.Message.IsEmpty())
        {
            LegacyResult.Details.Add(Result.Message.ToString());
        }
    }

    if (!LegacyResult.bPassed && LegacyResult.FailureReason.IsEmpty())
    {
        LegacyResult.FailureReason = FText::FromString(TEXT("Rule validation failed"));
    }

    if (LegacyResult.bPassed && LegacyResult.FailureReason.IsEmpty())
    {
        LegacyResult.FailureReason = FText::FromString(TEXT("All rules passed"));
    }

    return LegacyResult;
}

FRuleEvaluationResult USuspenseCoreRulesCoordinator::ConvertSingleResult(const FSuspenseCoreRuleCheckResult& NewResult) const
{
    FRuleEvaluationResult LegacyResult;
    LegacyResult.bPassed = NewResult.bPassed;
    LegacyResult.FailureReason = NewResult.Message;
    LegacyResult.RuleType = NewResult.RuleTag;
    LegacyResult.ConfidenceScore = NewResult.ConfidenceScore;

    for (const auto& ContextPair : NewResult.Context)
    {
        LegacyResult.Details.Add(FString::Printf(TEXT("%s: %s"), *ContextPair.Key, *ContextPair.Value));
    }

    return LegacyResult;
}

FRuleEvaluationResult USuspenseCoreRulesCoordinator::ConvertAggregatedResult(const FSuspenseCoreAggregatedRuleResult& AggregatedResult) const
{
    return ConvertToLegacyResult(AggregatedResult.Results);
}

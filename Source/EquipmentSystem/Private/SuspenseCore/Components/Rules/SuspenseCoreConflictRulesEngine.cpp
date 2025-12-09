// SuspenseCoreConflictRulesEngine.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Rules/SuspenseCoreConflictRulesEngine.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Algo/Transform.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Types/Rules/SuspenseCoreRulesTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogConflictRules, Log, All);

USuspenseCoreConflictRulesEngine::USuspenseCoreConflictRulesEngine()
{
    // Инициализация произойдет через Initialize()
}

bool USuspenseCoreConflictRulesEngine::Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    if (!InDataProvider.GetInterface())
    {
        UE_LOG(LogConflictRules, Error, TEXT("Initialize failed - DataProvider is null"));
        return false;
    }

    DataProvider = InDataProvider;
    bIsInitialized = true;

    // Настройка правил конфликтов по умолчанию
    InitializeDefaultRules();

    UE_LOG(LogConflictRules, Log, TEXT("Conflict Rules Engine initialized with data provider"));

    return true;
}

void USuspenseCoreConflictRulesEngine::InitializeDefaultRules()
{
    // Настройка общих взаимоисключающих типов
    RegisterMutualExclusion(
        FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Heavy")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Light"))
    );

    RegisterMutualExclusion(
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.TwoHanded")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Shield"))
    );

    // Настройка общих наборов предметов
    TArray<FName> KnightSetItems;
    KnightSetItems.Add(TEXT("Knight_Helmet"));
    KnightSetItems.Add(TEXT("Knight_Chestplate"));
    KnightSetItems.Add(TEXT("Knight_Gauntlets"));
    KnightSetItems.Add(TEXT("Knight_Boots"));
    RegisterItemSet(
        FGameplayTag::RequestGameplayTag(TEXT("Set.Knight")),
        KnightSetItems,
        4
    );

    UE_LOG(LogConflictRules, Log, TEXT("Conflict Rules Engine initialized with default rules"));
}

FSuspenseCoreRuleCheckResult USuspenseCoreConflictRulesEngine::CheckItemConflicts(
    const FSuspenseCoreInventoryItemInstance& NewItem,
    const TArray<FSuspenseCoreInventoryItemInstance>& ExistingItems) const
{
    FSuspenseCoreRuleCheckResult Result = FSuspenseCoreRuleCheckResult::Success();
    Result.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.Conflict.ItemCheck"));
    Result.RuleType = ESuspenseCoreRuleType::Conflict;

    // Проверка инициализации движка
    if (!bIsInitialized || !DataProvider.GetInterface())
    {
        Result.bPassed = false;
        Result.Severity = ESuspenseCoreRuleSeverity::Critical;
        Result.Message = NSLOCTEXT("ConflictRules", "NotInitialized", "Conflict engine not properly initialized");
        Result.ConfidenceScore = 0.0f;
        return Result;
    }

    // Получение типа нового предмета
    FGameplayTag NewItemType = GetItemType(NewItem);

    if (!NewItemType.IsValid())
    {
        Result.Message = NSLOCTEXT("ConflictRules", "NoTypeInfo", "Item has no type information");
        Result.ConfidenceScore = 0.8f;
        return Result;
    }

    // Проверка каждого существующего предмета
    for (const FSuspenseCoreInventoryItemInstance& ExistingItem : ExistingItems)
    {
        if (!ExistingItem.IsValid())
        {
            continue;
        }

        FGameplayTag ExistingType = GetItemType(ExistingItem);

        // Проверка взаимного исключения
        if (CheckMutualExclusion(NewItemType, ExistingType))
        {
            FSuspenseCoreUnifiedItemData ExistingData;
            if (GetItemData(ExistingItem.ItemID, ExistingData))
            {
                Result.bPassed = false;
                Result.Severity = ESuspenseCoreRuleSeverity::Error;
                Result.Message = FText::Format(
                    NSLOCTEXT("ConflictRules", "MutuallyExclusive", "{0} cannot be equipped with {1}"),
                    FText::FromString(NewItemType.ToString()),
                    ExistingData.DisplayName
                );
                Result.ConfidenceScore = 0.0f;
                Result.bCanOverride = false;

                // Добавление деталей конфликта
                Result.Context.Add(TEXT("ConflictType"), TEXT("MutualExclusion"));
                Result.Context.Add(TEXT("ConflictingItem"), ExistingItem.ItemID.ToString());

                return Result; // Ранний выход при первом конфликте
            }
        }

        // Проверка несовместимости типов
        ESuspenseCoreConflictType ConflictType = GetConflictType(NewItem, ExistingItem);
        if (ConflictType != ESuspenseCoreConflictType::None && ConflictType != ESuspenseCoreConflictType::SetInterference)
        {
            FSuspenseCoreUnifiedItemData NewData, ExistingData;
            if (GetItemData(NewItem.ItemID, NewData) && GetItemData(ExistingItem.ItemID, ExistingData))
            {
                Result.bPassed = false;
                Result.Severity = ESuspenseCoreRuleSeverity::Error;
                Result.Message = FText::Format(
                    NSLOCTEXT("ConflictRules", "ItemsIncompatible", "{0} is incompatible with {1}"),
                    NewData.DisplayName,
                    ExistingData.DisplayName
                );
                Result.ConfidenceScore = 0.0f;

                Result.Context.Add(TEXT("ConflictType"), GetConflictTypeString(ConflictType));
                Result.Context.Add(TEXT("ConflictingItem"), ExistingItem.ItemID.ToString());

                return Result;
            }
        }
    }

    // Проверка необходимых компаньонов
    if (!CheckRequiredCompanions(NewItem, ExistingItems))
    {
        Result.bPassed = false;
        Result.Severity = ESuspenseCoreRuleSeverity::Warning;
        Result.Message = NSLOCTEXT("ConflictRules", "MissingCompanion", "This item requires companion items to function properly");
        Result.ConfidenceScore = 0.5f;
        Result.bCanOverride = true;

        Result.Context.Add(TEXT("ConflictType"), TEXT("MissingCompanion"));
    }

    // Проверка вмешательства в бонус набора
    if (WouldBreakSetBonus(NewItem, ExistingItems))
    {
        if (Result.bPassed) // Добавлять предупреждение только если нет других ошибок
        {
            Result.Severity = ESuspenseCoreRuleSeverity::Warning;
            Result.Message = NSLOCTEXT("ConflictRules", "BreaksSetBonus", "Equipping this item will break an active set bonus");
            Result.ConfidenceScore = 0.7f;
            Result.bCanOverride = true;

            Result.Context.Add(TEXT("Warning"), TEXT("BreaksSetBonus"));
        }
    }

    if (Result.bPassed)
    {
        Result.Message = NSLOCTEXT("ConflictRules", "NoConflicts", "No equipment conflicts detected");
        Result.ConfidenceScore = 1.0f;
    }

    return Result;
}

FSuspenseCoreRuleCheckResult USuspenseCoreConflictRulesEngine::CheckSlotConflicts(
    const FSuspenseCoreInventoryItemInstance& NewItem,
    int32 TargetSlot,
    const TArray<FEquipmentSlotSnapshot>& Slots) const
{
    FSuspenseCoreRuleCheckResult Result = FSuspenseCoreRuleCheckResult::Success();
    Result.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.Conflict.SlotCheck"));
    Result.RuleType = ESuspenseCoreRuleType::Conflict;

    // Критическая разница: теперь работаем с реальными снапшотами слотов из координатора,
    // а не с самодельной картой TMap<int32, FSuspenseCoreInventoryItemInstance>, которая давала ложные индексы
    const FEquipmentSlotSnapshot* TargetSlotSnapshot = Slots.FindByPredicate(
        [TargetSlot](const FEquipmentSlotSnapshot& S)
        {
            return S.SlotIndex == TargetSlot;
        });

    // Проверяем, занят ли целевой слот
    if (TargetSlotSnapshot && TargetSlotSnapshot->ItemInstance.IsValid())
    {
        // Анализируем типы предметов для выявления конфликтов одного типа в одном слоте
        FGameplayTag NewType = GetItemType(NewItem);
        FGameplayTag ExistingType = GetItemType(TargetSlotSnapshot->ItemInstance);

        // Пример проверки: два основных оружия не могут находиться в одном слоте
        if (NewType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Primary"))) &&
            ExistingType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Primary"))))
        {
            Result.bPassed = false;
            Result.Severity = ESuspenseCoreRuleSeverity::Error;
            Result.Message = NSLOCTEXT("ConflictRules", "SlotOccupied", "Cannot equip multiple primary weapons in the same slot");
            Result.ConfidenceScore = 0.0f;
            Result.bCanOverride = false;
        }
    }

    // Ключевое улучшение: правильная проверка двуручных предметов
    // Старая версия использовала примитивную логику "если слот 0, то проверить слот 1"
    // Новая версия использует семантические теги слотов для корректного определения
    FSuspenseCoreUnifiedItemData NewItemData;
    if (GetItemData(NewItem.ItemID, NewItemData))
    {
        // Кэшируем часто используемые теги для производительности
        static const FGameplayTag TagRequiresBothHands = FGameplayTag::RequestGameplayTag(TEXT("Item.RequiresBothHands"));
        static const FGameplayTag TagHandMain          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Hand.Main"));
        static const FGameplayTag TagHandOff           = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Hand.Off"));

        if (NewItemData.ItemTags.HasTag(TagRequiresBothHands))
        {
            // Ищем любой слот руки (кроме целевого), который уже занят
            // Это решает проблему с неправильными индексами - мы проверяем по семантике слотов
            const bool bBlocksOtherHand = !!Slots.FindByPredicate([&](const FEquipmentSlotSnapshot& S)
            {
                // Пропускаем пустые слоты
                if (!S.ItemInstance.IsValid())
                    return false;

                // Пропускаем целевой слот (в него мы как раз экипируем)
                const bool bIsSameSlot = (S.SlotIndex == TargetSlot);
                if (bIsSameSlot)
                    return false;

                // Проверяем, является ли этот слот слотом руки
                const FGameplayTag SlotTag = S.Configuration.SlotTag;
                return SlotTag.MatchesTag(TagHandMain) || SlotTag.MatchesTag(TagHandOff);
            });

            if (bBlocksOtherHand)
            {
                Result.bPassed = false;
                Result.Severity = ESuspenseCoreRuleSeverity::Error;
                Result.Message = NSLOCTEXT("ConflictRules", "RequiresBothHands", "Two-handed items require both hand slots to be free");
                Result.ConfidenceScore = 0.0f;
                Result.Context.Add(TEXT("RequiredSlots"), TEXT("BothHands"));
            }
        }
    }

    // Устанавливаем результат успеха, если никаких конфликтов не обнаружено
    if (Result.bPassed)
    {
        Result.Message = NSLOCTEXT("ConflictRules", "SlotCompatible", "Item is compatible with target slot");
        Result.ConfidenceScore = 1.0f;
    }

    return Result;
}

FSuspenseCoreAggregatedRuleResult USuspenseCoreConflictRulesEngine::EvaluateConflictRules(
    const FSuspenseCoreRuleContext& Context) const
{
    FSuspenseCoreAggregatedRuleResult AggregatedResult;

    // Основная проверка конфликтов предметов остается без изменений
    FSuspenseCoreRuleCheckResult ItemConflictResult = CheckItemConflicts(
        Context.ItemInstance,
        Context.CurrentItems
    );
    AggregatedResult.AddResult(ItemConflictResult);

    // ВАЖНОЕ ПРЕДУПРЕЖДЕНИЕ: эта перегрузка больше не выполняет слотные проверки!
    // Причина: старая логика создавала TMap<int32, FSuspenseCoreInventoryItemInstance> из CurrentItems,
    // где индексы не соответствовали реальным SlotIndex, что приводило к ложным срабатываниям.
    // Координатор должен использовать EvaluateConflictRulesWithSlots для корректной работы.

    // Проверка общей совместимости сохраняется
    float CompatibilityScore = CalculateCompatibilityScore(Context.ItemInstance, Context.CurrentItems);

    if (CompatibilityScore < 0.3f)
    {
        FSuspenseCoreRuleCheckResult CompatResult;
        CompatResult.bPassed = false;
        CompatResult.Severity = ESuspenseCoreRuleSeverity::Warning;
        CompatResult.Message = NSLOCTEXT("ConflictRules", "PoorCompatibility", "Item has poor compatibility with current equipment");
        CompatResult.ConfidenceScore = CompatibilityScore;
        CompatResult.bCanOverride = true;
        CompatResult.RuleType = ESuspenseCoreRuleType::Conflict;

        AggregatedResult.AddResult(CompatResult);
    }

    UE_LOG(LogConflictRules, Verbose, TEXT("Conflict evaluation complete: %s"),
        AggregatedResult.bAllPassed ? TEXT("PASS") : TEXT("FAIL"));

    return AggregatedResult;
}

TArray<FSuspenseCoreConflictResolution> USuspenseCoreConflictRulesEngine::FindAllConflicts(
    const FSuspenseCoreInventoryItemInstance& Item,
    const TArray<FSuspenseCoreInventoryItemInstance>& CurrentItems) const
{
    TArray<FSuspenseCoreConflictResolution> Conflicts;

    FGameplayTag ItemType = GetItemType(Item);

    for (const FSuspenseCoreInventoryItemInstance& CurrentItem : CurrentItems)
    {
        if (!CurrentItem.IsValid())
        {
            continue;
        }

        ESuspenseCoreConflictType ConflictType = GetConflictType(Item, CurrentItem);

        if (ConflictType != ESuspenseCoreConflictType::None)
        {
            FSuspenseCoreConflictResolution Conflict;
            Conflict.ConflictType = ConflictType;
            Conflict.ConflictingItems.Add(CurrentItem);

            // Определение стратегии разрешения
            switch (ConflictType)
            {
                case ESuspenseCoreConflictType::MutualExclusion:
                    Conflict.Strategy = ESuspenseCoreConflictResolution::Replace;
                    Conflict.Description = NSLOCTEXT("ConflictRules", "MustReplace", "Must replace existing item");
                    Conflict.bCanAutoResolve = true;
                    break;

                case ESuspenseCoreConflictType::SlotConflict:
                    Conflict.Strategy = ESuspenseCoreConflictResolution::Replace;
                    Conflict.Description = NSLOCTEXT("ConflictRules", "SlotConflictReplace", "Replace item in slot");
                    Conflict.bCanAutoResolve = true;
                    break;

                case ESuspenseCoreConflictType::TypeIncompatibility:
                    Conflict.Strategy = ESuspenseCoreConflictResolution::Reject;
                    Conflict.Description = NSLOCTEXT("ConflictRules", "CannotEquipTogether", "Items cannot be equipped together");
                    Conflict.bCanAutoResolve = false;
                    break;

                case ESuspenseCoreConflictType::SetInterference:
                    Conflict.Strategy = ESuspenseCoreConflictResolution::Prompt;
                    Conflict.Description = NSLOCTEXT("ConflictRules", "WouldBreakSet", "Would break equipment set bonus");
                    Conflict.bCanAutoResolve = false;
                    break;

                default:
                    Conflict.Strategy = ESuspenseCoreConflictResolution::Prompt;
                    Conflict.bCanAutoResolve = false;
                    break;
            }

            Conflicts.Add(Conflict);
        }
    }

    return Conflicts;
}

TArray<FSuspenseCoreConflictResolution> USuspenseCoreConflictRulesEngine::PredictConflicts(
    const TArray<FSuspenseCoreInventoryItemInstance>& PlannedItems) const
{
    TArray<FSuspenseCoreConflictResolution> AllConflicts;

    // Проверяем каждую пару предметов на потенциальные конфликты
    // Это алгоритм O(n²), но для типичных размеров экипировки (10-20 слотов) это приемлемо
    for (int32 i = 0; i < PlannedItems.Num(); i++)
    {
        if (!PlannedItems[i].IsValid())
        {
            continue;  // Пропускаем недействительные предметы
        }

        for (int32 j = i + 1; j < PlannedItems.Num(); j++)
        {
            if (!PlannedItems[j].IsValid())
            {
                continue;
            }

            // Определяем тип конфликта между двумя предметами
            ESuspenseCoreConflictType ConflictType = GetConflictType(PlannedItems[i], PlannedItems[j]);

            if (ConflictType != ESuspenseCoreConflictType::None)
            {
                FSuspenseCoreConflictResolution Conflict;
                Conflict.ConflictType = ConflictType;
                Conflict.ConflictingItems.Add(PlannedItems[i]);
                Conflict.ConflictingItems.Add(PlannedItems[j]);

                // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: замена braced-init на явное создание массива
                // Старый код: Conflict.Strategy = SuggestResolutionStrategy({ Conflict });
                // Новый код: создаем временный массив для совместимости с UE4/5
                {
                    TArray<FSuspenseCoreConflictResolution> TemporaryConflictArray;
                    TemporaryConflictArray.Add(Conflict);
                    Conflict.Strategy = SuggestResolutionStrategy(TemporaryConflictArray);
                }

                // Формируем понятное описание конфликта для пользователя
                FSuspenseCoreUnifiedItemData Data1, Data2;
                if (GetItemData(PlannedItems[i].ItemID, Data1) &&
                    GetItemData(PlannedItems[j].ItemID, Data2))
                {
                    Conflict.Description = FText::Format(
                        NSLOCTEXT("ConflictRules", "PredictedConflict", "Predicted conflict between {0} and {1}"),
                        Data1.DisplayName,
                        Data2.DisplayName
                    );
                }

                AllConflicts.Add(Conflict);
            }
        }
    }

    return AllConflicts;
}

ESuspenseCoreConflictType USuspenseCoreConflictRulesEngine::GetConflictType(
    const FSuspenseCoreInventoryItemInstance& Item1,
    const FSuspenseCoreInventoryItemInstance& Item2) const
{
    FGameplayTag Type1 = GetItemType(Item1);
    FGameplayTag Type2 = GetItemType(Item2);

    // Проверка взаимного исключения
    if (CheckMutualExclusion(Type1, Type2))
    {
        return ESuspenseCoreConflictType::MutualExclusion;
    }

    // Проверка несовместимости типов
    FSuspenseCoreUnifiedItemData Data1, Data2;
    if (GetItemData(Item1.ItemID, Data1) && GetItemData(Item2.ItemID, Data2))
    {
        // Двуручное оружие со щитом
        if ((Data1.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.RequiresBothHands"))) &&
             Data2.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Shield")))) ||
            (Data2.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.RequiresBothHands"))) &&
             Data1.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Shield")))))
        {
            return ESuspenseCoreConflictType::TypeIncompatibility;
        }

        // Несколько предметов в одном уникальном слоте
        if (Data1.EquipmentSlot == Data2.EquipmentSlot &&
            Data1.EquipmentSlot.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Unique"))))
        {
            return ESuspenseCoreConflictType::SlotConflict;
        }
    }

    return ESuspenseCoreConflictType::None;
}

bool USuspenseCoreConflictRulesEngine::AreItemsCompatible(
    const FSuspenseCoreInventoryItemInstance& Item1,
    const FSuspenseCoreInventoryItemInstance& Item2) const
{
    return GetConflictType(Item1, Item2) == ESuspenseCoreConflictType::None;
}

float USuspenseCoreConflictRulesEngine::CalculateCompatibilityScore(
    const FSuspenseCoreInventoryItemInstance& Item,
    const TArray<FSuspenseCoreInventoryItemInstance>& ExistingItems) const
{
    if (ExistingItems.Num() == 0)
    {
        return 1.0f; // Идеальная совместимость с пустотой
    }

    float TotalScore = 0.0f;
    int32 ValidComparisons = 0;

    FSuspenseCoreUnifiedItemData NewItemData;
    if (!GetItemData(Item.ItemID, NewItemData))
    {
        return 0.5f; // Неизвестная совместимость
    }

    for (const FSuspenseCoreInventoryItemInstance& ExistingItem : ExistingItems)
    {
        if (!ExistingItem.IsValid())
        {
            continue;
        }

        FSuspenseCoreUnifiedItemData ExistingData;
        if (!GetItemData(ExistingItem.ItemID, ExistingData))
        {
            continue;
        }

        float PairScore = 1.0f;

        // Проверка конфликтов
        ESuspenseCoreConflictType ConflictType = GetConflictType(Item, ExistingItem);
        if (ConflictType != ESuspenseCoreConflictType::None)
        {
            PairScore = 0.0f;
        }
        else
        {
            // Проверка синергий

            // Один набор бонусов
            for (const auto& SetPair : ItemSets)
            {
                if (SetPair.Value.Contains(Item.ItemID) &&
                    SetPair.Value.Contains(ExistingItem.ItemID))
                {
                    PairScore = 1.5f; // Бонус за предметы набора
                    break;
                }
            }

            // Дополняющие типы (например, меч и щит)
            if (NewItemData.ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Melee"))) &&
                ExistingData.ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Shield"))))
            {
                PairScore = 1.3f;
            }

            // Соответствующие типы брони
            if (NewItemData.ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor"))) &&
                ExistingData.ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor"))))
            {
                // Проверка принадлежности к одному классу брони (тяжелая, средняя, легкая)
                FGameplayTag NewArmorClass = GetArmorClass(NewItemData);
                FGameplayTag ExistingArmorClass = GetArmorClass(ExistingData);

                if (NewArmorClass == ExistingArmorClass && NewArmorClass.IsValid())
                {
                    PairScore = 1.2f; // Бонус за соответствие класса брони
                }
            }
        }

        TotalScore += PairScore;
        ValidComparisons++;
    }

    return ValidComparisons > 0 ? FMath::Clamp(TotalScore / ValidComparisons, 0.0f, 1.0f) : 1.0f;
}

FSuspenseCoreRuleCheckResult USuspenseCoreConflictRulesEngine::CheckTypeExclusivity(
    const FGameplayTag& NewItemType,
    const TArray<FGameplayTag>& ExistingTypes) const
{
    FSuspenseCoreRuleCheckResult Result = FSuspenseCoreRuleCheckResult::Success();
    Result.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.Conflict.TypeExclusivity"));
    Result.RuleType = ESuspenseCoreRuleType::Conflict;

    for (const FGameplayTag& ExistingType : ExistingTypes)
    {
        if (CheckMutualExclusion(NewItemType, ExistingType))
        {
            Result.bPassed = false;
            Result.Severity = ESuspenseCoreRuleSeverity::Error;
            Result.Message = FText::Format(
                NSLOCTEXT("ConflictRules", "TypesExclusive", "Item type {0} cannot be equipped with {1}"),
                FText::FromString(NewItemType.ToString()),
                FText::FromString(ExistingType.ToString())
            );
            Result.ConfidenceScore = 0.0f;
            Result.bCanOverride = false;

            Result.Context.Add(TEXT("NewType"), NewItemType.ToString());
            Result.Context.Add(TEXT("ConflictingType"), ExistingType.ToString());

            return Result;
        }
    }

    Result.Message = NSLOCTEXT("ConflictRules", "NoTypeConflicts", "No type exclusivity conflicts");
    Result.ConfidenceScore = 1.0f;

    return Result;
}

TArray<FSuspenseCoreSetBonusInfo> USuspenseCoreConflictRulesEngine::DetectSetBonuses(
    const TArray<FSuspenseCoreInventoryItemInstance>& Items) const
{
    TArray<FSuspenseCoreSetBonusInfo> ActiveSets;

    FScopeLock Lock(&RulesLock);

    // Проверка каждого зарегистрированного набора
    for (const auto& SetPair : ItemSets)
    {
        FSuspenseCoreSetBonusInfo SetInfo;
        SetInfo.SetTag = SetPair.Key;
        SetInfo.SetItems = SetPair.Value;

        // Поиск необходимого количества для этого набора
        const int32* RequiredCount = SetBonusRequirements.Find(SetPair.Key);
        SetInfo.RequiredCount = RequiredCount ? *RequiredCount : 2;

        // Подсчет экипированных предметов из этого набора
        for (const FSuspenseCoreInventoryItemInstance& Item : Items)
        {
            if (Item.IsValid() && SetPair.Value.Contains(Item.ItemID))
            {
                SetInfo.EquippedItems.Add(Item.ItemID);
            }
        }

        // Проверка активности бонуса
        SetInfo.bBonusActive = SetInfo.EquippedItems.Num() >= SetInfo.RequiredCount;

        // Генерация описания
        if (SetInfo.bBonusActive)
        {
            SetInfo.BonusDescription = FText::Format(
                NSLOCTEXT("ConflictRules", "SetBonusActive", "{0} Set Bonus Active ({1}/{2} pieces)"),
                FText::FromString(SetPair.Key.ToString()),
                SetInfo.EquippedItems.Num(),
                SetInfo.RequiredCount
            );
        }
        else if (SetInfo.EquippedItems.Num() > 0)
        {
            SetInfo.BonusDescription = FText::Format(
                NSLOCTEXT("ConflictRules", "SetBonusPartial", "{0} Set ({1}/{2} pieces)"),
                FText::FromString(SetPair.Key.ToString()),
                SetInfo.EquippedItems.Num(),
                SetInfo.RequiredCount
            );
        }

        if (SetInfo.EquippedItems.Num() > 0)
        {
            ActiveSets.Add(SetInfo);
        }
    }

    return ActiveSets;
}

bool USuspenseCoreConflictRulesEngine::WouldBreakSetBonus(
    const FSuspenseCoreInventoryItemInstance& ItemToRemove,
    const TArray<FSuspenseCoreInventoryItemInstance>& CurrentItems) const
{
    // Получение текущих активных наборов
    TArray<FSuspenseCoreSetBonusInfo> CurrentSets = DetectSetBonuses(CurrentItems);

    // Проверка каждого активного набора
    for (const FSuspenseCoreSetBonusInfo& SetInfo : CurrentSets)
    {
        if (SetInfo.bBonusActive && SetInfo.EquippedItems.Contains(ItemToRemove.ItemID))
        {
            // Нарушит ли удаление этого предмета бонус?
            if (SetInfo.EquippedItems.Num() - 1 < SetInfo.RequiredCount)
            {
                return true;
            }
        }
    }

    return false;
}

TArray<FName> USuspenseCoreConflictRulesEngine::GetMissingSetItems(
    const FGameplayTag& SetTag,
    const TArray<FSuspenseCoreInventoryItemInstance>& CurrentItems) const
{
    TArray<FName> MissingItems;

    FScopeLock Lock(&RulesLock);

    const TArray<FName>* SetItems = ItemSets.Find(SetTag);
    if (!SetItems)
    {
        return MissingItems;
    }

    // Поиск отсутствующих предметов
    for (const FName& SetItemID : *SetItems)
    {
        bool bHasItem = false;

        for (const FSuspenseCoreInventoryItemInstance& Item : CurrentItems)
        {
            if (Item.IsValid() && Item.ItemID == SetItemID)
            {
                bHasItem = true;
                break;
            }
        }

        if (!bHasItem)
        {
            MissingItems.Add(SetItemID);
        }
    }

    return MissingItems;
}

bool USuspenseCoreConflictRulesEngine::SuggestResolutions(
    const TArray<FSuspenseCoreConflictResolution>& Conflicts,
    ESuspenseCoreConflictResolution Strategy,
    TArray<FSuspenseCoreResolutionAction>& OutActions) const
{
    OutActions.Reset();

    if (Strategy == ESuspenseCoreConflictResolution::Auto)
    {
        Strategy = SuggestResolutionStrategy(Conflicts);
        if (Strategy == ESuspenseCoreConflictResolution::Auto)
        {
            Strategy = ESuspenseCoreConflictResolution::Prompt;
        }
    }

    for (const FSuspenseCoreConflictResolution& C : Conflicts)
    {
        switch (Strategy)
        {
            case ESuspenseCoreConflictResolution::Reject:
            {
                FSuspenseCoreResolutionAction A;
                A.ActionTag = FGameplayTag::RequestGameplayTag(TEXT("Resolution.Action.Reject"));
                A.bBlocking = true;
                A.Reason    = NSLOCTEXT("ConflictRules", "RejectReason", "Operation rejected due to conflicts");
                OutActions.Add(MoveTemp(A));
                return false; // немедленный отказ
            }

            case ESuspenseCoreConflictResolution::Replace:
            {
                for (const FSuspenseCoreInventoryItemInstance& It : C.ConflictingItems)
                {
                    FSuspenseCoreResolutionAction A;
                    A.ActionTag    = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unequip"));
                    A.ItemInstance = It;
                    A.bBlocking    = false;
                    OutActions.Add(MoveTemp(A));
                }
                break;
            }

            case ESuspenseCoreConflictResolution::Stack:
            {
                if (C.ConflictingItems.Num() > 0)
                {
                    FSuspenseCoreResolutionAction A;
                    A.ActionTag    = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set")); // «свести»
                    A.ItemInstance = C.ConflictingItems[0];
                    A.bBlocking    = false;
                    OutActions.Add(MoveTemp(A));
                }
                break;
            }

            case ESuspenseCoreConflictResolution::Prompt:
            {
                FSuspenseCoreResolutionAction A;
                A.ActionTag = FGameplayTag::RequestGameplayTag(TEXT("Resolution.Action.Prompt"));
                A.bBlocking = true;
                A.Reason    = NSLOCTEXT("ConflictRules", "PromptRequired", "User input required to resolve conflict");
                OutActions.Add(MoveTemp(A));
                return false; // блокирующее
            }

            default:
                break;
        }
    }

    return true;
}

ESuspenseCoreConflictResolution USuspenseCoreConflictRulesEngine::SuggestResolutionStrategy(
    const TArray<FSuspenseCoreConflictResolution>& Conflicts) const
{
    if (Conflicts.Num() == 0)
    {
        return ESuspenseCoreConflictResolution::Auto;
    }

    // Проверка возможности автоматического разрешения всех конфликтов
    bool bAllAutoResolvable = true;
    for (const FSuspenseCoreConflictResolution& Conflict : Conflicts)
    {
        if (!Conflict.bCanAutoResolve)
        {
            bAllAutoResolvable = false;
            break;
        }
    }

    if (bAllAutoResolvable)
    {
        // Если все взаимоисключения или конфликты слотов, предложить замену
        bool bAllReplaceable = true;
        for (const FSuspenseCoreConflictResolution& Conflict : Conflicts)
        {
            if (Conflict.ConflictType != ESuspenseCoreConflictType::MutualExclusion &&
                Conflict.ConflictType != ESuspenseCoreConflictType::SlotConflict)
            {
                bAllReplaceable = false;
                break;
            }
        }

        if (bAllReplaceable)
        {
            return ESuspenseCoreConflictResolution::Replace;
        }
    }

    // По умолчанию запросить решение пользователя
    return ESuspenseCoreConflictResolution::Prompt;
}

FText USuspenseCoreConflictRulesEngine::GetConflictDescription(const FSuspenseCoreConflictResolution& Conflict) const
{
    // Предоставляем пользователю понятные описания различных типов конфликтов
    switch (Conflict.ConflictType)
    {
    case ESuspenseCoreConflictType::MutualExclusion:
        return NSLOCTEXT("ConflictRules", "MutualExclusionDesc",
            "These items cannot be equipped together due to mutual exclusivity");

    case ESuspenseCoreConflictType::SlotConflict:
        return NSLOCTEXT("ConflictRules", "SlotConflictDesc",
            "Multiple items are competing for the same equipment slot");

    case ESuspenseCoreConflictType::TypeIncompatibility:
        return NSLOCTEXT("ConflictRules", "TypeIncompatibilityDesc",
            "These item types are incompatible with each other");

    case ESuspenseCoreConflictType::SetInterference:
        return NSLOCTEXT("ConflictRules", "SetInterferenceDesc",
            "Equipping this item will interfere with an equipment set bonus");

    default:
        // ВАЖНОЕ ИСПРАВЛЕНИЕ: добавлено дефолтное описание для случаев,
        // когда конфликт не имеет собственного описания
        return Conflict.Description.IsEmpty()
            ? NSLOCTEXT("ConflictRules", "GenericConflict", "Equipment conflict detected")
            : Conflict.Description;
    }
}

void USuspenseCoreConflictRulesEngine::RegisterMutualExclusion(
    const FGameplayTag& Type1,
    const FGameplayTag& Type2)
{
    FScopeLock Lock(&RulesLock);

    // Добавление двустороннего исключения
    MutuallyExclusiveTypes.FindOrAdd(Type1).Add(Type2);
    MutuallyExclusiveTypes.FindOrAdd(Type2).Add(Type1);

    UE_LOG(LogConflictRules, Log, TEXT("Registered mutual exclusion: %s <-> %s"),
        *Type1.ToString(), *Type2.ToString());
}

void USuspenseCoreConflictRulesEngine::RegisterRequiredCompanions(
    const FGameplayTag& ItemTag,
    const TArray<FGameplayTag>& CompanionTags)
{
    FScopeLock Lock(&RulesLock);

    RequiredCompanions.Add(ItemTag, CompanionTags);

    UE_LOG(LogConflictRules, Log, TEXT("Registered %d required companions for %s"),
        CompanionTags.Num(), *ItemTag.ToString());
}

void USuspenseCoreConflictRulesEngine::RegisterItemSet(
    const FGameplayTag& SetTag,
    const TArray<FName>& SetItems,
    int32 RequiredCount)
{
    FScopeLock Lock(&RulesLock);

    ItemSets.Add(SetTag, SetItems);
    SetBonusRequirements.Add(SetTag, RequiredCount);

    UE_LOG(LogConflictRules, Log, TEXT("Registered item set %s with %d items (requires %d)"),
        *SetTag.ToString(), SetItems.Num(), RequiredCount);
}

void USuspenseCoreConflictRulesEngine::ClearAllRules()
{
    FScopeLock Lock(&RulesLock);

    MutuallyExclusiveTypes.Empty();
    RequiredCompanions.Empty();
    ItemSets.Empty();
    SetBonusRequirements.Empty();

    UE_LOG(LogConflictRules, Log, TEXT("All conflict rules cleared"));
}

void USuspenseCoreConflictRulesEngine::ClearCache()
{
    // В данной реализации нет кэширования для конфликт-движка,
    // но метод предоставлен для совместимости с интерфейсом
    UE_LOG(LogConflictRules, Log, TEXT("Cache cleared (no cache in conflict engine)"));
}

void USuspenseCoreConflictRulesEngine::ResetStatistics()
{
    // Сброс статистики - в данной реализации статистика ведется координатором
    UE_LOG(LogConflictRules, Log, TEXT("Statistics reset (statistics managed by coordinator)"));
}

bool USuspenseCoreConflictRulesEngine::CheckMutualExclusion(
    const FGameplayTag& Type1,
    const FGameplayTag& Type2) const
{
    FScopeLock Lock(&RulesLock);

    const TSet<FGameplayTag>* ExclusiveTypes = MutuallyExclusiveTypes.Find(Type1);
    if (ExclusiveTypes)
    {
        return ExclusiveTypes->Contains(Type2);
    }

    return false;
}

bool USuspenseCoreConflictRulesEngine::CheckRequiredCompanions(
    const FSuspenseCoreInventoryItemInstance& Item,
    const TArray<FSuspenseCoreInventoryItemInstance>& CurrentItems) const
{
    FGameplayTag ItemType = GetItemType(Item);

    FScopeLock Lock(&RulesLock);

    const TArray<FGameplayTag>* RequiredTypes = RequiredCompanions.Find(ItemType);
    if (!RequiredTypes || RequiredTypes->Num() == 0)
    {
        return true; // Компаньоны не требуются
    }

    // Проверка наличия всех необходимых компаньонов
    for (const FGameplayTag& RequiredType : *RequiredTypes)
    {
        bool bHasCompanion = false;

        for (const FSuspenseCoreInventoryItemInstance& CurrentItem : CurrentItems)
        {
            if (GetItemType(CurrentItem).MatchesTag(RequiredType))
            {
                bHasCompanion = true;
                break;
            }
        }

        if (!bHasCompanion)
        {
            return false; // Отсутствует необходимый компаньон
        }
    }

    return true;
}

FGameplayTag USuspenseCoreConflictRulesEngine::GetItemType(const FSuspenseCoreInventoryItemInstance& Item) const
{
    FSuspenseCoreUnifiedItemData ItemData;
    if (GetItemData(Item.ItemID, ItemData))
    {
        return ItemData.GetEffectiveItemType();
    }

    return FGameplayTag::EmptyTag;
}

FGameplayTag USuspenseCoreConflictRulesEngine::GetArmorClass(const FSuspenseCoreUnifiedItemData& ItemData) const
{
    if (ItemData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Heavy"))))
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Heavy"));
    }
    else if (ItemData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Medium"))))
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Medium"));
    }
    else if (ItemData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Light"))))
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Light"));
    }

    return FGameplayTag::EmptyTag;
}

FString USuspenseCoreConflictRulesEngine::GetConflictTypeString(ESuspenseCoreConflictType ConflictType) const
{
    switch (ConflictType)
    {
        case ESuspenseCoreConflictType::None:
            return TEXT("None");
        case ESuspenseCoreConflictType::MutualExclusion:
            return TEXT("MutualExclusion");
        case ESuspenseCoreConflictType::SlotConflict:
            return TEXT("SlotConflict");
        case ESuspenseCoreConflictType::TypeIncompatibility:
            return TEXT("TypeIncompatibility");
        case ESuspenseCoreConflictType::SetInterference:
            return TEXT("SetInterference");
        case ESuspenseCoreConflictType::Custom:
            return TEXT("Custom");
        default:
            return TEXT("Unknown");
    }
}

bool USuspenseCoreConflictRulesEngine::GetItemData(FName ItemID, FSuspenseCoreUnifiedItemData& OutData) const
{
    if (!bIsInitialized || !DataProvider.GetObject())
    {
        UE_LOG(LogConflictRules, Warning, TEXT("GetItemData: engine not initialized or provider missing"));
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
            OutData = MoveTemp(Params.OutData);
            return true;
        }
    }

    UE_LOG(LogConflictRules, Warning, TEXT("GetItemData: provider has no GetUnifiedItemData, item=%s"), *ItemID.ToString());
    return false;
}

FSuspenseCoreAggregatedRuleResult USuspenseCoreConflictRulesEngine::EvaluateConflictRulesWithSlots(
    const FSuspenseCoreRuleContext& Context,
    const TArray<FEquipmentSlotSnapshot>& Slots) const
{
    FSuspenseCoreAggregatedRuleResult AggregatedResult;

    // Этап 1: Проверка конфликтов предметов между собой
    // Это включает взаимные исключения, несовместимые типы, нарушения сетов
    FSuspenseCoreRuleCheckResult ItemConflictResult = CheckItemConflicts(
        Context.ItemInstance,
        Context.CurrentItems
    );
    AggregatedResult.AddResult(ItemConflictResult);

    // Этап 2: Проверка слотных конфликтов с реальными индексами слотов
    // Критически важно: используем реальные снапшоты слотов из координатора,
    // а не самодельную карту, которая приводила к ложным срабатываниям
    if (Context.TargetSlotIndex != INDEX_NONE)
    {
        FSuspenseCoreRuleCheckResult SlotConflictResult = CheckSlotConflicts(
            Context.ItemInstance,
            Context.TargetSlotIndex,
            Slots  // Передаем реальные слоты!
        );
        AggregatedResult.AddResult(SlotConflictResult);
    }

    // Этап 3: Анализ общей совместимости предмета с текущим снаряжением
    // Это мягкая проверка, которая может дать предупреждение, но не блокировать операцию
    const float CompatibilityScore = CalculateCompatibilityScore(Context.ItemInstance, Context.CurrentItems);
    if (CompatibilityScore < 0.3f)
    {
        FSuspenseCoreRuleCheckResult CompatResult;
        CompatResult.bPassed = false;  // Технически провал, но с возможностью переопределения
        CompatResult.Severity = ESuspenseCoreRuleSeverity::Warning;  // Предупреждение, не критическая ошибка
        CompatResult.Message = NSLOCTEXT("ConflictRules", "PoorCompatibility", "Item has poor compatibility with current equipment");
        CompatResult.ConfidenceScore = CompatibilityScore;  // Передаем реальный скор
        CompatResult.bCanOverride = true;  // Пользователь может проигнорировать
        CompatResult.RuleType = ESuspenseCoreRuleType::Conflict;

        AggregatedResult.AddResult(CompatResult);
    }

    // Логирование для отладки и мониторинга производительности
    UE_LOG(LogConflictRules, Verbose, TEXT("Conflict evaluation (WithSlots) complete: %s"),
        AggregatedResult.bAllPassed ? TEXT("PASS") : TEXT("FAIL"));

    return AggregatedResult;
}

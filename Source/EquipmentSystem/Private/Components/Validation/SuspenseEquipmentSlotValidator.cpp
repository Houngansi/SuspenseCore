// MedComEquipmentSlotValidator.cpp
// Copyright MedCom

#include "Components/Validation/SuspenseEquipmentSlotValidator.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
// Единые макросы/логи проекта
#include "Services/EquipmentServiceMacros.h"

// Типы/лоадаут
#include "Types/Equipment/EquipmentTypes.h"
#include "Types/Loadout/LoadoutSettings.h"

// ==============================================
// Статическая матрица совместимости типов
// ==============================================
const TMap<EEquipmentSlotType, TArray<FGameplayTag>> USuspenseEquipmentSlotValidator::TypeCompatibilityMatrix =
	USuspenseEquipmentSlotValidator::CreateTypeCompatibilityMatrix();

// ==============================================
// ctor
// ==============================================
USuspenseEquipmentSlotValidator::USuspenseEquipmentSlotValidator()
{
	InitializeDefaultRules();
}

// ==============================================
// ISuspenseSlotValidator
// ==============================================

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::CanPlaceItemInSlot(
    const FEquipmentSlotConfig& SlotConfig,
    const FSuspenseInventoryItemInstance& ItemInstance) const
{
    // ДИАГНОСТИКА: Логируем ВСЕ попытки валидации
    UE_LOG(LogEquipmentValidation, Warning, 
        TEXT("🔴 VALIDATOR CALLED: Item=%s, Slot=%s"), 
        *ItemInstance.ItemID.ToString(), 
        *SlotConfig.SlotTag.ToString());
    
    // Зафиксируем вызов
    ValidationCallCount.fetch_add(1, std::memory_order_relaxed);

    // 1) Кэш под локом
    FString CacheKey;
    {
        FScopeLock L(&CacheLock);
        CacheKey = GenerateCacheKey(ItemInstance, SlotConfig);
        
        // ДИАГНОСТИКА: Логируем ключ кеша
        UE_LOG(LogEquipmentValidation, Warning, 
            TEXT("   Cache Key: %s"), *CacheKey);

        FSuspenseSlotValidationResult Cached;
        if (GetCachedValidation(CacheKey, Cached))
        {
            CacheHitCount.fetch_add(1, std::memory_order_relaxed);
            
            // ДИАГНОСТИКА: Логируем кеш-хит
            UE_LOG(LogEquipmentValidation, Error, 
                TEXT("   🟡 CACHE HIT! Returning cached result: %s"), 
                Cached.bIsValid ? TEXT("PASS") : TEXT("FAIL"));
            
            if (!Cached.bIsValid)
            {
                UE_LOG(LogEquipmentValidation, Error, 
                    TEXT("   ❌ Cached FAIL reason: %s"), 
                    *Cached.ErrorMessage.ToString());
            }
            
            return Cached;
        }
        CacheMissCount.fetch_add(1, std::memory_order_relaxed);
        
        // ДИАГНОСТИКА: Логируем кеш-мисс
        UE_LOG(LogEquipmentValidation, Warning, 
            TEXT("   🟢 CACHE MISS - Will perform real validation"));
    }

    // 2) Без локов — сама валидация
    const FSuspenseSlotValidationResult Result = CanPlaceItemInSlot_NoLock(SlotConfig, ItemInstance);
    
    // ДИАГНОСТИКА: Логируем результат реальной валидации
    UE_LOG(LogEquipmentValidation, Warning, 
        TEXT("   Real validation result: %s"), 
        Result.bIsValid ? TEXT("✅ PASS") : TEXT("❌ FAIL"));
    
    if (!Result.bIsValid)
    {
        UE_LOG(LogEquipmentValidation, Error, 
            TEXT("   FAIL reason: %s"), 
            *Result.ErrorMessage.ToString());
    }

    // 3) Пишем в кэш
    {
        FScopeLock L(&CacheLock);
        CacheValidationResult(CacheKey, Result);
        
        UE_LOG(LogEquipmentValidation, Log, 
            TEXT("   Cached new result: %s"), 
            Result.bIsValid ? TEXT("PASS") : TEXT("FAIL"));
    }
    
    if (!Result.bIsValid)
    {
        FailedValidationCount.fetch_add(1, std::memory_order_relaxed);
    }

    return Result;
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::CanSwapItems(
	const FEquipmentSlotConfig& SlotConfigA,
	const FSuspenseInventoryItemInstance& ItemA,
	const FEquipmentSlotConfig& SlotConfigB,
	const FSuspenseInventoryItemInstance& ItemB) const
{
	ValidationCallCount.fetch_add(1, std::memory_order_relaxed);

	if (ItemA.IsValid())
	{
		FSuspenseSlotValidationResult R = CanPlaceItemInSlot_NoLock(SlotConfigB, ItemA);
		if (!R.bIsValid)
		{
			R.Context.Add(TEXT("SwapDirection"), TEXT("A->B"));
			return R;
		}
	}
	if (ItemB.IsValid())
	{
		FSuspenseSlotValidationResult R = CanPlaceItemInSlot_NoLock(SlotConfigA, ItemB);
		if (!R.bIsValid)
		{
			R.Context.Add(TEXT("SwapDirection"), TEXT("B->A"));
			return R;
		}
	}

	return FSuspenseSlotValidationResult::Success();
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::ValidateSlotConfiguration(
	const FEquipmentSlotConfig& SlotConfig) const
{
	ValidationCallCount.fetch_add(1, std::memory_order_relaxed);

	FSuspenseSlotValidationResult R;
	R.bIsValid = true;

	if (!SlotConfig.IsValid())
	{
		R = FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("Invalid slot configuration")),
			EEquipmentValidationFailure::InvalidSlot,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.InvalidSlotConfig")));
		return R;
	}

	// Без AllowedItemTypes разрешаем всё (см. реализацию FEquipmentSlotConfig::CanEquipItemType)
	if (SlotConfig.AllowedItemTypes.IsEmpty())
	{
		R.Warnings.Add(FText::FromString(TEXT("AllowedItemTypes is empty — falls back to allow all")));
	}

	return R;
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::CheckSlotRequirements(
	const FEquipmentSlotConfig& SlotConfig,
	const FGameplayTagContainer& Requirements) const
{
	ValidationCallCount.fetch_add(1, std::memory_order_relaxed);

	FSuspenseSlotValidationResult R;
	R.bIsValid = true;

	for (const FGameplayTag& Need : Requirements)
	{
		const bool bSlotTagOk = SlotConfig.SlotTag.MatchesTag(Need);
		const bool bAllowedTypesOk = SlotConfig.AllowedItemTypes.HasTag(Need);
		if (!bSlotTagOk && !bAllowedTypesOk)
		{
			R = FSuspenseSlotValidationResult::Failure(
				FText::Format(FText::FromString(TEXT("Slot requirement not met: {0}")), FText::FromString(Need.ToString())),
				EEquipmentValidationFailure::RequirementsNotMet,
				FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.RequirementNotMet")));
			R.Context.Add(TEXT("MissingTag"), Need.ToString());
			return R;
		}
	}

	return R;
}

bool USuspenseEquipmentSlotValidator::IsItemTypeCompatibleWithSlot(
	const FGameplayTag& ItemType,
	EEquipmentSlotType SlotType) const
{
	if (SlotType == EEquipmentSlotType::None)
	{
		return true;
	}
	if (const TArray<FGameplayTag>* Types = TypeCompatibilityMatrix.Find(SlotType))
	{
		for (const FGameplayTag& T : *Types)
		{
			if (ItemType.MatchesTag(T))
			{
				return true;
			}
		}
	}
	return false;
}

// ==============================================
// Extended API
// ==============================================

FSlotValidationResultEx USuspenseEquipmentSlotValidator::CanPlaceItemInSlotEx(
	const FEquipmentSlotConfig& SlotConfig,
	const FSuspenseInventoryItemInstance& ItemInstance) const
{
	const double Start = FPlatformTime::Seconds();

	// Кэш (расширенный)
	FString CacheKey;
	{
		FScopeLock L(&CacheLock);
		CacheKey = GenerateCacheKey(ItemInstance, SlotConfig);

		FSlotValidationResultEx Cached;
		if (GetCachedValidationEx(CacheKey, Cached))
		{
			CacheHitCount.fetch_add(1, std::memory_order_relaxed);
			return Cached;
		}
		CacheMissCount.fetch_add(1, std::memory_order_relaxed);
	}

	// Достаём ограничения слота (копия — без удержания DataLock в валидаторах правил)
	FSlotRestrictionData Restrictions = GetSlotRestrictions(SlotConfig.SlotTag);

	FSlotValidationResultEx Result = ExecuteValidationRulesEx_NoLock(ItemInstance, SlotConfig, &Restrictions);
	Result.ValidationDurationMs = float((FPlatformTime::Seconds() - Start) * 1000.0);

	// Обогащаем диагностикой
	Result.Details.Add(TEXT("ItemID"), ItemInstance.ItemID.ToString());
	Result.Details.Add(TEXT("SlotTag"), SlotConfig.SlotTag.ToString());
	Result.Details.Add(TEXT("SlotType"), UEnum::GetValueAsString(SlotConfig.SlotType));
	Result.Details.Add(TEXT("ValidationTimeMs"), FString::Printf(TEXT("%.3f"), Result.ValidationDurationMs));

	// Обновляем метрики и пишем в кэш
	TotalValidationTimeMs.fetch_add(Result.ValidationDurationMs, std::memory_order_relaxed);
	if (!Result.bIsValid)
	{
		FailedValidationCount.fetch_add(1, std::memory_order_relaxed);
	}
	{
		FScopeLock L(&CacheLock);
		CacheValidationResultEx(CacheKey, Result);
	}

	return Result;
}

FBatchValidationResult USuspenseEquipmentSlotValidator::ValidateBatch(const FBatchValidationRequest& Request) const
{
	const double Start = FPlatformTime::Seconds();
	BatchValidationCount.fetch_add(1, std::memory_order_relaxed);

	FBatchValidationResult Out;
	Out.bAllValid = true;

	if (!Request.DataProvider.GetInterface())
	{
		Out.bAllValid = false;
		Out.SummaryMessage = FText::FromString(TEXT("Invalid DataProvider"));
		return Out;
	}

	// Первая фаза — индивидуальная проверка каждой операции
	Out.OperationResults.Reserve(Request.Operations.Num());
	for (int32 i = 0; i < Request.Operations.Num(); ++i)
	{
		const FTransactionOperation& Op = Request.Operations[i];
		if (!Request.DataProvider->IsValidSlotIndex(Op.SlotIndex))
		{
			FSlotValidationResultEx Fail;
			Fail.bIsValid = false;
			Fail.ErrorMessage = FText::FromString(TEXT("Invalid slot index in operation"));
			Fail.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.InvalidSlotIndex"));
			Fail.ResultCode = GetResultCodeForFailure(EEquipmentValidationFailure::InvalidSlot);
			Out.OperationResults.Add(Fail);
			Out.bAllValid = false;
			continue;
		}

		const FEquipmentSlotConfig SlotCfg = Request.DataProvider->GetSlotConfiguration(Op.SlotIndex);
		FSlotValidationResultEx R = CanPlaceItemInSlotEx(SlotCfg, Op.ItemAfter);
		R.Details.Add(TEXT("OperationIndex"), FString::FromInt(i));
		Out.OperationResults.Add(R);
		if (!R.bIsValid)
		{
			Out.bAllValid = false;
		}
	}

	// Вторая фаза — поиск конфликтов
	Out.ConflictingIndices = FindOperationConflicts(Request.Operations, Request.DataProvider);
	if (!Out.ConflictingIndices.IsEmpty())
	{
		Out.bAllValid = false;
		for (int32 Idx : Out.ConflictingIndices)
		{
			if (Out.OperationResults.IsValidIndex(Idx))
			{
				Out.OperationResults[Idx].bIsValid = false;
				Out.OperationResults[Idx].Warnings.Add(FText::FromString(TEXT("Conflicts with another operation")));
				Out.OperationResults[Idx].ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.Conflict"));
				Out.OperationResults[Idx].ResultCode = GetResultCodeForFailure(EEquipmentValidationFailure::ConflictingItem);
			}
		}
	}

	Out.TotalValidationTimeMs = float((FPlatformTime::Seconds() - Start) * 1000.0);
	TotalValidationTimeMs.fetch_add(Out.TotalValidationTimeMs, std::memory_order_relaxed);

	if (Out.bAllValid)
	{
		Out.SummaryMessage = FText::Format(
			FText::FromString(TEXT("Validated {0} operations successfully")),
			FText::AsNumber(Request.Operations.Num()));
	}
	else
	{
		int32 Failed = 0;
		for (const auto& R : Out.OperationResults) { if (!R.bIsValid) ++Failed; }
		Out.SummaryMessage = FText::Format(
			FText::FromString(TEXT("{0} of {1} operations failed validation")),
			FText::AsNumber(Failed),
			FText::AsNumber(Request.Operations.Num()));
	}

	return Out;
}

bool USuspenseEquipmentSlotValidator::QuickValidateOperations(
	const TArray<FTransactionOperation>& Operations,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	if (!DataProvider.GetInterface())
	{
		return false;
	}

	for (const auto& Op : Operations)
	{
		if (!DataProvider->IsValidSlotIndex(Op.SlotIndex)) return false;

		const FEquipmentSlotConfig SlotCfg = DataProvider->GetSlotConfiguration(Op.SlotIndex);
		const FSuspenseSlotValidationResult R = CanPlaceItemInSlot_NoLock(SlotCfg, Op.ItemAfter);
		if (!R.bIsValid) return false;
	}

	const TArray<int32> Conflicts = FindOperationConflicts(Operations, DataProvider);
	return Conflicts.Num() == 0;
}

TArray<int32> USuspenseEquipmentSlotValidator::FindOperationConflicts(
	const TArray<FTransactionOperation>& Operations,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	TArray<int32> Result;

	// 1) Конфликты индексов слотов (несколько операций на один слот)
	TMap<int32, TArray<int32>> SlotOps;
	for (int32 i = 0; i < Operations.Num(); ++i)
	{
		SlotOps.FindOrAdd(Operations[i].SlotIndex).Add(i);
	}
	for (const auto& P : SlotOps)
	{
		if (P.Value.Num() > 1)
		{
			Result.Append(P.Value);
		}
	}

	// 2) Дублирование одного и того же InstanceID в нескольких слотах
	TMap<FGuid, TArray<int32>> InstanceOps;
	for (int32 i = 0; i < Operations.Num(); ++i)
	{
		if (Operations[i].ItemAfter.IsValid())
		{
			InstanceOps.FindOrAdd(Operations[i].ItemAfter.InstanceID).Add(i);
		}
	}
	for (const auto& P : InstanceOps)
	{
		if (P.Value.Num() > 1)
		{
			for (int32 Idx : P.Value)
			{
				if (!Result.Contains(Idx)) Result.Add(Idx);
			}
		}
	}

	// 3) Совместимость слотов (взаимоисключения/зависимости) — данные берём из провайдера слотов.
	if (DataProvider.GetInterface())
	{
		for (int32 i = 0; i < Operations.Num(); ++i)
		{
			for (int32 j = i + 1; j < Operations.Num(); ++j)
			{
				const int32 A = Operations[i].SlotIndex;
				const int32 B = Operations[j].SlotIndex;

				if (!DataProvider->IsValidSlotIndex(A) || !DataProvider->IsValidSlotIndex(B))
				{
					continue;
				}

				// Если слоты конфликтуют и обе операции ставят предмет — это конфликт
				const bool bConflict = CheckSlotCompatibilityConflicts(A, B, DataProvider);
				if (bConflict && Operations[i].ItemAfter.IsValid() && Operations[j].ItemAfter.IsValid())
				{
					if (!Result.Contains(i)) Result.Add(i);
					if (!Result.Contains(j)) Result.Add(j);
				}
			}
		}
	}

	return Result;
}

// ==============================================
// Business helpers
// ==============================================

TArray<int32> USuspenseEquipmentSlotValidator::FindCompatibleSlots(
	const FGameplayTag& ItemType,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	TArray<int32> Out;
	if (!DataProvider.GetInterface()) return Out;

	const int32 Count = DataProvider->GetSlotCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const FEquipmentSlotConfig SlotCfg = DataProvider->GetSlotConfiguration(i);
		if (SlotCfg.CanEquipItemType(ItemType))
		{
			Out.Add(i);
		}
	}
	return Out;
}

TArray<int32> USuspenseEquipmentSlotValidator::GetSlotsByType(
	EEquipmentSlotType EquipmentType,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	TArray<int32> Out;
	if (!DataProvider.GetInterface()) return Out;

	const int32 Count = DataProvider->GetSlotCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const FEquipmentSlotConfig SlotCfg = DataProvider->GetSlotConfiguration(i);
		if (SlotCfg.SlotType == EquipmentType)
		{
			Out.Add(i);
		}
	}
	return Out;
}

int32 USuspenseEquipmentSlotValidator::GetFirstEmptySlotOfType(
	EEquipmentSlotType EquipmentType,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	if (!DataProvider.GetInterface()) return INDEX_NONE;

	const TArray<int32> Slots = GetSlotsByType(EquipmentType, DataProvider);
	for (int32 Idx : Slots)
	{
		if (!DataProvider->IsSlotOccupied(Idx))
		{
			return Idx;
		}
	}
	return INDEX_NONE;
}

// ==============================================
// Rule management
// ==============================================

bool USuspenseEquipmentSlotValidator::RegisterValidationRule(
	const FGameplayTag& RuleTag, int32 Priority, const FText& ErrorMessage)
{
	FScopeLock L(&RulesLock);

	// Не дублируем
	for (const auto& R : ValidationRules)
	{
		if (R.RuleTag == RuleTag)
		{
			UE_LOG(LogEquipmentValidation, Warning, TEXT("Rule already registered: %s"), *RuleTag.ToString());
			return false;
		}
	}

	FEquipmentValidationRule NR;
	NR.RuleTag = RuleTag;
	NR.Priority = Priority;
	NR.ErrorMessage = ErrorMessage;
	NR.bIsStrict = true;

	ValidationRules.Add(NR);
	ValidationRules.Sort([](const FEquipmentValidationRule& A, const FEquipmentValidationRule& B) { return A.Priority > B.Priority; });

	ClearValidationCache();
	return true;
}

bool USuspenseEquipmentSlotValidator::UnregisterValidationRule(const FGameplayTag& RuleTag)
{
	FScopeLock L(&RulesLock);

	const int32 Removed = ValidationRules.RemoveAll([&](const FEquipmentValidationRule& R) { return R.RuleTag == RuleTag; });
	if (Removed > 0)
	{
		ClearValidationCache();
		return true;
	}
	return false;
}

void USuspenseEquipmentSlotValidator::SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled)
{
	FScopeLock L(&RulesLock);
	if (bEnabled) DisabledRules.Remove(RuleTag);
	else          DisabledRules.Add(RuleTag);
	ClearValidationCache();
}

TArray<FGameplayTag> USuspenseEquipmentSlotValidator::GetRegisteredRules() const
{
	FScopeLock L(&RulesLock);
	TArray<FGameplayTag> Out;
	Out.Reserve(ValidationRules.Num());
	for (const auto& R : ValidationRules) { Out.Add(R.RuleTag); }
	return Out;
}

// ==============================================
// Config & DI
// ==============================================

void USuspenseEquipmentSlotValidator::InitializeDefaultRules()
{
	InitializeBuiltInRules();
}

void USuspenseEquipmentSlotValidator::ClearValidationCache()
{
	UE_LOG(LogEquipmentValidation, Error, 
		TEXT("=== CLEARING VALIDATION CACHE ==="));
    
	FScopeLock L(&CacheLock);
    
	const int32 OldCacheSize = ValidationCache.Num();
	const int32 OldExtendedSize = ExtendedCache.Num();
    
	ValidationCache.Empty();
	ExtendedCache.Empty();
    
	UE_LOG(LogEquipmentValidation, Error, 
		TEXT("Cache cleared: Base(%d) + Extended(%d) entries removed"), 
		OldCacheSize, OldExtendedSize);
}

FString USuspenseEquipmentSlotValidator::GetValidationStatistics() const
{
	const int32 Calls = ValidationCallCount.load();
	const int32 Hits = CacheHitCount.load();
	const int32 Miss = CacheMissCount.load();
	const int32 Fails = FailedValidationCount.load();
	const int32 Batches = BatchValidationCount.load();
	const double TotalMs = TotalValidationTimeMs.load();

	FString Out;
	Out += TEXT("=== SlotValidator Stats ===\n");
	Out += FString::Printf(TEXT("Calls: %d | Hits: %d | Misses: %d | Fails: %d | Batches: %d\n"), Calls, Hits, Miss, Fails, Batches);
	Out += FString::Printf(TEXT("Total ms: %.3f | Avg: %.3f\n"), TotalMs, (Calls > 0 ? TotalMs / Calls : 0.0));
	{
		FScopeLock RL(&RulesLock);
		Out += FString::Printf(TEXT("Rules: %d | Disabled: %d\n"), ValidationRules.Num(), DisabledRules.Num());
	}
	{
		FScopeLock CL(&CacheLock);
		Out += FString::Printf(TEXT("Cache size: %d (ex) + %d (base)\n"), ExtendedCache.Num(), ValidationCache.Num());
	}
	return Out;
}

void USuspenseEquipmentSlotValidator::SetItemDataProvider(TSharedPtr<ISuspenseItemDataProvider> Provider)
{
	FScopeLock DL(&DataLock);
	ItemDataProvider = MoveTemp(Provider);
	ClearValidationCache();
}

void USuspenseEquipmentSlotValidator::SetSlotRestrictions(const FGameplayTag& SlotTag, const FSlotRestrictionData& Restrictions)
{
	FScopeLock DL(&DataLock);
	SlotRestrictionsByTag.FindOrAdd(SlotTag) = MakeShared<FSlotRestrictionData>(Restrictions);
	ClearValidationCache();
}

FSlotRestrictionData USuspenseEquipmentSlotValidator::GetSlotRestrictions(const FGameplayTag& SlotTag) const
{
	FScopeLock DL(&DataLock);
	if (const TSharedPtr<FSlotRestrictionData>* P = SlotRestrictionsByTag.Find(SlotTag))
	{
		if (P && P->IsValid())
		{
			return *(*P).Get();
		}
	}
	return FSlotRestrictionData{};
}

void USuspenseEquipmentSlotValidator::SetSlotCompatibilityMatrix(int32 SlotIndex, const TArray<FSlotCompatibilityEntry>& Entries)
{
	FScopeLock DL(&DataLock);
	SlotCompatibilityMatrix.FindOrAdd(SlotIndex) = MakeShared<TArray<FSlotCompatibilityEntry>>(Entries);
	ClearValidationCache();
}

uint32 USuspenseEquipmentSlotValidator::GetCurrentDataVersion() const
{
	// При отсутствии явного источника версии — используем 0 (кэш инвалидируется только по TTL).
	// Если есть DataProvider с версией — читаем её.
	// Опциональная контрактная точка: ISuspenseEquipmentDataProvider может иметь метод GetDataVersion().
	uint32 Version = 0;
	// Попытка RTTI/рефлексии не делается осознанно. В проекте обычно даём явный метод.
	return Version;
}

// ==============================================
// No-lock core
// ==============================================

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::CanPlaceItemInSlot_NoLock(
	const FEquipmentSlotConfig& SlotConfig,
	const FSuspenseInventoryItemInstance& ItemInstance) const
{
	if (!ItemInstance.IsValid())
	{
		return FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("Invalid item instance")),
			EEquipmentValidationFailure::InvalidSlot,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.InvalidItem")));
	}

	// Снепшот ограничений
	const FSlotRestrictionData Restrictions = GetSlotRestrictions(SlotConfig.SlotTag);

	return ExecuteValidationRules_NoLock(ItemInstance, SlotConfig, &Restrictions);
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::ExecuteValidationRules_NoLock(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig,
	const FSlotRestrictionData* Restrictions) const
{
	FSuspenseSlotValidationResult Out;
	Out.bIsValid = true;

	// Копируем правила под локом, чтобы не держать его на выполнении лямбд
	TArray<FEquipmentValidationRule> RulesCopy;
	TSet<FGameplayTag> DisabledCopy;
	bool bStrictLocal = true;
	{
		FScopeLock RL(&RulesLock);
		RulesCopy = ValidationRules;                 // TArray копируется
		DisabledCopy = DisabledRules;                // TSet копируется
		bStrictLocal = bStrictValidation;
	}

	for (const auto& Rule : RulesCopy)
	{
		if (DisabledCopy.Contains(Rule.RuleTag))
		{
			continue;
		}

		bool bPass = true;
		if (Rule.RuleFunction)
		{
			bPass = Rule.RuleFunction(ItemInstance, SlotConfig, Restrictions);
		}

		if (!bPass)
		{
			if (Rule.bIsStrict || bStrictLocal)
			{
				Out.bIsValid = false;
				Out.ErrorMessage = Rule.ErrorMessage;
				Out.ErrorTag = Rule.RuleTag;
				Out.FailureType = EEquipmentValidationFailure::RequirementsNotMet;
				return Out;
			}
			else
			{
				Out.Warnings.Add(Rule.ErrorMessage);
			}
		}
	}

	return Out;
}

FSlotValidationResultEx USuspenseEquipmentSlotValidator::ExecuteValidationRulesEx_NoLock(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig,
	const FSlotRestrictionData* Restrictions) const
{
	const FSuspenseSlotValidationResult Base = ExecuteValidationRules_NoLock(ItemInstance, SlotConfig, Restrictions);

	FSlotValidationResultEx Ex(Base);
	Ex.ResultCode = GetResultCodeForFailure(Base.FailureType);
	Ex.ReasonTag = Base.ErrorTag;

	// Доп. контекст
	Ex.Details.Add(TEXT("ItemInstanceID"), ItemInstance.InstanceID.ToString());
	Ex.Details.Add(TEXT("SlotTag"), SlotConfig.SlotTag.ToString());
	Ex.Details.Add(TEXT("SlotType"), UEnum::GetValueAsString(SlotConfig.SlotType));

	// Если упали по несовместимости типа — добавим подсказку
	if (Base.FailureType == EEquipmentValidationFailure::IncompatibleType)
	{
		const TArray<FGameplayTag> Compat = GetCompatibleItemTypes(SlotConfig.SlotType);
		FString S;
		for (int32 i = 0; i < Compat.Num(); ++i)
		{
			if (i) S += TEXT(", ");
			S += Compat[i].ToString();
		}
		Ex.Details.Add(TEXT("CompatibleTypes"), S);
	}

	return Ex;
}

// ==============================================
// Built-in rules
// ==============================================

void USuspenseEquipmentSlotValidator::InitializeBuiltInRules()
{
	FScopeLock L(&RulesLock);
	ValidationRules.Empty();
	DisabledRules.Empty();
	bStrictValidation = true;

	// 1) Совместимость типа предмета со слотом
	FEquipmentValidationRule TypeRule;
	TypeRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Validation.Rule.ItemType"));
	TypeRule.Priority = 100;
	TypeRule.ErrorMessage = FText::FromString(TEXT("Item type is not compatible with slot"));
	TypeRule.bIsStrict = true;
	TypeRule.RuleFunction = [this](const FSuspenseInventoryItemInstance& Item, const FEquipmentSlotConfig& Slot, const FSlotRestrictionData*)
	{
		UE_LOG(LogEquipmentValidation, Error, TEXT("    🔵 TypeRule executing..."));
    
		// Достаём тип из unified item data
		FSuspenseUnifiedItemData Data;
		const bool bGotData = GetItemData(Item.ItemID, Data);
    
		UE_LOG(LogEquipmentValidation, Error, TEXT("       GetItemData result: %s"), 
			bGotData ? TEXT("SUCCESS") : TEXT("FAILED"));
    
		if (!bGotData)
		{
			UE_LOG(LogEquipmentValidation, Error, TEXT("       ❌ Cannot get item data for: %s"), 
				*Item.ItemID.ToString());
			return false;
		}
    
		UE_LOG(LogEquipmentValidation, Error, TEXT("       Item Type: %s"), 
			Data.ItemType.IsValid() ? *Data.ItemType.ToString() : TEXT("NONE"));
    
		// Проверяем Allowed/Disallowed в конфиге + матрицу
		const bool bSlotAllows = Slot.CanEquipItemType(Data.ItemType);
		UE_LOG(LogEquipmentValidation, Error, TEXT("       Slot.CanEquipItemType: %s"), 
			bSlotAllows ? TEXT("TRUE") : TEXT("FALSE"));
    
		const bool bMatrixOk = IsItemTypeCompatibleWithSlot(Data.ItemType, Slot.SlotType);
		UE_LOG(LogEquipmentValidation, Error, TEXT("       IsItemTypeCompatibleWithSlot: %s"), 
			bMatrixOk ? TEXT("TRUE") : TEXT("FALSE"));
    
		const bool bFinalResult = bSlotAllows && bMatrixOk;
		UE_LOG(LogEquipmentValidation, Error, TEXT("       🎯 TypeRule FINAL: %s"), 
			bFinalResult ? TEXT("✅ PASS") : TEXT("❌ FAIL"));
    
		return bFinalResult;
	};
	ValidationRules.Add(TypeRule);

	// 2) Уровень/требования (минимально — RequiredLevel свойство айтема)
	FEquipmentValidationRule LevelRule;
	LevelRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Validation.Rule.Level"));
	LevelRule.Priority = 90;
	LevelRule.ErrorMessage = FText::FromString(TEXT("Level requirement not met"));
	LevelRule.bIsStrict = true;
	LevelRule.RuleFunction = [this](const FSuspenseInventoryItemInstance& Item, const FEquipmentSlotConfig&, const FSlotRestrictionData*)
	{
		// Валидация без доступа к персонажу — используем runtime prop, 0 — нет требования
		const float RequiredLevel = Item.GetRuntimeProperty(TEXT("RequiredLevel"), 0.0f);
		if (RequiredLevel <= 0.0f)
		{
			return true;
		}
		// Здесь должен быть источник уровня персонажа; без него правило пропускаем (true)
		// При наличии внешнего сервиса — подменится лямбда/правило.
		return true;
	};
	ValidationRules.Add(LevelRule);

	// 3) Ограничения слота (вес/размер)
	FEquipmentValidationRule WeightRule;
	WeightRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Validation.Rule.Weight"));
	WeightRule.Priority = 80;
	WeightRule.ErrorMessage = FText::FromString(TEXT("Item exceeds slot restrictions"));
	WeightRule.bIsStrict = false; // предупреждение по умолчанию
	WeightRule.RuleFunction = [this](const FSuspenseInventoryItemInstance& Item, const FEquipmentSlotConfig&, const FSlotRestrictionData* R)
	{
		if (!R) return true;

		const float ItemWeight = Item.GetRuntimeProperty(TEXT("Weight"), 0.0f);
		if (R->MaxWeight > 0.0f && ItemWeight > R->MaxWeight)
		{
			return false;
		}

		// Габариты как целочисленные ячейки (если используются)
		const int32 SX = int32(Item.GetRuntimeProperty(TEXT("SizeX"), 0.0f));
		const int32 SY = int32(Item.GetRuntimeProperty(TEXT("SizeY"), 0.0f));
		const int32 SZ = int32(Item.GetRuntimeProperty(TEXT("SizeZ"), 0.0f));
		if ((R->MaxSize.X > 0 && SX > R->MaxSize.X) ||
			(R->MaxSize.Y > 0 && SY > R->MaxSize.Y) ||
			(R->MaxSize.Z > 0 && SZ > R->MaxSize.Z))
		{
			return false;
		}

		return true;
	};
	ValidationRules.Add(WeightRule);

	// 4) Уникальность предмета в группе (по тегу UniqueGroupTag слота)
	FEquipmentValidationRule UniqueRule;
	UniqueRule.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Validation.Rule.Unique"));
	UniqueRule.Priority = 70;
	UniqueRule.ErrorMessage = FText::FromString(TEXT("Unique item constraint violated"));
	UniqueRule.bIsStrict = true;
	UniqueRule.RuleFunction = [this](const FSuspenseInventoryItemInstance& Item, const FEquipmentSlotConfig& Slot, const FSlotRestrictionData* R)
	{
		if (!R || !R->UniqueGroupTag.IsValid())
		{
			return true;
		}

		// Без DataProvider этот дефолтный валидатор не может проверить занятость в других слотах.
		// Предусмотрен расширенный ValidateUniqueItem(...) c провайдером — может быть подменён в рантайме.
		return true;
	};
	ValidationRules.Add(UniqueRule);

	ValidationRules.Sort([](const FEquipmentValidationRule& A, const FEquipmentValidationRule& B) { return A.Priority > B.Priority; });
}

// ==============================================
// Helper impl
// ==============================================

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::ValidateItemType(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig) const
{
	FSuspenseUnifiedItemData Data;
	if (!GetItemData(ItemInstance.ItemID, Data))
	{
		return FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("No item data")),
			EEquipmentValidationFailure::InvalidSlot,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.NoItemData")));
	}

	if (!SlotConfig.CanEquipItemType(Data.ItemType))
	{
		return FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("Item type not allowed by slot config")),
			EEquipmentValidationFailure::IncompatibleType,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.TypeDisallowed")));
	}
	if (!IsItemTypeCompatibleWithSlot(Data.ItemType, SlotConfig.SlotType))
	{
		return FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("Item type not compatible with slot type")),
			EEquipmentValidationFailure::IncompatibleType,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.TypeMatrix")));
	}

	return FSuspenseSlotValidationResult::Success();
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::ValidateItemLevel(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& /*SlotConfig*/) const
{
	const float RequiredLevel = ItemInstance.GetRuntimeProperty(TEXT("RequiredLevel"), 0.0f);
	if (RequiredLevel <= 0.0f)
	{
		return FSuspenseSlotValidationResult::Success();
	}
	// Без источника уровня персонажа считаем правило пройденным.
	return FSuspenseSlotValidationResult::Success();
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::ValidateItemWeight(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& /*SlotConfig*/,
	const FSlotRestrictionData& Restrictions) const
{
	const float ItemWeight = ItemInstance.GetRuntimeProperty(TEXT("Weight"), 0.0f);
	if (Restrictions.MaxWeight > 0.0f && ItemWeight > Restrictions.MaxWeight)
	{
		return FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("Item overweight for slot")),
			EEquipmentValidationFailure::WeightLimit,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.TooHeavy")));
	}

	const int32 SX = int32(ItemInstance.GetRuntimeProperty(TEXT("SizeX"), 0.0f));
	const int32 SY = int32(ItemInstance.GetRuntimeProperty(TEXT("SizeY"), 0.0f));
	const int32 SZ = int32(ItemInstance.GetRuntimeProperty(TEXT("SizeZ"), 0.0f));
	if ((Restrictions.MaxSize.X > 0 && SX > Restrictions.MaxSize.X) ||
		(Restrictions.MaxSize.Y > 0 && SY > Restrictions.MaxSize.Y) ||
		(Restrictions.MaxSize.Z > 0 && SZ > Restrictions.MaxSize.Z))
	{
		return FSuspenseSlotValidationResult::Failure(
			FText::FromString(TEXT("Item size exceeds slot bounds")),
			EEquipmentValidationFailure::RequirementsNotMet,
			FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.TooLarge")));
	}

	return FSuspenseSlotValidationResult::Success();
}

FSuspenseSlotValidationResult USuspenseEquipmentSlotValidator::ValidateUniqueItem(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& /*SlotConfig*/,
	const FSlotRestrictionData* Restrictions,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	if (!Restrictions || !Restrictions->UniqueGroupTag.IsValid() || !DataProvider.GetInterface())
	{
		return FSuspenseSlotValidationResult::Success();
	}

	const int32 Count = DataProvider->GetSlotCount();
	for (int32 i = 0; i < Count; ++i)
	{
		if (!DataProvider->IsSlotOccupied(i))
		{
			continue;
		}

		// было: GetItemInSlot(i)
		const FSuspenseInventoryItemInstance Other = DataProvider->GetSlotItem(i);

		// если вдруг валидируем переустановку того же инстанса — пропускаем
		if (Other.InstanceID.IsValid() && Other.InstanceID == ItemInstance.InstanceID)
		{
			continue;
		}

		FSuspenseUnifiedItemData Data;
		if (GetItemData(Other.ItemID, Data))
		{
			if (Data.ItemType.MatchesTag(Restrictions->UniqueGroupTag))
			{
				return FSuspenseSlotValidationResult::Failure(
					FText::FromString(TEXT("Unique item of the same group already equipped")),
					EEquipmentValidationFailure::UniqueConstraint,
					FGameplayTag::RequestGameplayTag(TEXT("Validation.Error.UniqueGroup")));
			}
		}
	}

	return FSuspenseSlotValidationResult::Success();
}

bool USuspenseEquipmentSlotValidator::GetItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutData) const
{
	UE_LOG(LogEquipmentValidation, Error, TEXT("      GetItemData called for: %s"), *ItemID.ToString());
    
	// Внедрённый провайдер — авторитетный источник
	{
		FScopeLock DL(&DataLock);
        
		UE_LOG(LogEquipmentValidation, Error, TEXT("      ItemDataProvider.IsValid(): %s"), 
			ItemDataProvider.IsValid() ? TEXT("TRUE") : TEXT("FALSE"));
        
		if (ItemDataProvider.IsValid())
		{
			const bool bResult = ItemDataProvider->GetUnifiedItemData(ItemID, OutData);
            
			UE_LOG(LogEquipmentValidation, Error, TEXT("      ItemDataProvider->GetUnifiedItemData result: %s"), 
				bResult ? TEXT("SUCCESS") : TEXT("FAILED"));
            
			if (bResult)
			{
				UE_LOG(LogEquipmentValidation, Error, TEXT("      Retrieved ItemType: %s"), 
					OutData.ItemType.IsValid() ? *OutData.ItemType.ToString() : TEXT("NONE"));
			}
            
			return bResult;
		}
	}

	UE_LOG(LogEquipmentValidation, Error, TEXT("      ❌ No ItemDataProvider available!"));
	return false;
}

bool USuspenseEquipmentSlotValidator::ItemHasTag(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FGameplayTag& RequiredTag) const
{
	FSuspenseUnifiedItemData Data;
	return GetItemData(ItemInstance.ItemID, Data) && Data.ItemType.MatchesTag(RequiredTag);
}

TArray<FGameplayTag> USuspenseEquipmentSlotValidator::GetCompatibleItemTypes(EEquipmentSlotType SlotType) const
{
	if (const TArray<FGameplayTag>* Found = TypeCompatibilityMatrix.Find(SlotType))
	{
		return *Found;
	}
	return {};
}

int32 USuspenseEquipmentSlotValidator::GetResultCodeForFailure(EEquipmentValidationFailure FailureType) const
{
	switch (FailureType)
	{
	case EEquipmentValidationFailure::None:               return 0;
	case EEquipmentValidationFailure::InvalidSlot:        return 1001;
	case EEquipmentValidationFailure::SlotOccupied:       return 1002;
	case EEquipmentValidationFailure::IncompatibleType:   return 2001;
	case EEquipmentValidationFailure::RequirementsNotMet: return 6001;
	case EEquipmentValidationFailure::WeightLimit:        return 4001;
	case EEquipmentValidationFailure::ConflictingItem:    return 7001;
	case EEquipmentValidationFailure::LevelRequirement:   return 3001;
	case EEquipmentValidationFailure::ClassRestriction:   return 3002;
	case EEquipmentValidationFailure::UniqueConstraint:   return 7100;
	case EEquipmentValidationFailure::CooldownActive:     return 8001;
	case EEquipmentValidationFailure::TransactionActive:  return 9001;
	case EEquipmentValidationFailure::NetworkError:       return 9100;
	case EEquipmentValidationFailure::SystemError:        return 9999;
	default:                                              return 9999;
	}
}

bool USuspenseEquipmentSlotValidator::CheckSlotCompatibilityConflicts(
	int32 SlotIndexA,
	int32 SlotIndexB,
	const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider) const
{
	if (!DataProvider.GetInterface()) return false;

	// Читаем матрицу для A и для B под DataLock
	TSharedPtr<TArray<FSlotCompatibilityEntry>> AtoB;
	TSharedPtr<TArray<FSlotCompatibilityEntry>> BtoA;
	{
		FScopeLock DL(&DataLock);
		if (const TSharedPtr<TArray<FSlotCompatibilityEntry>>* P = SlotCompatibilityMatrix.Find(SlotIndexA))
		{
			AtoB = *P;
		}
		if (const TSharedPtr<TArray<FSlotCompatibilityEntry>>* P2 = SlotCompatibilityMatrix.Find(SlotIndexB))
		{
			BtoA = *P2;
		}
	}

	auto CheckDir = [&](const TSharedPtr<TArray<FSlotCompatibilityEntry>>& Dir, int32 From, int32 To) -> bool
	{
		if (!Dir.IsValid()) return false;
		for (const FSlotCompatibilityEntry& E : *Dir.Get())
		{
			if (E.TargetSlotIndex != To) continue;

			if (E.bMutuallyExclusive)
			{
				return true;
			}
			if (E.bRequiresTargetFilled && !DataProvider->IsSlotOccupied(To))
			{
				return true;
			}
		}
		return false;
	};

	return CheckDir(AtoB, SlotIndexA, SlotIndexB) || CheckDir(BtoA, SlotIndexB, SlotIndexA);
}

// ==============================================
// Cache internals
// ==============================================

bool USuspenseEquipmentSlotValidator::GetCachedValidation(
	const FString& CacheKey,
	FSuspenseSlotValidationResult& OutResult) const
{
	const uint32 Version = GetCurrentDataVersion();

	if (const FSlotValidationCacheEntry* Entry = ValidationCache.Find(CacheKey))
	{
		if (!Entry->IsExpired(CacheDuration, Version))
		{
			OutResult = Entry->Result;
			return true;
		}
		else
		{
			ValidationCache.Remove(CacheKey);
		}
	}
	return false;
}

bool USuspenseEquipmentSlotValidator::GetCachedValidationEx(
	const FString& CacheKey,
	FSlotValidationResultEx& OutResult) const
{
	const uint32 Version = GetCurrentDataVersion();

	if (const FSlotValidationExtendedCacheEntry* Entry = ExtendedCache.Find(CacheKey))
	{
		if (!Entry->IsExpired(CacheDuration, Version))
		{
			OutResult = Entry->Result;
			return true;
		}
		else
		{
			ExtendedCache.Remove(CacheKey);
		}
	}
	return false;
}

void USuspenseEquipmentSlotValidator::CacheValidationResult(
	const FString& CacheKey,
	const FSuspenseSlotValidationResult& Result) const
{
	// Простой контроль размера
	if (ValidationCache.Num() >= MaxCacheSize)
	{
		CleanExpiredCacheEntries();
		if (ValidationCache.Num() >= MaxCacheSize / 2)
		{
			ValidationCache.Empty(MaxCacheSize / 4); // агрессивная очистка
		}
	}

	FSlotValidationCacheEntry E;
	E.Result = Result;
	E.Timestamp = FDateTime::Now();
	E.DataVersion = GetCurrentDataVersion();
	ValidationCache.Add(CacheKey, E);
}

void USuspenseEquipmentSlotValidator::CacheValidationResultEx(
	const FString& CacheKey,
	const FSlotValidationResultEx& Result) const
{
	if (ExtendedCache.Num() >= MaxCacheSize)
	{
		CleanExpiredCacheEntries();
		if (ExtendedCache.Num() >= MaxCacheSize / 2)
		{
			ExtendedCache.Empty(MaxCacheSize / 4);
		}
	}

	FSlotValidationExtendedCacheEntry E;
	E.Result = Result;
	E.Timestamp = FDateTime::Now();
	E.DataVersion = GetCurrentDataVersion();
	ExtendedCache.Add(CacheKey, E);
}

FString USuspenseEquipmentSlotValidator::GenerateCacheKey(
	const FSuspenseInventoryItemInstance& Item,
	const FEquipmentSlotConfig& Slot) const
{
	// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Включаем ItemType в ключ кеша
	// чтобы инвалидировать кеш при изменении типа предмета в DataTable
    
	FString ItemTypeStr = TEXT("Unknown");
    
	// Получаем актуальный тип предмета из ItemManager
	FSuspenseUnifiedItemData ItemData;
	if (GetItemData(Item.ItemID, ItemData))
	{
		ItemTypeStr = ItemData.ItemType.IsValid() ? 
			ItemData.ItemType.ToString() : TEXT("None");
	}
    
	// Генерируем хеш для AllowedItemTypes вручную
	uint32 AllowedTypesHash = 0;
	for (const FGameplayTag& Tag : Slot.AllowedItemTypes)
	{
		AllowedTypesHash = HashCombine(AllowedTypesHash, GetTypeHash(Tag));
	}
    
	const uint32 ItemHash = GetTypeHash(Item);
	const uint32 SlotTagHash = GetTypeHash(Slot.SlotTag);
    
	// Новый ключ включает:
	// 1. ItemID (как раньше)
	// 2. АКТУАЛЬНЫЙ ItemType из DataTable (НОВОЕ!)
	// 3. SlotTag (как раньше)
	// 4. Hash разрешенных типов слота (НОВОЕ!)
	return FString::Printf(TEXT("%s|%s|%s|%u|%u|%u"),
		*Item.ItemID.ToString(),
		*ItemTypeStr,                    // ← КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ
		*Slot.SlotTag.ToString(),
		ItemHash,
		SlotTagHash,
		AllowedTypesHash);               // ← ДОПОЛНИТЕЛЬНАЯ ЗАЩИТА
}

void USuspenseEquipmentSlotValidator::CleanExpiredCacheEntries() const
{
	const uint32 Version = GetCurrentDataVersion();

	// base cache
	{
		TArray<FString> ToRemove;
		for (const auto& Pair : ValidationCache)
		{
			if (Pair.Value.IsExpired(CacheDuration, Version))
			{
				ToRemove.Add(Pair.Key);
			}
		}
		for (const FString& K : ToRemove) ValidationCache.Remove(K);
	}

	// extended cache
	{
		TArray<FString> ToRemove;
		for (const auto& Pair : ExtendedCache)
		{
			if (Pair.Value.IsExpired(CacheDuration, Version))
			{
				ToRemove.Add(Pair.Key);
			}
		}
		for (const FString& K : ToRemove) ExtendedCache.Remove(K);
	}
}

// ==============================================
// Static: Type compatibility matrix
// ==============================================

TMap<EEquipmentSlotType, TArray<FGameplayTag>> USuspenseEquipmentSlotValidator::CreateTypeCompatibilityMatrix()
{
    TMap<EEquipmentSlotType, TArray<FGameplayTag>> M;

    // Weapon classes
    M.Add(EEquipmentSlotType::PrimaryWeapon, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Rifle")),    // ← ДОБАВЛЕНО!
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.AR")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.DMR")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.SR")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Sniper")),   // ← ДОБАВЛЕНО для совместимости
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.LMG")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shotgun")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Primary"))   // ← ДОБАВЛЕНО родительский тег
    });
    
    M.Add(EEquipmentSlotType::SecondaryWeapon, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.SMG")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shotgun")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.PDW"))
    });
    
    M.Add(EEquipmentSlotType::Holster, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Pistol")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Revolver"))
    });
    
    M.Add(EEquipmentSlotType::Scabbard, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Melee.Knife"))
    });

    // Head gear
    M.Add(EEquipmentSlotType::Headwear, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Helmet")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Headwear"))
    });
    
    M.Add(EEquipmentSlotType::Earpiece, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Earpiece"))
    });
    
    M.Add(EEquipmentSlotType::Eyewear, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Eyewear"))
    });
    
    M.Add(EEquipmentSlotType::FaceCover, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.FaceCover"))
    });

    // Body gear
    M.Add(EEquipmentSlotType::BodyArmor, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.BodyArmor"))
    });
    
    M.Add(EEquipmentSlotType::TacticalRig, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.TacticalRig"))
    });

    // Storage
    M.Add(EEquipmentSlotType::Backpack, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Backpack"))
    });
    
    M.Add(EEquipmentSlotType::SecureContainer, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.SecureContainer"))
    });

    // Quick slots — широкая категория
    TArray<FGameplayTag> QuickTypes = {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Consumable")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Medical")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Throwable")),
        FGameplayTag::RequestGameplayTag(TEXT("Item.Ammo"))
    };
    M.Add(EEquipmentSlotType::QuickSlot1, QuickTypes);
    M.Add(EEquipmentSlotType::QuickSlot2, QuickTypes);
    M.Add(EEquipmentSlotType::QuickSlot3, QuickTypes);
    M.Add(EEquipmentSlotType::QuickSlot4, QuickTypes);

    // Special
    M.Add(EEquipmentSlotType::Armband, {
        FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Armband"))
    });

    return M;
}
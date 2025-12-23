// SuspenseCoreEquipmentSlotValidator.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Templates/SharedPointer.h"
#include <atomic>

// Единый источник макросов и лог-категорий проекта
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"

#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreSlotValidator.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Components/Transaction/SuspenseCoreEquipmentTransactionProcessor.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"

#include "SuspenseCoreEquipmentSlotValidator.generated.h"

/**
 * Interface for item data provider to abstract item manager access (authoritative source).
 * SRP: Slot validator depends only on this interface for item meta-data.
 */
class ISuspenseCoreItemDataProvider
{
public:
	virtual ~ISuspenseCoreItemDataProvider() = default;
	virtual bool GetUnifiedItemData(const FName& ItemID, struct FSuspenseCoreUnifiedItemData& OutData) const = 0;
};

/** Extended validation result carrying diagnostics for UI/metrics */
USTRUCT(BlueprintType)
struct FSuspenseCoreSlotValidationResultEx : public FSuspenseCoreSlotValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag ReasonTag;

	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> Details;

	UPROPERTY(BlueprintReadOnly)
	FDateTime Timestamp;

	UPROPERTY(BlueprintReadOnly)
	float ValidationDurationMs = 0.0f;

	FSuspenseCoreSlotValidationResultEx()
	{
		Timestamp = FDateTime::Now();
	}

	explicit FSuspenseCoreSlotValidationResultEx(const FSuspenseCoreSlotValidationResult& Base)
		: FSuspenseCoreSlotValidationResult(Base)
	{
		Timestamp = FDateTime::Now();
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("Valid=%s Code=%d Reason=%s Msg=%s"),
			bIsValid ? TEXT("true") : TEXT("false"),
			ResultCode,
			*ReasonTag.ToString(),
			*ErrorMessage.ToString());
	}
};

/** Batch validation request */
USTRUCT(BlueprintType)
struct FSuspenseCoreBatchValidationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TArray<FSuspenseCoreTransactionOperation> Operations;

	UPROPERTY()
	TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;

	UPROPERTY(BlueprintReadWrite)
	int32 ValidationFlags = 0;

	UPROPERTY(BlueprintReadWrite)
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly)
	FDateTime Timestamp;

	/** Static factory method to create request with generated ID */
	static FSuspenseCoreBatchValidationRequest Create()
	{
		FSuspenseCoreBatchValidationRequest Request;
		Request.TransactionId = FGuid::NewGuid();
		Request.Timestamp = FDateTime::Now();
		return Request;
	}
};

/** Batch validation result */
USTRUCT(BlueprintType)
struct FSuspenseCoreBatchValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bAllValid = true;

	UPROPERTY(BlueprintReadOnly)
	TArray<FSuspenseCoreSlotValidationResultEx> OperationResults;

	UPROPERTY(BlueprintReadOnly)
	TArray<int32> ConflictingIndices;

	UPROPERTY(BlueprintReadOnly)
	float TotalValidationTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FText SummaryMessage;
};

/** Backward compatibility aliases */
using FBatchValidationRequest = FSuspenseCoreBatchValidationRequest;
using FBatchValidationResult = FSuspenseCoreBatchValidationResult;

/** Runtime slot restriction snapshot (lightweight copy for read path) */
USTRUCT(BlueprintType)
struct FSuspenseCoreSlotRestrictionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector MaxSize = FIntVector::ZeroValue;

	/** Minimum level requirement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinLevel = 0;

	/** Required tags for items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;

	/** Excluded tags for items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer ExcludedTags;

	/** Optional unique group tag; item group that must be unique across inventory/section */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag UniqueGroupTag;

	/** Slot is locked (cannot modify) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsLocked = false;

	/** Slot is disabled (cannot use) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDisabled = false;
};

/** Backward compatibility alias */
using FSlotRestrictionData = FSuspenseCoreSlotRestrictionData;

/** Slot ↔ Slot compatibility entry (mutual exclusion, dependencies, etc.) */
USTRUCT(BlueprintType)
struct FSuspenseCoreSlotCompatibilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TargetSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMutuallyExclusive = false;

	/** If true, this slot requires target slot to be filled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequiresTargetFilled = false;
};

/** Backward compatibility alias */
using FSlotCompatibilityEntry = FSuspenseCoreSlotCompatibilityEntry;

/** Internal rule descriptor */
struct FEquipmentValidationRule
{
	FGameplayTag RuleTag;
	int32 Priority = 0;
	FText ErrorMessage;
	bool bIsStrict = true;

	// Rule function must be pure/read-only. No external locks inside.
	TFunction<bool(const FSuspenseCoreInventoryItemInstance&, const FEquipmentSlotConfig&, const FSuspenseCoreSlotRestrictionData*)> RuleFunction;
};

/** Cache entries with TTL and DataVersion pin
 *  Переименовано, чтобы не конфликтовать с шаблоном FCacheEntry<T> из FEquipmentCacheManager.h
 */
struct FSlotValidationCacheEntry
{
	FSuspenseCoreSlotValidationResult Result;
	FDateTime Timestamp;
	uint32 DataVersion = 0;

	bool IsExpired(float TtlSeconds, uint32 CurrentVersion) const
	{
		const bool bTtlExpired = (TtlSeconds > 0.0f) && (FDateTime::Now() - Timestamp).GetTotalSeconds() > TtlSeconds;
		const bool bVersionMismatch = CurrentVersion != DataVersion;
		return bTtlExpired || bVersionMismatch;
	}
};

struct FSlotValidationExtendedCacheEntry
{
	FSuspenseCoreSlotValidationResultEx Result;
	FDateTime Timestamp;
	uint32 DataVersion = 0;

	bool IsExpired(float TtlSeconds, uint32 CurrentVersion) const
	{
		const bool bTtlExpired = (TtlSeconds > 0.0f) && (FDateTime::Now() - Timestamp).GetTotalSeconds() > TtlSeconds;
		const bool bVersionMismatch = CurrentVersion != DataVersion;
		return bTtlExpired || bVersionMismatch;
	}
};

UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentSlotValidator : public UObject, public ISuspenseCoreSlotValidator
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentSlotValidator();

	//===============================
	// ISuspenseCoreSlotValidator
	//===============================
	[[nodiscard]] virtual FSuspenseCoreSlotValidationResult CanPlaceItemInSlot(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const override;

	[[nodiscard]] virtual FSuspenseCoreSlotValidationResult CanSwapItems(
		const FEquipmentSlotConfig& SlotConfigA,
		const FSuspenseCoreInventoryItemInstance& ItemA,
		const FEquipmentSlotConfig& SlotConfigB,
		const FSuspenseCoreInventoryItemInstance& ItemB) const override;

	[[nodiscard]] virtual FSuspenseCoreSlotValidationResult ValidateSlotConfiguration(
		const FEquipmentSlotConfig& SlotConfig) const override;

	[[nodiscard]] virtual FSuspenseCoreSlotValidationResult CheckSlotRequirements(
		const FEquipmentSlotConfig& SlotConfig,
		const FGameplayTagContainer& Requirements) const override;

	[[nodiscard]] virtual bool IsItemTypeCompatibleWithSlot(
		const FGameplayTag& ItemType,
		EEquipmentSlotType SlotType) const override;

	//===============================
	// Extended API
	//===============================
	[[nodiscard]] FSuspenseCoreSlotValidationResultEx CanPlaceItemInSlotEx(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const;

	[[nodiscard]] FSuspenseCoreBatchValidationResult ValidateBatch(const FSuspenseCoreBatchValidationRequest& Request) const;

	[[nodiscard]] bool QuickValidateOperations(
		const TArray<FTransactionOperation>& Operations,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	[[nodiscard]] TArray<int32> FindOperationConflicts(
		const TArray<FTransactionOperation>& Operations,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	//===============================
	// ISuspenseCoreSlotValidator - Batch Validation
	//===============================
	[[nodiscard]] virtual FSuspenseCoreSlotBatchResult ValidateBatch(
		const FSuspenseCoreSlotBatchRequest& Request) const override;

	[[nodiscard]] virtual bool QuickValidate(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const override;

	//===============================
	// ISuspenseCoreSlotValidator - Specialized Checks
	//===============================
	[[nodiscard]] virtual FSuspenseCoreSlotValidationResult CheckWeightLimit(
		float ItemWeight,
		float SlotMaxWeight) const override;

	[[nodiscard]] virtual FSuspenseCoreSlotValidationResult CheckLevelRequirement(
		int32 RequiredLevel,
		int32 ActualLevel) const override;

	//===============================
	// ISuspenseCoreSlotValidator - Slot Query
	//===============================
	[[nodiscard]] virtual TArray<EEquipmentSlotType> GetCompatibleSlotTypes(
		const FGameplayTag& ItemType) const override;

	[[nodiscard]] virtual TArray<FGameplayTag> GetCompatibleItemTypes(
		EEquipmentSlotType SlotType) const override;

	//===============================
	// ISuspenseCoreSlotValidator - Restrictions Management
	//===============================
	virtual void SetSlotRestrictions(
		const FGameplayTag& SlotTag,
		const FSuspenseCoreSlotRestrictions& Restrictions) override;

	[[nodiscard]] virtual FSuspenseCoreSlotRestrictions GetSlotRestrictions(
		const FGameplayTag& SlotTag) const override;

	virtual void ClearSlotRestrictions(const FGameplayTag& SlotTag) override;

	//===============================
	// ISuspenseCoreSlotValidator - Cache Management
	//===============================
	virtual void ClearValidationCache() override;
	[[nodiscard]] virtual FString GetCacheStatistics() const override;

	//===============================
	// ISuspenseCoreSlotValidator - Custom Rules
	//===============================
	[[nodiscard]] virtual bool RegisterValidationRule(
		const FGameplayTag& RuleTag,
		int32 Priority,
		const FText& ErrorMessage) override;

	[[nodiscard]] virtual bool UnregisterValidationRule(const FGameplayTag& RuleTag) override;
	virtual void SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) override;
	[[nodiscard]] virtual TArray<FGameplayTag> GetRegisteredRules() const override;

	//===============================
	// ISuspenseCoreSlotValidator - Diagnostics
	//===============================
	[[nodiscard]] virtual FString GetValidationStatistics() const override;
	virtual void ResetStatistics() override;

	//===============================
	// Business helpers
	//===============================
	[[nodiscard]] TArray<int32> FindCompatibleSlots(
		const FGameplayTag& ItemType,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	[[nodiscard]] TArray<int32> GetSlotsByType(
		EEquipmentSlotType EquipmentType,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	[[nodiscard]] int32 GetFirstEmptySlotOfType(
		EEquipmentSlotType EquipmentType,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	//===============================
	// Config & DI
	//===============================
	void InitializeDefaultRules();

	void SetItemDataProvider(TSharedPtr<ISuspenseCoreItemDataProvider> Provider);
	void SetSlotRestrictionsData(const FGameplayTag& SlotTag, const FSuspenseCoreSlotRestrictionData& Restrictions);
	[[nodiscard]] FSuspenseCoreSlotRestrictionData GetSlotRestrictionsData(const FGameplayTag& SlotTag) const;
	void SetSlotCompatibilityMatrix(int32 SlotIndex, const TArray<FSuspenseCoreSlotCompatibilityEntry>& Entries);

	/** Monotonic version for cache keys; from authoritative data source (items/slots). */
	[[nodiscard]] uint32 GetCurrentDataVersion() const;

protected:
	//===============================
	// No-lock core (read-only path)
	//===============================
	FSuspenseCoreSlotValidationResult CanPlaceItemInSlot_NoLock(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const;

	FSuspenseCoreSlotValidationResult ExecuteValidationRules_NoLock(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreSlotRestrictionData* Restrictions) const;

	FSuspenseCoreSlotValidationResultEx ExecuteValidationRulesEx_NoLock(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreSlotRestrictionData* Restrictions) const;

	// Built-in rule set
	void InitializeBuiltInRules();

	// Rule implementations
	FSuspenseCoreSlotValidationResult ValidateItemType(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig) const;
	FSuspenseCoreSlotValidationResult ValidateItemLevel(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig) const;
	FSuspenseCoreSlotValidationResult ValidateItemWeight(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig, const FSuspenseCoreSlotRestrictionData& Restrictions) const;
	FSuspenseCoreSlotValidationResult ValidateUniqueItem(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig, const FSuspenseCoreSlotRestrictionData* Restrictions, const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider = TScriptInterface<ISuspenseCoreEquipmentDataProvider>()) const;

	// Helpers
	bool GetItemData(const FName& ItemID, struct FSuspenseCoreUnifiedItemData& OutData) const;
	bool ItemHasTag(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FGameplayTag& RequiredTag) const;
	TArray<FGameplayTag> GetCompatibleItemTypesInternal(EEquipmentSlotType SlotType) const;
	int32 GetResultCodeForFailure(ESuspenseCoreValidationFailure FailureType) const;
	bool CheckSlotCompatibilityConflicts(int32 SlotIndexA, int32 SlotIndexB, const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	//===============================
	// Cache internals
	//===============================
	bool GetCachedValidation(const FString& CacheKey, FSuspenseCoreSlotValidationResult& OutResult) const;
	bool GetCachedValidationEx(const FString& CacheKey, FSuspenseCoreSlotValidationResultEx& OutResult) const;
	void CacheValidationResult(const FString& CacheKey, const FSuspenseCoreSlotValidationResult& Result) const;
	void CacheValidationResultEx(const FString& CacheKey, const FSuspenseCoreSlotValidationResultEx& Result) const;
	FString GenerateCacheKey(const FSuspenseCoreInventoryItemInstance& Item, const FEquipmentSlotConfig& Slot) const;
	void CleanExpiredCacheEntries() const;

	//===============================
	// Type compatibility matrix (lazy-initialized)
	// Uses GetTypeCompatibilityMatrix() for thread-safe access
	//===============================
	static TMap<EEquipmentSlotType, TArray<FGameplayTag>> CreateTypeCompatibilityMatrix();
	static const TMap<EEquipmentSlotType, TArray<FGameplayTag>>& GetTypeCompatibilityMatrix();

private:
	//---------------- Rules storage (split lock) ----------------
	mutable FCriticalSection RulesLock;
	TArray<FEquipmentValidationRule> ValidationRules;        // Not UPROPERTY: holds TFunction
	UPROPERTY()
	TSet<FGameplayTag> DisabledRules;
	UPROPERTY()
	bool bStrictValidation = true;

	//---------------- Cache (split lock) ----------------
	mutable FCriticalSection CacheLock;
	mutable TMap<FString, FSlotValidationCacheEntry> ValidationCache;
	mutable TMap<FString, FSlotValidationExtendedCacheEntry> ExtendedCache;
	UPROPERTY(EditDefaultsOnly, Category="Performance")
	float CacheDuration = 5.0f; // seconds
	static constexpr int32 MaxCacheSize = 2048;

	//---------------- Data (split lock) ----------------
	mutable FCriticalSection DataLock;
	TMap<FGameplayTag, TSharedPtr<FSuspenseCoreSlotRestrictionData>> SlotRestrictionsByTag;
	TMap<int32, TSharedPtr<TArray<FSuspenseCoreSlotCompatibilityEntry>>> SlotCompatibilityMatrix;
	TSharedPtr<ISuspenseCoreItemDataProvider> ItemDataProvider;

	//---------------- Metrics (atomic counters) ----------------
	mutable std::atomic<int32> ValidationCallCount{0};
	mutable std::atomic<int32> CacheHitCount{0};
	mutable std::atomic<int32> CacheMissCount{0};
	mutable std::atomic<int32> FailedValidationCount{0};
	mutable std::atomic<int32> BatchValidationCount{0};
	mutable std::atomic<double> TotalValidationTimeMs{0.0};
};

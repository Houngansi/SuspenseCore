// SuspenseCoreEquipmentSlotValidator.h
// Copyright MedCom

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Templates/SharedPointer.h"

// Единый источник макросов и лог-категорий проекта
#include "Services/SuspenseCoreEquipmentServiceMacros.h"

#include "Interfaces/Equipment/ISuspenseCoreSlotValidator.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "Components/Transaction/SuspenseCoreEquipmentTransactionProcessor.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Types/Equipment/SuspenseCoreEquipmentTypes.h"

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
struct FSuspenseCoreSlotValidationResultEx : public FSlotValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 ResultCode = 0;

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

	explicit FSuspenseCoreSlotValidationResultEx(const FSlotValidationResult& Base)
		: FSlotValidationResult(Base)
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
	TArray<FTransactionOperation> Operations;

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

/** Runtime slot restriction snapshot (lightweight copy for read path) */
USTRUCT(BlueprintType)
struct FSuspenseCoreSlotRestrictionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector MaxSize = FIntVector::ZeroValue;

	/** Optional unique group tag; item group that must be unique across inventory/section */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag UniqueGroupTag;
};

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
	FSlotValidationResult Result;
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
	virtual FSlotValidationResult CanPlaceItemInSlot(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const override;

	virtual FSlotValidationResult CanSwapItems(
		const FEquipmentSlotConfig& SlotConfigA,
		const FSuspenseCoreInventoryItemInstance& ItemA,
		const FEquipmentSlotConfig& SlotConfigB,
		const FSuspenseCoreInventoryItemInstance& ItemB) const override;

	virtual FSlotValidationResult ValidateSlotConfiguration(
		const FEquipmentSlotConfig& SlotConfig) const override;

	virtual FSlotValidationResult CheckSlotRequirements(
		const FEquipmentSlotConfig& SlotConfig,
		const FGameplayTagContainer& Requirements) const override;

	virtual bool IsItemTypeCompatibleWithSlot(
		const FGameplayTag& ItemType,
		EEquipmentSlotType SlotType) const override;

	//===============================
	// Extended API
	//===============================
	FSuspenseCoreSlotValidationResultEx CanPlaceItemInSlotEx(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const;

	FSuspenseCoreBatchValidationResult ValidateBatch(const FSuspenseCoreBatchValidationRequest& Request) const;

	bool QuickValidateOperations(
		const TArray<FTransactionOperation>& Operations,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	TArray<int32> FindOperationConflicts(
		const TArray<FTransactionOperation>& Operations,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	//===============================
	// Business helpers
	//===============================
	TArray<int32> FindCompatibleSlots(
		const FGameplayTag& ItemType,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	TArray<int32> GetSlotsByType(
		EEquipmentSlotType EquipmentType,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	int32 GetFirstEmptySlotOfType(
		EEquipmentSlotType EquipmentType,
		const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	//===============================
	// Rule management
	//===============================
	bool RegisterValidationRule(const FGameplayTag& RuleTag, int32 Priority, const FText& ErrorMessage);
	bool UnregisterValidationRule(const FGameplayTag& RuleTag);
	void SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled);
	TArray<FGameplayTag> GetRegisteredRules() const;

	//===============================
	// Config & DI
	//===============================
	void InitializeDefaultRules();
	void ClearValidationCache();
	FString GetValidationStatistics() const;

	void SetItemDataProvider(TSharedPtr<ISuspenseCoreItemDataProvider> Provider);
	void SetSlotRestrictions(const FGameplayTag& SlotTag, const FSuspenseCoreSlotRestrictionData& Restrictions);
	FSuspenseCoreSlotRestrictionData GetSlotRestrictions(const FGameplayTag& SlotTag) const;
	void SetSlotCompatibilityMatrix(int32 SlotIndex, const TArray<FSuspenseCoreSlotCompatibilityEntry>& Entries);

	/** Monotonic version for cache keys; from authoritative data source (items/slots). */
	uint32 GetCurrentDataVersion() const;

protected:
	//===============================
	// No-lock core (read-only path)
	//===============================
	FSlotValidationResult CanPlaceItemInSlot_NoLock(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) const;

	FSlotValidationResult ExecuteValidationRules_NoLock(
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
	FSlotValidationResult ValidateItemType(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig) const;
	FSlotValidationResult ValidateItemLevel(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig) const;
	FSlotValidationResult ValidateItemWeight(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig, const FSuspenseCoreSlotRestrictionData& Restrictions) const;
	FSlotValidationResult ValidateUniqueItem(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FEquipmentSlotConfig& SlotConfig, const FSuspenseCoreSlotRestrictionData* Restrictions, const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider = TScriptInterface<ISuspenseCoreEquipmentDataProvider>()) const;

	// Helpers
	bool GetItemData(const FName& ItemID, struct FSuspenseCoreUnifiedItemData& OutData) const;
	bool ItemHasTag(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FGameplayTag& RequiredTag) const;
	TArray<FGameplayTag> GetCompatibleItemTypes(EEquipmentSlotType SlotType) const;
	int32 GetResultCodeForFailure(EEquipmentValidationFailure FailureType) const;
	bool CheckSlotCompatibilityConflicts(int32 SlotIndexA, int32 SlotIndexB, const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider) const;

	//===============================
	// Cache internals
	//===============================
	bool GetCachedValidation(const FString& CacheKey, FSlotValidationResult& OutResult) const;
	bool GetCachedValidationEx(const FString& CacheKey, FSuspenseCoreSlotValidationResultEx& OutResult) const;
	void CacheValidationResult(const FString& CacheKey, const FSlotValidationResult& Result) const;
	void CacheValidationResultEx(const FString& CacheKey, const FSuspenseCoreSlotValidationResultEx& Result) const;
	FString GenerateCacheKey(const FSuspenseCoreInventoryItemInstance& Item, const FEquipmentSlotConfig& Slot) const;
	void CleanExpiredCacheEntries() const;

	//===============================
	// Type compatibility matrix
	//===============================
	static TMap<EEquipmentSlotType, TArray<FGameplayTag>> CreateTypeCompatibilityMatrix();
	static const TMap<EEquipmentSlotType, TArray<FGameplayTag>> TypeCompatibilityMatrix;

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

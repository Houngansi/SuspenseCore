// SuspenseCoreConflictRulesEngine.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreConflictRulesEngine.generated.h"

// Forward declarations
struct FEquipmentSlotSnapshot;

/**
 * Conflict type enumeration
 */
UENUM(BlueprintType)
enum class ESuspenseCoreConflictType : uint8
{
    None                UMETA(DisplayName = "No Conflict"),
    MutualExclusion     UMETA(DisplayName = "Mutually Exclusive"),
    SlotConflict        UMETA(DisplayName = "Slot Conflict"),
    TypeIncompatibility UMETA(DisplayName = "Type Incompatibility"),
    SetInterference     UMETA(DisplayName = "Set Interference"),
    Custom              UMETA(DisplayName = "Custom Conflict")
};

/**
 * Conflict resolution action
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreConflictResolution
{
    GENERATED_BODY()

    /** Type of conflict */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    ESuspenseCoreConflictType ConflictType = ESuspenseCoreConflictType::None;

    /** Items involved in conflict */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    TArray<FSuspenseInventoryItemInstance> ConflictingItems;

    /** Suggested resolution strategy */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    ESuspenseConflictResolution Strategy = ESuspenseConflictResolution::Reject;

    /** Resolution description */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    FText Description;

    /** Can be auto-resolved */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    bool bCanAutoResolve = false;
};

/**
 * Set bonus information
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreSetBonusInfo
{
    GENERATED_BODY()

    /** Set identifier */
    UPROPERTY(BlueprintReadOnly, Category = "Set")
    FGameplayTag SetTag;

    /** Items in set */
    UPROPERTY(BlueprintReadOnly, Category = "Set")
    TArray<FName> SetItems;

    /** Currently equipped from set */
    UPROPERTY(BlueprintReadOnly, Category = "Set")
    TArray<FName> EquippedItems;

    /** Number required for bonus */
    UPROPERTY(BlueprintReadOnly, Category = "Set")
    int32 RequiredCount = 2;

    /** Is bonus active */
    UPROPERTY(BlueprintReadOnly, Category = "Set")
    bool bBonusActive = false;

    /** Bonus description */
    UPROPERTY(BlueprintReadOnly, Category = "Set")
    FText BonusDescription;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreResolutionAction
{
	GENERATED_BODY()

	/** Что сделать (тег-намерение), например Equipment.Operation.Unequip, Resolution.Action.Reject и т.д. */
	UPROPERTY(BlueprintReadOnly, Category="Resolution")
	FGameplayTag ActionTag;

	/** Необязательный предмет, к которому относится действие (для Unequip/Set/и пр.) */
	UPROPERTY(BlueprintReadOnly, Category="Resolution")
	FSuspenseInventoryItemInstance ItemInstance;

	/** Блокирующее ли действие (true = требуется UI/подтверждение) */
	UPROPERTY(BlueprintReadOnly, Category="Resolution")
	bool bBlocking = false;

	/** Причина/пояснение (для UI/логов) */
	UPROPERTY(BlueprintReadOnly, Category="Resolution")
	FText Reason;
};

/**
 * Specialized conflict detection and resolution engine
 *
 * Philosophy: Manages equipment conflicts, incompatibilities, and set bonuses.
 * Detects conflicts between items and provides resolution strategies.
 *
 * Key Principles:
 * - Pure read-only validation (no world access)
 * - Data from unified provider interface only
 * - Proactive conflict detection
 * - Multiple resolution strategies
 * - Set bonus management
 * - Clear conflict reporting
 *
 * Thread Safety: Safe for concurrent reads after initialization
 *
 * ВАЖНО: С версии Block D появилась новая архитектура со специализированными движками.
 * Координатор должен использовать методы *WithSlots для получения корректных результатов
 * проверки конфликтов с правильными индексами слотов.
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreConflictRulesEngine : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreConflictRulesEngine();

    //========================================
    // Initialization
    //========================================

    /**
     * Initialize engine with data provider
     * @param InDataProvider Data provider interface
     * @return True if successfully initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    bool Initialize(TScriptInterface<class ISuspenseCoreEquipmentDataProvider> InDataProvider);

    //========================================
    // Core Conflict Detection
    //========================================

    /**
     * Check for conflicts with new item
     * @param NewItem Item to check
     * @param ExistingItems Currently equipped items
     * @return Rule check result with conflict details
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    FSuspenseRuleCheckResult CheckItemConflicts(
        const FSuspenseInventoryItemInstance& NewItem,
        const TArray<FSuspenseInventoryItemInstance>& ExistingItems) const;

    /**
     * Check for slot-specific conflicts - ОБНОВЛЕННАЯ СИГНАТУРА
     *
     * КРИТИЧЕСКИ ВАЖНОЕ ИЗМЕНЕНИЕ: теперь принимает реальные снапшоты слотов
     * вместо самодельной карты TMap<int32, FSuspenseInventoryItemInstance>.
     *
     * Преимущества новой сигнатуры:
     * - Корректные индексы слотов (не позиции в массиве)
     * - Доступ к конфигурации слотов и их тегам
     * - Семантическая проверка совместимости (например, "Hand.Main" vs "Hand.Off")
     * - Устранение ложных срабатываний при проверке двуручного оружия
     *
     * @param NewItem Item to equip
     * @param TargetSlot Target slot index (реальный индекс, не позиция в массиве)
     * @param Slots Real slot snapshots from coordinator with configurations
     * @return Rule check result
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    FSuspenseRuleCheckResult CheckSlotConflicts(
        const FSuspenseInventoryItemInstance& NewItem,
        int32 TargetSlot,
        const TArray<FEquipmentSlotSnapshot>& Slots) const;  // ИЗМЕНЕНО: новый тип параметра

    /**
     * Comprehensive conflict evaluation - СТАРАЯ ВЕРСИЯ
     *
     * ВНИМАНИЕ: эта перегрузка НЕ выполняет слотные проверки из-за проблем
     * с неправильной индексацией. Координатор должен использовать
     * EvaluateConflictRulesWithSlots для получения корректных результатов.
     *
     * @param Context Rule evaluation context
     * @return Aggregated conflict results (БЕЗ слотных проверок)
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    FSuspenseAggregatedRuleResult EvaluateConflictRules(
        const FSuspenseRuleContext& Context) const;

    /**
     * Comprehensive conflict evaluation - НОВАЯ ВЕРСИЯ с корректными слотами
     *
     * Эта перегрузка должна использоваться координатором для получения
     * полной и корректной оценки конфликтов, включая слотные проверки.
     *
     * Отличия от старой версии:
     * - Использует реальные снапшоты слотов для проверки конфликтов
     * - Корректно обрабатывает двуручные предметы и щиты
     * - Семантическая проверка по тегам слотов, а не по индексам массива
     * - Устраняет ложные срабатывания "Primary vs Primary"
     *
     * @param Context Rule evaluation context
     * @param Slots Real slot snapshots with proper indices and configurations
     * @return Aggregated conflict results (включая корректные слотные проверки)
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    FSuspenseAggregatedRuleResult EvaluateConflictRulesWithSlots(
        const FSuspenseRuleContext& Context,
        const TArray<FEquipmentSlotSnapshot>& Slots) const;  // НОВЫЙ МЕТОД

    //========================================
    // Conflict Analysis
    //========================================

    /**
     * Find all conflicts for an item
     * @param Item Item to analyze
     * @param CurrentItems Currently equipped items
     * @return List of all conflicts
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    TArray<FSuspenseCoreConflictResolution> FindAllConflicts(
        const FSuspenseInventoryItemInstance& Item,
        const TArray<FSuspenseInventoryItemInstance>& CurrentItems) const;

    /**
     * Predict conflicts for planned loadout
     * @param PlannedItems Items planned to equip
     * @return Potential conflicts
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    TArray<FSuspenseCoreConflictResolution> PredictConflicts(
        const TArray<FSuspenseInventoryItemInstance>& PlannedItems) const;

    /**
     * Get conflict type between items
     * @param Item1 First item
     * @param Item2 Second item
     * @return Type of conflict
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    ESuspenseCoreConflictType GetConflictType(
        const FSuspenseInventoryItemInstance& Item1,
        const FSuspenseInventoryItemInstance& Item2) const;

    //========================================
    // Compatibility Checks
    //========================================

    /**
     * Check if two items are compatible
     * @param Item1 First item
     * @param Item2 Second item
     * @return True if compatible
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    bool AreItemsCompatible(
        const FSuspenseInventoryItemInstance& Item1,
        const FSuspenseInventoryItemInstance& Item2) const;

    /**
     * Calculate compatibility score
     * @param Item Item to check
     * @param ExistingItems Current equipment
     * @return Compatibility score (0.0 - 1.0)
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    float CalculateCompatibilityScore(
        const FSuspenseInventoryItemInstance& Item,
        const TArray<FSuspenseInventoryItemInstance>& ExistingItems) const;

    /**
     * Check type exclusivity rules
     * @param NewItemType New item's type
     * @param ExistingTypes Types of existing items
     * @return Rule check result
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    FSuspenseRuleCheckResult CheckTypeExclusivity(
        const FGameplayTag& NewItemType,
        const TArray<FGameplayTag>& ExistingTypes) const;

    //========================================
    // Set Bonus Management
    //========================================

    /**
     * Detect active set bonuses
     * @param Items Currently equipped items
     * @return Active set bonuses
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    TArray<FSuspenseCoreSetBonusInfo> DetectSetBonuses(
        const TArray<FSuspenseInventoryItemInstance>& Items) const;

    /**
     * Check if removing item breaks set bonus
     * @param ItemToRemove Item being removed
     * @param CurrentItems Current equipment
     * @return True if breaks active bonus
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    bool WouldBreakSetBonus(
        const FSuspenseInventoryItemInstance& ItemToRemove,
        const TArray<FSuspenseInventoryItemInstance>& CurrentItems) const;

    /**
     * Get items needed to complete set
     * @param SetTag Set identifier
     * @param CurrentItems Current equipment
     * @return Missing items for set completion
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    TArray<FName> GetMissingSetItems(
        const FGameplayTag& SetTag,
        const TArray<FSuspenseInventoryItemInstance>& CurrentItems) const;

    //========================================
    // Conflict Resolution
    //========================================

    /**
     * Resolve conflicts automatically
     * @param Conflicts Conflicts to resolve
     * @param Strategy Resolution strategy
     * @param OutActions Output operations to perform
     * @return True if resolved successfully
     */
	UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
	bool SuggestResolutions(
		const TArray<FSuspenseCoreConflictResolution>& Conflicts,
		ESuspenseConflictResolution Strategy,
		TArray<FSuspenseCoreResolutionAction>& OutActions) const;

    /**
     * Suggest best resolution strategy
     * @param Conflicts Conflicts to analyze
     * @return Recommended strategy
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    ESuspenseConflictResolution SuggestResolutionStrategy(
        const TArray<FSuspenseCoreConflictResolution>& Conflicts) const;

    /**
     * Get user-friendly conflict description
     * @param Conflict Conflict to describe
     * @return Human-readable description
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    FText GetConflictDescription(const FSuspenseCoreConflictResolution& Conflict) const;

    //========================================
    // Configuration
    //========================================

    /**
     * Register mutually exclusive types
     * @param Type1 First type
     * @param Type2 Second type
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    void RegisterMutualExclusion(
        const FGameplayTag& Type1,
        const FGameplayTag& Type2);

    /**
     * Register required companion items
     * @param ItemTag Item requiring companion
     * @param CompanionTags Required companions
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    void RegisterRequiredCompanions(
        const FGameplayTag& ItemTag,
        const TArray<FGameplayTag>& CompanionTags);

    /**
     * Register item set
     * @param SetTag Set identifier
     * @param SetItems Items in set
     * @param RequiredCount Number for bonus
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    void RegisterItemSet(
        const FGameplayTag& SetTag,
        const TArray<FName>& SetItems,
        int32 RequiredCount = 2);

    /**
     * Clear all conflict rules
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    void ClearAllRules();

    //========================================
    // Cache Management
    //========================================

    /**
     * Clear internal caches
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    void ClearCache();

    /**
     * Reset statistics
     */
    UFUNCTION(BlueprintCallable, Category = "Conflict Rules")
    void ResetStatistics();

protected:
    /**
     * Check mutual exclusion between types
     * @param Type1 First type
     * @param Type2 Second type
     * @return True if mutually exclusive
     */
    bool CheckMutualExclusion(
        const FGameplayTag& Type1,
        const FGameplayTag& Type2) const;

    /**
     * Check if item has required companions
     * @param Item Item to check
     * @param CurrentItems Current equipment
     * @return True if companions present
     */
    bool CheckRequiredCompanions(
        const FSuspenseInventoryItemInstance& Item,
        const TArray<FSuspenseInventoryItemInstance>& CurrentItems) const;

    /**
     * Get item type from unified data
     * @param Item Item instance
     * @return Item type tag
     */
    FGameplayTag GetItemType(const FSuspenseInventoryItemInstance& Item) const;

    /**
     * Get armor class from unified data
     * @param ItemData Item data
     * @return Armor class tag
     */
    FGameplayTag GetArmorClass(const FSuspenseUnifiedItemData& ItemData) const;

    /**
     * Get conflict type string representation
     * @param ConflictType Type to convert
     * @return String representation
     */
    FString GetConflictTypeString(ESuspenseCoreConflictType ConflictType) const;

    /**
     * Get item data from provider (replaces world access)
     * @param ItemID Item identifier
     * @param OutData Output data structure
     * @return True if data retrieved successfully
     */
    bool GetItemData(FName ItemID, FSuspenseUnifiedItemData& OutData) const;

private:
    /** Data provider interface - single source of truth */
    UPROPERTY()
    TScriptInterface<class ISuspenseCoreEquipmentDataProvider> DataProvider;

    /** Mutually exclusive type pairs */
    TMap<FGameplayTag, TSet<FGameplayTag>> MutuallyExclusiveTypes;

    /** Required companion items */
    TMap<FGameplayTag, TArray<FGameplayTag>> RequiredCompanions;

    /** Item set definitions */
    TMap<FGameplayTag, TArray<FName>> ItemSets;

    /** Set bonus requirements */
    TMap<FGameplayTag, int32> SetBonusRequirements;

    /** Thread safety for rule modifications */
    mutable FCriticalSection RulesLock;

    /** Initialization flag */
    UPROPERTY()
    bool bIsInitialized = false;

    /**
     * Initialize default conflict rules
     */
    void InitializeDefaultRules();
};

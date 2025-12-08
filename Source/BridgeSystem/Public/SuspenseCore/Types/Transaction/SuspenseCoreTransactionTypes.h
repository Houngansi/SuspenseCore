// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_TYPES_TRANSACTION_SUSPENSECORETRANSACTIONTYPES_H
#define SUSPENSECORE_TYPES_TRANSACTION_SUSPENSECORETRANSACTIONTYPES_H

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "SuspenseTransactionTypes.generated.h"

/**
 * Transaction operation types enumeration
 *
 * Определяет возможные типы операций в транзакционной системе экипировки.
 * Каждый тип операции имеет собственную логику применения и отката.
 */
UENUM(BlueprintType)
enum class ETransactionOperationType : uint8
{
    /** Неопределенная операция */
    None            UMETA(DisplayName = "None"),

    /** Экипировка предмета в слот */
    Equip           UMETA(DisplayName = "Equip Item"),

    /** Снятие предмета со слота */
    Unequip         UMETA(DisplayName = "Unequip Item"),

    /** Перестановка предметов между слотами */
    Swap            UMETA(DisplayName = "Swap Items"),

    /** Перемещение предмета в другой слот */
    Move            UMETA(DisplayName = "Move Item"),

    /** Изменение свойств предмета */
    Modify          UMETA(DisplayName = "Modify Item"),

    /** Разделение стакируемого предмета */
    Split           UMETA(DisplayName = "Split Stack"),

    /** Объединение стакируемых предметов */
    Merge           UMETA(DisplayName = "Merge Stacks"),

    /** Ремонт предмета */
    Repair          UMETA(DisplayName = "Repair Item"),

    /** Улучшение предмета */
    Upgrade         UMETA(DisplayName = "Upgrade Item"),

    /** Кастомная операция */
    Custom          UMETA(DisplayName = "Custom Operation")
};

/**
 * Transaction operation priority levels
 *
 * Определяет приоритет операций при разрешении конфликтов.
 * Операции с высшим приоритетом выполняются первыми.
 */
UENUM(BlueprintType)
enum class ETransactionPriority : uint8
{
    /** Низкий приоритет - неважные операции */
    Low         UMETA(DisplayName = "Low Priority"),

    /** Нормальный приоритет - обычные операции */
    Normal      UMETA(DisplayName = "Normal Priority"),

    /** Высокий приоритет - важные операции */
    High        UMETA(DisplayName = "High Priority"),

    /** Критический приоритет - системные операции */
    Critical    UMETA(DisplayName = "Critical Priority"),

    /** Аварийный приоритет - операции восстановления */
    Emergency   UMETA(DisplayName = "Emergency Priority")
};

/**
 * Equipment change delta structure
 *
 * Представляет атомарное изменение в состоянии экипировки.
 * Используется как для внутреннего отслеживания изменений в DataStore,
 * так и для передачи дельт между транзакционной системой и репликацией.
 *
 * Philosophy: Unified delta representation для всех компонентов системы экипировки.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentDelta
{
    GENERATED_BODY()

    /** Тип изменения - геймплейный тег для расширяемости */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FGameplayTag ChangeType;

    /** Индекс затронутого слота (-1 для глобальных изменений) */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    int32 SlotIndex = INDEX_NONE;

    /** Состояние предмета до изменения */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FSuspenseInventoryItemInstance ItemBefore;

    /** Состояние предмета после изменения */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FSuspenseInventoryItemInstance ItemAfter;

    /** Причина изменения - тег для категоризации */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FGameplayTag ReasonTag;

    /** ID исходной транзакции (если изменение из транзакции) */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FGuid SourceTransactionId;

    /** ID операции внутри транзакции */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FGuid OperationId;

    /** Временная метка изменения */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    FDateTime Timestamp;

    /** Дополнительные метаданные */
    UPROPERTY(BlueprintReadOnly, Category = "Delta")
    TMap<FString, FString> Metadata;

    /** Static factory method to create delta with generated ID */
    static FEquipmentDelta Create()
    {
        FEquipmentDelta Delta;
        Delta.OperationId = FGuid::NewGuid();
        Delta.Timestamp = FDateTime::Now();
        return Delta;
    }

    /** Static factory method to create delta with specific ID */
    static FEquipmentDelta CreateWithID(const FGuid& InOperationId)
    {
        FEquipmentDelta Delta;
        Delta.OperationId = InOperationId;
        Delta.Timestamp = FDateTime::Now();
        return Delta;
    }

    /** Проверка, является ли дельта значимым изменением */
    bool IsValid() const
    {
        return ChangeType.IsValid() && (ItemBefore != ItemAfter || SlotIndex == INDEX_NONE);
    }

    /** Получить человекочитаемое описание дельты */
    FString ToString() const
    {
        return FString::Printf(TEXT("Delta[%s]: Slot %d, %s -> %s, Reason: %s"),
            *ChangeType.ToString(),
            SlotIndex,
            ItemBefore.IsValid() ? *ItemBefore.ItemID.ToString() : TEXT("Empty"),
            ItemAfter.IsValid() ? *ItemAfter.ItemID.ToString() : TEXT("Empty"),
            *ReasonTag.ToString());
    }
};

/** Delegate for equipment delta notifications */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentDelta, const FEquipmentDelta&);

/**
 * Transaction operation record
 *
 * Детализированная запись операции в транзакционной системе.
 * Содержит всю информацию, необходимую для выполнения и отката операции.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FTransactionOperation
{
    GENERATED_BODY()

    /** Уникальный идентификатор операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FGuid OperationId;

    /** Тип операции (тег геймплея для расширяемости) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FGameplayTag OperationType;

    /** Приоритет выполнения операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    ETransactionPriority Priority = ETransactionPriority::Normal;

    /** Индекс слота, на который воздействует операция */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    int32 SlotIndex = INDEX_NONE;

    /** Дополнительный индекс слота (для операций перестановки) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    int32 SecondarySlotIndex = INDEX_NONE;

    /** Состояние предмета до выполнения операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FSuspenseInventoryItemInstance ItemBefore;

    /** Состояние предмета после выполнения операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FSuspenseInventoryItemInstance ItemAfter;

    /** Дополнительное состояние предмета (для операций swap/move) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FSuspenseInventoryItemInstance SecondaryItemBefore;

    /** Дополнительное состояние предмета после операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FSuspenseInventoryItemInstance SecondaryItemAfter;

    /** Временная метка создания операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    float Timestamp = 0.0f;

    /** Временная метка выполнения операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    float ExecutionTimestamp = 0.0f;

    /** Дополнительные метаданные операции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    TMap<FString, FString> Metadata;

    /** Может ли операция быть отменена */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    bool bReversible = true;

    /** Выполнена ли операция */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    bool bExecuted = false;

    /** Требует ли операция валидации перед выполнением */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    bool bRequiresValidation = true;

    /** Генерирует ли операция события для репликации */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    bool bGeneratesEvents = true;

    /** Static factory method to create operation with generated ID */
    static FTransactionOperation Create()
    {
        FTransactionOperation Operation;
        Operation.OperationId = FGuid::NewGuid();
        Operation.Timestamp = FPlatformTime::Seconds();
        return Operation;
    }

    /** Static factory method for simple operations */
    static FTransactionOperation Create(
        const FGameplayTag& InOperationType,
        int32 InSlotIndex,
        const FSuspenseInventoryItemInstance& InItemBefore,
        const FSuspenseInventoryItemInstance& InItemAfter)
    {
        FTransactionOperation Operation;
        Operation.OperationId = FGuid::NewGuid();
        Operation.OperationType = InOperationType;
        Operation.SlotIndex = InSlotIndex;
        Operation.ItemBefore = InItemBefore;
        Operation.ItemAfter = InItemAfter;
        Operation.Timestamp = FPlatformTime::Seconds();
        return Operation;
    }

    /** Проверка валидности операции */
    bool IsValid() const
    {
        return OperationId.IsValid() &&
               OperationType.IsValid() &&
               SlotIndex != INDEX_NONE;
    }

    /** Получить читаемое описание операции */
    FString GetDescription() const
    {
        return FString::Printf(TEXT("%s on slot %d"),
            *OperationType.ToString(),
            SlotIndex);
    }

    /** Операторы сравнения для сортировки по приоритету и времени */
    bool operator<(const FTransactionOperation& Other) const
    {
        if (Priority != Other.Priority)
        {
            return Priority > Other.Priority; // Высший приоритет первым
        }
        return Timestamp < Other.Timestamp; // Ранние операции первыми
    }

    bool operator==(const FTransactionOperation& Other) const
    {
        return OperationId == Other.OperationId;
    }

    /** Хеширование для использования в контейнерах */
    friend uint32 GetTypeHash(const FTransactionOperation& Operation)
    {
        return GetTypeHash(Operation.OperationId);
    }
};

/**
 * Transaction conflict information
 *
 * Описывает конфликт между транзакциями для системы разрешения конфликтов.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FTransactionConflict
{
    GENERATED_BODY()

    /** Идентификатор конфликта */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FGuid ConflictId;

    /** Первая конфликтующая операция */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FTransactionOperation FirstOperation;

    /** Вторая конфликтующая операция */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FTransactionOperation SecondOperation;

    /** Тип конфликта */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FGameplayTag ConflictType;

    /** Описание конфликта */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FText Description;

    /** Рекомендуемое разрешение */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    FGameplayTag RecommendedResolution;

    /** Временная метка обнаружения конфликта */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conflict")
    float DetectionTimestamp = 0.0f;

    /** Static factory method to create conflict with generated ID */
    static FTransactionConflict Create()
    {
        FTransactionConflict Conflict;
        Conflict.ConflictId = FGuid::NewGuid();
        Conflict.DetectionTimestamp = FPlatformTime::Seconds();
        return Conflict;
    }
};

/**
 * Transaction performance metrics
 *
 * Метрики производительности для мониторинга транзакционной системы.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FTransactionMetrics
{
    GENERATED_BODY()

    /** Время начала измерения */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metrics")
    float StartTime = 0.0f;

    /** Время завершения */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metrics")
    float EndTime = 0.0f;

    /** Количество операций в транзакции */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metrics")
    int32 OperationCount = 0;

    /** Размер транзакции в байтах */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metrics")
    int32 TransactionSize = 0;

    /** Количество конфликтов */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metrics")
    int32 ConflictCount = 0;

    /** Количество повторных попыток */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metrics")
    int32 RetryCount = 0;

    /** Получить длительность транзакции */
    float GetDuration() const
    {
        return EndTime > StartTime ? EndTime - StartTime : 0.0f;
    }

    /** Получить операций в секунду */
    float GetOperationsPerSecond() const
    {
        float Duration = GetDuration();
        return Duration > 0.0f ? OperationCount / Duration : 0.0f;
    }
};


#endif // SUSPENSECORE_TYPES_TRANSACTION_SUSPENSECORETRANSACTIONTYPES_H
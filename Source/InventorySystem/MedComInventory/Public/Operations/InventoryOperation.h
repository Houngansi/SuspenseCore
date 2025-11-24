// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/Inventory/InventoryTypes.h"
#include "InventoryOperation.generated.h"

class UMedComInventoryComponent;

/**
 * Перечисление типов операций с инвентарем.
 * Используется для идентификации и фильтрации операций в истории.
 */
UENUM(BlueprintType)
enum class EInventoryOperationType : uint8
{
    None        UMETA(DisplayName = "None"),
    Rotate      UMETA(DisplayName = "Rotate"),
    Move        UMETA(DisplayName = "Move"),
    Stack       UMETA(DisplayName = "Stack"),
    Split       UMETA(DisplayName = "Split"),
    Add         UMETA(DisplayName = "Add"),
    Remove      UMETA(DisplayName = "Remove"),
    Swap        UMETA(DisplayName = "Swap"),
    Equip       UMETA(DisplayName = "Equip"),
    Unequip     UMETA(DisplayName = "Unequip"),
    Use         UMETA(DisplayName = "Use"),
    Custom      UMETA(DisplayName = "Custom")
};

/**
 * Базовая структура для отслеживания операций с инвентарем.
 * Используется для реализации отмены и повтора операций, а также для логирования.
 */
USTRUCT()
struct MEDCOMINVENTORY_API FInventoryOperation
{
    GENERATED_BODY()
    
    /** Тип операции */
    EInventoryOperationType OperationType = EInventoryOperationType::None;
    
    /** Флаг успешного выполнения операции */
    bool bSuccess = false;
    
    /** Код ошибки при неудаче */
    EInventoryErrorCode ErrorCode = EInventoryErrorCode::Success;
    
    /** Указатель на компонент инвентаря */
    UMedComInventoryComponent* InventoryComponent = nullptr;
    
    /** Конструктор по умолчанию */
    FInventoryOperation() = default;
    
    /** 
     * Конструктор с указанием типа и компонента
     * @param InOperationType Тип операции
     * @param InInventoryComponent Компонент инвентаря
     */
    FInventoryOperation(EInventoryOperationType InOperationType, UMedComInventoryComponent* InInventoryComponent)
        : OperationType(InOperationType)
        , bSuccess(false)
        , ErrorCode(EInventoryErrorCode::Success)
        , InventoryComponent(InInventoryComponent)
    {
    }
    
    /**
     * Проверяет, может ли операция быть отменена
     * @return true если операцию можно отменить
     */
    virtual bool CanUndo() const
    {
        return bSuccess && InventoryComponent != nullptr;
    }
    
    /**
     * Отменяет операцию
     * @return true если операция успешно отменена
     */
    virtual bool Undo()
    {
        // Базовая реализация не делает ничего
        return false;
    }
    
    /**
     * Проверяет, может ли операция быть повторена
     * @return true если операцию можно повторить
     */
    virtual bool CanRedo() const
    {
        return InventoryComponent != nullptr;
    }
    
    /**
     * Повторяет операцию
     * @return true если операция успешно повторена
     */
    virtual bool Redo()
    {
        // Базовая реализация не делает ничего
        return false;
    }
    
    /**
     * Возвращает информационную строку о состоянии операции
     * @return Описание операции
     */
    virtual FString ToString() const
    {
        return FString::Printf(TEXT("Operation[Type=%d, Success=%s, Error=%d]"),
            (int32)OperationType,
            bSuccess ? TEXT("Yes") : TEXT("No"),
            (int32)ErrorCode);
    }
};
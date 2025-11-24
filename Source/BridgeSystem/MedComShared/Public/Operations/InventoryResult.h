// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/Inventory/InventoryTypes.h"
#include "InventoryResult.generated.h"

/**
 * Структура для хранения результата операции с инвентарем
 * 
 * Используется во всех методах компонентов инвентаря для возврата детальной информации
 * о результате выполненных действий. Обеспечивает единообразную обработку ошибок
 * и упрощает отладку операций с инвентарем.
 * 
 * Архитектурные принципы:
 * - Единый способ возврата результатов операций
 * - Подробная информация об ошибках для UI и отладки  
 * - Поддержка контекста операции для трассировки
 * - Возможность передачи связанных объектов
 */
USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FInventoryOperationResult
{
    GENERATED_BODY()

    /** Флаг успешного выполнения операции */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    bool bSuccess = false;
    
    /** Код ошибки при неудаче (соответствует EInventoryErrorCode) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    EInventoryErrorCode ErrorCode = EInventoryErrorCode::Success;
    
    /** Детальное сообщение об ошибке для UI или логирования */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    FText ErrorMessage;
    
    /** Контекст операции (обычно имя метода или тип операции) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    FName Context = NAME_None;
    
    /** Связанный объект результата операции (опционально) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    UObject* ResultObject = nullptr;
    
    /** Дополнительные данные результата в виде key-value пар */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    TMap<FName, FString> ResultData;
    
    /** Список предметов, затронутых операцией */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Result")
    TArray<FInventoryItemInstance> AffectedItems;
    //==================================================================
    // Конструкторы
    //==================================================================
    
    /** Конструктор по умолчанию создает неуспешный результат */
    FInventoryOperationResult() = default;
    
    /** 
     * Конструктор с полным набором параметров
     * @param InSuccess Успешность операции
     * @param InErrorCode Код ошибки
     * @param InErrorMessage Сообщение об ошибке
     * @param InContext Контекст операции
     * @param InResultObject Связанный объект
     */
    FInventoryOperationResult(bool InSuccess, EInventoryErrorCode InErrorCode, const FText& InErrorMessage, 
                             const FName& InContext, UObject* InResultObject = nullptr)
        : bSuccess(InSuccess)
        , ErrorCode(InErrorCode)
        , ErrorMessage(InErrorMessage)
        , Context(InContext)
        , ResultObject(InResultObject)
    {
    }
    
    //==================================================================
    // Методы проверки состояния
    //==================================================================
    
    /**
     * Проверяет, была ли операция успешной
     * @return true если операция выполнена без ошибок
     */
    bool IsSuccess() const
    {
        return bSuccess && ErrorCode == EInventoryErrorCode::Success;
    }
    
    /**
     * Проверяет, была ли операция неудачной
     * @return true если произошла ошибка
     */
    bool IsFailure() const
    {
        return !IsSuccess();
    }
    
    /**
     * Проверяет, связана ли ошибка с недостатком места
     * @return true если ошибка связана с переполнением инвентаря
     */
    bool IsSpaceError() const
    {
        return ErrorCode == EInventoryErrorCode::NoSpace;
    }
    
    /**
     * Проверяет, связана ли ошибка с превышением веса
     * @return true если ошибка связана с лимитом веса
     */
    bool IsWeightError() const
    {
        return ErrorCode == EInventoryErrorCode::WeightLimit;
    }
    
    //==================================================================
    // Статические методы создания результатов
    //==================================================================
    
    /**
     * Создает результат успешной операции
     * @param InContext Контекст операции
     * @param InResultObject Связанный объект (опционально)
     * @return Структура успешного результата
     */
    static FInventoryOperationResult Success(const FName& InContext, UObject* InResultObject = nullptr)
    {
        return FInventoryOperationResult(
            true,
            EInventoryErrorCode::Success,
            FText::GetEmpty(),
            InContext,
            InResultObject
        );
    }
    
    /**
     * Создает результат неудачной операции
     * @param InErrorCode Код ошибки
     * @param InErrorMessage Сообщение об ошибке
     * @param InContext Контекст операции
     * @param InResultObject Связанный объект (опционально)
     * @return Структура неудачного результата
     */
    static FInventoryOperationResult Failure(EInventoryErrorCode InErrorCode, const FText& InErrorMessage, 
                                           const FName& InContext, UObject* InResultObject = nullptr)
    {
        return FInventoryOperationResult(
            false,
            InErrorCode,
            InErrorMessage,
            InContext,
            InResultObject
        );
    }
    
    /**
     * Создает результат ошибки "нет места"
     * @param InContext Контекст операции
     * @param InErrorMessage Дополнительное сообщение (опционально)
     * @return Результат ошибки недостатка места
     */
    static FInventoryOperationResult NoSpace(const FName& InContext, const FText& InErrorMessage = FText())
    {
        FText Message = InErrorMessage.IsEmpty() 
            ? FText::FromString(TEXT("Not enough space in inventory")) 
            : InErrorMessage;
            
        return Failure(EInventoryErrorCode::NoSpace, Message, InContext);
    }
    
    /**
     * Создает результат ошибки превышения веса
     * @param InContext Контекст операции
     * @param InErrorMessage Дополнительное сообщение (опционально)
     * @return Результат ошибки превышения веса
     */
    static FInventoryOperationResult WeightLimit(const FName& InContext, const FText& InErrorMessage = FText())
    {
        FText Message = InErrorMessage.IsEmpty() 
            ? FText::FromString(TEXT("Weight limit exceeded")) 
            : InErrorMessage;
            
        return Failure(EInventoryErrorCode::WeightLimit, Message, InContext);
    }
    
    /**
     * Создает результат ошибки "предмет не найден"
     * @param InContext Контекст операции
     * @param ItemID ID предмета который не найден
     * @return Результат ошибки отсутствия предмета
     */
    static FInventoryOperationResult ItemNotFound(const FName& InContext, const FName& ItemID = NAME_None)
    {
        FText Message = ItemID.IsNone() 
            ? FText::FromString(TEXT("Item not found")) 
            : FText::Format(FText::FromString(TEXT("Item '{0}' not found")), FText::FromName(ItemID));
            
        return Failure(EInventoryErrorCode::ItemNotFound, Message, InContext);
    }
    
    //==================================================================
    // Вспомогательные методы
    //==================================================================
    
    /**
     * Возвращает строковое представление кода ошибки
     * ИСПРАВЛЕНО: Теперь соответствует актуальному enum EInventoryErrorCode
     * @param InErrorCode Код ошибки
     * @return Строковое представление
     */
    static FString GetErrorCodeString(EInventoryErrorCode InErrorCode)
    {
        switch (InErrorCode)
        {
            case EInventoryErrorCode::Success:
                return TEXT("Success");
            case EInventoryErrorCode::NoSpace:
                return TEXT("NoSpace");
            case EInventoryErrorCode::WeightLimit:
                return TEXT("WeightLimit");
            case EInventoryErrorCode::InvalidItem:
                return TEXT("InvalidItem");
            case EInventoryErrorCode::ItemNotFound:
                return TEXT("ItemNotFound");
            case EInventoryErrorCode::InsufficientQuantity:
                return TEXT("InsufficientQuantity");
            case EInventoryErrorCode::InvalidSlot:
                return TEXT("InvalidSlot");
            case EInventoryErrorCode::SlotOccupied:
                return TEXT("SlotOccupied");
            case EInventoryErrorCode::TransactionActive:
                return TEXT("TransactionActive");
            case EInventoryErrorCode::NotInitialized:
                return TEXT("NotInitialized");
            case EInventoryErrorCode::NetworkError:
                return TEXT("NetworkError");
            case EInventoryErrorCode::UnknownError:
            default:
                return TEXT("UnknownError");
        }
    }
    
    /**
     * Добавить дополнительные данные к результату
     * @param Key Ключ данных
     * @param Value Значение данных
     */
    void AddResultData(const FName& Key, const FString& Value)
    {
        ResultData.Add(Key, Value);
    }
    
    /**
     * Получить дополнительные данные результата
     * @param Key Ключ для поиска
     * @param DefaultValue Значение по умолчанию
     * @return Найденное значение или значение по умолчанию
     */
    FString GetResultData(const FName& Key, const FString& DefaultValue = TEXT("")) const
    {
        const FString* Found = ResultData.Find(Key);
        return Found ? *Found : DefaultValue;
    }
    
    /**
     * Получить полное описание результата для отладки
     * @return Детальная строка с информацией о результате
     */
    FString GetDetailedString() const
    {
        FString Result = FString::Printf(
            TEXT("InventoryResult[%s]: %s (%s)"),
            *Context.ToString(),
            bSuccess ? TEXT("SUCCESS") : TEXT("FAILURE"),
            *GetErrorCodeString(ErrorCode)
        );
        
        if (!ErrorMessage.IsEmpty())
        {
            Result += FString::Printf(TEXT(" - %s"), *ErrorMessage.ToString());
        }
        
        if (ResultObject)
        {
            Result += FString::Printf(TEXT(" [Object: %s]"), *ResultObject->GetName());
        }
        
        if (ResultData.Num() > 0)
        {
            Result += TEXT(" {");
            for (const auto& DataPair : ResultData)
            {
                Result += FString::Printf(TEXT(" %s=%s"), *DataPair.Key.ToString(), *DataPair.Value);
            }
            Result += TEXT(" }");
        }
        
        return Result;
    }
};
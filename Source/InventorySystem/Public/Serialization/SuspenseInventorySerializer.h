// MedComInventory/Serialization/SuspenseInventorySerializer.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventorySerializer.generated.h"

// Forward declarations для чистого разделения модулей
class USuspenseInventoryComponent;
class USuspenseItemManager;
struct FSuspenseUnifiedItemData;

/**
 * Структура для сериализованных runtime данных инвентаря
 * 
 * АРХИТЕКТУРНАЯ КОНЦЕПЦИЯ:
 * В новой системе мы сериализуем ТОЛЬКО runtime состояние предметов.
 * Статические данные всегда получаются из DataTable при загрузке.
 * Это обеспечивает consistency при обновлении предметов в DataTable.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSerializedInventoryData
{
    GENERATED_BODY()
    
    /** Версия формата для backward compatibility и migration */
    UPROPERTY()
    int32 Version = 2; // Увеличено для новой архитектуры
    
    /** Размеры сетки инвентаря */
    UPROPERTY()
    int32 GridWidth = 0;
    
    UPROPERTY()
    int32 GridHeight = 0;
    
    /** Ограничения по весу */
    UPROPERTY()
    float MaxWeight = 0.0f;
    
    /** 
     * Текущий вес НЕ сериализуется - рассчитывается при загрузке
     * на основе загруженных предметов и их данных из DataTable
     */
    
    /** Разрешенные типы предметов для этого инвентаря */
    UPROPERTY()
    FGameplayTagContainer AllowedItemTypes;
    
    /** Запрещенные типы предметов */
    UPROPERTY()
    FGameplayTagContainer DisallowedItemTypes;
    
    /** 
     * КЛЮЧЕВОЕ ИЗМЕНЕНИЕ: Сериализуем runtime экземпляры вместо старых структур
     * Каждый экземпляр содержит ItemID для связи с DataTable + runtime состояние
     */
    UPROPERTY()
    TArray<FSuspenseInventoryItemInstance> ItemInstances;
    
    /** Время сохранения для отслеживания версий */
    UPROPERTY()
    FDateTime SaveTime;
    
    /** Дополнительные метаданные инвентаря */
    UPROPERTY()
    TMap<FName, FString> InventoryMetadata;
    
    /** Конструктор с правильными значениями по умолчанию */
    FSerializedInventoryData()
        : Version(2)
        , GridWidth(0)
        , GridHeight(0)
        , MaxWeight(0.0f)
        , SaveTime(FDateTime::Now())
    {
    }
    
    /** Проверка валидности сериализованных данных */
    bool IsValid() const
    {
        return Version > 0 && GridWidth > 0 && GridHeight > 0 && MaxWeight > 0.0f;
    }
    
    /** Получить общее количество предметов для валидации */
    int32 GetTotalItemCount() const
    {
        int32 TotalCount = 0;
        for (const FSuspenseInventoryItemInstance& Instance : ItemInstances)
        {
            TotalCount += Instance.Quantity;
        }
        return TotalCount;
    }
};

/**
 * Сериализатор инвентаря для новой DataTable архитектуры
 * 
 * ПРИНЦИПЫ РАБОТЫ:
 * 1. Сериализует только runtime данные предметов (FSuspenseInventoryItemInstance)
 * 2. При загрузке валидирует предметы против текущего DataTable
 * 3. Поддерживает migration от старых форматов
 * 4. Обеспечивает thread-safe операции для multiplayer
 * 5. Интегрирован с ItemManager для доступа к DataTable
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseInventorySerializer : public UObject
{
    GENERATED_BODY()
    
public:
    //==================================================================
    // Core Serialization Methods (ОБНОВЛЕНО для новой архитектуры)
    //==================================================================
    
    /**
     * Сериализует инвентарь в runtime структуру данных
     * 
     * Этот метод извлекает только runtime состояние предметов.
     * Статические данные не сохраняются - они всегда берутся из DataTable.
     * 
     * @param InventoryComponent Компонент для сериализации
     * @return Структура с runtime данными инвентаря
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Serialization")
    static FSerializedInventoryData SerializeInventory(USuspenseInventoryComponent* InventoryComponent);
    
    /**
     * Десериализует инвентарь из runtime структуры данных
     * 
     * При загрузке каждый предмет валидируется против текущего DataTable.
     * Если предмет не найден в DataTable, он пропускается с предупреждением.
     * 
     * @param InventoryComponent Компонент для заполнения данными
     * @param SerializedData Данные для загрузки
     * @return true если десериализация прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Serialization")
    static bool DeserializeInventory(USuspenseInventoryComponent* InventoryComponent, 
                                   const FSerializedInventoryData& SerializedData);
    
    //==================================================================
    // JSON Serialization (ОБНОВЛЕНО для производительности)
    //==================================================================
    
    /**
     * Сериализует инвентарь в JSON строку для хранения в файлах/БД
     * 
     * JSON формат обеспечивает human-readable сохранения и легкую отладку.
     * Поддерживает pretty printing для development builds.
     * 
     * @param InventoryComponent Компонент для сериализации
     * @param bPrettyPrint Форматировать JSON для читаемости (только для отладки)
     * @return JSON строка с данными инвентаря
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Serialization")
    static FString SerializeInventoryToJson(USuspenseInventoryComponent* InventoryComponent, 
                                          bool bPrettyPrint = false);
    
    /**
     * Десериализует инвентарь из JSON строки
     * 
     * Включает детальную валидацию JSON структуры и graceful error handling.
     * 
     * @param InventoryComponent Компонент для загрузки
     * @param JsonString JSON данные для парсинга
     * @return true если загрузка прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Serialization")
    static bool DeserializeInventoryFromJson(USuspenseInventoryComponent* InventoryComponent, 
                                           const FString& JsonString);
    
    //==================================================================
    // File Operations (РАСШИРЕНО с error handling)
    //==================================================================
    
    /**
     * Сохраняет инвентарь в файл с проверкой целостности
     * 
     * @param InventoryComponent Компонент для сохранения
     * @param FilePath Путь к файлу сохранения
     * @param bUseJson Использовать JSON формат (true) или binary (false)
     * @param bCreateBackup Создать backup существующего файла
     * @return true если сохранение прошло успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Serialization")
    static bool SaveInventoryToFile(USuspenseInventoryComponent* InventoryComponent, 
                                  const FString& FilePath, 
                                  bool bUseJson = true, 
                                  bool bCreateBackup = true);
    
    /**
     * Загружает инвентарь из файла с автоматическим определением формата
     * 
     * @param InventoryComponent Компонент для загрузки
     * @param FilePath Путь к файлу загрузки
     * @param bValidateAfterLoad Выполнить валидацию после загрузки
     * @return true если загрузка прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Serialization")
    static bool LoadInventoryFromFile(USuspenseInventoryComponent* InventoryComponent, 
                                    const FString& FilePath, 
                                    bool bValidateAfterLoad = true);
    
    //==================================================================
    // Validation and Migration (НОВЫЕ МЕТОДЫ)
    //==================================================================
    
    /**
     * Валидирует сериализованные данные против текущего DataTable
     * 
     * Проверяет что все предметы существуют в DataTable и имеют валидные runtime свойства.
     * 
     * @param SerializedData Данные для валидации
     * @param OutMissingItems Список предметов отсутствующих в DataTable
     * @param OutValidationErrors Список ошибок валидации
     * @return true если все данные валидны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    static bool ValidateSerializedData(const FSerializedInventoryData& SerializedData,
                                     TArray<FName>& OutMissingItems,
                                     TArray<FString>& OutValidationErrors);
    
    /**
     * Мигрирует данные от старой версии к новой
     * 
     * Поддерживает upgrade от версии 1 (старая архитектура) к версии 2 (DataTable).
     * 
     * @param SerializedData Данные для миграции (модифицируется in-place)
     * @return true если миграция прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Migration")
    static bool MigrateSerializedData(FSerializedInventoryData& SerializedData);
    
    /**
     * Очищает недопустимые предметы из сериализованных данных
     * 
     * Удаляет предметы которые не существуют в текущем DataTable.
     * Полезно для migration и cleanup старых save файлов.
     * 
     * @param SerializedData Данные для очистки (модифицируется in-place)
     * @return Количество удаленных предметов
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Maintenance")
    static int32 CleanupInvalidItems(FSerializedInventoryData& SerializedData);
    
    //==================================================================
    // Utility Methods (ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ)
    //==================================================================
    
    /**
     * Получает статистику сериализованного инвентаря
     * 
     * @param SerializedData Данные для анализа
     * @return Human-readable строка со статистикой
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static FString GetInventoryStatistics(const FSerializedInventoryData& SerializedData);
    
    /**
     * Сравнивает два сериализованных инвентаря
     * 
     * Полезно для debugging и тестирования serialization consistency.
     * 
     * @param DataA Первый набор данных
     * @param DataB Второй набор данных
     * @param OutDifferences Список найденных различий
     * @return true если инвентари идентичны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static bool CompareInventoryData(const FSerializedInventoryData& DataA,
                                   const FSerializedInventoryData& DataB,
                                   TArray<FString>& OutDifferences);

private:
    //==================================================================
    // Internal Helper Methods (ВНУТРЕННИЕ МЕТОДЫ)
    //==================================================================
    
    /** Конвертирует структуру в JSON с error handling */
    static FString StructToJson(const FSerializedInventoryData& Data, bool bPrettyPrint = false);
    
    /** Парсит JSON в структуру с детальной валидацией */
    static bool JsonToStruct(const FString& JsonString, FSerializedInventoryData& OutData);
    
    /** Получает ItemManager для валидации предметов */
    static USuspenseItemManager* GetItemManager(const UObject* WorldContext);
    
    /** Валидирует отдельный runtime экземпляр против DataTable */
    static bool ValidateItemInstance(const FSuspenseInventoryItemInstance& Instance, 
                                   USuspenseItemManager* ItemManager,
                                   FString& OutError);
    
    /** Создает backup файла перед перезаписью */
    static bool CreateFileBackup(const FString& OriginalPath);
    
    /** Автоматически определяет формат файла (JSON или binary) */
    static bool DetectFileFormat(const FString& FilePath, bool& bOutIsJson);
    
    /** Мигрирует runtime свойства от старого формата к новому */
    static void MigrateRuntimeProperties(FSuspenseInventoryItemInstance& Instance);
    
    /** Константы для сериализации */
    static constexpr int32 CURRENT_VERSION = 2;
    static constexpr int32 MIN_SUPPORTED_VERSION = 1;
    static const FString BACKUP_EXTENSION;
};
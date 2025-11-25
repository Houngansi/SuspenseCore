// MedComInventory/Debug/SuspenseInventoryDebugger.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "SuspenseInventoryDebugger.generated.h"

// Forward declarations для чистого разделения модулей
class USuspenseInventoryComponent;
class UMedComItemManager;

/**
 * Enhanced performance metrics structure for modern inventory system
 * Включает метрики для новой DataTable архитектуры и runtime instances
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FInventoryPerformanceMetrics
{
    GENERATED_BODY()
    
    //==================================================================
    // Core Operation Metrics (Обновлено для новой архитектуры)
    //==================================================================
    
    /** Количество операций добавления предметов */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Operations")
    int32 AddOperations;
    
    /** Количество операций удаления предметов */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Operations")
    int32 RemoveOperations;
    
    /** Количество операций перемещения предметов */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Operations")
    int32 MoveOperations;
    
    /** Количество операций обмена предметов */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Operations")
    int32 SwapOperations;
    
    /** Количество операций создания runtime экземпляров */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Operations")
    int32 InstanceCreationOperations;
    
    /** Количество операций stacking/splitting */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Operations")
    int32 StackOperations;
    
    //==================================================================
    // Performance Timing Metrics
    //==================================================================
    
    /** Среднее время операции добавления (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Performance")
    float AverageAddTime;
    
    /** Среднее время операции удаления (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Performance")
    float AverageRemoveTime;
    
    /** Среднее время операции перемещения (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Performance")
    float AverageMoveTime;
    
    /** Среднее время операции обмена (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Performance")
    float AverageSwapTime;
    
    /** Среднее время создания runtime экземпляра (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Performance")
    float AverageInstanceCreationTime;
    
    /** Среднее время операций stacking (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Performance")
    float AverageStackTime;
    
    //==================================================================
    // DataTable Access Metrics (Новые метрики)
    //==================================================================
    
    /** Количество обращений к DataTable */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|DataTable")
    int32 DataTableAccesses;
    
    /** Количество cache hits при обращении к данным предметов */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|DataTable")
    int32 DataCacheHits;
    
    /** Количество cache misses при обращении к данным предметов */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|DataTable")
    int32 DataCacheMisses;
    
    /** Среднее время доступа к данным предмета (мс) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|DataTable")
    float AverageDataAccessTime;
    
    //==================================================================
    // Network Metrics (Расширенные)
    //==================================================================
    
    /** Количество отправленных сетевых сообщений */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Network")
    int32 NetworkMessagesSent;
    
    /** Количество полученных сетевых сообщений */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Network")
    int32 NetworkMessagesReceived;
    
    /** Количество отправленных байт */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Network")
    int32 BytesSent;
    
    /** Количество полученных байт */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Network")
    int32 BytesReceived;
    
    /** Количество сетевых ошибок */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Network")
    int32 NetworkErrors;
    
    //==================================================================
    // Memory and Resource Metrics (Новые)
    //==================================================================
    
    /** Количество активных runtime экземпляров */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Memory")
    int32 ActiveInstances;
    
    /** Приблизительное использование памяти (байт) */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Memory")
    int32 EstimatedMemoryUsage;
    
    /** Количество runtime properties across all instances */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Debug|Memory")
    int32 TotalRuntimeProperties;
    
    //==================================================================
    // Constructor with improved initialization
    //==================================================================
    
    FInventoryPerformanceMetrics()
    {
        // Инициализируем все метрики нулевыми значениями
        FMemory::Memzero(this, sizeof(FInventoryPerformanceMetrics));
    }
    
    /**
     * Получить hit rate для DataTable cache
     * @return Процент попаданий в кэш (0.0 - 1.0)
     */
    float GetCacheHitRate() const
    {
        int32 TotalAccesses = DataCacheHits + DataCacheMisses;
        return TotalAccesses > 0 ? (float)DataCacheHits / TotalAccesses : 0.0f;
    }
    
    /**
     * Получить среднее время всех операций
     * @return Средневзвешенное время операций в мс
     */
    float GetAverageOperationTime() const
    {
        int32 TotalOps = AddOperations + RemoveOperations + MoveOperations + SwapOperations;
        if (TotalOps == 0) return 0.0f;
        
        float TotalTime = (AverageAddTime * AddOperations) +
                         (AverageRemoveTime * RemoveOperations) +
                         (AverageMoveTime * MoveOperations) +
                         (AverageSwapTime * SwapOperations);
        
        return TotalTime / TotalOps;
    }
};

/**
 * Advanced debugging tool for modern inventory system
 * 
 * АРХИТЕКТУРНЫЕ УЛУЧШЕНИЯ:
 * Этот обновленный debugger полностью интегрирован с новой DataTable архитектурой:
 * 
 * - Работа с FInventoryItemInstance вместо legacy структур
 * - Мониторинг производительности DataTable доступа и кэширования
 * - Расширенная валидация consistency между grid state и runtime instances
 * - Поддержка новых типов операций (instance creation, stacking)
 * - Improved memory usage tracking и resource monitoring
 * 
 * ИНТЕГРАЦИЯ С НОВОЙ СИСТЕМОЙ:
 * - Использует UMedComItemManager для доступа к DataTable
 * - Поддерживает runtime properties validation
 * - Monitors grid placement integrity в новой архитектуре
 * - Provides comprehensive performance analysis для optimization
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseInventoryDebugger : public UObject
{
    GENERATED_BODY()
    
public:
    //==================================================================
    // Core Debugger Lifecycle
    //==================================================================
    
    /**
     * Инициализирует debugger с компонентом инвентаря
     * @param InInventoryComponent Компонент инвентаря для отладки
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void Initialize(USuspenseInventoryComponent* InInventoryComponent);
    
    /**
     * Начинает мониторинг операций инвентаря
     * Подписывается на события и начинает сбор метрик
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void StartMonitoring();
    
    /**
     * Останавливает мониторинг операций инвентаря
     * Отписывается от событий и сохраняет финальные метрики
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void StopMonitoring();
    
    /**
     * Проверяет активность мониторинга
     * @return true если мониторинг активен
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    bool IsMonitoring() const { return bMonitoringActive; }
    
    //==================================================================
    // Enhanced Metrics and Analysis
    //==================================================================
    
    /**
     * Получает текущие метрики производительности
     * @return Структура с подробными метриками
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FInventoryPerformanceMetrics GetPerformanceMetrics() const;
    
    /**
     * Сбрасывает все метрики производительности
     * Полезно для начала нового цикла измерений
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void ResetMetrics();
    
    /**
     * Получает детальную статистику использования памяти
     * @return Строка с детальной информацией о памяти
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetMemoryUsageReport() const;
    
    /**
     * Получает статистику DataTable cache performance
     * @return Строка с информацией о производительности кэша
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetCachePerformanceReport() const;
    
    //==================================================================
    // Enhanced Inventory State Analysis
    //==================================================================
    
    /**
     * Получает полный дамп состояния инвентаря
     * Включает runtime instances, grid state и DataTable references
     * @param bIncludeRuntimeProperties Включать ли runtime свойства в дамп
     * @return Многострочное текстовое представление состояния
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetInventoryDump(bool bIncludeRuntimeProperties = true) const;
    
    /**
     * Получает дамп конкретного runtime экземпляра
     * @param InstanceID GUID экземпляра для анализа
     * @return Детальная информация об экземпляре
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetInstanceDump(const FGuid& InstanceID) const;
    
    /**
     * Получает карту занятости сетки в текстовом виде
     * @return ASCII представление сетки инвентаря
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetGridOccupancyMap() const;
    
 //==================================================================
 // Advanced Validation and Integrity Checking
 //==================================================================
    
 /**
  * Валидирует целостность инвентаря с расширенными проверками
  * Включает проверку grid consistency, DataTable integrity и runtime state
  * @param bVerbose Выводить ли детальные результаты в лог
  * @param OutErrorMessages Массив найденных ошибок
  * @return true если инвентарь полностью консистентен
  */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
 bool ValidateInventoryConsistency(bool bVerbose, UPARAM(ref) TArray<FString>& OutErrorMessages) const;
    
 /**
  * Быстрая проверка консистентности инвентаря
  * Использует валидацию без сохранения списка ошибок
  * @param bVerbose Выводить ли детальные результаты в лог
  * @return true если инвентарь консистентен
  */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Debug", DisplayName = "Quick Validate Inventory")
 bool QuickValidateInventory(bool bVerbose = false) const;
    
 /**
  * Получить последние ошибки валидации
  * Возвращает ошибки из последней выполненной валидации
  * @return Массив строк с описанием ошибок
  */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
 TArray<FString> GetLastValidationErrors() const { return LastValidationErrors; }
    /**
     * Валидирует конкретный runtime экземпляр
     * @param InstanceID GUID экземпляра для проверки
     * @param OutErrorMessages Массив найденных ошибок
     * @return true если экземпляр валиден
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateItemInstance(const FGuid& InstanceID, TArray<FString>& OutErrorMessages) const;
    
    /**
     * Проверяет integrity связей между grid и runtime instances
     * @param OutErrorMessages Массив найденных ошибок связности
     * @return true если связи корректны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateGridInstanceIntegrity(TArray<FString>& OutErrorMessages) const;
    
    /**
     * Валидирует доступность всех referenced DataTable entries
     * @param OutMissingItems Массив ItemID которые не найдены в DataTable
     * @return true если все ссылки валидны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateDataTableReferences(TArray<FName>& OutMissingItems) const;
    
    //==================================================================
    // Enhanced Performance Testing
    //==================================================================
    
    /**
     * Выполняет комплексный тест производительности
     * Тестирует различные операции с современной архитектурой
     * @param OperationCount Количество операций для каждого теста
     * @param bTestDataTableAccess Тестировать ли производительность DataTable
     * @param bTestInstanceOperations Тестировать ли runtime operations
     * @return Детальные результаты тестирования
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString RunComprehensivePerformanceTest(int32 OperationCount = 100, 
                                          bool bTestDataTableAccess = true,
                                          bool bTestInstanceOperations = true) const;
    
    /**
     * Тестирует производительность DataTable доступа
     * @param AccessCount Количество обращений к DataTable для тестирования
     * @return Результаты тестирования DataTable performance
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString RunDataTableAccessTest(int32 AccessCount = 100) const;
    
    /**
     * Тестирует производительность grid operations
     * @param GridTestCount Количество тестов размещения
     * @return Результаты тестирования grid операций
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString RunGridPerformanceTest(int32 GridTestCount = 50) const;
    
    /**
     * Тестирует производительность создания runtime экземпляров
     * @param InstanceCount Количество экземпляров для создания
     * @return Результаты тестирования создания экземпляров
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString RunInstanceCreationTest(int32 InstanceCount = 100) const;
    
    //==================================================================
    // Development and Debugging Utilities
    //==================================================================
    
    /**
     * Включает/выключает детальное логирование событий
     * @param bEnable Включить ли verbose logging
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void EnableEventLogging(bool bEnable);
    
    /**
     * Создает полную копию инвентаря для тестирования
     * @return Копия компонента инвентаря или nullptr при ошибке
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    USuspenseInventoryComponent* CreateInventoryCopy() const;
    
    /**
     * Экспортирует состояние инвентаря в JSON формат
     * @param bIncludeRuntimeData Включать ли runtime свойства
     * @return JSON строка с состоянием инвентаря
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString ExportInventoryToJSON(bool bIncludeRuntimeData = true) const;
    
    /**
     * Симулирует нагрузку на инвентарь для stress testing
     * @param Duration Длительность теста в секундах
     * @param OperationsPerSecond Операций в секунду
     * @return Результаты stress теста
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString RunStressTest(float Duration = 10.0f, int32 OperationsPerSecond = 50) const;
    
private:
    //==================================================================
    // Internal State Management
    //==================================================================
 /** Кэш последних ошибок валидации для удобства доступа */
 mutable TArray<FString> LastValidationErrors;
    /** Компонент инвентаря для отладки */
    UPROPERTY()
    USuspenseInventoryComponent* InventoryComponent;
    
    /** Флаг активности мониторинга */
    bool bMonitoringActive;
    
    /** 
     * Метрики производительности (mutable для обновления в const методах)
     * Это позволяет UpdateMemoryMetrics работать корректно из const контекста
     */
    mutable FInventoryPerformanceMetrics Metrics;
    
    /** Временные метки начала операций для измерения производительности */
    TMap<FName, double> OperationStartTimes;
    
    /** Кэш для validation результатов чтобы избежать repeated calculations */
    mutable TMap<FGuid, bool> ValidationCache;
    
    /** Время последнего сброса кэша валидации */
    mutable double LastValidationCacheReset;
    
    //==================================================================
    // Event Handlers (Обновлено для новой архитектуры)
    //==================================================================
    
    /** Обработчик добавления runtime экземпляра */
    UFUNCTION()
    void OnInstanceAdded(const FInventoryItemInstance& ItemInstance, int32 SlotIndex);
    
    /** Обработчик удаления runtime экземпляра */
    UFUNCTION()
    void OnInstanceRemoved(const FName& ItemID, int32 Quantity, int32 SlotIndex);
    
    /** Обработчик перемещения предмета */
    UFUNCTION()
    void OnItemMoved(UObject* Item, int32 OldSlotIndex, int32 NewSlotIndex, bool bWasRotated);
    
    /** Обработчик обмена предметов */
    UFUNCTION()
    void OnItemsSwapped(UObject* FirstItem, UObject* SecondItem, int32 FirstNewIndex, int32 SecondNewIndex);
    
    /** Обработчик ошибок инвентаря */
    UFUNCTION()
    void OnInventoryError(EInventoryErrorCode ErrorCode, const FString& Context);
    
    /** Обработчик операций stacking */
    UFUNCTION()
    void OnStackOperation(const FInventoryItemInstance& SourceInstance, const FInventoryItemInstance& TargetInstance, bool bSuccess);
    
    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /** Записывает время выполнения операции в метрики */
    void RecordOperationTime(const FName& OperationType, double StartTime);
    
    /** Подписывается на события инвентаря */
    void SubscribeToEvents();
    
    /** Отписывается от событий инвентаря */
    void UnsubscribeFromEvents();
    
    /** Обновляет метрики использования памяти */
    void UpdateMemoryMetrics() const;
    
    /** Валидирует runtime экземпляр (internal) */
    bool ValidateInstanceInternal(const FInventoryItemInstance& Instance, TArray<FString>& OutErrors) const;
    
    /** Получает ItemManager для DataTable операций */
    UMedComItemManager* GetItemManager() const;
    
    /** Очищает validation cache если он устарел */
    void ClearValidationCacheIfNeeded() const;
    
    /** Генерирует тестовый runtime экземпляр для performance testing */
    FInventoryItemInstance CreateTestInstance(const FName& ItemID, int32 Quantity = 1) const;
    
    /** Находит свободное место в сетке для тестирования */
    int32 FindFreeGridSlotForTesting() const;
    
    /** Форматирует время в читаемый вид */
    static FString FormatTime(double TimeInSeconds);
    
    /** Форматирует размер памяти в читаемый вид */
    static FString FormatMemorySize(int32 SizeInBytes);
};
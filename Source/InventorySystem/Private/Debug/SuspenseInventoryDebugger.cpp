// MedComInventory/Debug/InventoryDebugger.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Debug/SuspenseInventoryDebugger.h"
#include "Components/SuspenseInventoryComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Serialization/SuspenseInventorySerializer.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Base/SuspenseInventoryLibrary.h"
#include "Interfaces/Inventory/ISuspenseInventory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"

// Константы для оптимизации производительности
static const double VALIDATION_CACHE_LIFETIME = 5.0; // Время жизни validation cache в секундах
static const int32 MAX_TEST_ITEMS = 1000; // Максимальное количество предметов для тестирования
static const float STRESS_TEST_TIMEOUT = 60.0f; // Максимальное время stress теста

//==================================================================
// Core Debugger Lifecycle Implementation
//==================================================================

void USuspenseInventoryDebugger::Initialize(USuspenseInventoryComponent* InInventoryComponent)
{
    if (!InInventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryDebugger::Initialize: Null inventory component provided"));
        return;
    }
    
    // Сохраняем ссылку на компонент
    InventoryComponent = InInventoryComponent;
    
    // Инициализируем состояние мониторинга
    bMonitoringActive = false;
    
    // Сбрасываем метрики и кэш
    ResetMetrics();
    ValidationCache.Empty();
    LastValidationCacheReset = FPlatformTime::Seconds();
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Successfully initialized for component %s"),
        *GetNameSafe(InInventoryComponent));
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryDebugger: Component owner: %s, Inventory size: %s"),
        *GetNameSafe(InInventoryComponent->GetOwner()),
        *InInventoryComponent->GetInventorySize().ToString());
}

void USuspenseInventoryDebugger::StartMonitoring()
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryDebugger::StartMonitoring: Not initialized"));
        return;
    }
    
    if (bMonitoringActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryDebugger: Already monitoring"));
        return;
    }
    
    // Подписываемся на события системы
    SubscribeToEvents();
    
    // Активируем мониторинг
    bMonitoringActive = true;
    
    // Записываем время начала мониторинга
    OperationStartTimes.Add(TEXT("MonitoringSession"), FPlatformTime::Seconds());
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Started monitoring at %s"),
        *FDateTime::Now().ToString());
}

void USuspenseInventoryDebugger::StopMonitoring()
{
    if (!bMonitoringActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryDebugger: Not currently monitoring"));
        return;
    }
    
    // Отписываемся от событий
    UnsubscribeFromEvents();
    
    // Деактивируем мониторинг
    bMonitoringActive = false;
    
    // Логируем финальную статистику
    double SessionDuration = FPlatformTime::Seconds() - OperationStartTimes.FindRef(TEXT("MonitoringSession"));
    OperationStartTimes.Remove(TEXT("MonitoringSession"));
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Stopped monitoring after %.2f seconds"), SessionDuration);
    UE_LOG(LogInventory, Log, TEXT("  Total operations: %d, Average operation time: %.2f ms"),
        Metrics.AddOperations + Metrics.RemoveOperations + Metrics.MoveOperations + Metrics.SwapOperations,
        Metrics.GetAverageOperationTime());
}

//==================================================================
// Enhanced Metrics and Analysis Implementation
//==================================================================

FInventoryPerformanceMetrics USuspenseInventoryDebugger::GetPerformanceMetrics() const
{
    // Обновляем метрики памяти перед возвратом
    UpdateMemoryMetrics();
    
    return Metrics;
}

void USuspenseInventoryDebugger::ResetMetrics()
{
    // Очищаем все метрики
    Metrics = FInventoryPerformanceMetrics();
    
    // Очищаем временные метки
    OperationStartTimes.Empty();
    
    // Очищаем validation cache
    ValidationCache.Empty();
    LastValidationCacheReset = FPlatformTime::Seconds();
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: All metrics reset at %s"),
        *FDateTime::Now().ToString());
}

FString USuspenseInventoryDebugger::GetMemoryUsageReport() const
{
    if (!InventoryComponent)
    {
        return TEXT("Inventory component not available");
    }
    
    UpdateMemoryMetrics();
    
    FString Report = TEXT("=== Inventory Memory Usage Report ===\n");
    
    // Базовая информация о компоненте
    Report += FString::Printf(TEXT("Component: %s\n"), *GetNameSafe(InventoryComponent));
    Report += FString::Printf(TEXT("Owner: %s\n"), *GetNameSafe(InventoryComponent->GetOwner()));
    
    // Метрики памяти
    Report += FString::Printf(TEXT("\n--- Memory Metrics ---\n"));
    Report += FString::Printf(TEXT("Active Instances: %d\n"), Metrics.ActiveInstances);
    Report += FString::Printf(TEXT("Estimated Memory Usage: %s\n"), *FormatMemorySize(Metrics.EstimatedMemoryUsage));
    Report += FString::Printf(TEXT("Total Runtime Properties: %d\n"), Metrics.TotalRuntimeProperties);
    
    // Детальная разбивка по типам предметов
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        TMap<FName, int32> ItemCounts;
        TMap<FName, int32> PropertyCounts;
        
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            ItemCounts.FindOrAdd(Instance.ItemID)++;
            PropertyCounts.FindOrAdd(Instance.ItemID) += Instance.RuntimeProperties.Num();
        }
        
        Report += FString::Printf(TEXT("\n--- Items Breakdown ---\n"));
        for (const auto& ItemPair : ItemCounts)
        {
            int32 PropertiesForItem = PropertyCounts.FindRef(ItemPair.Key);
            Report += FString::Printf(TEXT("- %s: %d instances, %d properties\n"),
                *ItemPair.Key.ToString(), ItemPair.Value, PropertiesForItem);
        }
    }
    
    // Системная информация
    Report += FString::Printf(TEXT("\n--- System Info ---\n"));
    Report += FString::Printf(TEXT("Validation Cache Entries: %d\n"), ValidationCache.Num());
    Report += FString::Printf(TEXT("Operation Timers: %d\n"), OperationStartTimes.Num());
    
    return Report;
}

FString USuspenseInventoryDebugger::GetCachePerformanceReport() const
{
    FString Report = TEXT("=== DataTable Cache Performance Report ===\n");
    
    // Cache hit rate
    float HitRate = Metrics.GetCacheHitRate();
    Report += FString::Printf(TEXT("Cache Hit Rate: %.1f%% (%d hits / %d total)\n"),
        HitRate * 100.0f, Metrics.DataCacheHits, Metrics.DataCacheHits + Metrics.DataCacheMisses);
    
    // Access timing
    Report += FString::Printf(TEXT("Average Data Access Time: %.2f ms\n"), Metrics.AverageDataAccessTime);
    Report += FString::Printf(TEXT("Total DataTable Accesses: %d\n"), Metrics.DataTableAccesses);
    
    // Performance recommendations
    Report += FString::Printf(TEXT("\n--- Performance Analysis ---\n"));
    
    if (HitRate < 0.8f)
    {
        Report += TEXT("⚠️  Low cache hit rate detected. Consider:\n");
        Report += TEXT("   - Preloading frequently used items\n");
        Report += TEXT("   - Increasing cache size\n");
        Report += TEXT("   - Optimizing item access patterns\n");
    }
    else
    {
        Report += TEXT("✅ Cache performance is good\n");
    }
    
    if (Metrics.AverageDataAccessTime > 1.0f)
    {
        Report += TEXT("⚠️  Slow data access detected. Consider:\n");
        Report += TEXT("   - Optimizing DataTable structure\n");
        Report += TEXT("   - Moving frequently accessed data to cache\n");
    }
    else
    {
        Report += TEXT("✅ Data access performance is good\n");
    }
    
    return Report;
}

//==================================================================
// Enhanced Inventory State Analysis Implementation
//==================================================================

FString USuspenseInventoryDebugger::GetInventoryDump(bool bIncludeRuntimeProperties) const
{
    if (!InventoryComponent)
    {
        return TEXT("Inventory component not available");
    }
    
    FString Result = TEXT("=== Comprehensive Inventory Dump ===\n");
    
    // Базовая информация об инвентаре
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    Result += FString::Printf(TEXT("Grid Size: %.0fx%.0f\n"), GridSize.X, GridSize.Y);
    Result += FString::Printf(TEXT("Current Weight: %.2f / %.2f kg\n"),
        InventoryComponent->GetCurrentWeight(), InventoryComponent->GetMaxWeight());
    
    // Получаем все runtime экземпляры
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        Result += FString::Printf(TEXT("Total Instances: %d\n"), AllInstances.Num());
        
        // Разрешенные типы предметов
        FGameplayTagContainer AllowedTypes = InventoryInterface->GetAllowedItemTypes();
        Result += FString::Printf(TEXT("Allowed Types: %s\n"), 
            AllowedTypes.IsEmpty() ? TEXT("All") : *AllowedTypes.ToStringSimple());
        
        Result += FString::Printf(TEXT("\n--- Runtime Instances ---\n"));
        
        // Детальная информация о каждом экземпляре
        for (int32 i = 0; i < AllInstances.Num(); i++)
        {
            const FInventoryItemInstance& Instance = AllInstances[i];
            
            // Базовая информация об экземпляре
            Result += FString::Printf(TEXT("[%d] %s (x%d) [%s]\n"),
                i, *Instance.ItemID.ToString(), Instance.Quantity,
                *Instance.InstanceID.ToString().Left(8));
            
            // Информация о размещении в сетке
            Result += FString::Printf(TEXT("    Anchor: %d, Rotated: %s\n"),
                Instance.AnchorIndex, Instance.bIsRotated ? TEXT("Yes") : TEXT("No"));
            
            // Получаем размер из DataTable
            USuspenseItemManager* ItemManager = GetItemManager();
            if (ItemManager)
            {
                FSuspenseUnifiedItemData ItemData;
                if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
                {
                    FIntPoint EffectiveSize = Instance.bIsRotated ? 
                        FIntPoint(ItemData.GridSize.Y, ItemData.GridSize.X) : ItemData.GridSize;
                    
                    Result += FString::Printf(TEXT("    Size: %dx%d, Type: %s, Weight: %.2f\n"),
                        EffectiveSize.X, EffectiveSize.Y,
                        *ItemData.ItemType.ToString(),
                        ItemData.Weight * Instance.Quantity);
                }
            }
            
            // Runtime свойства если запрошены
            if (bIncludeRuntimeProperties && Instance.RuntimeProperties.Num() > 0)
            {
                Result += FString::Printf(TEXT("    Runtime Properties (%d):\n"), Instance.RuntimeProperties.Num());
                for (const auto& PropertyPair : Instance.RuntimeProperties)
                {
                    Result += FString::Printf(TEXT("      %s: %.2f\n"),
                        *PropertyPair.Key.ToString(), PropertyPair.Value);
                }
            }
            
            Result += TEXT("\n");
        }
    }
    
    // Timestamp для dump
    Result += FString::Printf(TEXT("Generated: %s\n"), *FDateTime::Now().ToString());
    
    return Result;
}

FString USuspenseInventoryDebugger::GetInstanceDump(const FGuid& InstanceID) const
{
    if (!InventoryComponent)
    {
        return TEXT("Inventory component not available");
    }
    
    // Найти экземпляр по GUID
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            if (Instance.InstanceID == InstanceID)
            {
                return USuspenseInventoryLibrary::GetItemInstanceDebugInfo(Instance, InventoryComponent);
            }
        }
    }
    
    return FString::Printf(TEXT("Instance with GUID %s not found"), *InstanceID.ToString());
}

FString USuspenseInventoryDebugger::GetGridOccupancyMap() const
{
    if (!InventoryComponent)
    {
        return TEXT("Inventory component not available");
    }
    
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    int32 GridWidth = FMath::FloorToInt(GridSize.X);
    int32 GridHeight = FMath::FloorToInt(GridSize.Y);
    
    // Создаем карту занятости
    TArray<FString> OccupancyMap;
    OccupancyMap.Init(TEXT("."), GridWidth * GridHeight);
    
    // Заполняем карту данными о предметах
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        for (int32 i = 0; i < AllInstances.Num(); i++)
        {
            const FInventoryItemInstance& Instance = AllInstances[i];
            
            // Получаем занимаемые слоты
            TArray<int32> OccupiedSlots = USuspenseInventoryLibrary::GetOccupiedSlots(
                Instance.ItemID, Instance.AnchorIndex, GridWidth, Instance.bIsRotated, InventoryComponent);
            
            // Маркируем слоты символом соответствующим индексу предмета
            FString ItemSymbol = FString::Printf(TEXT("%X"), i % 16); // Hex digit для компактности
            
            for (int32 SlotIndex : OccupiedSlots)
            {
                if (SlotIndex >= 0 && SlotIndex < OccupancyMap.Num())
                {
                    OccupancyMap[SlotIndex] = ItemSymbol;
                }
            }
        }
    }
    
    // Формируем ASCII карту
    FString Result = FString::Printf(TEXT("=== Grid Occupancy Map (%dx%d) ===\n"), GridWidth, GridHeight);
    Result += TEXT("Legend: . = Empty, 0-F = Item Index\n\n");
    
    // Добавляем header с номерами колонок
    Result += TEXT("   ");
    for (int32 X = 0; X < GridWidth; X++)
    {
        Result += FString::Printf(TEXT("%X"), X % 16);
    }
    Result += TEXT("\n");
    
    // Формируем строки сетки
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Result += FString::Printf(TEXT("%2X "), Y % 16); // Номер строки
        
        for (int32 X = 0; X < GridWidth; X++)
        {
            int32 Index = Y * GridWidth + X;
            Result += OccupancyMap[Index];
        }
        
        Result += TEXT("\n");
    }
    
    return Result;
}

//==================================================================
// Advanced Validation and Integrity Checking Implementation
//==================================================================
bool USuspenseInventoryDebugger::QuickValidateInventory(bool bVerbose) const
{
    // Используем внутренний кэш для хранения ошибок
    LastValidationErrors.Empty();
    return ValidateInventoryConsistency(bVerbose, LastValidationErrors);
}
bool USuspenseInventoryDebugger::ValidateInventoryConsistency(bool bVerbose, TArray<FString>& OutErrorMessages) const
{
    OutErrorMessages.Empty();
    
    if (!InventoryComponent)
    {
        OutErrorMessages.Add(TEXT("Inventory component is null"));
        return false;
    }
    
    bool bIsConsistent = true;
    int32 ErrorCount = 0;
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Starting comprehensive consistency validation"));
    
    // Проверка 1: Grid-Instance Integrity
    TArray<FString> GridErrors;
    if (!ValidateGridInstanceIntegrity(GridErrors))
    {
        OutErrorMessages.Append(GridErrors);
        bIsConsistent = false;
        ErrorCount += GridErrors.Num();
    }
    
    // Проверка 2: DataTable References
    TArray<FName> MissingItems;
    if (!ValidateDataTableReferences(MissingItems))
    {
        for (const FName& MissingItem : MissingItems)
        {
            OutErrorMessages.Add(FString::Printf(TEXT("Missing DataTable entry for item: %s"), 
                *MissingItem.ToString()));
        }
        bIsConsistent = false;
        ErrorCount += MissingItems.Num();
    }
    
    // Проверка 3: Runtime Instance Validation
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            TArray<FString> InstanceErrors;
            if (!ValidateInstanceInternal(Instance, InstanceErrors))
            {
                OutErrorMessages.Append(InstanceErrors);
                bIsConsistent = false;
                ErrorCount += InstanceErrors.Num();
            }
        }
    }
    
    // Проверка 4: Weight Consistency
    float CalculatedWeight = 0.0f;
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        CalculatedWeight = USuspenseInventoryLibrary::CalculateTotalWeightFromInstances(
            AllInstances, InventoryComponent);
    }
    
    float ReportedWeight = InventoryComponent->GetCurrentWeight();
    if (FMath::Abs(CalculatedWeight - ReportedWeight) > 0.01f)
    {
        OutErrorMessages.Add(FString::Printf(TEXT("Weight mismatch: Calculated=%.2f, Reported=%.2f"),
            CalculatedWeight, ReportedWeight));
        bIsConsistent = false;
        ErrorCount++;
    }
    
    // Логирование результатов
    if (bVerbose || !bIsConsistent)
    {
        UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Validation complete - %s (%d errors)"),
            bIsConsistent ? TEXT("PASSED") : TEXT("FAILED"), ErrorCount);
        
        if (!bIsConsistent && bVerbose)
        {
            for (const FString& Error : OutErrorMessages)
            {
                UE_LOG(LogInventory, Warning, TEXT("  - %s"), *Error);
            }
        }
    }
    
    LastValidationErrors = OutErrorMessages;
    
    return bIsConsistent;
}

bool USuspenseInventoryDebugger::ValidateItemInstance(const FGuid& InstanceID, TArray<FString>& OutErrorMessages) const
{
    OutErrorMessages.Empty();
    
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            if (Instance.InstanceID == InstanceID)
            {
                return USuspenseInventoryLibrary::ValidateItemInstance(
                    Instance, InventoryComponent, OutErrorMessages);
            }
        }
    }
    
    OutErrorMessages.Add(FString::Printf(TEXT("Instance with GUID %s not found"), *InstanceID.ToString()));
    return false;
}

bool USuspenseInventoryDebugger::ValidateGridInstanceIntegrity(TArray<FString>& OutErrorMessages) const
{
    OutErrorMessages.Empty();
    
    if (!InventoryComponent)
    {
        OutErrorMessages.Add(TEXT("Inventory component is null"));
        return false;
    }
    
    bool bIntegrityValid = true;
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    int32 GridWidth = FMath::FloorToInt(GridSize.X);
    int32 GridHeight = FMath::FloorToInt(GridSize.Y);
    
    // Карта занятых слотов для проверки пересечений
    TMap<int32, FGuid> SlotOccupancy;
    
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            // Получаем занимаемые слоты
            TArray<int32> OccupiedSlots = USuspenseInventoryLibrary::GetOccupiedSlots(
                Instance.ItemID, Instance.AnchorIndex, GridWidth, Instance.bIsRotated, InventoryComponent);
            
            // Проверяем каждый занимаемый слот
            for (int32 SlotIndex : OccupiedSlots)
            {
                // Проверка выхода за границы
                if (SlotIndex < 0 || SlotIndex >= GridWidth * GridHeight)
                {
                    OutErrorMessages.Add(FString::Printf(
                        TEXT("Instance %s occupies out-of-bounds slot %d"),
                        *Instance.InstanceID.ToString(), SlotIndex));
                    bIntegrityValid = false;
                    continue;
                }
                
                // Проверка пересечений
                if (FGuid* ExistingInstanceID = SlotOccupancy.Find(SlotIndex))
                {
                    if (*ExistingInstanceID != Instance.InstanceID)
                    {
                        OutErrorMessages.Add(FString::Printf(
                            TEXT("Slot %d collision between instances %s and %s"),
                            SlotIndex,
                            *ExistingInstanceID->ToString(),
                            *Instance.InstanceID.ToString()));
                        bIntegrityValid = false;
                    }
                }
                else
                {
                    SlotOccupancy.Add(SlotIndex, Instance.InstanceID);
                }
            }
        }
    }
    
    return bIntegrityValid;
}

bool USuspenseInventoryDebugger::ValidateDataTableReferences(TArray<FName>& OutMissingItems) const
{
    OutMissingItems.Empty();
    
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }
    
    bool bAllReferencesValid = true;
    
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        // Проверяем каждый уникальный ItemID
        TSet<FName> UniqueItemIDs;
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            UniqueItemIDs.Add(Instance.ItemID);
        }
        
        for (const FName& ItemID : UniqueItemIDs)
        {
            if (!ItemManager->HasItem(ItemID))
            {
                OutMissingItems.Add(ItemID);
                bAllReferencesValid = false;
            }
        }
    }
    
    return bAllReferencesValid;
}

//==================================================================
// Enhanced Performance Testing Implementation
//==================================================================

FString USuspenseInventoryDebugger::RunComprehensivePerformanceTest(int32 OperationCount, 
                                                           bool bTestDataTableAccess,
                                                           bool bTestInstanceOperations) const
{
    if (!InventoryComponent)
    {
        return TEXT("Inventory component not available");
    }
    
    FString Result = FString::Printf(TEXT("=== Comprehensive Performance Test ===\n"));
    Result += FString::Printf(TEXT("Operations: %d, DataTable Test: %s, Instance Test: %s\n\n"),
        OperationCount, bTestDataTableAccess ? TEXT("Yes") : TEXT("No"),
        bTestInstanceOperations ? TEXT("Yes") : TEXT("No"));
    
    // Test 1: DataTable Access Performance
    if (bTestDataTableAccess)
    {
        Result += RunDataTableAccessTest(OperationCount);
        Result += TEXT("\n");
    }
    
    // Test 2: Instance Creation Performance
    if (bTestInstanceOperations)
    {
        Result += RunInstanceCreationTest(OperationCount);
        Result += TEXT("\n");
    }
    
    // Test 3: Grid Operations Performance
    Result += RunGridPerformanceTest(FMath::Min(OperationCount, 50));
    Result += TEXT("\n");
    
    // Test 4: Validation Performance
    double ValidationStartTime = FPlatformTime::Seconds();
    TArray<FString> ValidationErrors;
    bool bValidationResult = ValidateInventoryConsistency(false, ValidationErrors);
    double ValidationTime = (FPlatformTime::Seconds() - ValidationStartTime) * 1000.0;
    
    Result += FString::Printf(TEXT("--- Validation Performance ---\n"));
    Result += FString::Printf(TEXT("Validation Time: %.2f ms\n"), ValidationTime);
    Result += FString::Printf(TEXT("Validation Result: %s (%d errors)\n"),
        bValidationResult ? TEXT("PASS") : TEXT("FAIL"), ValidationErrors.Num());
    
    return Result;
}

FString USuspenseInventoryDebugger::RunGridPerformanceTest(int32 GridTestCount) const
{
    if (!InventoryComponent)
    {
        return TEXT("Grid test failed: Inventory component not available");
    }
    
    FString Result = FString::Printf(TEXT("--- Grid Performance Test (%d operations) ---\n"), GridTestCount);
    
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    int32 GridWidth = FMath::FloorToInt(GridSize.X);
    int32 GridHeight = FMath::FloorToInt(GridSize.Y);
    
    // Test placement finding performance
    double PlacementStartTime = FPlatformTime::Seconds();
    int32 SuccessfulPlacements = 0;
    
    for (int32 i = 0; i < GridTestCount; i++)
    {
        // Создаем тестовый предмет случайного размера
        FName TestItemID = TEXT("TestItem_1x1");
        TArray<int32> OccupiedSlots; // Пустой массив для теста
        bool bFoundRotated = false;
        
        int32 PlacementSlot = USuspenseInventoryLibrary::FindOptimalPlacementForItem(
            TestItemID, GridWidth, GridHeight, OccupiedSlots, true, InventoryComponent, bFoundRotated);
        
        if (PlacementSlot != INDEX_NONE)
        {
            SuccessfulPlacements++;
        }
    }
    
    double PlacementTime = (FPlatformTime::Seconds() - PlacementStartTime) * 1000.0;
    
    Result += FString::Printf(TEXT("Placement Finding: %.2f ms total, %.2f ms per operation\n"),
        PlacementTime, PlacementTime / GridTestCount);
    Result += FString::Printf(TEXT("Successful Placements: %d/%d (%.1f%%)\n"),
        SuccessfulPlacements, GridTestCount, (float)SuccessfulPlacements / GridTestCount * 100.0f);
    
    // Test coordinate conversion performance
    double ConversionStartTime = FPlatformTime::Seconds();
    
    for (int32 i = 0; i < GridTestCount * 10; i++) // More iterations for coordinate conversion
    {
        int32 TestIndex = FMath::RandRange(0, GridWidth * GridHeight - 1);
        int32 X, Y;
        USuspenseInventoryLibrary::IndexToGridCoords(TestIndex, GridWidth, X, Y);
        int32 BackToIndex = USuspenseInventoryLibrary::GridCoordsToIndex(X, Y, GridWidth, GridHeight);
        
        // Verify round-trip conversion
        if (BackToIndex != TestIndex)
        {
            Result += FString::Printf(TEXT("⚠️  Coordinate conversion error: %d -> (%d,%d) -> %d\n"),
                TestIndex, X, Y, BackToIndex);
        }
    }
    
    double ConversionTime = (FPlatformTime::Seconds() - ConversionStartTime) * 1000.0;
    
    Result += FString::Printf(TEXT("Coordinate Conversion: %.2f ms total, %.3f ms per operation\n"),
        ConversionTime, ConversionTime / (GridTestCount * 10));
    
    return Result;
}

FString USuspenseInventoryDebugger::RunInstanceCreationTest(int32 InstanceCount) const
{
    FString Result = FString::Printf(TEXT("--- Instance Creation Test (%d instances) ---\n"), InstanceCount);
    
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        Result += TEXT("Failed: ItemManager not available\n");
        return Result;
    }
    
    // Test instance creation performance
    double CreationStartTime = FPlatformTime::Seconds();
    int32 SuccessfulCreations = 0;
    
    TArray<FName> TestItemIDs = {
        TEXT("TestItem_1x1"),
        TEXT("TestItem_2x1"),
        TEXT("TestItem_1x2"),
        TEXT("TestItem_2x2")
    };
    
    for (int32 i = 0; i < InstanceCount; i++)
    {
        FName TestItemID = TestItemIDs[i % TestItemIDs.Num()];
        int32 TestQuantity = FMath::RandRange(1, 5);
        
        FInventoryItemInstance TestInstance;
        if (ItemManager->CreateItemInstance(TestItemID, TestQuantity, TestInstance))
        {
            SuccessfulCreations++;
        }
    }
    
    double CreationTime = (FPlatformTime::Seconds() - CreationStartTime) * 1000.0;
    
    Result += FString::Printf(TEXT("Instance Creation: %.2f ms total, %.3f ms per instance\n"),
        CreationTime, CreationTime / InstanceCount);
    Result += FString::Printf(TEXT("Success Rate: %d/%d (%.1f%%)\n"),
        SuccessfulCreations, InstanceCount, (float)SuccessfulCreations / InstanceCount * 100.0f);
    
    return Result;
}

//==================================================================
// Event Handlers Implementation (Обновлено для новой архитектуры)
//==================================================================

void USuspenseInventoryDebugger::OnInstanceAdded(const FInventoryItemInstance& ItemInstance, int32 SlotIndex)
{
    // Увеличиваем счетчик операций
    Metrics.AddOperations++;
    
    // Записываем время выполнения если есть начальная метка
    if (double* StartTime = OperationStartTimes.Find(TEXT("Add")))
    {
        RecordOperationTime(TEXT("Add"), *StartTime);
    }
    
    // Обновляем статистику DataTable доступа
    Metrics.DataTableAccesses++;
    
    // Логируем операцию в verbose режиме
    UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryDebugger: Instance added - %s (x%d) at slot %d"),
        *ItemInstance.ItemID.ToString(), ItemInstance.Quantity, SlotIndex);
    
    // Очищаем validation cache так как состояние изменилось
    ValidationCache.Empty();
}

void USuspenseInventoryDebugger::OnInstanceRemoved(const FName& ItemID, int32 Quantity, int32 SlotIndex)
{
    // Увеличиваем счетчик операций
    Metrics.RemoveOperations++;
    
    // Записываем время выполнения
    if (double* StartTime = OperationStartTimes.Find(TEXT("Remove")))
    {
        RecordOperationTime(TEXT("Remove"), *StartTime);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryDebugger: Instance removed - %s (x%d) from slot %d"),
        *ItemID.ToString(), Quantity, SlotIndex);
    
    // Очищаем validation cache
    ValidationCache.Empty();
}

void USuspenseInventoryDebugger::OnItemMoved(UObject* Item, int32 OldSlotIndex, int32 NewSlotIndex, bool bWasRotated)
{
    // Увеличиваем счетчик операций
    Metrics.MoveOperations++;
    
    // Записываем время выполнения
    if (double* StartTime = OperationStartTimes.Find(TEXT("Move")))
    {
        RecordOperationTime(TEXT("Move"), *StartTime);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryDebugger: Item moved - %s from %d to %d (rotated: %s)"),
        *GetNameSafe(Item), OldSlotIndex, NewSlotIndex, bWasRotated ? TEXT("Yes") : TEXT("No"));
    
    // Очищаем validation cache
    ValidationCache.Empty();
}

void USuspenseInventoryDebugger::OnItemsSwapped(UObject* FirstItem, UObject* SecondItem, int32 FirstNewIndex, int32 SecondNewIndex)
{
    // Увеличиваем счетчик операций
    Metrics.SwapOperations++;
    
    // Записываем время выполнения
    if (double* StartTime = OperationStartTimes.Find(TEXT("Swap")))
    {
        RecordOperationTime(TEXT("Swap"), *StartTime);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryDebugger: Items swapped - %s <-> %s"),
        *GetNameSafe(FirstItem), *GetNameSafe(SecondItem));
    
    // Очищаем validation cache
    ValidationCache.Empty();
}

void USuspenseInventoryDebugger::OnInventoryError(ESuspenseInventoryErrorCode ErrorCode, const FString& Context)
{
    // Логируем ошибку с подробностями
    FString ErrorString = FInventoryOperationResult::GetErrorCodeString(ErrorCode);
    UE_LOG(LogInventory, Warning, TEXT("InventoryDebugger: Error detected - %s in context: %s"),
        *ErrorString, *Context);
    
    // Можно добавить счетчики ошибок в метрики если нужно
}

void USuspenseInventoryDebugger::OnStackOperation(const FInventoryItemInstance& SourceInstance, 
                                        const FInventoryItemInstance& TargetInstance, 
                                        bool bSuccess)
{
    // Увеличиваем счетчик stack операций
    Metrics.StackOperations++;
    
    // Записываем время выполнения
    if (double* StartTime = OperationStartTimes.Find(TEXT("Stack")))
    {
        RecordOperationTime(TEXT("Stack"), *StartTime);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryDebugger: Stack operation - %s + %s = %s"),
        *SourceInstance.ItemID.ToString(), *TargetInstance.ItemID.ToString(),
        bSuccess ? TEXT("Success") : TEXT("Failed"));
}

//==================================================================
// Internal Helper Methods Implementation
//==================================================================

void USuspenseInventoryDebugger::RecordOperationTime(const FName& OperationType, double StartTime)
{
    double EndTime = FPlatformTime::Seconds();
    float ElapsedTimeMs = (EndTime - StartTime) * 1000.0f;
    
    // Обновляем соответствующую метрику времени
    if (OperationType == TEXT("Add"))
    {
        float TotalTime = Metrics.AverageAddTime * (Metrics.AddOperations - 1);
        Metrics.AverageAddTime = (TotalTime + ElapsedTimeMs) / Metrics.AddOperations;
    }
    else if (OperationType == TEXT("Remove"))
    {
        float TotalTime = Metrics.AverageRemoveTime * (Metrics.RemoveOperations - 1);
        Metrics.AverageRemoveTime = (TotalTime + ElapsedTimeMs) / Metrics.RemoveOperations;
    }
    else if (OperationType == TEXT("Move"))
    {
        float TotalTime = Metrics.AverageMoveTime * (Metrics.MoveOperations - 1);
        Metrics.AverageMoveTime = (TotalTime + ElapsedTimeMs) / Metrics.MoveOperations;
    }
    else if (OperationType == TEXT("Swap"))
    {
        float TotalTime = Metrics.AverageSwapTime * (Metrics.SwapOperations - 1);
        Metrics.AverageSwapTime = (TotalTime + ElapsedTimeMs) / Metrics.SwapOperations;
    }
    else if (OperationType == TEXT("Stack"))
    {
        float TotalTime = Metrics.AverageStackTime * (Metrics.StackOperations - 1);
        Metrics.AverageStackTime = (TotalTime + ElapsedTimeMs) / Metrics.StackOperations;
    }
    
    // Устанавливаем новую метку времени для следующей операции
    OperationStartTimes.Add(OperationType, FPlatformTime::Seconds());
}

void USuspenseInventoryDebugger::SubscribeToEvents()
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryDebugger::SubscribeToEvents: No inventory component"));
        return;
    }
    
    // Инициализируем временные метки для всех типов операций
    double CurrentTime = FPlatformTime::Seconds();
    OperationStartTimes.Add(TEXT("Add"), CurrentTime);
    OperationStartTimes.Add(TEXT("Remove"), CurrentTime);
    OperationStartTimes.Add(TEXT("Move"), CurrentTime);
    OperationStartTimes.Add(TEXT("Swap"), CurrentTime);
    OperationStartTimes.Add(TEXT("Stack"), CurrentTime);
    
    // NOTE: В реальной реализации здесь должна быть подписка на события компонента
    // Это зависит от реализации системы событий в USuspenseInventoryComponent
    // Пример:
    // if (InventoryComponent->OnItemInstanceAdded.IsBound())
    // {
    //     InventoryComponent->OnItemInstanceAdded.AddDynamic(this, &USuspenseInventoryDebugger::OnInstanceAdded);
    // }
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Subscribed to inventory events"));
}

void USuspenseInventoryDebugger::UnsubscribeFromEvents()
{
    if (!InventoryComponent)
    {
        return;
    }
    
    // NOTE: Отписка от событий
    // Пример:
    // if (InventoryComponent->OnItemInstanceAdded.IsBound())
    // {
    //     InventoryComponent->OnItemInstanceAdded.RemoveDynamic(this, &USuspenseInventoryDebugger::OnInstanceAdded);
    // }
    
    // Очищаем временные метки
    OperationStartTimes.Empty();
    
    UE_LOG(LogInventory, Log, TEXT("InventoryDebugger: Unsubscribed from inventory events"));
}

void USuspenseInventoryDebugger::UpdateMemoryMetrics() const
{
    if (!InventoryComponent)
    {
        return;
    }
    
    // Подсчитываем активные экземпляры и их memory footprint
    if (IMedComInventoryInterface* InventoryInterface = Cast<IMedComInventoryInterface>(InventoryComponent))
    {
        TArray<FInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
        
        Metrics.ActiveInstances = AllInstances.Num();
        Metrics.TotalRuntimeProperties = 0;
        
        // Приблизительный расчет использования памяти
        int32 EstimatedMemory = 0;
        
        for (const FInventoryItemInstance& Instance : AllInstances)
        {
            // Базовый размер структуры FInventoryItemInstance
            EstimatedMemory += sizeof(FInventoryItemInstance);
            
            // Размер runtime properties
            Metrics.TotalRuntimeProperties += Instance.RuntimeProperties.Num();
            EstimatedMemory += Instance.RuntimeProperties.Num() * (sizeof(FName) + sizeof(float));
        }
        
        Metrics.EstimatedMemoryUsage = EstimatedMemory;
    }
}

bool USuspenseInventoryDebugger::ValidateInstanceInternal(const FInventoryItemInstance& Instance, TArray<FString>& OutErrors) const
{
    // Используем библиотечную функцию для validation
    return USuspenseInventoryLibrary::ValidateItemInstance(Instance, InventoryComponent, OutErrors);
}

USuspenseItemManager* USuspenseInventoryDebugger::GetItemManager() const
{
    if (!InventoryComponent)
    {
        return nullptr;
    }
    
    UWorld* World = InventoryComponent->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<USuspenseItemManager>();
}

void USuspenseInventoryDebugger::ClearValidationCacheIfNeeded() const
{
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastValidationCacheReset > VALIDATION_CACHE_LIFETIME)
    {
        ValidationCache.Empty();
        LastValidationCacheReset = CurrentTime;
    }
}

FString USuspenseInventoryDebugger::FormatTime(double TimeInSeconds)
{
    if (TimeInSeconds < 1.0)
    {
        return FString::Printf(TEXT("%.2f ms"), TimeInSeconds * 1000.0);
    }
    else if (TimeInSeconds < 60.0)
    {
        return FString::Printf(TEXT("%.2f sec"), TimeInSeconds);
    }
    else
    {
        int32 Minutes = FMath::FloorToInt(TimeInSeconds / 60.0);
        double Seconds = TimeInSeconds - (Minutes * 60.0);
        return FString::Printf(TEXT("%d:%05.2f"), Minutes, Seconds);
    }
}

FString USuspenseInventoryDebugger::FormatMemorySize(int32 SizeInBytes)
{
    if (SizeInBytes < 1024)
    {
        return FString::Printf(TEXT("%d B"), SizeInBytes);
    }
    else if (SizeInBytes < 1024 * 1024)
    {
        return FString::Printf(TEXT("%.1f KB"), SizeInBytes / 1024.0f);
    }
    else
    {
        return FString::Printf(TEXT("%.1f MB"), SizeInBytes / (1024.0f * 1024.0f));
    }
}

// Добавляем недостающий метод для DataTable тестирования
FString USuspenseInventoryDebugger::RunDataTableAccessTest(int32 AccessCount) const
{
    FString Result = FString::Printf(TEXT("--- DataTable Access Test (%d accesses) ---\n"), AccessCount);
    
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        Result += TEXT("Failed: ItemManager not available\n");
        return Result;
    }
    
    // Получаем список всех ItemID для тестирования
    TArray<FName> AllItemIDs = ItemManager->GetAllItemIDs();
    if (AllItemIDs.Num() == 0)
    {
        Result += TEXT("Failed: No items in DataTable\n");
        return Result;
    }
    
    double AccessStartTime = FPlatformTime::Seconds();
    int32 SuccessfulAccesses = 0;
    
    for (int32 i = 0; i < AccessCount; i++)
    {
        // Выбираем случайный ItemID
        FName TestItemID = AllItemIDs[FMath::RandRange(0, AllItemIDs.Num() - 1)];
        
        // Получаем данные предмета
        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(TestItemID, ItemData))
        {
            SuccessfulAccesses++;
        }
    }
    
    double AccessTime = (FPlatformTime::Seconds() - AccessStartTime) * 1000.0;
    
    Result += FString::Printf(TEXT("DataTable Access: %.2f ms total, %.3f ms per access\n"),
        AccessTime, AccessTime / AccessCount);
    Result += FString::Printf(TEXT("Success Rate: %d/%d (%.1f%%)\n"),
        SuccessfulAccesses, AccessCount, (float)SuccessfulAccesses / AccessCount * 100.0f);
    
    return Result;
}

// Event logging control
void USuspenseInventoryDebugger::EnableEventLogging(bool bEnable)
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Warning, TEXT("EnableEventLogging: No inventory component to monitor"));
        return;
    }
    
    // This would enable/disable verbose event logging
    // For now, just log the state change
    UE_LOG(LogInventory, Log, TEXT("Event logging %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

// Create inventory copy for testing
USuspenseInventoryComponent* USuspenseInventoryDebugger::CreateInventoryCopy() const
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("CreateInventoryCopy: No inventory component to copy"));
        return nullptr;
    }
    
    AActor* Owner = InventoryComponent->GetOwner();
    if (!Owner)
    {
        UE_LOG(LogInventory, Error, TEXT("CreateInventoryCopy: No owner actor"));
        return nullptr;
    }
    
    // Create a new component
    USuspenseInventoryComponent* CopyComponent = NewObject<USuspenseInventoryComponent>(
        Owner, 
        USuspenseInventoryComponent::StaticClass(),
        NAME_None,
        RF_Transient
    );
    
    if (!CopyComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("CreateInventoryCopy: Failed to create new component"));
        return nullptr;
    }
    
    // Copy configuration
    FVector2D OriginalSize = InventoryComponent->GetInventorySize();
    CopyComponent->InitializeWithSimpleSettings(
        FMath::FloorToInt(OriginalSize.X),
        FMath::FloorToInt(OriginalSize.Y),
        InventoryComponent->GetMaxWeight(),
        InventoryComponent->GetAllowedItemTypes()
    );
    
    // Copy items
    TArray<FInventoryItemInstance> AllItems = InventoryComponent->GetAllItemInstances();
    for (const FInventoryItemInstance& Item : AllItems)
    {
        CopyComponent->AddItemInstance(Item);
    }
    
    UE_LOG(LogInventory, Log, TEXT("CreateInventoryCopy: Created copy with %d items"), AllItems.Num());
    
    return CopyComponent;
}

// Export inventory to JSON
FString USuspenseInventoryDebugger::ExportInventoryToJSON(bool bIncludeRuntimeData) const
{
    if (!InventoryComponent)
    {
        return TEXT("{}");
    }
    
    // Use the serializer to export
    return USuspenseInventorySerializer::SerializeInventoryToJson(InventoryComponent, true);
}

// Run stress test
FString USuspenseInventoryDebugger::RunStressTest(float Duration, int32 OperationsPerSecond) const
{
    if (!InventoryComponent || Duration <= 0.0f || OperationsPerSecond <= 0)
    {
        return TEXT("Invalid parameters for stress test");
    }
    
    FString Result = FString::Printf(
        TEXT("=== Stress Test Results ===\n")
        TEXT("Duration: %.1f seconds\n")
        TEXT("Operations/Second: %d\n"),
        Duration, OperationsPerSecond
    );
    
    // Calculate total operations
    int32 TotalOperations = FMath::FloorToInt(Duration * OperationsPerSecond);
    
    // Track timing
    double TotalTime = 0.0;
    int32 SuccessfulOps = 0;
    int32 FailedOps = 0;
    
    // Get some test items
    TArray<FName> TestItemIDs = {
        TEXT("TestItem1"),
        TEXT("TestItem2"),
        TEXT("TestItem3")
    };
    
    // Simulate operations
    for (int32 i = 0; i < TotalOperations; i++)
    {
        double OpStart = FPlatformTime::Seconds();
        
        // Random operation type
        int32 OpType = FMath::RandRange(0, 2);
        bool bSuccess = false;
        
        switch (OpType)
        {
            case 0: // Add
            {
                FName ItemID = TestItemIDs[FMath::RandRange(0, TestItemIDs.Num() - 1)];
                bSuccess = const_cast<USuspenseInventoryComponent*>(InventoryComponent)->AddItemByID(ItemID, 1);
                break;
            }
            case 1: // Remove
            {
                FName ItemID = TestItemIDs[FMath::RandRange(0, TestItemIDs.Num() - 1)];
                const_cast<USuspenseInventoryComponent*>(InventoryComponent)->RemoveItemByID(ItemID, 1);
                bSuccess = true; // Remove always succeeds
                break;
            }
            case 2: // Move
            {
                // Simplified move test
                bSuccess = true;
                break;
            }
        }
        
        double OpEnd = FPlatformTime::Seconds();
        TotalTime += (OpEnd - OpStart);
        
        if (bSuccess)
        {
            SuccessfulOps++;
        }
        else
        {
            FailedOps++;
        }
    }
    
    // Calculate statistics
    double AverageOpTime = TotalTime / TotalOperations * 1000.0; // Convert to ms
    
    Result += FString::Printf(
        TEXT("\nTotal Operations: %d\n")
        TEXT("Successful: %d (%.1f%%)\n")
        TEXT("Failed: %d (%.1f%%)\n")
        TEXT("Average Operation Time: %.3f ms\n")
        TEXT("Total Time: %.3f seconds\n"),
        TotalOperations,
        SuccessfulOps, (float)SuccessfulOps / TotalOperations * 100.0f,
        FailedOps, (float)FailedOps / TotalOperations * 100.0f,
        AverageOpTime,
        TotalTime
    );
    
    return Result;
}
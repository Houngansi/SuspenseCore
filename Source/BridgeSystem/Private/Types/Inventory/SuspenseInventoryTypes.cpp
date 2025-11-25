// Copyright Suspense Team. All Rights Reserved.

#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Inventory/SuspenseInventoryUtils.h" 
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "ItemSystem/SuspenseItemSystemAccess.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryUtils, Log, All);

//==============================================================================
// Utility Functions для работы с инвентарем и DataTable интеграцией
// 
// АРХИТЕКТУРНОЕ ИЗМЕНЕНИЕ:
// Все функции теперь принимают WorldContextObject как первый параметр.
// Это explicit dependency injection который делает код более надежным и тестируемым.
// 
// Для MMO FPS это критически важно потому что гарантирует что сервер и клиенты
// всегда используют одинаковый источник данных (ItemManager) без возможности
// использовать устаревшие или дублированные данные.
//==============================================================================

namespace InventoryUtils
{
    /**
     * Получить unified данные предмета из DataTable через ItemManager
     * Это основная функция для доступа к статическим данным предметов
     * 
     * ИЗМЕНЕНО: Теперь использует FItemSystemAccess для надежного получения ItemManager
     * 
     * @param WorldContextObject - Любой UObject с валидным World context
     * @param ItemID - ID предмета для поиска в DataTable
     * @param OutData - Выходная структура с данными предмета
     * @return true если данные найдены и загружены успешно
     */
    bool GetUnifiedItemData(const UObject* WorldContextObject, const FName& ItemID, FSuspenseUnifiedItemData& OutData)
    {
        // Используем централизованный accessor для получения ItemManager
        USuspenseItemManager* ItemManager = FItemSystemAccess::GetItemManager(WorldContextObject);
        if (!ItemManager)
        {
            UE_LOG(LogInventoryUtils, Warning, 
                TEXT("GetUnifiedItemData: Failed to get ItemManager for item: %s"), 
                *ItemID.ToString());
            UE_LOG(LogInventoryUtils, Warning, 
                TEXT("  Context object: %s (class: %s)"),
                WorldContextObject ? *WorldContextObject->GetName() : TEXT("nullptr"),
                WorldContextObject ? *WorldContextObject->GetClass()->GetName() : TEXT("N/A"));
            return false;
        }
        
        // Получаем данные через ItemManager - единственный источник истины
        bool bSuccess = ItemManager->GetUnifiedItemData(ItemID, OutData);
        
        if (!bSuccess)
        {
            UE_LOG(LogInventoryUtils, Verbose, 
                TEXT("GetUnifiedItemData: Item '%s' not found in DataTable"), 
                *ItemID.ToString());
        }
        
        return bSuccess;
    }
    
    /**
     * ЗАГЛУШКА: Получить максимальную прочность по умолчанию для типа предмета
     * 
     * ВАЖНО: Это временная реализация! В будущем значения будут браться из AttributeSet.
     * Сейчас мы используем простые константы чтобы система работала до интеграции с GAS.
     * 
     * @param UnifiedData - Данные предмета из DataTable
     * @return Максимальная прочность по умолчанию (заглушка)
     */
    float GetDefaultMaxDurability(const FSuspenseUnifiedItemData& UnifiedData)
    {
        // TODO: Replace with AttributeSet values after GAS integration
        // These values will come from:
        // - WeaponAttributeSet for weapons
        // - ArmorAttributeSet for armor  
        // - EquipmentAttributeSet for other equipment
        
        if (UnifiedData.bIsWeapon)
        {
            return 150.0f; // STUB: Medium durability for weapons
        }
        else if (UnifiedData.bIsArmor)
        {
            return 200.0f; // STUB: Higher durability for armor
        }
        else if (UnifiedData.bIsEquippable)
        {
            return 100.0f; // STUB: Base durability for equipment
        }
        
        return 0.0f; // Non-equippable items have no durability
    }
    
    /**
     * ЗАГЛУШКА: Получить стандартную емкость магазина для типа оружия
     * 
     * ВАЖНО: Это временная реализация! В будущем значения будут браться из AmmoAttributeSet.
     * Сейчас мы используем простое определение по строке тега для работоспособности системы.
     * 
     * @param WeaponArchetype - Тип оружия из DataTable
     * @return Емкость магазина по умолчанию (заглушка)
     */
    int32 GetDefaultAmmoCapacity(const FGameplayTag& WeaponArchetype)
    {
        // TODO: Replace with AmmoAttributeSet values after GAS integration
        
        if (!WeaponArchetype.IsValid())
        {
            return 30; // STUB: Universal default value
        }
        
        FString ArchetypeString = WeaponArchetype.ToString();
        
        // STUB: Simple logic based on weapon type name
        // This will be replaced with AttributeSet queries
        if (ArchetypeString.Contains(TEXT("Rifle")))
        {
            return 30; // Assault rifle
        }
        else if (ArchetypeString.Contains(TEXT("Pistol")))
        {
            return 15; // Pistol
        }
        else if (ArchetypeString.Contains(TEXT("Shotgun")))
        {
            return 8; // Shotgun
        }
        else if (ArchetypeString.Contains(TEXT("Sniper")))
        {
            return 5; // Sniper rifle
        }
        else if (ArchetypeString.Contains(TEXT("SMG")) || ArchetypeString.Contains(TEXT("Submachine")))
        {
            return 25; // SMG
        }
        else if (ArchetypeString.Contains(TEXT("LMG")) || ArchetypeString.Contains(TEXT("Machine")))
        {
            return 100; // LMG
        }
        
        return 30; // STUB: Default for unknown types
    }
    
    /**
     * Инициализировать runtime свойства экземпляра на основе его типа
     * 
     * Эта функция устанавливает начальные значения для runtime свойств предмета.
     * Использует заглушки до тех пор, пока не будет готова интеграция с AttributeSet.
     * 
     * @param Instance - Экземпляр предмета для инициализации (изменяется)
     * @param UnifiedData - Статические данные из DataTable (только чтение)
     */
    void InitializeRuntimeProperties(FSuspenseInventoryItemInstance& Instance, const FSuspenseUnifiedItemData& UnifiedData)
    {
        // Initialize durability for equippable items
        if (UnifiedData.bIsEquippable)
        {
            float MaxDurability = GetDefaultMaxDurability(UnifiedData);
            
            Instance.SetRuntimeProperty(TEXT("MaxDurability"), MaxDurability);
            Instance.SetRuntimeProperty(TEXT("Durability"), MaxDurability);
            
            UE_LOG(LogInventoryUtils, VeryVerbose, 
                TEXT("InitializeRuntimeProperties: Initialized durability for %s: %.1f/%.1f"), 
                *UnifiedData.ItemID.ToString(), MaxDurability, MaxDurability);
        }
        
        // Initialize ammo for weapons
        if (UnifiedData.bIsWeapon)
        {
            FGameplayTag WeaponArchetype;
            if (UnifiedData.WeaponArchetype.IsValid())
            {
                WeaponArchetype = UnifiedData.WeaponArchetype;
            }
            
            int32 MaxAmmo = GetDefaultAmmoCapacity(WeaponArchetype);
            
            Instance.SetRuntimeProperty(TEXT("MaxAmmo"), static_cast<float>(MaxAmmo));
            Instance.SetRuntimeProperty(TEXT("Ammo"), static_cast<float>(MaxAmmo));
            
            UE_LOG(LogInventoryUtils, VeryVerbose, 
                TEXT("InitializeRuntimeProperties: Initialized ammo for %s: %d/%d"), 
                *UnifiedData.ItemID.ToString(), MaxAmmo, MaxAmmo);
        }
        
        // Initialize charges for consumables
        if (UnifiedData.bIsConsumable)
        {
            float InitialCharges = static_cast<float>(Instance.Quantity);
            Instance.SetRuntimeProperty(TEXT("Charges"), InitialCharges);
            
            UE_LOG(LogInventoryUtils, VeryVerbose, 
                TEXT("InitializeRuntimeProperties: Initialized charges for %s: %.0f"), 
                *UnifiedData.ItemID.ToString(), InitialCharges);
        }
        
        // STUB: Initialize item condition
        Instance.SetRuntimeProperty(TEXT("Condition"), 1.0f); // 1.0 = perfect condition
    }
    
/**
 * Создать правильно инициализированный экземпляр предмета
 * 
 * Эта функция является основным способом создания новых экземпляров предметов.
 * Она автоматически устанавливает все необходимые runtime свойства на основе DataTable.
 * 
 * ИЗМЕНЕНО: Теперь требует WorldContextObject для доступа к ItemManager
 * 
 * @param WorldContextObject - Валидный UObject с World context
 * @param ItemID - ID предмета из DataTable
 * @param Quantity - Количество предметов в стеке
 * @return Полностью инициализированный экземпляр предмета
 */
FSuspenseInventoryItemInstance CreateItemInstance(const UObject* WorldContextObject, const FName& ItemID, int32 Quantity)
{
    // Create base instance with ID and quantity using factory method
    FSuspenseInventoryItemInstance Instance = FSuspenseInventoryItemInstance::Create(ItemID, Quantity);
    
    // Get static data from DataTable for proper initialization
    FSuspenseUnifiedItemData UnifiedData;
    if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
    {
        UE_LOG(LogInventoryUtils, Error, 
            TEXT("CreateItemInstance: Failed to create item instance - ItemID not found in DataTable: %s"), 
            *ItemID.ToString());
        UE_LOG(LogInventoryUtils, Error, 
            TEXT("CreateItemInstance: Check that the ItemID exists in your DataTable and ItemManager is properly configured"));
        UE_LOG(LogInventoryUtils, Error,
            TEXT("CreateItemInstance: Context: %s (class: %s)"),
            WorldContextObject ? *WorldContextObject->GetName() : TEXT("nullptr"),
            WorldContextObject ? *WorldContextObject->GetClass()->GetName() : TEXT("N/A"));
        return Instance; // Return base instance without runtime properties
    }
    
    // Initialize runtime properties based on item type
    InitializeRuntimeProperties(Instance, UnifiedData);
    
    UE_LOG(LogInventoryUtils, Log, 
        TEXT("CreateItemInstance: Successfully created item instance: %s"), 
        *Instance.GetShortDebugString());
    return Instance;
}
    
    /**
     * Получить размер предмета в сетке с учетом поворота
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для доступа к DataTable
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param ItemID - ID предмета
     * @param bIsRotated - Повернут ли предмет на 90 градусов
     * @return Размер предмета в ячейках сетки (ширина, высота)
     */
    FVector2D GetItemGridSize(const UObject* WorldContextObject, const FName& ItemID, bool bIsRotated)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            UE_LOG(LogInventoryUtils, Warning, 
                TEXT("GetItemGridSize: Unknown item size for: %s, using default 1x1"), 
                *ItemID.ToString());
            return FVector2D(1, 1); // Safe fallback - 1x1 size
        }
        
        // Get size from DataTable
        FVector2D GridSize(UnifiedData.GridSize.X, UnifiedData.GridSize.Y);
        
        // Apply rotation if needed (swap width and height)
        if (bIsRotated)
        {
            GridSize = FVector2D(GridSize.Y, GridSize.X);
        }
        
        return GridSize;
    }
    
    /**
     * Проверить, может ли предмет быть размещен в указанной позиции
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для размера предмета
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param Item - Экземпляр предмета для размещения
     * @param AnchorIndex - Индекс якорной ячейки
     * @param GridWidth - Ширина сетки инвентаря
     * @param GridHeight - Высота сетки инвентаря
     * @return true если предмет может быть размещен в указанной позиции
     */
    bool CanPlaceItemAt(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Item, 
        int32 AnchorIndex, int32 GridWidth, int32 GridHeight)
    {
        // Basic validation of anchor index
        int32 TotalCells = GridWidth * GridHeight;
        if (AnchorIndex < 0 || AnchorIndex >= TotalCells)
        {
            UE_LOG(LogInventoryUtils, VeryVerbose, 
                TEXT("CanPlaceItemAt: Invalid anchor index %d for grid size %dx%d"), 
                AnchorIndex, GridWidth, GridHeight);
            return false;
        }
        
        // Get item size accounting for rotation
        FVector2D ItemSize = GetItemGridSize(WorldContextObject, Item.ItemID, Item.bIsRotated);
        
        // Convert linear index to 2D coordinates
        int32 AnchorX = AnchorIndex % GridWidth;
        int32 AnchorY = AnchorIndex / GridWidth;
        
        // Check that item doesn't extend beyond right edge
        if (AnchorX + ItemSize.X > GridWidth)
        {
            UE_LOG(LogInventoryUtils, VeryVerbose, 
                TEXT("CanPlaceItemAt: Item %s extends beyond right edge: %d + %.0f > %d"), 
                *Item.ItemID.ToString(), AnchorX, ItemSize.X, GridWidth);
            return false;
        }
        
        // Check that item doesn't extend beyond bottom edge
        if (AnchorY + ItemSize.Y > GridHeight)
        {
            UE_LOG(LogInventoryUtils, VeryVerbose, 
                TEXT("CanPlaceItemAt: Item %s extends beyond bottom edge: %d + %.0f > %d"), 
                *Item.ItemID.ToString(), AnchorY, ItemSize.Y, GridHeight);
            return false;
        }
        
        return true;
    }
    
    /**
     * Получить все индексы ячеек занимаемые предметом
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для размера предмета
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param Item - Экземпляр предмета
     * @param GridWidth - Ширина сетки инвентаря
     * @return Массив индексов всех занимаемых ячеек
     */
    TArray<int32> GetOccupiedCellIndices(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Item, int32 GridWidth)
    {
        TArray<int32> OccupiedIndices;
        
        // If item not placed in inventory, return empty array
        if (Item.AnchorIndex == INDEX_NONE)
        {
            return OccupiedIndices;
        }
        
        // Get item size and anchor cell coordinates
        FVector2D ItemSize = GetItemGridSize(WorldContextObject, Item.ItemID, Item.bIsRotated);
        
        int32 AnchorX = Item.AnchorIndex % GridWidth;
        int32 AnchorY = Item.AnchorIndex / GridWidth;
        
        // Reserve memory for optimization
        int32 ExpectedCells = static_cast<int32>(ItemSize.X * ItemSize.Y);
        OccupiedIndices.Reserve(ExpectedCells);
        
        // Iterate over all cells occupied by item
        for (int32 Y = AnchorY; Y < AnchorY + ItemSize.Y; ++Y)
        {
            for (int32 X = AnchorX; X < AnchorX + ItemSize.X; ++X)
            {
                int32 CellIndex = Y * GridWidth + X;
                OccupiedIndices.Add(CellIndex);
            }
        }
        
        return OccupiedIndices;
    }
    
    /**
     * Проверить совместимость предметов для стакинга
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для доступа к DataTable
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param Item1 - Первый экземпляр предмета
     * @param Item2 - Второй экземпляр предмета  
     * @return true если предметы могут быть объединены в один стек
     */
    bool CanStackItems(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Item1, const FSuspenseInventoryItemInstance& Item2)
    {
        // Basic check - items must be same type
        if (Item1.ItemID != Item2.ItemID)
        {
            return false;
        }
        
        // Get stacking data from DataTable
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, Item1.ItemID, UnifiedData))
        {
            UE_LOG(LogInventoryUtils, Warning, 
                TEXT("CanStackItems: Cannot check stacking for unknown item: %s"), 
                *Item1.ItemID.ToString());
            return false;
        }
        
        // Check if item can stack (MaxStackSize > 1)
        bool bCanStack = UnifiedData.MaxStackSize > 1;
        
        // TODO: Add additional checks in future:
        // - Same durability for equipment
        // - Same modifications for weapons
        // - Expiration dates for consumables
        
        return bCanStack;
    }
    
    /**
     * Получить максимальный размер стека для предмета из DataTable
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для доступа к ItemManager
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param ItemID - ID предмета
     * @return Максимальный размер стека (1 = не стакается)
     */
    int32 GetMaxStackSize(const UObject* WorldContextObject, const FName& ItemID)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            return 1; // Safe fallback - not stackable
        }
        
        return UnifiedData.MaxStackSize;
    }
    
    /**
     * Получить вес одного предмета из DataTable
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для доступа к ItemManager
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param ItemID - ID предмета
     * @return Вес одного экземпляра предмета в игровых единицах
     */
    float GetItemWeight(const UObject* WorldContextObject, const FName& ItemID)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            return 1.0f; // Safe fallback - standard weight
        }
        
        return UnifiedData.Weight;
    }
    
    /**
     * Вычислить общий вес экземпляра предмета с учетом количества в стеке
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для поиска веса
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param Instance - Экземпляр предмета
     * @return Общий вес всего стека
     */
    float CalculateInstanceWeight(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Instance)
    {
        float ItemWeight = GetItemWeight(WorldContextObject, Instance.ItemID);
        return ItemWeight * Instance.Quantity;
    }
    
    /**
     * Проверить, разрешен ли предмет в указанном инвентаре
     * 
     * ИЗМЕНЕНО: Теперь требует WorldContextObject для поиска типа предмета
     * 
     * @param WorldContextObject - Валидный UObject с World context
     * @param ItemID - ID предмета для проверки
     * @param AllowedTypes - Разрешенные типы предметов (пустой = все разрешены)
     * @param DisallowedTypes - Запрещенные типы предметов (приоритет над разрешенными)
     * @return true если предмет может быть помещен в этот инвентарь
     */
    bool IsItemAllowedInInventory(const UObject* WorldContextObject, const FName& ItemID, 
        const FGameplayTagContainer& AllowedTypes, const FGameplayTagContainer& DisallowedTypes)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            UE_LOG(LogInventoryUtils, Warning, 
                TEXT("IsItemAllowedInInventory: Cannot check item type filter for unknown item: %s"), 
                *ItemID.ToString());
            return false; // Unknown items not allowed for safety
        }
        
        // First check blacklist (priority)
        if (!DisallowedTypes.IsEmpty() && DisallowedTypes.HasTag(UnifiedData.ItemType))
        {
            return false;
        }
        
        // If whitelist empty, all types allowed (except blacklisted)
        if (AllowedTypes.IsEmpty())
        {
            return true;
        }
        
        // Check whitelist
        return AllowedTypes.HasTag(UnifiedData.ItemType);
    }
}
// SuspenseCoreInventoryLegacyTypes.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.
//
// REFACTORED: Now uses USuspenseCoreDataManager instead of legacy USuspenseItemManager

#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryUtils.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreInventoryUtils, Log, All);

//==============================================================================
// Utility Functions для работы с инвентарем и DataTable интеграцией
//
// ARCHITECTURE:
// - Uses USuspenseCoreDataManager as single source of truth
// - All functions require WorldContextObject for proper subsystem access
// - Optimized for MMO/FPS with minimal allocations
//==============================================================================

namespace InventoryUtils
{
    /**
     * Получить unified данные предмета через USuspenseCoreDataManager
     *
     * @param WorldContextObject - Любой UObject с валидным World context
     * @param ItemID - ID предмета для поиска
     * @param OutData - Выходная структура (legacy format для совместимости)
     * @return true если данные найдены
     */
    bool GetUnifiedItemData(const UObject* WorldContextObject, const FName& ItemID, FSuspenseUnifiedItemData& OutData)
    {
        // Use new Clean Architecture DataManager
        USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(WorldContextObject);
        if (!DataManager)
        {
            UE_LOG(LogSuspenseCoreInventoryUtils, Warning,
                TEXT("GetUnifiedItemData: DataManager not available for item: %s"),
                *ItemID.ToString());
            return false;
        }

        // Get native SuspenseCore item data
        FSuspenseCoreItemData ItemData;
        if (!DataManager->GetItemData(ItemID, ItemData))
        {
            UE_LOG(LogSuspenseCoreInventoryUtils, Verbose,
                TEXT("GetUnifiedItemData: Item '%s' not found"),
                *ItemID.ToString());
            return false;
        }

        // Convert to legacy format for backward compatibility
        OutData.ItemID = ItemData.Identity.ItemID;
        OutData.DisplayName = ItemData.Identity.DisplayName;
        OutData.Description = ItemData.Identity.Description;
        OutData.ItemType = ItemData.Classification.ItemType;
        OutData.Rarity = ItemData.Classification.Rarity;
        OutData.MaxStackSize = ItemData.InventoryProps.MaxStackSize;
        OutData.Weight = ItemData.InventoryProps.Weight;
        OutData.GridSize = ItemData.InventoryProps.GridSize;
        OutData.bIsEquippable = ItemData.bIsEquippable;
        OutData.bIsWeapon = ItemData.bIsWeapon;
        OutData.bIsArmor = ItemData.bIsArmor;
        OutData.bIsConsumable = ItemData.bIsConsumable;

        if (ItemData.bIsWeapon)
        {
            OutData.WeaponArchetype = ItemData.WeaponConfig.WeaponArchetype;
        }

        return true;
    }

    /**
     * Получить максимальную прочность по умолчанию
     */
    float GetDefaultMaxDurability(const FSuspenseUnifiedItemData& UnifiedData)
    {
        // TODO: Replace with AttributeSet values after GAS integration
        if (UnifiedData.bIsWeapon)
        {
            return 150.0f;
        }
        else if (UnifiedData.bIsArmor)
        {
            return 200.0f;
        }
        else if (UnifiedData.bIsEquippable)
        {
            return 100.0f;
        }
        return 0.0f;
    }

    /**
     * Получить стандартную емкость магазина
     */
    int32 GetDefaultAmmoCapacity(const FGameplayTag& WeaponArchetype)
    {
        if (!WeaponArchetype.IsValid())
        {
            return 30;
        }

        FString ArchetypeString = WeaponArchetype.ToString();

        if (ArchetypeString.Contains(TEXT("Rifle")))
        {
            return 30;
        }
        else if (ArchetypeString.Contains(TEXT("Pistol")))
        {
            return 15;
        }
        else if (ArchetypeString.Contains(TEXT("Shotgun")))
        {
            return 8;
        }
        else if (ArchetypeString.Contains(TEXT("Sniper")))
        {
            return 5;
        }
        else if (ArchetypeString.Contains(TEXT("SMG")) || ArchetypeString.Contains(TEXT("Submachine")))
        {
            return 25;
        }
        else if (ArchetypeString.Contains(TEXT("LMG")) || ArchetypeString.Contains(TEXT("Machine")))
        {
            return 100;
        }

        return 30;
    }

    /**
     * Инициализировать runtime свойства экземпляра
     */
    void InitializeRuntimeProperties(FSuspenseInventoryItemInstance& Instance, const FSuspenseUnifiedItemData& UnifiedData)
    {
        if (UnifiedData.bIsEquippable)
        {
            float MaxDurability = GetDefaultMaxDurability(UnifiedData);
            Instance.SetRuntimeProperty(TEXT("MaxDurability"), MaxDurability);
            Instance.SetRuntimeProperty(TEXT("Durability"), MaxDurability);
        }

        if (UnifiedData.bIsWeapon)
        {
            int32 MaxAmmo = GetDefaultAmmoCapacity(UnifiedData.WeaponArchetype);
            Instance.SetRuntimeProperty(TEXT("MaxAmmo"), static_cast<float>(MaxAmmo));
            Instance.SetRuntimeProperty(TEXT("Ammo"), static_cast<float>(MaxAmmo));
        }

        if (UnifiedData.bIsConsumable)
        {
            Instance.SetRuntimeProperty(TEXT("Charges"), static_cast<float>(Instance.Quantity));
        }

        Instance.SetRuntimeProperty(TEXT("Condition"), 1.0f);
    }

    /**
     * Создать экземпляр предмета через USuspenseCoreDataManager
     */
    FSuspenseInventoryItemInstance CreateItemInstance(const UObject* WorldContextObject, const FName& ItemID, int32 Quantity)
    {
        FSuspenseInventoryItemInstance Instance = FSuspenseInventoryItemInstance::Create(ItemID, Quantity);

        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            UE_LOG(LogSuspenseCoreInventoryUtils, Warning,
                TEXT("CreateItemInstance: Item '%s' not found in DataManager"),
                *ItemID.ToString());
            return Instance;
        }

        InitializeRuntimeProperties(Instance, UnifiedData);

        UE_LOG(LogSuspenseCoreInventoryUtils, Verbose,
            TEXT("CreateItemInstance: Created %s x%d"),
            *ItemID.ToString(), Quantity);

        return Instance;
    }

    /**
     * Получить размер предмета в сетке
     */
    FVector2D GetItemGridSize(const UObject* WorldContextObject, const FName& ItemID, bool bIsRotated)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            return FVector2D(1, 1);
        }

        FVector2D GridSize(UnifiedData.GridSize.X, UnifiedData.GridSize.Y);

        if (bIsRotated)
        {
            GridSize = FVector2D(GridSize.Y, GridSize.X);
        }

        return GridSize;
    }

    /**
     * Проверить возможность размещения предмета
     */
    bool CanPlaceItemAt(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Item,
        int32 AnchorIndex, int32 GridWidth, int32 GridHeight)
    {
        int32 TotalCells = GridWidth * GridHeight;
        if (AnchorIndex < 0 || AnchorIndex >= TotalCells)
        {
            return false;
        }

        FVector2D ItemSize = GetItemGridSize(WorldContextObject, Item.ItemID, Item.bIsRotated);

        int32 AnchorX = AnchorIndex % GridWidth;
        int32 AnchorY = AnchorIndex / GridWidth;

        if (AnchorX + ItemSize.X > GridWidth)
        {
            return false;
        }

        if (AnchorY + ItemSize.Y > GridHeight)
        {
            return false;
        }

        return true;
    }

    /**
     * Получить все занимаемые ячейки
     */
    TArray<int32> GetOccupiedCellIndices(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Item, int32 GridWidth)
    {
        TArray<int32> OccupiedIndices;

        if (Item.AnchorIndex == INDEX_NONE)
        {
            return OccupiedIndices;
        }

        FVector2D ItemSize = GetItemGridSize(WorldContextObject, Item.ItemID, Item.bIsRotated);

        int32 AnchorX = Item.AnchorIndex % GridWidth;
        int32 AnchorY = Item.AnchorIndex / GridWidth;

        OccupiedIndices.Reserve(static_cast<int32>(ItemSize.X * ItemSize.Y));

        for (int32 Y = AnchorY; Y < AnchorY + ItemSize.Y; ++Y)
        {
            for (int32 X = AnchorX; X < AnchorX + ItemSize.X; ++X)
            {
                OccupiedIndices.Add(Y * GridWidth + X);
            }
        }

        return OccupiedIndices;
    }

    /**
     * Проверить совместимость для стакинга
     */
    bool CanStackItems(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Item1, const FSuspenseInventoryItemInstance& Item2)
    {
        if (Item1.ItemID != Item2.ItemID)
        {
            return false;
        }

        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, Item1.ItemID, UnifiedData))
        {
            return false;
        }

        return UnifiedData.MaxStackSize > 1;
    }

    /**
     * Получить максимальный размер стека
     */
    int32 GetMaxStackSize(const UObject* WorldContextObject, const FName& ItemID)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            return 1;
        }
        return UnifiedData.MaxStackSize;
    }

    /**
     * Получить вес предмета
     */
    float GetItemWeight(const UObject* WorldContextObject, const FName& ItemID)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            return 1.0f;
        }
        return UnifiedData.Weight;
    }

    /**
     * Вычислить общий вес экземпляра
     */
    float CalculateInstanceWeight(const UObject* WorldContextObject, const FSuspenseInventoryItemInstance& Instance)
    {
        return GetItemWeight(WorldContextObject, Instance.ItemID) * Instance.Quantity;
    }

    /**
     * Проверить разрешён ли предмет в инвентаре
     */
    bool IsItemAllowedInInventory(const UObject* WorldContextObject, const FName& ItemID,
        const FGameplayTagContainer& AllowedTypes, const FGameplayTagContainer& DisallowedTypes)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (!GetUnifiedItemData(WorldContextObject, ItemID, UnifiedData))
        {
            return false;
        }

        if (!DisallowedTypes.IsEmpty() && DisallowedTypes.HasTag(UnifiedData.ItemType))
        {
            return false;
        }

        if (AllowedTypes.IsEmpty())
        {
            return true;
        }

        return AllowedTypes.HasTag(UnifiedData.ItemType);
    }
}

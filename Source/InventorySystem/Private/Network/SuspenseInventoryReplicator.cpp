// MedComInventory/Network/InventoryReplicator.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Network/SuspenseInventoryReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Interfaces/Inventory/ISuspenseInventoryItem.h"
#include "Base/SuspenseItemBase.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

//==============================================================================
// FReplicatedCellsState Implementation
//==============================================================================

void FReplicatedCellsState::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
    if (OwnerComponent)
    {
        // Уведомляем владельца об изменениях
        OwnerComponent->OnReplicationUpdated.Broadcast();
        
        UE_LOG(LogInventory, Verbose, TEXT("ReplicatedCells: Added %d cells, total size: %d"), 
            AddedIndices.Num(), FinalSize);
    }
}

void FReplicatedCellsState::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
    if (OwnerComponent)
    {
        // Уведомляем владельца об изменениях
        OwnerComponent->OnReplicationUpdated.Broadcast();
        
        UE_LOG(LogInventory, Verbose, TEXT("ReplicatedCells: Changed %d cells"), 
            ChangedIndices.Num());
    }
}

void FReplicatedCellsState::PostReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
    if (OwnerComponent)
    {
        // Уведомляем владельца об изменениях
        OwnerComponent->OnReplicationUpdated.Broadcast();
        
        UE_LOG(LogInventory, Verbose, TEXT("ReplicatedCells: Removed %d cells, remaining: %d"), 
            RemovedIndices.Num(), FinalSize);
    }
}

//==============================================================================
// FReplicatedItemsMetaState Implementation
//==============================================================================

void FReplicatedItemsMetaState::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
    if (OwnerComponent)
    {
        // Логируем добавленные предметы с расширенной информацией
        for (int32 Index : AddedIndices)
        {
            if (Items.IsValidIndex(Index))
            {
                const FReplicatedItemMeta& Meta = Items[Index];
                UE_LOG(LogInventory, Log, TEXT("ReplicatedItems: Added item %s at index %d (stack: %d, weight: %.2f, instance: %s)"), 
                    *Meta.ItemID.ToString(), Index, Meta.Stack, Meta.ItemWeight, 
                    *Meta.InstanceID.ToString().Left(8));
            }
        }
        
        // Уведомляем владельца
        OwnerComponent->OnReplicationUpdated.Broadcast();
    }
}

void FReplicatedItemsMetaState::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
    if (OwnerComponent)
    {
        // Логируем изменённые предметы с детальной информацией
        for (int32 Index : ChangedIndices)
        {
            if (Items.IsValidIndex(Index))
            {
                const FReplicatedItemMeta& Meta = Items[Index];
                UE_LOG(LogInventory, Verbose, TEXT("ReplicatedItems: Changed item %s (stack: %d, durability: %.1f%%, runtime props: %d)"), 
                    *Meta.ItemID.ToString(), Meta.Stack, Meta.GetDurabilityAsPercent() * 100.0f, Meta.RuntimePropertiesCount);
            }
        }
        
        // Уведомляем владельца
        OwnerComponent->OnReplicationUpdated.Broadcast();
    }
}

void FReplicatedItemsMetaState::PostReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
    if (OwnerComponent)
    {
        UE_LOG(LogInventory, Log, TEXT("ReplicatedItems: Removed %d items, remaining: %d"), 
            RemovedIndices.Num(), FinalSize);
        
        // Уведомляем владельца
        OwnerComponent->OnReplicationUpdated.Broadcast();
    }
}

//==============================================================================
// FReplicatedItemMeta Static Methods - ПОЛНОСТЬЮ ОБНОВЛЕНО
//==============================================================================

FReplicatedItemMeta FReplicatedItemMeta::FromItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, USuspenseItemManager* ItemManager)
{
    FReplicatedItemMeta Result;
    
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogInventory, Warning, TEXT("FromItemInstance: Invalid ItemInstance provided"));
        return Result;
    }
    
    // Устанавливаем основные данные из instance
    Result.ItemID = ItemInstance.ItemID;
    Result.InstanceID = ItemInstance.InstanceID;
    Result.Stack = ItemInstance.Quantity;
    Result.AnchorIndex = ItemInstance.AnchorIndex;
    
    // Устанавливаем флаги состояния из instance
    Result.SetRotated(ItemInstance.bIsRotated);
    
    // Если есть ItemManager, получаем данные из DataTable
    if (ItemManager)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (ItemManager->GetUnifiedItemData(ItemInstance.ItemID, UnifiedData))
        {
            // Устанавливаем размер сетки из DataTable
            Result.SetGridSize(UnifiedData.GridSize);
            
            // Устанавливаем вес предмета из DataTable
            Result.ItemWeight = UnifiedData.Weight;
            
            // Устанавливаем флаги данных на основе свойств DataTable
            if (UnifiedData.MaxStackSize > 1)
                Result.ItemDataFlags |= EItemDataFlags::Stackable;
            if (UnifiedData.bIsConsumable)
                Result.ItemDataFlags |= EItemDataFlags::Consumable;
            if (UnifiedData.bIsEquippable)
                Result.ItemDataFlags |= EItemDataFlags::Equippable;
            if (UnifiedData.bCanDrop)
                Result.ItemDataFlags |= EItemDataFlags::Droppable;
            if (UnifiedData.bCanTrade)
                Result.ItemDataFlags |= EItemDataFlags::Tradeable;
            if (UnifiedData.bIsQuestItem)
                Result.ItemDataFlags |= EItemDataFlags::QuestItem;
            
            // Определяем если это материал для крафта на основе тегов
            if (UnifiedData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Category.CraftingMaterial"))))
                Result.ItemDataFlags |= EItemDataFlags::CraftingMaterial;
        }
        else
        {
            UE_LOG(LogInventory, Warning, TEXT("FromItemInstance: Could not find DataTable entry for ItemID: %s"), 
                *ItemInstance.ItemID.ToString());
        }
    }
    
    // Обрабатываем runtime properties
    if (ItemInstance.RuntimeProperties.Num() > 0)
    {
        Result.SetHasRuntimeProperties(true);
        Result.RuntimePropertiesCount = FMath::Min(ItemInstance.RuntimeProperties.Num(), 255);
    
        // Упаковываем критически важные runtime properties
        for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
        {
            // Конвертируем часто используемые имена свойств в ключи
            if (PropertyPair.Key == TEXT("Durability"))
            {
                // ИСПРАВЛЕНО: используем прямое значение без создания локальной переменной
                Result.SetDurabilityFromPercent(PropertyPair.Value / 100.0f);
            }
            else if (PropertyPair.Key == TEXT("Ammo"))
            {
                Result.SetPackedRuntimeProperty(ERuntimePropertyKeys::AmmoCount, PropertyPair.Value);
            }
            else if (PropertyPair.Key == TEXT("ReserveAmmo"))
            {
                Result.SetPackedRuntimeProperty(ERuntimePropertyKeys::ReserveAmmo, PropertyPair.Value);
            }
            else if (PropertyPair.Key == TEXT("ChargeCurrent"))
            {
                Result.SetPackedRuntimeProperty(ERuntimePropertyKeys::ChargeCurrent, PropertyPair.Value);
            }
            else if (PropertyPair.Key == TEXT("ChargeMax"))
            {
                Result.SetPackedRuntimeProperty(ERuntimePropertyKeys::ChargeMax, PropertyPair.Value);
            }
        }
    
        // Проверяем если предмет был модифицирован от базового состояния
        Result.SetIsModified(ItemInstance.RuntimeProperties.Num() > 0);
    }
    
    return Result;
}

FReplicatedItemMeta FReplicatedItemMeta::FromItemInterface(const ISuspenseInventoryItemInterface* ItemInterface)
{
    FReplicatedItemMeta Result;
    
    if (!ItemInterface)
    {
        UE_LOG(LogInventory, Warning, TEXT("FromItemInterface: Null ItemInterface provided"));
        return Result;
    }
    
    // Получаем основные данные через interface
    Result.ItemID = ItemInterface->GetItemID();
    Result.Stack = ItemInterface->GetAmount();
    Result.AnchorIndex = ItemInterface->GetAnchorIndex();
    Result.InstanceID = ItemInterface->GetInstanceID();
    
    // Устанавливаем флаги состояния
    Result.SetRotated(ItemInterface->IsRotated());
    Result.SetHasSavedAmmoState(ItemInterface->HasSavedAmmoState());
    
    // Получаем размер сетки через interface
    FVector2D GridSize = ItemInterface->GetEffectiveGridSize();
    Result.SetGridSize(GridSize);
    
    // Получаем вес предмета
    Result.ItemWeight = ItemInterface->GetWeight();
    
    // Получаем unified данные из DataTable через interface
    FSuspenseUnifiedItemData UnifiedData;
    if (ItemInterface->GetItemData(UnifiedData))
    {
        // Устанавливаем флаги данных из DataTable
        if (UnifiedData.MaxStackSize > 1)
            Result.ItemDataFlags |= EItemDataFlags::Stackable;
        if (UnifiedData.bIsConsumable)
            Result.ItemDataFlags |= EItemDataFlags::Consumable;
        if (UnifiedData.bIsEquippable)
            Result.ItemDataFlags |= EItemDataFlags::Equippable;
        if (UnifiedData.bCanDrop)
            Result.ItemDataFlags |= EItemDataFlags::Droppable;
        if (UnifiedData.bCanTrade)
            Result.ItemDataFlags |= EItemDataFlags::Tradeable;
        if (UnifiedData.bIsQuestItem)
            Result.ItemDataFlags |= EItemDataFlags::QuestItem;
    }
    
    // Получаем runtime свойства через interface
    if (ItemInterface->HasRuntimeProperty(TEXT("Durability")))
    {
        float ItemDurabilityPercent = ItemInterface->GetDurabilityPercent();
        Result.SetDurabilityFromPercent(ItemDurabilityPercent);
        Result.SetHasRuntimeProperties(true);
    }
    
    // Получаем ammo данные если доступны
    if (ItemInterface->IsWeapon())
    {
        int32 CurrentAmmo = ItemInterface->GetCurrentAmmo();
        if (CurrentAmmo > 0)
        {
            Result.SetPackedRuntimeProperty(ERuntimePropertyKeys::AmmoCount, static_cast<float>(CurrentAmmo));
            Result.SetHasRuntimeProperties(true);
        }
    }
    
    return Result;
}

FReplicatedItemMeta FReplicatedItemMeta::FromUnifiedItemData(const FSuspenseUnifiedItemData& ItemData, int32 Amount, int32 AnchorIdx, const FGuid& InstanceID)
{
    FReplicatedItemMeta Result;
    
    // Устанавливаем основные данные
    Result.ItemID = ItemData.ItemID;
    Result.InstanceID = InstanceID;
    Result.Stack = Amount;
    Result.AnchorIndex = AnchorIdx;
    
    // Устанавливаем размер сетки из DataTable
    Result.SetGridSize(ItemData.GridSize);
    
    // Устанавливаем вес предмета из DataTable
    Result.ItemWeight = ItemData.Weight;
    
    // Устанавливаем флаги данных из unified структуры
    if (ItemData.MaxStackSize > 1)
        Result.ItemDataFlags |= EItemDataFlags::Stackable;
    if (ItemData.bIsConsumable)
        Result.ItemDataFlags |= EItemDataFlags::Consumable;
    if (ItemData.bIsEquippable)
        Result.ItemDataFlags |= EItemDataFlags::Equippable;
    if (ItemData.bCanDrop)
        Result.ItemDataFlags |= EItemDataFlags::Droppable;
    if (ItemData.bCanTrade)
        Result.ItemDataFlags |= EItemDataFlags::Tradeable;
    if (ItemData.bIsQuestItem)
        Result.ItemDataFlags |= EItemDataFlags::QuestItem;
    
    return Result;
}

FSuspenseInventoryItemInstance FReplicatedItemMeta::ToItemInstance() const
{
    FSuspenseInventoryItemInstance Result;
    
    // Устанавливаем основные данные
    Result.ItemID = ItemID;
    Result.InstanceID = InstanceID;
    Result.Quantity = Stack;
    Result.AnchorIndex = AnchorIndex;
    Result.bIsRotated = IsRotated();
    
    // Восстанавливаем runtime properties из упакованных данных
    if (HasRuntimeProperties())
    {
        // Восстанавливаем durability
        if (ItemHasDurability())
        {
            // ИСПРАВЛЕНО: Используем другое имя для локальной переменной
            float CurrentDurabilityPercent = GetDurabilityAsPercent();
            if (CurrentDurabilityPercent < 1.0f)
            {
                Result.RuntimeProperties.Add(TEXT("Durability"), CurrentDurabilityPercent * 100.0f);
            }
        }
        
        // Восстанавливаем упакованные свойства
        for (const auto& PackedProperty : PackedRuntimeProperties)
        {
            switch (PackedProperty.Key)
            {
                case ERuntimePropertyKeys::AmmoCount:
                    Result.RuntimeProperties.Add(TEXT("Ammo"), PackedProperty.Value);
                    break;
                case ERuntimePropertyKeys::ReserveAmmo:
                    Result.RuntimeProperties.Add(TEXT("ReserveAmmo"), PackedProperty.Value);
                    break;
                case ERuntimePropertyKeys::ChargeCurrent:
                    Result.RuntimeProperties.Add(TEXT("ChargeCurrent"), PackedProperty.Value);
                    break;
                case ERuntimePropertyKeys::ChargeMax:
                    Result.RuntimeProperties.Add(TEXT("ChargeMax"), PackedProperty.Value);
                    break;
                default:
                    // Пользовательские свойства
                    FString PropertyName = FString::Printf(TEXT("UserProperty%d"), 
                        PackedProperty.Key - ERuntimePropertyKeys::UserProperty1 + 1);
                    Result.RuntimeProperties.Add(FName(*PropertyName), PackedProperty.Value);
                    break;
            }
        }
    }
    
    return Result;
}

void FReplicatedItemMeta::UpdateFromItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, USuspenseItemManager* ItemManager)
{
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogInventory, Warning, TEXT("UpdateFromItemInstance: Invalid ItemInstance"));
        return;
    }
    
    // Обновляем основные данные
    Stack = ItemInstance.Quantity;
    AnchorIndex = ItemInstance.AnchorIndex;
    SetRotated(ItemInstance.bIsRotated);
    
    // Очищаем и обновляем runtime properties
    PackedRuntimeProperties.Empty();
    RuntimePropertiesCount = 0;
    SetHasRuntimeProperties(false);
    
    if (ItemInstance.RuntimeProperties.Num() > 0)
    {
        SetHasRuntimeProperties(true);
        RuntimePropertiesCount = FMath::Min(ItemInstance.RuntimeProperties.Num(), 255);
        
        // Упаковываем обновленные runtime properties
        for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
        {
            if (PropertyPair.Key == TEXT("Durability"))
            {
                SetDurabilityFromPercent(PropertyPair.Value / 100.0f);
            }
            else if (PropertyPair.Key == TEXT("Ammo"))
            {
                SetPackedRuntimeProperty(ERuntimePropertyKeys::AmmoCount, PropertyPair.Value);
            }
            else if (PropertyPair.Key == TEXT("ReserveAmmo"))
            {
                SetPackedRuntimeProperty(ERuntimePropertyKeys::ReserveAmmo, PropertyPair.Value);
            }
            // Добавляем поддержку для других свойств по мере необходимости
        }
        
        SetIsModified(true);
    }
}

//==============================================================================
// FInventoryReplicatedState Implementation - ОБНОВЛЕНО
//==============================================================================

void FInventoryReplicatedState::Initialize(USuspenseInventoryReplicator* InOwner, int32 GridWidth, int32 GridHeight)
{
    // Устанавливаем ссылки на owner
    OwnerComponent = InOwner;
    CellsState.OwnerComponent = InOwner;
    ItemsState.OwnerComponent = InOwner;
    
    // Инициализируем ячейки сетки
    int32 TotalCells = GridWidth * GridHeight;
    CellsState.Cells.SetNum(TotalCells);
    
    // Очищаем все ячейки
    for (int32 i = 0; i < TotalCells; i++)
    {
        CellsState.Cells[i].Clear();
    }
    
    // Очищаем данные о предметах
    ItemsState.Items.Empty();
    ItemInstances.Empty();
    ItemObjects.Empty();
    
    // Помечаем для репликации
    MarkArrayDirty();
    
    UE_LOG(LogInventory, Log, TEXT("ReplicatedState: Initialized with grid %dx%d (%d cells)"), 
        GridWidth, GridHeight, TotalCells);
}

void FInventoryReplicatedState::Reset()
{
    // Очищаем ячейки
    for (auto& Cell : CellsState.Cells)
    {
        Cell.Clear();
    }
    
    // Очищаем все данные о предметах
    ItemsState.Items.Empty();
    ItemInstances.Empty();
    ItemObjects.Empty();
    
    // Помечаем для репликации
    MarkArrayDirty();
    
    UE_LOG(LogInventory, Log, TEXT("ReplicatedState: Reset complete"));
}

int32 FInventoryReplicatedState::AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    if (!ItemInstance.IsValid() || AnchorIndex < 0 || AnchorIndex >= CellsState.Cells.Num())
    {
        UE_LOG(LogInventory, Warning, TEXT("AddItemInstance: Invalid parameters - ItemID:%s, Anchor:%d"), 
            *ItemInstance.ItemID.ToString(), AnchorIndex);
        return INDEX_NONE;
    }
    
    // Получаем ItemManager для доступа к размерам из DataTable
    USuspenseItemManager* ItemManager = OwnerComponent ? OwnerComponent->GetItemManager() : nullptr;
    
    // Создаем metadata из instance
    FReplicatedItemMeta Meta = FReplicatedItemMeta::FromItemInstance(ItemInstance, ItemManager);
    Meta.AnchorIndex = AnchorIndex;
    
    // Добавляем metadata
    int32 MetaIndex = ItemsState.Items.Add(Meta);
    
    // Добавляем instance в наш локальный массив
    if (ItemInstances.Num() <= MetaIndex)
    {
        ItemInstances.SetNum(MetaIndex + 1);
    }
    ItemInstances[MetaIndex] = ItemInstance;
    
    // Поддерживаем legacy массив objects для backward compatibility
    if (ItemObjects.Num() <= MetaIndex)
    {
        ItemObjects.SetNum(MetaIndex + 1);
    }
    ItemObjects[MetaIndex] = nullptr; // Будет установлен если нужен legacy доступ
    
    // Получаем размер предмета из DataTable или из metadata
    FVector2D ItemSize = Meta.GetGridSize();
    if (ItemManager)
    {
        FSuspenseUnifiedItemData UnifiedData;
        if (ItemManager->GetUnifiedItemData(ItemInstance.ItemID, UnifiedData))
        {
            // Применяем поворот если нужно
            if (ItemInstance.bIsRotated)
            {
                ItemSize = FVector2D(UnifiedData.GridSize.Y, UnifiedData.GridSize.X);
            }
            else
            {
                ItemSize = FVector2D(UnifiedData.GridSize.X, UnifiedData.GridSize.Y);
            }
        }
    }
    
    // Вычисляем размеры сетки
    int32 GridWidth = FMath::Sqrt(static_cast<float>(CellsState.Cells.Num()));
    int32 AnchorX = AnchorIndex % GridWidth;
    int32 AnchorY = AnchorIndex / GridWidth;
    
    // Устанавливаем ячейки
    int32 ItemWidth = FMath::CeilToInt(ItemSize.X);
    int32 ItemHeight = FMath::CeilToInt(ItemSize.Y);
    
    for (int32 Y = 0; Y < ItemHeight; Y++)
    {
        for (int32 X = 0; X < ItemWidth; X++)
        {
            int32 CellIndex = (AnchorY + Y) * GridWidth + (AnchorX + X);
            if (CellIndex >= 0 && CellIndex < CellsState.Cells.Num())
            {
                CellsState.Cells[CellIndex] = FCompactReplicatedCell(MetaIndex, FIntPoint(X, Y));
            }
        }
    }
    
    // Помечаем массивы как измененные
    MarkArrayDirty();
    
    UE_LOG(LogInventory, Log, TEXT("AddItemInstance: Added %s at index %d (meta:%d, size:%dx%d, weight:%.2f, instance:%s)"), 
        *ItemInstance.ItemID.ToString(), AnchorIndex, MetaIndex, ItemWidth, ItemHeight, 
        Meta.ItemWeight, *ItemInstance.InstanceID.ToString().Left(8));
    
    return MetaIndex;
}

int32 FInventoryReplicatedState::AddItem(UObject* ItemObject, const FReplicatedItemMeta& Meta, int32 AnchorIndex)
{
    // Legacy метод с автоматическим получением размера из DataTable
    if (!ItemObject || AnchorIndex < 0 || AnchorIndex >= CellsState.Cells.Num())
    {
        UE_LOG(LogInventory, Warning, TEXT("AddItem: Invalid parameters - Object:%s, Anchor:%d"), 
            ItemObject ? TEXT("Valid") : TEXT("Null"), AnchorIndex);
        return INDEX_NONE;
    }
    
    // Создаем FSuspenseInventoryItemInstance для consistency
    FSuspenseInventoryItemInstance ItemInstance;
    ItemInstance.ItemID = Meta.ItemID;
    ItemInstance.InstanceID = Meta.InstanceID;
    ItemInstance.Quantity = Meta.Stack;
    ItemInstance.AnchorIndex = AnchorIndex;
    ItemInstance.bIsRotated = Meta.IsRotated();
    
    // Конвертируем metadata обратно в runtime properties
    ItemInstance = Meta.ToItemInstance();
    ItemInstance.AnchorIndex = AnchorIndex;
    
    // Используем новый метод
    int32 MetaIndex = AddItemInstance(ItemInstance, AnchorIndex);
    
    // Сохраняем legacy object reference
    if (MetaIndex != INDEX_NONE && ItemObjects.IsValidIndex(MetaIndex))
    {
        ItemObjects[MetaIndex] = ItemObject;
    }
    
    return MetaIndex;
}

bool FInventoryReplicatedState::UpdateItemInstance(int32 MetaIndex, const FSuspenseInventoryItemInstance& NewInstance)
{
    if (MetaIndex < 0 || MetaIndex >= ItemsState.Items.Num())
    {
        return false;
    }
    
    // Обновляем metadata из нового instance
    USuspenseItemManager* ItemManager = OwnerComponent ? OwnerComponent->GetItemManager() : nullptr;
    ItemsState.Items[MetaIndex].UpdateFromItemInstance(NewInstance, ItemManager);
    
    // Обновляем локальный instance
    if (ItemInstances.IsValidIndex(MetaIndex))
    {
        ItemInstances[MetaIndex] = NewInstance;
    }
    
    // Помечаем как измененный для репликации
    ItemsState.MarkItemDirty(ItemsState.Items[MetaIndex]);
    
    UE_LOG(LogInventory, Verbose, TEXT("UpdateItemInstance: Updated %s (meta:%d, stack:%d, runtime props:%d)"), 
        *NewInstance.ItemID.ToString(), MetaIndex, NewInstance.Quantity, NewInstance.RuntimeProperties.Num());
    
    return true;
}

const FSuspenseInventoryItemInstance* FInventoryReplicatedState::GetItemInstance(int32 MetaIndex) const
{
    if (ItemInstances.IsValidIndex(MetaIndex))
    {
        return &ItemInstances[MetaIndex];
    }
    return nullptr;
}

FSuspenseInventoryItemInstance* FInventoryReplicatedState::GetItemInstance(int32 MetaIndex)
{
    if (ItemInstances.IsValidIndex(MetaIndex))
    {
        return &ItemInstances[MetaIndex];
    }
    return nullptr;
}

int32 FInventoryReplicatedState::FindMetaIndexByInstanceID(const FGuid& InstanceID) const
{
    if (!InstanceID.IsValid())
    {
        return INDEX_NONE;
    }
    
    // Поиск по metadata
    for (int32 i = 0; i < ItemsState.Items.Num(); i++)
    {
        if (ItemsState.Items[i].InstanceID == InstanceID && ItemsState.Items[i].Stack > 0)
        {
            return i;
        }
    }
    
    return INDEX_NONE;
}

bool FInventoryReplicatedState::AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated) const
{
    // Получаем размер предмета из DataTable
    USuspenseItemManager* ItemManager = OwnerComponent ? OwnerComponent->GetItemManager() : nullptr;
    if (!ItemManager)
    {
        UE_LOG(LogInventory, Warning, TEXT("AreCellsFreeForItem: No ItemManager available"));
        return false;
    }
    
    FSuspenseUnifiedItemData UnifiedData;
    if (!ItemManager->GetUnifiedItemData(ItemID, UnifiedData))
    {
        UE_LOG(LogInventory, Warning, TEXT("AreCellsFreeForItem: Could not find data for ItemID: %s"), *ItemID.ToString());
        return false;
    }
    
    // Применяем поворот к размеру
    FVector2D ItemSize;
    if (bIsRotated)
    {
        ItemSize = FVector2D(UnifiedData.GridSize.Y, UnifiedData.GridSize.X);
    }
    else
    {
        ItemSize = FVector2D(UnifiedData.GridSize.X, UnifiedData.GridSize.Y);
    }
    
    return AreCellsFree(StartIndex, ItemSize);
}

void FInventoryReplicatedState::SynchronizeWithItemManager(USuspenseItemManager* ItemManager)
{
    if (!ItemManager)
    {
        return;
    }
    
    bool bNeedsUpdate = false;
    
    // Обновляем все metadata с актуальными данными из DataTable
    for (int32 i = 0; i < ItemsState.Items.Num(); i++)
    {
        FReplicatedItemMeta& Meta = ItemsState.Items[i];
        if (!Meta.ItemID.IsNone())
        {
            FSuspenseUnifiedItemData UnifiedData;
            if (ItemManager->GetUnifiedItemData(Meta.ItemID, UnifiedData))
            {
                // Обновляем вес если изменился в DataTable
                if (FMath::Abs(Meta.ItemWeight - UnifiedData.Weight) > 0.01f)
                {
                    Meta.ItemWeight = UnifiedData.Weight;
                    bNeedsUpdate = true;
                }
                
                // Обновляем размер сетки если изменился
                FIntPoint CurrentSize = Meta.GetGridSizeInt();
                if (CurrentSize != UnifiedData.GridSize)
                {
                    Meta.SetGridSize(UnifiedData.GridSize);
                    bNeedsUpdate = true;
                }
            }
        }
    }
    
    if (bNeedsUpdate)
    {
        MarkArrayDirty();
        UE_LOG(LogInventory, Log, TEXT("SynchronizeWithItemManager: Updated metadata from DataTable changes"));
    }
}

bool FInventoryReplicatedState::ValidateIntegrity(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();
    bool bIsValid = true;
    
    // Проверяем соответствие между metadata и instances
    if (ItemsState.Items.Num() != ItemInstances.Num())
    {
        OutErrors.Add(FString::Printf(TEXT("Metadata count (%d) doesn't match instances count (%d)"), 
            ItemsState.Items.Num(), ItemInstances.Num()));
        bIsValid = false;
    }
    
    // Проверяем валидность каждого metadata
    for (int32 i = 0; i < ItemsState.Items.Num(); i++)
    {
        const FReplicatedItemMeta& Meta = ItemsState.Items[i];
        
        if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
        {
            // Проверяем instance ID
            if (!Meta.InstanceID.IsValid())
            {
                OutErrors.Add(FString::Printf(TEXT("Item at index %d has invalid InstanceID"), i));
                bIsValid = false;
            }
            
            // Проверяем соответствие с local instance
            if (ItemInstances.IsValidIndex(i))
            {
                const FSuspenseInventoryItemInstance& Instance = ItemInstances[i];
                if (Instance.ItemID != Meta.ItemID)
                {
                    OutErrors.Add(FString::Printf(TEXT("ItemID mismatch at index %d: Meta=%s, Instance=%s"), 
                        i, *Meta.ItemID.ToString(), *Instance.ItemID.ToString()));
                    bIsValid = false;
                }
                
                if (Instance.Quantity != Meta.Stack)
                {
                    OutErrors.Add(FString::Printf(TEXT("Quantity mismatch at index %d: Meta=%d, Instance=%d"), 
                        i, Meta.Stack, Instance.Quantity));
                    bIsValid = false;
                }
            }
        }
    }
    
    return bIsValid;
}

// Остальные методы FInventoryReplicatedState остаются без изменений...
// (RemoveItem, UpdateItem, FindMetaIndexByObject, FindMetaIndexByItemID, AreCellsFree)

//==============================================================================
// USuspenseInventoryReplicator Implementation - ОБНОВЛЕНО
//==============================================================================

USuspenseInventoryReplicator::USuspenseInventoryReplicator()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
    
    NetworkUpdateInterval = 0.1f;
    NetworkUpdateTimer = 0.0f;
    bNetUpdateNeeded = false;
    bForceFullResync = false;
    
    ItemManager = nullptr;
    ReplicationUpdateCount = 0;
    BytesSentThisFrame = 0;
    LastUpdateTime = 0.0f;
    
    SetIsReplicatedByDefault(true);
}

void USuspenseInventoryReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Реплицируем состояние всем клиентам
    DOREPLIFETIME(USuspenseInventoryReplicator, ReplicationState);
}

void USuspenseInventoryReplicator::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Обрабатываем сетевые обновления только на сервере
    if (GetOwner() && GetOwner()->HasAuthority() && (bNetUpdateNeeded || bForceFullResync))
    {
        NetworkUpdateTimer += DeltaTime;
        if (NetworkUpdateTimer >= NetworkUpdateInterval || bForceFullResync)
        {
            // Принудительное обновление сети
            GetOwner()->ForceNetUpdate();
            
            // Обновляем статистику
            UpdateReplicationStats();
            
            // Сбрасываем таймер и флаги
            NetworkUpdateTimer = 0.0f;
            bNetUpdateNeeded = false;
            bForceFullResync = false;
            
            UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryReplicator: Network update sent (Update #%d)"), 
                ReplicationUpdateCount);
        }
    }
    
    // Периодическая очистка устаревших данных
    static float CleanupTimer = 0.0f;
    CleanupTimer += DeltaTime;
    if (CleanupTimer >= 30.0f) // Каждые 30 секунд
    {
        CleanupStaleData();
        CleanupTimer = 0.0f;
    }
}

bool USuspenseInventoryReplicator::Initialize(int32 GridWidth, int32 GridHeight, USuspenseItemManager* InItemManager)
{
    if (GridWidth <= 0 || GridHeight <= 0)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryReplicator: Invalid grid dimensions %dx%d"), 
            GridWidth, GridHeight);
        return false;
    }
    
    // Устанавливаем ItemManager
    if (InItemManager)
    {
        ItemManager = InItemManager;
    }
    else
    {
        // Пытаемся получить ItemManager автоматически
        ItemManager = GetOrCreateItemManager();
    }
    
    // Инициализируем состояние репликации
    ReplicationState.Initialize(this, GridWidth, GridHeight);
    
    UE_LOG(LogInventory, Log, TEXT("InventoryReplicator: Initialized with grid %dx%d (ItemManager: %s)"), 
        GridWidth, GridHeight, ItemManager ? TEXT("Available") : TEXT("Not Available"));
    return true;
}

int32 USuspenseInventoryReplicator::AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        int32 MetaIndex = ReplicationState.AddItemInstance(ItemInstance, AnchorIndex);
        if (MetaIndex != INDEX_NONE)
        {
            RequestNetUpdate();
        }
        return MetaIndex;
    }
    
    UE_LOG(LogInventory, Warning, TEXT("AddItemInstance: Called on non-authoritative instance"));
    return INDEX_NONE;
}

bool USuspenseInventoryReplicator::UpdateItemRuntimeProperties(int32 MetaIndex, const TMap<FName, float>& NewProperties)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogInventory, Warning, TEXT("UpdateItemRuntimeProperties: Called on non-authoritative instance"));
        return false;
    }
    
    FSuspenseInventoryItemInstance* Instance = ReplicationState.GetItemInstance(MetaIndex);
    if (!Instance)
    {
        return false;
    }
    
    // Обновляем runtime свойства
    for (const auto& PropertyPair : NewProperties)
    {
        Instance->RuntimeProperties.Add(PropertyPair.Key, PropertyPair.Value);
    }
    
    // Обновляем реплицированное состояние
    bool bResult = ReplicationState.UpdateItemInstance(MetaIndex, *Instance);
    if (bResult)
    {
        RequestNetUpdate();
    }
    
    return bResult;
}

bool USuspenseInventoryReplicator::GetItemInstanceByIndex(int32 MetaIndex, FSuspenseInventoryItemInstance& OutInstance) const
{
    const FSuspenseInventoryItemInstance* Instance = ReplicationState.GetItemInstance(MetaIndex);
    if (Instance)
    {
        OutInstance = *Instance;
        return true;
    }
    return false;
}

int32 USuspenseInventoryReplicator::FindItemByInstanceID(const FGuid& InstanceID) const
{
    return ReplicationState.FindMetaIndexByInstanceID(InstanceID);
}

bool USuspenseInventoryReplicator::ValidateReplicationState(TArray<FString>& OutErrors) const
{
    return ReplicationState.ValidateIntegrity(OutErrors);
}

FString USuspenseInventoryReplicator::GetReplicationStats() const
{
    FString Stats;
    Stats += FString::Printf(TEXT("=== Inventory Replication Statistics ===\n"));
    Stats += FString::Printf(TEXT("Update Count: %d\n"), ReplicationUpdateCount);
    Stats += FString::Printf(TEXT("Network Update Interval: %.2f seconds\n"), NetworkUpdateInterval);
    Stats += FString::Printf(TEXT("Last Update Time: %.2f\n"), LastUpdateTime);
    Stats += FString::Printf(TEXT("Items in State: %d\n"), ReplicationState.ItemsState.Items.Num());
    Stats += FString::Printf(TEXT("Cells in Grid: %d\n"), ReplicationState.CellsState.Cells.Num());
    Stats += FString::Printf(TEXT("Item Instances: %d\n"), ReplicationState.ItemInstances.Num());
    Stats += FString::Printf(TEXT("ItemManager Available: %s\n"), ItemManager ? TEXT("Yes") : TEXT("No"));
    
    if (ItemManager)
    {
        Stats += FString::Printf(TEXT("Total Items in DataTable: %d\n"), ItemManager->GetAllItemIDs().Num());
    }
    
    return Stats;
}

void USuspenseInventoryReplicator::ForceFullResync()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bForceFullResync = true;
        ReplicationState.MarkArrayDirty();
        
        UE_LOG(LogInventory, Log, TEXT("InventoryReplicator: Forced full resync requested"));
    }
}

void USuspenseInventoryReplicator::OnRep_ReplicationState()
{
    // Вызывается на клиенте при получении обновлений репликации
    UE_LOG(LogInventory, Log, TEXT("InventoryReplicator: Replication state updated on client"));
    
    // Подсчитываем количество реплицированных предметов
    int32 ItemCount = 0;
    int32 InstanceCount = 0;
    
    for (const FReplicatedItemMeta& Meta : ReplicationState.ItemsState.Items)
    {
        if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
        {
            ItemCount++;
            if (Meta.InstanceID.IsValid())
            {
                InstanceCount++;
            }
        }
    }
    
    UE_LOG(LogInventory, Log, TEXT("InventoryReplicator: Received %d items (%d with valid instances) in replication"), 
        ItemCount, InstanceCount);
    
    // Синхронизируем с ItemManager если доступен
    if (!ItemManager)
    {
        ItemManager = GetOrCreateItemManager();
    }
    
    if (ItemManager)
    {
        ReplicationState.SynchronizeWithItemManager(ItemManager);
    }
    
    // Уведомляем подписчиков об обновлении
    OnReplicationUpdated.Broadcast();
}

USuspenseItemManager* USuspenseInventoryReplicator::GetOrCreateItemManager()
{
    if (ItemManager)
    {
        return ItemManager;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    ItemManager = GameInstance->GetSubsystem<USuspenseItemManager>();
    if (!ItemManager)
    {
        UE_LOG(LogInventory, Warning, TEXT("GetOrCreateItemManager: ItemManager subsystem not found"));
    }
    
    return ItemManager;
}

void USuspenseInventoryReplicator::UpdateReplicationStats()
{
    ReplicationUpdateCount++;
    LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Здесь можно добавить дополнительные метрики репликации
    // например, размер отправленных данных, частоту обновлений и т.д.
}

void USuspenseInventoryReplicator::CleanupStaleData()
{
    // Очищаем устаревшие данные и оптимизируем структуры
    // Например, удаляем empty metadata entries
    
    bool bNeedsCleanup = false;
    for (int32 i = ReplicationState.ItemsState.Items.Num() - 1; i >= 0; i--)
    {
        const FReplicatedItemMeta& Meta = ReplicationState.ItemsState.Items[i];
        if (Meta.ItemID.IsNone() && Meta.Stack <= 0)
        {
            // Это пустая запись, можно удалить
            bNeedsCleanup = true;
            break;
        }
    }
    
    if (bNeedsCleanup)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryReplicator: Cleanup would be beneficial"));
        // Реализация cleanup логики по необходимости
    }
}
// Продолжение MedComInventory/Network/InventoryReplicator.cpp
// НЕДОСТАЮЩИЕ МЕТОДЫ FInventoryReplicatedState

bool FInventoryReplicatedState::RemoveItem(int32 MetaIndex)
{
    if (MetaIndex < 0 || MetaIndex >= ItemsState.Items.Num())
    {
        UE_LOG(LogInventory, Warning, TEXT("RemoveItem: Invalid MetaIndex %d"), MetaIndex);
        return false;
    }
    
    // Получаем информацию о предмете для логирования
    FName ItemID = ItemsState.Items.IsValidIndex(MetaIndex) ? ItemsState.Items[MetaIndex].ItemID : NAME_None;
    FGuid InstanceID = ItemsState.Items.IsValidIndex(MetaIndex) ? ItemsState.Items[MetaIndex].InstanceID : FGuid();
    
    // Очищаем затронутые ячейки сетки
    int32 ClearedCells = 0;
    for (int32 i = 0; i < CellsState.Cells.Num(); i++)
    {
        if (CellsState.Cells[i].ItemMetaIndex == MetaIndex)
        {
            CellsState.Cells[i].Clear();
            ClearedCells++;
        }
    }
    
    // Очищаем ссылку на legacy object
    if (ItemObjects.IsValidIndex(MetaIndex))
    {
        ItemObjects[MetaIndex] = nullptr;
    }
    
    // Очищаем FSuspenseInventoryItemInstance
    if (ItemInstances.IsValidIndex(MetaIndex))
    {
        // Создаем пустой instance для замещения
        FSuspenseInventoryItemInstance EmptyInstance;
        ItemInstances[MetaIndex] = EmptyInstance;
    }
    
    // Помечаем metadata как недействительные (не удаляем для сохранения индексов)
    if (ItemsState.Items.IsValidIndex(MetaIndex))
    {
        FReplicatedItemMeta& Meta = ItemsState.Items[MetaIndex];
        Meta.ItemID = NAME_None;
        Meta.InstanceID = FGuid(); // Обнуляем GUID
        Meta.Stack = 0;
        Meta.AnchorIndex = INDEX_NONE;
        Meta.ItemStateFlags = 0;
        Meta.ItemDataFlags = 0;
        Meta.PackedGridSize = 0;
        Meta.ItemWeight = 0.0f;
        Meta.DurabilityPercent = 255;
        Meta.RuntimePropertiesCount = 0;
        Meta.PackedRuntimeProperties.Empty();
    }
    
    // Помечаем массивы как измененные для репликации
    MarkArrayDirty();
    
    UE_LOG(LogInventory, Log, TEXT("RemoveItem: Removed %s (meta:%d, cleared %d cells, instance:%s)"), 
        *ItemID.ToString(), MetaIndex, ClearedCells, *InstanceID.ToString().Left(8));
    
    return true;
}

bool FInventoryReplicatedState::UpdateItem(int32 MetaIndex, const FReplicatedItemMeta& NewMeta)
{
    if (MetaIndex < 0 || MetaIndex >= ItemsState.Items.Num())
    {
        UE_LOG(LogInventory, Warning, TEXT("UpdateItem: Invalid MetaIndex %d"), MetaIndex);
        return false;
    }
    
    // Сохраняем старые данные для логирования
    const FReplicatedItemMeta& OldMeta = ItemsState.Items[MetaIndex];
    FName OldItemID = OldMeta.ItemID;
    int32 OldStack = OldMeta.Stack;
    
    // Обновляем metadata
    ItemsState.Items[MetaIndex] = NewMeta;
    
    // Синхронизируем с локальным FSuspenseInventoryItemInstance если он существует
    if (ItemInstances.IsValidIndex(MetaIndex))
    {
        FSuspenseInventoryItemInstance UpdatedInstance = NewMeta.ToItemInstance();
        ItemInstances[MetaIndex] = UpdatedInstance;
    }
    
    // Помечаем как измененное для репликации
    ItemsState.MarkItemDirty(ItemsState.Items[MetaIndex]);
    
    UE_LOG(LogInventory, Verbose, TEXT("UpdateItem: Updated %s->%s (meta:%d, stack:%d->%d, durability:%.1f%%)"), 
        *OldItemID.ToString(), *NewMeta.ItemID.ToString(), MetaIndex, 
        OldStack, NewMeta.Stack, NewMeta.GetDurabilityAsPercent() * 100.0f);
    
    return true;
}

int32 FInventoryReplicatedState::FindMetaIndexByObject(UObject* ItemObject) const
{
    if (!ItemObject)
    {
        return INDEX_NONE;
    }
    
    // Поиск через legacy массив объектов
    for (int32 i = 0; i < ItemObjects.Num(); i++)
    {
        if (ItemObjects[i] == ItemObject)
        {
            // Дополнительная проверка что metadata действительно активные
            if (ItemsState.Items.IsValidIndex(i) && 
                !ItemsState.Items[i].ItemID.IsNone() && 
                ItemsState.Items[i].Stack > 0)
            {
                return i;
            }
        }
    }
    
    // Если legacy поиск не дал результатов, пытаемся через interface
    if (const ISuspenseInventoryItemInterface* ItemInterface = Cast<ISuspenseInventoryItemInterface>(ItemObject))
    {
        FGuid InstanceID = ItemInterface->GetInstanceID();
        if (InstanceID.IsValid())
        {
            return FindMetaIndexByInstanceID(InstanceID);
        }
        
        // Fallback поиск по ItemID
        FName ItemID = ItemInterface->GetItemID();
        return FindMetaIndexByItemID(ItemID);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FindMetaIndexByObject: Object %s not found in replication state"), 
        *GetNameSafe(ItemObject));
    
    return INDEX_NONE;
}

int32 FInventoryReplicatedState::FindMetaIndexByItemID(const FName& ItemID) const
{
    if (ItemID.IsNone())
    {
        return INDEX_NONE;
    }
    
    // Поиск первого активного предмета с данным ItemID
    for (int32 i = 0; i < ItemsState.Items.Num(); i++)
    {
        const FReplicatedItemMeta& Meta = ItemsState.Items[i];
        if (Meta.ItemID == ItemID && Meta.Stack > 0 && Meta.InstanceID.IsValid())
        {
            return i;
        }
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FindMetaIndexByItemID: ItemID %s not found in active items"), 
        *ItemID.ToString());
    
    return INDEX_NONE;
}

bool FInventoryReplicatedState::AreCellsFree(int32 StartIndex, const FVector2D& Size) const
{
    // Вычисляем размеры сетки из общего количества ячеек
    int32 TotalCells = CellsState.Cells.Num();
    int32 GridWidth = FMath::Sqrt(static_cast<float>(TotalCells));
    int32 GridHeight = TotalCells / GridWidth;
    
    if (GridWidth <= 0 || StartIndex < 0 || StartIndex >= TotalCells)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("AreCellsFree: Invalid parameters - StartIndex:%d, GridWidth:%d, TotalCells:%d"), 
            StartIndex, GridWidth, TotalCells);
        return false;
    }
    
    // Вычисляем координаты начальной позиции
    int32 StartX = StartIndex % GridWidth;
    int32 StartY = StartIndex / GridWidth;
    
    // Проверяем размер предмета
    int32 ItemWidth = FMath::CeilToInt(Size.X);
    int32 ItemHeight = FMath::CeilToInt(Size.Y);
    
    if (ItemWidth <= 0 || ItemHeight <= 0)
    {
        UE_LOG(LogInventory, Warning, TEXT("AreCellsFree: Invalid item size %.1fx%.1f"), Size.X, Size.Y);
        return false;
    }
    
    // Проверяем что предмет помещается в сетку
    if (StartX + ItemWidth > GridWidth || StartY + ItemHeight > GridHeight)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("AreCellsFree: Item %dx%d doesn't fit at position (%d,%d) in grid %dx%d"), 
            ItemWidth, ItemHeight, StartX, StartY, GridWidth, GridHeight);
        return false;
    }
    
    // Проверяем что все необходимые ячейки свободны
    for (int32 Y = 0; Y < ItemHeight; Y++)
    {
        for (int32 X = 0; X < ItemWidth; X++)
        {
            int32 CellIndex = (StartY + Y) * GridWidth + (StartX + X);
            
            if (CellIndex >= 0 && CellIndex < TotalCells)
            {
                const FCompactReplicatedCell& Cell = CellsState.Cells[CellIndex];
                if (Cell.IsOccupied())
                {
                    UE_LOG(LogInventory, VeryVerbose, TEXT("AreCellsFree: Cell %d is occupied (ItemMeta:%d)"), 
                        CellIndex, Cell.ItemMetaIndex);
                    return false;
                }
            }
            else
            {
                UE_LOG(LogInventory, Warning, TEXT("AreCellsFree: Calculated CellIndex %d is out of bounds"), CellIndex);
                return false;
            }
        }
    }
    
    return true;
}

//==============================================================================
// USuspenseInventoryReplicator НЕДОСТАЮЩИЕ МЕТОДЫ
//==============================================================================

void USuspenseInventoryReplicator::SetUpdateInterval(float IntervalSeconds)
{
    float OldInterval = NetworkUpdateInterval;
    NetworkUpdateInterval = FMath::Clamp(IntervalSeconds, 0.01f, 5.0f); // Разумные пределы от 10ms до 5 секунд
    
    // Сбрасываем таймер если интервал уменьшился
    if (NetworkUpdateInterval < OldInterval)
    {
        NetworkUpdateTimer = 0.0f;
    }
    
    UE_LOG(LogInventory, Log, TEXT("InventoryReplicator: Update interval changed from %.3f to %.3f seconds"), 
        OldInterval, NetworkUpdateInterval);
}

void USuspenseInventoryReplicator::RequestNetUpdate()
{
    // Запрашиваем обновление только на authoritative instance
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bNetUpdateNeeded = true;
        
        // Если интервал обновления очень маленький, отправляем немедленно
        if (NetworkUpdateInterval <= 0.02f) // 20ms или меньше
        {
            NetworkUpdateTimer = NetworkUpdateInterval;
        }
        else
        {
            NetworkUpdateTimer = FMath::Max(0.0f, NetworkUpdateTimer); // Обеспечиваем что таймер не отрицательный
        }
        
        UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryReplicator: Network update requested (Timer: %.3f/%.3f)"), 
            NetworkUpdateTimer, NetworkUpdateInterval);
    }
    else
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("InventoryReplicator: Network update request ignored (not authoritative)"));
    }
}

bool USuspenseInventoryReplicator::ConvertLegacyObjectToInstance(UObject* ItemObject, FSuspenseInventoryItemInstance& OutInstance) const
{
    if (!ItemObject)
    {
        UE_LOG(LogInventory, Warning, TEXT("ConvertLegacyObjectToInstance: Null ItemObject provided"));
        return false;
    }
    
    // Пытаемся получить данные через interface
    if (const ISuspenseInventoryItemInterface* ItemInterface = Cast<ISuspenseInventoryItemInterface>(ItemObject))
    {
        // Получаем полный instance из interface
        OutInstance = ItemInterface->GetItemInstance();
        
        UE_LOG(LogInventory, Verbose, TEXT("ConvertLegacyObjectToInstance: Converted %s to instance %s"), 
            *GetNameSafe(ItemObject), *OutInstance.InstanceID.ToString().Left(8));
        
        return OutInstance.IsValid();
    }
    
    // Fallback для legacy объектов без interface
    if (USuspenseItemBase* ItemBase = Cast<USuspenseItemBase>(ItemObject))
    {
        // Создаем instance из базового объекта
        OutInstance = FSuspenseInventoryItemInstance();
        
        // Получаем ItemID из объекта
        OutInstance.ItemID = ItemBase->ItemID;
        OutInstance.InstanceID = ItemBase->InstanceID;
        OutInstance.Quantity = 1; // Default количество
        OutInstance.AnchorIndex = INDEX_NONE;
        OutInstance.bIsRotated = false;
        
        // ИСПРАВЛЕНО: Вместо прямого копирования RuntimeProperties,
        // используем правильные методы доступа к runtime данным
        
        // Метод 1: Если у ItemBase есть метод GetRuntimeProperties()
        // if (ItemBase->GetRuntimeProperties(OutInstance.RuntimeProperties))
        // {
        //     // Properties успешно скопированы
        // }
        
        // Метод 2: Если ItemBase хранит properties по-другому,
        // собираем их вручную через доступные методы
        
        // Устанавливаем время последнего использования
        OutInstance.LastUsedTime = ItemBase->LastUsedTime;
        
        // Добавляем прочность если предмет её поддерживает
        if (ItemBase->HasDurability())
        {
            float CurrentDurability = ItemBase->CurrentDurability;
            float MaxDurability = ItemBase->GetMaxDurability();
            float DurabilityPercentValue = ItemBase->GetDurabilityPercent(); // Переименовали переменную
            
            // Сохраняем прочность в runtime properties
            OutInstance.RuntimeProperties.Add(TEXT("Durability"), CurrentDurability);
            OutInstance.RuntimeProperties.Add(TEXT("MaxDurability"), MaxDurability);
            OutInstance.RuntimeProperties.Add(TEXT("DurabilityPercent"), DurabilityPercentValue * 100.0f);
        }
        
        // Добавляем данные оружия если применимо
        if (ItemBase->IsEquippable() && ItemBase->GetItemData())
        {
            const FSuspenseUnifiedItemData* Data = ItemBase->GetItemData();
            if (Data && Data->bIsWeapon)
            {
                int32 CurrentAmmo = ItemBase->GetCurrentAmmo();
                int32 MaxAmmo = ItemBase->GetMaxAmmo();
                
                // Сохраняем данные о боеприпасах
                if (CurrentAmmo >= 0)
                {
                    OutInstance.RuntimeProperties.Add(TEXT("Ammo"), static_cast<float>(CurrentAmmo));
                }
                if (MaxAmmo > 0)
                {
                    OutInstance.RuntimeProperties.Add(TEXT("MaxAmmo"), static_cast<float>(MaxAmmo));
                }
            }
        }
        
        // Добавляем любые другие известные runtime свойства
        // через публичные методы ItemBase
        
        UE_LOG(LogInventory, Log, TEXT("ConvertLegacyObjectToInstance: Created instance from ItemBase %s (ItemID: %s, Properties: %d)"), 
            *GetNameSafe(ItemBase), *OutInstance.ItemID.ToString(), OutInstance.RuntimeProperties.Num());
        
        return OutInstance.IsValid();
    }
    
    // Последний fallback - создаем minimal instance
    OutInstance = FSuspenseInventoryItemInstance();
    OutInstance.ItemID = FName(*ItemObject->GetName());
    OutInstance.InstanceID = FGuid::NewGuid();
    OutInstance.Quantity = 1;
    OutInstance.AnchorIndex = INDEX_NONE;
    OutInstance.bIsRotated = false;
    
    UE_LOG(LogInventory, Warning, TEXT("ConvertLegacyObjectToInstance: Created minimal instance from generic object %s"), 
        *GetNameSafe(ItemObject));
    
    return true;
}

void USuspenseInventoryReplicator::SynchronizeItemSizesWithDataTable()
{
    if (!ItemManager)
    {
        ItemManager = GetOrCreateItemManager();
        if (!ItemManager)
        {
            UE_LOG(LogInventory, Warning, TEXT("SynchronizeItemSizesWithDataTable: No ItemManager available"));
            return;
        }
    }
    
    bool bFoundChanges = false;
    int32 UpdatedItems = 0;
    
    // Проходим через все активные metadata
    for (int32 i = 0; i < ReplicationState.ItemsState.Items.Num(); i++)
    {
        FReplicatedItemMeta& Meta = ReplicationState.ItemsState.Items[i];
        
        if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
        {
            // Получаем актуальные данные из DataTable
            FSuspenseUnifiedItemData UnifiedData;
            if (ItemManager->GetUnifiedItemData(Meta.ItemID, UnifiedData))
            {
                // Проверяем размер сетки
                FIntPoint CurrentSize = Meta.GetGridSizeInt();
                if (CurrentSize != UnifiedData.GridSize)
                {
                    Meta.SetGridSize(UnifiedData.GridSize);
                    bFoundChanges = true;
                    UpdatedItems++;
                    
                    UE_LOG(LogInventory, Log, TEXT("SynchronizeItemSizesWithDataTable: Updated size for %s from %dx%d to %dx%d"), 
                        *Meta.ItemID.ToString(), 
                        CurrentSize.X, CurrentSize.Y,
                        UnifiedData.GridSize.X, UnifiedData.GridSize.Y);
                }
                
                // Проверяем вес
                if (FMath::Abs(Meta.ItemWeight - UnifiedData.Weight) > 0.001f)
                {
                    Meta.ItemWeight = UnifiedData.Weight;
                    bFoundChanges = true;
                    
                    UE_LOG(LogInventory, Log, TEXT("SynchronizeItemSizesWithDataTable: Updated weight for %s to %.3f"), 
                        *Meta.ItemID.ToString(), UnifiedData.Weight);
                }
                
                // Обновляем соответствующий FSuspenseInventoryItemInstance если есть
                if (ReplicationState.ItemInstances.IsValidIndex(i))
                {
                    FSuspenseInventoryItemInstance& Instance = ReplicationState.ItemInstances[i];
                    if (Instance.ItemID == Meta.ItemID)
                    {
                        // Экземпляр уже содержит правильные данные, просто помечаем как обновленный
                        // Runtime properties остаются без изменений
                    }
                }
            }
            else
            {
                UE_LOG(LogInventory, Warning, TEXT("SynchronizeItemSizesWithDataTable: Could not find DataTable entry for %s"), 
                    *Meta.ItemID.ToString());
            }
        }
    }
    
    // Если были изменения, помечаем для репликации
    if (bFoundChanges)
    {
        ReplicationState.MarkArrayDirty();
        RequestNetUpdate();
        
        UE_LOG(LogInventory, Log, TEXT("SynchronizeItemSizesWithDataTable: Updated %d items, requesting network update"), 
            UpdatedItems);
    }
    else
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("SynchronizeItemSizesWithDataTable: No changes needed"));
    }
}

//==============================================================================
// ДОПОЛНИТЕЛЬНЫЕ UTILITY МЕТОДЫ ДЛЯ ПОЛНОТЫ ФУНКЦИОНАЛЬНОСТИ
//==============================================================================

void USuspenseInventoryReplicator::SetUpdateIntervalOptimized(float BaseInterval, int32 ItemCount)
{
    // Динамическая оптимизация интервала обновления на основе количества предметов
    float OptimizedInterval = BaseInterval;
    
    if (ItemCount > 50)
    {
        // Увеличиваем интервал для больших инвентарей для снижения network load
        OptimizedInterval = BaseInterval * (1.0f + (ItemCount - 50) * 0.01f);
    }
    else if (ItemCount < 10)
    {
        // Уменьшаем интервал для маленьких инвентарей для лучшей responsiveness
        OptimizedInterval = BaseInterval * 0.8f;
    }
    
    SetUpdateInterval(OptimizedInterval);
    
    UE_LOG(LogInventory, Log, TEXT("SetUpdateIntervalOptimized: Set interval to %.3f seconds for %d items"), 
        OptimizedInterval, ItemCount);
}

bool USuspenseInventoryReplicator::TryCompactReplication()
{
    // Попытка сжать реплицированные данные путем удаления пустых записей
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return false;
    }
    
    TArray<FReplicatedItemMeta> CompactedItems;
    TArray<FSuspenseInventoryItemInstance> CompactedInstances;
    TArray<UObject*> CompactedObjects;
    TMap<int32, int32> IndexMapping; // Старый индекс -> новый индекс
    
    // Собираем только активные предметы
    for (int32 i = 0; i < ReplicationState.ItemsState.Items.Num(); i++)
    {
        const FReplicatedItemMeta& Meta = ReplicationState.ItemsState.Items[i];
        
        if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
        {
            int32 NewIndex = CompactedItems.Num();
            IndexMapping.Add(i, NewIndex);
            
            CompactedItems.Add(Meta);
            
            if (ReplicationState.ItemInstances.IsValidIndex(i))
            {
                CompactedInstances.Add(ReplicationState.ItemInstances[i]);
            }
            else
            {
                CompactedInstances.Add(FSuspenseInventoryItemInstance());
            }
            
            if (ReplicationState.ItemObjects.IsValidIndex(i))
            {
                CompactedObjects.Add(ReplicationState.ItemObjects[i]);
            }
            else
            {
                CompactedObjects.Add(nullptr);
            }
        }
    }
    
    // Обновляем индексы в ячейках сетки
    for (FCompactReplicatedCell& Cell : ReplicationState.CellsState.Cells)
    {
        if (Cell.IsOccupied())
        {
            if (int32* NewIndex = IndexMapping.Find(Cell.ItemMetaIndex))
            {
                Cell.ItemMetaIndex = *NewIndex;
            }
            else
            {
                // Старый индекс больше не существует, очищаем ячейку
                Cell.Clear();
            }
        }
    }
    
    // Обновляем индексы в metadata
    for (FReplicatedItemMeta& Meta : CompactedItems)
    {
        if (Meta.AnchorIndex != INDEX_NONE)
        {
            // AnchorIndex ссылается на ячейку сетки, не на metadata, поэтому не изменяем
        }
    }
    
    // Заменяем старые массивы на сжатые
    int32 OldCount = ReplicationState.ItemsState.Items.Num();
    ReplicationState.ItemsState.Items = CompactedItems;
    ReplicationState.ItemInstances = CompactedInstances;
    ReplicationState.ItemObjects = CompactedObjects;
    
    // Помечаем для репликации
    ReplicationState.MarkArrayDirty();
    RequestNetUpdate();
    
    int32 NewCount = CompactedItems.Num();
    bool bWasCompacted = (NewCount < OldCount);
    
    if (bWasCompacted)
    {
        UE_LOG(LogInventory, Log, TEXT("TryCompactReplication: Compacted from %d to %d items (%d removed)"), 
            OldCount, NewCount, OldCount - NewCount);
    }
    else
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("TryCompactReplication: No compaction needed (%d items)"), NewCount);
    }
    
    return bWasCompacted;
}

FString USuspenseInventoryReplicator::GetDetailedReplicationDebugInfo() const
{
    FString DebugInfo;
    
    DebugInfo += FString::Printf(TEXT("=== Detailed Inventory Replication Debug Info ===\n"));
    DebugInfo += FString::Printf(TEXT("Component Owner: %s\n"), *GetNameSafe(GetOwner()));
    DebugInfo += FString::Printf(TEXT("Has Authority: %s\n"), GetOwner() && GetOwner()->HasAuthority() ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("ItemManager: %s\n"), ItemManager ? TEXT("Available") : TEXT("Not Available"));
    DebugInfo += FString::Printf(TEXT("\n"));
    
    // Статистика обновлений
    DebugInfo += FString::Printf(TEXT("--- Update Statistics ---\n"));
    DebugInfo += FString::Printf(TEXT("Update Count: %d\n"), ReplicationUpdateCount);
    DebugInfo += FString::Printf(TEXT("Update Interval: %.3f seconds\n"), NetworkUpdateInterval);
    DebugInfo += FString::Printf(TEXT("Update Timer: %.3f seconds\n"), NetworkUpdateTimer);
    DebugInfo += FString::Printf(TEXT("Update Needed: %s\n"), bNetUpdateNeeded ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("Force Resync: %s\n"), bForceFullResync ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("Last Update Time: %.2f\n"), LastUpdateTime);
    DebugInfo += FString::Printf(TEXT("\n"));
    
    // Состояние сетки
    DebugInfo += FString::Printf(TEXT("--- Grid State ---\n"));
    int32 TotalCells = ReplicationState.CellsState.Cells.Num();
    int32 OccupiedCells = 0;
    
    for (const FCompactReplicatedCell& Cell : ReplicationState.CellsState.Cells)
    {
        if (Cell.IsOccupied())
        {
            OccupiedCells++;
        }
    }
    
    DebugInfo += FString::Printf(TEXT("Total Cells: %d\n"), TotalCells);
    DebugInfo += FString::Printf(TEXT("Occupied Cells: %d (%.1f%%)\n"), 
        OccupiedCells, TotalCells > 0 ? (float)OccupiedCells / TotalCells * 100.0f : 0.0f);
    
    if (TotalCells > 0)
    {
        int32 GridWidth = FMath::Sqrt(static_cast<float>(TotalCells));
        int32 GridHeight = TotalCells / GridWidth;
        DebugInfo += FString::Printf(TEXT("Grid Dimensions: %dx%d\n"), GridWidth, GridHeight);
    }
    
    DebugInfo += FString::Printf(TEXT("\n"));
    
    // Состояние предметов
    DebugInfo += FString::Printf(TEXT("--- Items State ---\n"));
    DebugInfo += FString::Printf(TEXT("Metadata Entries: %d\n"), ReplicationState.ItemsState.Items.Num());
    DebugInfo += FString::Printf(TEXT("Item Instances: %d\n"), ReplicationState.ItemInstances.Num());
    DebugInfo += FString::Printf(TEXT("Legacy Objects: %d\n"), ReplicationState.ItemObjects.Num());
    
    int32 ActiveItems = 0;
    int32 ItemsWithRuntimeProps = 0;
    int32 RotatedItems = 0;
    
    for (const FReplicatedItemMeta& Meta : ReplicationState.ItemsState.Items)
    {
        if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
        {
            ActiveItems++;
            
            if (Meta.HasRuntimeProperties())
            {
                ItemsWithRuntimeProps++;
            }
            
            if (Meta.IsRotated())
            {
                RotatedItems++;
            }
        }
    }
    
    DebugInfo += FString::Printf(TEXT("Active Items: %d\n"), ActiveItems);
    DebugInfo += FString::Printf(TEXT("Items with Runtime Properties: %d\n"), ItemsWithRuntimeProps);
    DebugInfo += FString::Printf(TEXT("Rotated Items: %d\n"), RotatedItems);
    DebugInfo += FString::Printf(TEXT("\n"));
    
    // Детальная информация о предметах
    if (ActiveItems > 0)
    {
        DebugInfo += FString::Printf(TEXT("--- Active Items Details ---\n"));
        
        for (int32 i = 0; i < ReplicationState.ItemsState.Items.Num(); i++)
        {
            const FReplicatedItemMeta& Meta = ReplicationState.ItemsState.Items[i];
            
            if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
            {
                DebugInfo += FString::Printf(TEXT("[%d] %s (x%d) - Anchor:%d, Size:%dx%d, Weight:%.2f\n"),
                    i, 
                    *Meta.ItemID.ToString(),
                    Meta.Stack,
                    Meta.AnchorIndex,
                    Meta.GetGridSizeInt().X, Meta.GetGridSizeInt().Y,
                    Meta.ItemWeight);
                
                DebugInfo += FString::Printf(TEXT("    Instance: %s, Durability:%.1f%%, Runtime Props:%d\n"),
                    *Meta.InstanceID.ToString().Left(8),
                    Meta.GetDurabilityAsPercent() * 100.0f,
                    Meta.RuntimePropertiesCount);
                
                if (Meta.HasRuntimeProperties() && Meta.PackedRuntimeProperties.Num() > 0)
                {
                    DebugInfo += FString::Printf(TEXT("    Packed Properties: "));
                    for (const auto& PropPair : Meta.PackedRuntimeProperties)
                    {
                        DebugInfo += FString::Printf(TEXT("%d:%.1f "), PropPair.Key, PropPair.Value);
                    }
                    DebugInfo += FString::Printf(TEXT("\n"));
                }
                
                DebugInfo += FString::Printf(TEXT("\n"));
            }
        }
    }
    
    // Валидация целостности
    TArray<FString> ValidationErrors;
    bool bIsValid = ReplicationState.ValidateIntegrity(ValidationErrors);
    
    DebugInfo += FString::Printf(TEXT("--- Integrity Validation ---\n"));
    DebugInfo += FString::Printf(TEXT("State Valid: %s\n"), bIsValid ? TEXT("Yes") : TEXT("No"));
    
    if (!bIsValid && ValidationErrors.Num() > 0)
    {
        DebugInfo += FString::Printf(TEXT("Validation Errors (%d):\n"), ValidationErrors.Num());
        for (int32 i = 0; i < ValidationErrors.Num(); i++)
        {
            DebugInfo += FString::Printf(TEXT("  %d. %s\n"), i + 1, *ValidationErrors[i]);
        }
    }
    
    return DebugInfo;
}

//==============================================================================
// FINALIZING UTILITY METHODS
//==============================================================================

void USuspenseInventoryReplicator::PerformMaintenanceCleanup()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("PerformMaintenanceCleanup: Skipped (not authoritative)"));
        return;
    }
    
    UE_LOG(LogInventory, Log, TEXT("PerformMaintenanceCleanup: Starting maintenance cleanup"));
    
    // 1. Синхронизация с DataTable
    if (ItemManager)
    {
        SynchronizeItemSizesWithDataTable();
    }
    
    // 2. Попытка компактирования данных
    bool bWasCompacted = TryCompactReplication();
    
    // 3. Валидация целостности
    TArray<FString> ValidationErrors;
    bool bIsValid = ReplicationState.ValidateIntegrity(ValidationErrors);
    
    if (!bIsValid)
    {
        UE_LOG(LogInventory, Warning, TEXT("PerformMaintenanceCleanup: Validation failed with %d errors"), 
            ValidationErrors.Num());
        
        for (const FString& Error : ValidationErrors)
        {
            UE_LOG(LogInventory, Warning, TEXT("  - %s"), *Error);
        }
    }
    
    // 4. Оптимизация интервала обновления
    int32 ActiveItemCount = 0;
    for (const FReplicatedItemMeta& Meta : ReplicationState.ItemsState.Items)
    {
        if (!Meta.ItemID.IsNone() && Meta.Stack > 0)
        {
            ActiveItemCount++;
        }
    }
    
    SetUpdateIntervalOptimized(0.1f, ActiveItemCount); // Базовый интервал 100ms
    
    UE_LOG(LogInventory, Log, TEXT("PerformMaintenanceCleanup: Completed (Compacted:%s, Valid:%s, ActiveItems:%d)"), 
        bWasCompacted ? TEXT("Yes") : TEXT("No"), 
        bIsValid ? TEXT("Yes") : TEXT("No"), 
        ActiveItemCount);
}

bool USuspenseInventoryReplicator::EmergencyReset()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogInventory, Error, TEXT("EmergencyReset: Called on non-authoritative instance"));
        return false;
    }
    
    UE_LOG(LogInventory, Warning, TEXT("EmergencyReset: Performing emergency reset of replication state"));
    
    // Сохраняем критическую информацию для логирования
    int32 OldItemCount = ReplicationState.ItemsState.Items.Num();
    int32 OldCellCount = ReplicationState.CellsState.Cells.Num();
    
    // Полностью сбрасываем состояние
    ReplicationState.Reset();
    
    // Сбрасываем внутренние счетчики
    ReplicationUpdateCount = 0;
    NetworkUpdateTimer = 0.0f;
    bNetUpdateNeeded = false;
    bForceFullResync = true; // Принудительная полная синхронизация
    
    // Немедленно отправляем обновление
    if (GetOwner())
    {
        GetOwner()->ForceNetUpdate();
    }
    
    UE_LOG(LogInventory, Warning, TEXT("EmergencyReset: Reset complete (Was: %d items, %d cells)"), 
        OldItemCount, OldCellCount);
    
    // Уведомляем подписчиков
    OnReplicationUpdated.Broadcast();
    
    return true;
}
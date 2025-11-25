// Copyright Suspense Team. All Rights Reserved.

#include "Base/SuspenseInventoryItem.h"
#include "ItemSystem/MedComItemManager.h"
#include "ItemSystem/ItemSystemAccess.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Types/Inventory/InventoryUtils.h"
#include "Base/SuspenseItemBase.h"
#include "Interfaces/Equipment/IMedComItemDefinitionInterface.h"


//==================================================================
// Constructor and Core Actor Lifecycle
//==================================================================

AMedComInventoryItem::AMedComInventoryItem()
{
    // Настройка базовых параметров Actor
    PrimaryActorTick.bCanEverTick = false; // Этот Actor не требует постоянных обновлений
    bReplicates = false; // Репликация управляется на уровне компонента инвентаря
    
    // Инициализация состояния
    bIsInitialized = false;
    CachedItemManager = nullptr;
    
    // Создание валидного пустого экземпляра
    ItemInstance = FInventoryItemInstance();
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("AMedComInventoryItem: Created new inventory item actor"));
}

void AMedComInventoryItem::BeginPlay()
{
    Super::BeginPlay();
    
    // УЛУЧШЕНО: Используем FItemSystemAccess для надежного получения ItemManager
    CachedItemManager = FItemSystemAccess::GetItemManager(this);
    
    if (CachedItemManager)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("AMedComInventoryItem: Cached ItemManager reference"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AMedComInventoryItem: Failed to get ItemManager subsystem"));
    }
    
    // Валидируем состояние если предмет уже инициализирован
    if (bIsInitialized)
    {
        TArray<FString> ValidationErrors;
        if (!ValidateItemStateInternal(ValidationErrors))
        {
            UE_LOG(LogTemp, Warning, TEXT("AMedComInventoryItem: Item state validation failed on BeginPlay"));
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Error);
            }
        }
    }
}

void AMedComInventoryItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Очищаем кэш при уничтожении
    ClearCachedData();
    CachedItemManager = nullptr;
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("AMedComInventoryItem: Cleaned up inventory item actor"));
    
    Super::EndPlay(EndPlayReason);
}

//==================================================================
// Core Item Data Access (Updated for DataTable Architecture)
//==================================================================

bool AMedComInventoryItem::GetItemData(FMedComUnifiedItemData& OutItemData) const
{
    // Проверяем валидность ItemID
    if (ItemInstance.ItemID.IsNone())
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetItemData: Invalid ItemID"));
        return false;
    }
    
    // Проверяем кэш сначала для производительности
    if (CachedItemData.IsSet())
    {
        float CacheAge = (FDateTime::Now() - CacheTime).GetTotalSeconds();
        if (CacheAge < CACHE_DURATION)
        {
            OutItemData = CachedItemData.GetValue();
            UE_LOG(LogTemp, VeryVerbose, TEXT("GetItemData: Returned cached data for %s (age: %.1fs)"), 
                *ItemInstance.ItemID.ToString(), CacheAge);
            return true;
        }
        else
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("GetItemData: Cache expired for %s (age: %.1fs)"), 
                *ItemInstance.ItemID.ToString(), CacheAge);
        }
    }
    
    // Получаем данные из ItemManager
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetItemData: ItemManager not available"));
        return false;
    }
    
    if (ItemManager->GetUnifiedItemData(ItemInstance.ItemID, OutItemData))
    {
        // Обновляем кэш для будущих запросов
        CachedItemData = OutItemData;
        CacheTime = FDateTime::Now();
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetItemData: Retrieved and cached data for %s"), 
            *ItemInstance.ItemID.ToString());
        return true;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("GetItemData: Failed to get data for %s from ItemManager"), 
        *ItemInstance.ItemID.ToString());
    return false;
}

UMedComItemManager* AMedComInventoryItem::GetItemManager() const
{
    // Используем кэш если доступен
    if (CachedItemManager)
    {
        return CachedItemManager;
    }
    
    // УЛУЧШЕНО: Используем FItemSystemAccess для получения ItemManager с полной валидацией
    CachedItemManager = FItemSystemAccess::GetItemManager(this);
    
    if (CachedItemManager)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetItemManager: Cached new ItemManager reference"));
    }
    
    return CachedItemManager;
}

//==================================================================
// Quantity Management with DataTable Validation
//==================================================================

bool AMedComInventoryItem::TrySetAmount(int32 NewAmount)
{
    // Валидация базовых параметров
    if (NewAmount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TrySetAmount: Invalid amount %d (must be > 0)"), NewAmount);
        return false;
    }
    
    // Проверяем максимальный размер стека из DataTable
    FMedComUnifiedItemData ItemData;
    if (GetItemData(ItemData))
    {
        if (NewAmount > ItemData.MaxStackSize)
        {
            UE_LOG(LogTemp, Warning, TEXT("TrySetAmount: Amount %d exceeds max stack size %d for %s"), 
                NewAmount, ItemData.MaxStackSize, *ItemInstance.ItemID.ToString());
            return false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("TrySetAmount: Cannot validate max stack size - item data not available"));
        return false;
    }
    
    // Логируем изменение количества
    int32 OldAmount = ItemInstance.Quantity;
    ItemInstance.Quantity = NewAmount;
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("TrySetAmount: Changed quantity for %s from %d to %d"), 
        *ItemInstance.ItemID.ToString(), OldAmount, NewAmount);
    
    return true;
}

//==================================================================
// Enhanced Runtime Properties System
//==================================================================

float AMedComInventoryItem::GetRuntimeProperty(const FName& PropertyName, float DefaultValue) const
{
    float Value = ItemInstance.GetRuntimeProperty(PropertyName, DefaultValue);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetRuntimeProperty: %s.%s = %.2f"), 
        *ItemInstance.ItemID.ToString(), *PropertyName.ToString(), Value);
    
    return Value;
}

void AMedComInventoryItem::SetRuntimeProperty(const FName& PropertyName, float Value)
{
    float OldValue = ItemInstance.GetRuntimeProperty(PropertyName, 0.0f);
    ItemInstance.SetRuntimeProperty(PropertyName, Value);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("SetRuntimeProperty: %s.%s changed from %.2f to %.2f"), 
        *ItemInstance.ItemID.ToString(), *PropertyName.ToString(), OldValue, Value);
}

bool AMedComInventoryItem::HasRuntimeProperty(const FName& PropertyName) const
{
    return ItemInstance.HasRuntimeProperty(PropertyName);
}

void AMedComInventoryItem::ClearRuntimeProperty(const FName& PropertyName)
{
    if (ItemInstance.HasRuntimeProperty(PropertyName))
    {
        float OldValue = ItemInstance.GetRuntimeProperty(PropertyName, 0.0f);
        ItemInstance.RemoveRuntimeProperty(PropertyName);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("ClearRuntimeProperty: Removed %s.%s (was %.2f)"), 
            *ItemInstance.ItemID.ToString(), *PropertyName.ToString(), OldValue);
    }
}

//==================================================================
// Grid Size Management (DataTable-Driven)
//==================================================================

FVector2D AMedComInventoryItem::GetEffectiveGridSize() const
{
    // Проверяем валидность ItemID
    if (ItemInstance.ItemID.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("GetEffectiveGridSize: Invalid ItemID"));
        return FVector2D(1, 1); // Безопасная заглушка
    }
    
    // ИСПРАВЛЕНО: Добавлен WorldContextObject (this) как первый параметр
    // Используем InventoryUtils для единообразной логики
    FVector2D EffectiveSize = InventoryUtils::GetItemGridSize(this, ItemInstance.ItemID, ItemInstance.bIsRotated);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("GetEffectiveGridSize: %s = %.0fx%.0f (rotated: %s)"),
        *ItemInstance.ItemID.ToString(), 
        EffectiveSize.X, EffectiveSize.Y,
        ItemInstance.bIsRotated ? TEXT("yes") : TEXT("no"));
    
    return EffectiveSize;
}

FVector2D AMedComInventoryItem::GetBaseGridSize() const
{
    // Проверяем валидность ItemID
    if (ItemInstance.ItemID.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("GetBaseGridSize: Invalid ItemID"));
        return FVector2D(1, 1);
    }
    
    // Получаем базовый размер из DataTable
    FMedComUnifiedItemData ItemData;
    if (GetItemData(ItemData))
    {
        FVector2D BaseSize(ItemData.GridSize.X, ItemData.GridSize.Y);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("GetBaseGridSize: %s = %.0fx%.0f (from DataTable)"),
            *ItemInstance.ItemID.ToString(), BaseSize.X, BaseSize.Y);
        
        return BaseSize;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("GetBaseGridSize: Failed to get data for %s, using fallback"),
        *ItemInstance.ItemID.ToString());
    
    return FVector2D(1, 1);
}

//==================================================================
// Item Rotation Management
//==================================================================

void AMedComInventoryItem::SetRotated(bool bRotated)
{
    if (ItemInstance.bIsRotated != bRotated)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("SetRotated: %s rotation changed %s -> %s"),
            *ItemInstance.ItemID.ToString(),
            ItemInstance.bIsRotated ? TEXT("rotated") : TEXT("normal"),
            bRotated ? TEXT("rotated") : TEXT("normal"));
            
        ItemInstance.bIsRotated = bRotated;
    }
}

//==================================================================
// Advanced Grid and Placement System
//==================================================================

FVector2D AMedComInventoryItem::GetGridSizeForRotation(bool bForRotated) const
{
    if (ItemInstance.ItemID.IsNone())
    {
        return FVector2D(1, 1);
    }
    
    // ИСПРАВЛЕНО: Добавлен WorldContextObject (this) как первый параметр
    return InventoryUtils::GetItemGridSize(this, ItemInstance.ItemID, bForRotated);
}

bool AMedComInventoryItem::CanFitInGrid(int32 GridWidth, int32 GridHeight, bool bCheckRotated) const
{
    FVector2D RequiredSize = GetGridSizeForRotation(bCheckRotated);
    
    bool bCanFit = (RequiredSize.X <= GridWidth) && (RequiredSize.Y <= GridHeight);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("CanFitInGrid: %s requires %.0fx%.0f, grid is %dx%d, result: %s"),
        *ItemInstance.ItemID.ToString(),
        RequiredSize.X, RequiredSize.Y,
        GridWidth, GridHeight,
        bCanFit ? TEXT("fits") : TEXT("doesn't fit"));
    
    return bCanFit;
}

bool AMedComInventoryItem::GetOptimalRotationForGrid(int32 GridWidth, int32 GridHeight) const
{
    FVector2D NormalSize = GetGridSizeForRotation(false);
    FVector2D RotatedSize = GetGridSizeForRotation(true);
    
    bool bNormalFits = (NormalSize.X <= GridWidth) && (NormalSize.Y <= GridHeight);
    bool bRotatedFits = (RotatedSize.X <= GridWidth) && (RotatedSize.Y <= GridHeight);
    
    // Если только один вариант помещается, выбираем его
    if (bNormalFits && !bRotatedFits)
    {
        return false; // Не поворачиваем
    }
    
    if (!bNormalFits && bRotatedFits)
    {
        return true; // Поворачиваем
    }
    
    // Если оба помещаются или оба не помещаются, выбираем более компактный
    float NormalArea = NormalSize.X * NormalSize.Y;
    float RotatedArea = RotatedSize.X * RotatedSize.Y;
    
    // При равной площади предпочитаем текущую ориентацию
    if (FMath::IsNearlyEqual(NormalArea, RotatedArea))
    {
        return ItemInstance.bIsRotated;
    }
    
    return RotatedArea < NormalArea;
}

//==================================================================
// Item Type Query System
//==================================================================

bool AMedComInventoryItem::IsEquippable() const
{
    const FMedComUnifiedItemData* ItemData = GetCachedUnifiedData();
    return ItemData ? ItemData->bIsEquippable : false;
}

bool AMedComInventoryItem::IsWeapon() const
{
    const FMedComUnifiedItemData* ItemData = GetCachedUnifiedData();
    return ItemData ? ItemData->bIsWeapon : false;
}

bool AMedComInventoryItem::IsArmor() const
{
    const FMedComUnifiedItemData* ItemData = GetCachedUnifiedData();
    return ItemData ? ItemData->bIsArmor : false;
}

bool AMedComInventoryItem::IsConsumable() const
{
    const FMedComUnifiedItemData* ItemData = GetCachedUnifiedData();
    return ItemData ? ItemData->bIsConsumable : false;
}

bool AMedComInventoryItem::IsAmmo() const
{
    const FMedComUnifiedItemData* ItemData = GetCachedUnifiedData();
    return ItemData ? ItemData->bIsAmmo : false;
}

float AMedComInventoryItem::GetItemWeight() const
{
    if (ItemInstance.ItemID.IsNone())
    {
        return 0.0f;
    }
    
    // ИСПРАВЛЕНО: Добавлен WorldContextObject (this) как первый параметр
    return InventoryUtils::GetItemWeight(this, ItemInstance.ItemID);
}

float AMedComInventoryItem::GetTotalWeight() const
{
    return GetItemWeight() * ItemInstance.Quantity;
}

int32 AMedComInventoryItem::GetMaxStackSize() const
{
    if (ItemInstance.ItemID.IsNone())
    {
        return 1;
    }
    
    // ИСПРАВЛЕНО: Добавлен WorldContextObject (this) как первый параметр
    return InventoryUtils::GetMaxStackSize(this, ItemInstance.ItemID);
}

//==================================================================
// Enhanced Runtime Properties for Gameplay Systems
//==================================================================

float AMedComInventoryItem::GetCurrentDurability() const
{
    return ItemInstance.GetCurrentDurability();
}

float AMedComInventoryItem::GetMaxDurability() const
{
    return GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
}

float AMedComInventoryItem::GetDurabilityPercent() const
{
    return ItemInstance.GetDurabilityPercent();
}

void AMedComInventoryItem::SetCurrentDurability(float NewDurability)
{
    ItemInstance.SetCurrentDurability(NewDurability);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("SetCurrentDurability: %s durability set to %.1f"),
        *ItemInstance.ItemID.ToString(), NewDurability);
}

int32 AMedComInventoryItem::GetCurrentAmmo() const
{
    return ItemInstance.GetCurrentAmmo();
}

int32 AMedComInventoryItem::GetMaxAmmo() const
{
    return FMath::RoundToInt(GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
}

void AMedComInventoryItem::SetCurrentAmmo(int32 NewAmmo)
{
    ItemInstance.SetCurrentAmmo(NewAmmo);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("SetCurrentAmmo: %s ammo set to %d"),
        *ItemInstance.ItemID.ToString(), NewAmmo);
}

//==================================================================
// Item Initialization (Enhanced for New Architecture)
//==================================================================

bool AMedComInventoryItem::InitializeFromID(const FName& InItemID, int32 InAmount)
{
    // Валидация входных параметров
    if (InItemID.IsNone() || InAmount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeFromID: Invalid parameters - ID: %s, Amount: %d"), 
            *InItemID.ToString(), InAmount);
        return false;
    }
    
    // Проверяем доступность ItemManager
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeFromID: ItemManager not available"));
        return false;
    }
    
    // Проверяем что предмет существует в DataTable
    FMedComUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(InItemID, ItemData))
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeFromID: Item '%s' not found in DataTable"), 
            *InItemID.ToString());
        return false;
    }
    
    // ИСПРАВЛЕНО: Добавлен WorldContextObject (this) как первый параметр
    // КЛЮЧЕВОЕ ИЗМЕНЕНИЕ: Используем InventoryUtils для правильной инициализации
    // Это автоматически установит все runtime свойства на основе типа предмета
    ItemInstance = InventoryUtils::CreateItemInstance(this, InItemID, InAmount);
    
    // Проверяем что экземпляр создался корректно
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeFromID: Failed to create valid instance for '%s'"), 
            *InItemID.ToString());
        return false;
    }
    
    // Кэшируем данные для производительности
    CachedItemData = ItemData;
    CacheTime = FDateTime::Now();
    
    // Помечаем как инициализированный
    bIsInitialized = true;
    
    UE_LOG(LogTemp, Log, TEXT("InitializeFromID: Successfully initialized '%s' with amount %d"), 
        *InItemID.ToString(), InAmount);
    
    // Логируем runtime свойства для отладки
    if (ItemInstance.RuntimeProperties.Num() > 0)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("InitializeFromID: Item has %d runtime properties"),
            ItemInstance.RuntimeProperties.Num());
        
        for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("  %s = %.2f"), 
                *PropertyPair.Key.ToString(), PropertyPair.Value);
        }
    }
    
    return true;
}

//==================================================================
// Enhanced Item Instance Management
//==================================================================

bool AMedComInventoryItem::SetItemInstance(const FInventoryItemInstance& InInstance)
{
    // Валидация нового экземпляра
    if (!InInstance.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SetItemInstance: Invalid instance provided"));
        return false;
    }
    
    // Проверяем что предмет существует в DataTable
    FMedComUnifiedItemData ItemData;
    if (!GetItemManager() || !GetItemManager()->GetUnifiedItemData(InInstance.ItemID, ItemData))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetItemInstance: Item '%s' not found in DataTable"), 
            *InInstance.ItemID.ToString());
        return false;
    }
    
    // Логируем изменение экземпляра
    FGuid OldInstanceID = ItemInstance.InstanceID;
    FName OldItemID = ItemInstance.ItemID;
    
    // Применяем новый экземпляр
    ItemInstance = InInstance;
    
    // Очищаем кэш для принудительного обновления
    ClearCachedData();
    
    // Обновляем состояние инициализации
    bIsInitialized = ItemInstance.IsValid();
    
    UE_LOG(LogTemp, Log, TEXT("SetItemInstance: Changed from '%s'[%s] to '%s'[%s]"),
        *OldItemID.ToString(), *OldInstanceID.ToString(),
        *ItemInstance.ItemID.ToString(), *ItemInstance.InstanceID.ToString());
    
    return true;
}

void AMedComInventoryItem::RefreshFromDataTable()
{
    if (ItemInstance.ItemID.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("RefreshFromDataTable: Invalid ItemID"));
        return;
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("RefreshFromDataTable: Refreshing data for '%s'"),
        *ItemInstance.ItemID.ToString());
    
    // Очищаем кэш для принудительного обновления
    ClearCachedData();
    
    // Принудительно обновляем кэшированные данные
    UpdateCachedData();
    
    UE_LOG(LogTemp, Log, TEXT("RefreshFromDataTable: Successfully refreshed data for '%s'"),
        *ItemInstance.ItemID.ToString());
}

//==================================================================
// Weapon Ammo State Persistence
//==================================================================

float AMedComInventoryItem::GetSavedCurrentAmmo() const
{
    return GetRuntimeProperty(TEXT("SavedCurrentAmmo"), 0.0f);
}

float AMedComInventoryItem::GetSavedRemainingAmmo() const
{
    return GetRuntimeProperty(TEXT("SavedRemainingAmmo"), 0.0f);
}

bool AMedComInventoryItem::HasSavedAmmoState() const
{
    return GetRuntimeProperty(TEXT("HasSavedAmmoState"), 0.0f) > 0.0f;
}

void AMedComInventoryItem::SetSavedAmmoState(float CurrentAmmo, float RemainingAmmo)
{
    SetRuntimeProperty(TEXT("SavedCurrentAmmo"), CurrentAmmo);
    SetRuntimeProperty(TEXT("SavedRemainingAmmo"), RemainingAmmo);
    SetRuntimeProperty(TEXT("HasSavedAmmoState"), 1.0f);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("SetSavedAmmoState: %s saved ammo state Current=%.0f, Remaining=%.0f"),
        *ItemInstance.ItemID.ToString(), CurrentAmmo, RemainingAmmo);
}

void AMedComInventoryItem::ClearSavedAmmoState()
{
    ItemInstance.RuntimeProperties.Remove(TEXT("SavedCurrentAmmo"));
    ItemInstance.RuntimeProperties.Remove(TEXT("SavedRemainingAmmo"));
    ItemInstance.RuntimeProperties.Remove(TEXT("HasSavedAmmoState"));
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("ClearSavedAmmoState: Cleared saved ammo state for %s"),
        *ItemInstance.ItemID.ToString());
}

//==================================================================
// Validation and Debug Support
//==================================================================

bool AMedComInventoryItem::ValidateItemState(TArray<FString>& OutErrors) const
{
    return ValidateItemStateInternal(OutErrors);
}

bool AMedComInventoryItem::ValidateItemStateInternal(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();
    
    // Базовая валидация инициализации
    if (!bIsInitialized)
    {
        OutErrors.Add(TEXT("Item not initialized"));
        return false;
    }
    
    // Валидация ItemID
    if (ItemInstance.ItemID.IsNone())
    {
        OutErrors.Add(TEXT("Invalid ItemID"));
        return false;
    }
    
    // Валидация количества
    if (ItemInstance.Quantity <= 0)
    {
        OutErrors.Add(FString::Printf(TEXT("Invalid quantity: %d"), ItemInstance.Quantity));
    }
    
    // Валидация GUID
    if (!ItemInstance.InstanceID.IsValid())
    {
        OutErrors.Add(TEXT("Invalid instance GUID"));
    }
    
    // Валидация существования в DataTable
    FMedComUnifiedItemData ItemData;
    if (!GetItemData(ItemData))
    {
        OutErrors.Add(FString::Printf(TEXT("Item '%s' not found in DataTable"), *ItemInstance.ItemID.ToString()));
        return false;
    }
    
    // Валидация размера стека
    if (ItemInstance.Quantity > ItemData.MaxStackSize)
    {
        OutErrors.Add(FString::Printf(TEXT("Quantity %d exceeds max stack size %d"), 
            ItemInstance.Quantity, ItemData.MaxStackSize));
    }
    
    // Валидация runtime свойств для конкретных типов предметов
    if (ItemData.bIsWeapon)
    {
        if (!ItemInstance.HasRuntimeProperty(TEXT("MaxAmmo")))
        {
            OutErrors.Add(TEXT("Weapon missing MaxAmmo runtime property"));
        }
    }
    
    if (ItemData.bIsEquippable)
    {
        if (!ItemInstance.HasRuntimeProperty(TEXT("MaxDurability")))
        {
            OutErrors.Add(TEXT("Equippable item missing MaxDurability runtime property"));
        }
    }
    
    return OutErrors.Num() == 0;
}

FString AMedComInventoryItem::GetItemStateDebugInfo() const
{
    FString DebugInfo = FString::Printf(
        TEXT("=== AMedComInventoryItem Debug Info ===\n")
        TEXT("ItemID: %s\n")
        TEXT("InstanceID: %s\n")
        TEXT("Quantity: %d\n")
        TEXT("IsInitialized: %s\n")
        TEXT("IsRotated: %s\n")
        TEXT("AnchorIndex: %d\n"),
        *ItemInstance.ItemID.ToString(),
        *ItemInstance.InstanceID.ToString(),
        ItemInstance.Quantity,
        bIsInitialized ? TEXT("Yes") : TEXT("No"),
        ItemInstance.bIsRotated ? TEXT("Yes") : TEXT("No"),
        ItemInstance.AnchorIndex
    );
    
    // Добавляем информацию о размерах
    FVector2D BaseSize = GetBaseGridSize();
    FVector2D EffectiveSize = GetEffectiveGridSize();
    
    DebugInfo += FString::Printf(
        TEXT("Base Size: %.0fx%.0f\n")
        TEXT("Effective Size: %.0fx%.0f\n")
        TEXT("Weight: %.2f (Total: %.2f)\n"),
        BaseSize.X, BaseSize.Y,
        EffectiveSize.X, EffectiveSize.Y,
        GetItemWeight(), GetTotalWeight()
    );
    
    // Добавляем информацию о кэше
    DebugInfo += FString::Printf(
        TEXT("Cache Status: %s (Age: %.1fs)\n"),
        IsDataCached() ? TEXT("Valid") : TEXT("Invalid"),
        GetCacheAge()
    );
    
    // Добавляем runtime свойства
    if (ItemInstance.RuntimeProperties.Num() > 0)
    {
        DebugInfo += TEXT("Runtime Properties:\n");
        for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
        {
            DebugInfo += FString::Printf(TEXT("  %s: %.2f\n"), 
                *PropertyPair.Key.ToString(), PropertyPair.Value);
        }
    }
    else
    {
        DebugInfo += TEXT("Runtime Properties: None\n");
    }
    
    // Добавляем валидацию
    TArray<FString> ValidationErrors;
    bool bIsValid = ValidateItemStateInternal(ValidationErrors);
    
    DebugInfo += FString::Printf(TEXT("Validation: %s\n"), bIsValid ? TEXT("PASS") : TEXT("FAIL"));
    
    if (!bIsValid)
    {
        DebugInfo += TEXT("Validation Errors:\n");
        for (const FString& Error : ValidationErrors)
        {
            DebugInfo += FString::Printf(TEXT("  - %s\n"), *Error);
        }
    }
    
    return DebugInfo;
}

bool AMedComInventoryItem::IsDataCached() const
{
    if (!CachedItemData.IsSet())
    {
        return false;
    }
    
    float CacheAge = (FDateTime::Now() - CacheTime).GetTotalSeconds();
    return CacheAge < CACHE_DURATION;
}

float AMedComInventoryItem::GetCacheAge() const
{
    if (!CachedItemData.IsSet())
    {
        return -1.0f; // Нет кэша
    }
    
    return (FDateTime::Now() - CacheTime).GetTotalSeconds();
}

//==================================================================
// Internal Helper Methods
//==================================================================

void AMedComInventoryItem::UpdateCachedData() const
{
    if (!IsDataCached())
    {
        FMedComUnifiedItemData ItemData;
        if (GetItemData(ItemData))
        {
            CachedItemData = ItemData;
            CacheTime = FDateTime::Now();
            
            UE_LOG(LogTemp, VeryVerbose, TEXT("UpdateCachedData: Refreshed cache for %s"),
                *ItemInstance.ItemID.ToString());
        }
    }
}

void AMedComInventoryItem::ClearCachedData()
{
    if (CachedItemData.IsSet())
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("ClearCachedData: Cleared cache for %s"),
            *ItemInstance.ItemID.ToString());
    }
    
    CachedItemData.Reset();
}

const FMedComUnifiedItemData* AMedComInventoryItem::GetCachedUnifiedData() const
{
    UpdateCachedData();
    return CachedItemData.IsSet() ? &CachedItemData.GetValue() : nullptr;
}
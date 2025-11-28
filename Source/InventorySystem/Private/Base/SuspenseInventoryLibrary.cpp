// Copyright Suspense Team. All Rights Reserved.

#include "Base/SuspenseInventoryLibrary.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Base/SuspenseInventoryItem.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

// Определяем константы для производительности и consistency
static const float CACHE_DURATION = 10.0f; // Время жизни кэша в секундах
static const FLinearColor DEFAULT_RARITY_COLOR = FLinearColor::White;
static const FIntPoint DEFAULT_ITEM_SIZE = FIntPoint(1, 1);

//==================================================================
// Core Item Creation and Management Implementation
//==================================================================

FInventoryItemInstance USuspenseInventoryLibrary::CreateItemInstance(const FName& ItemID, int32 Quantity, const UObject* WorldContext)
{
    // Валидация входных параметров
    if (ItemID.IsNone() || Quantity <= 0)
    {
        LogError(TEXT("CreateItemInstance"), TEXT("Invalid parameters"), ItemID);
        return FInventoryItemInstance();
    }
    
    if (!ValidateWorldContext(WorldContext, TEXT("CreateItemInstance")))
    {
        return FInventoryItemInstance();
    }
    
    // Получаем ItemManager для создания экземпляра
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    if (!ItemManager)
    {
        LogError(TEXT("CreateItemInstance"), TEXT("ItemManager not available"), ItemID);
        return FInventoryItemInstance();
    }
    
    // Делегируем создание экземпляра ItemManager
    FInventoryItemInstance NewInstance;
    if (ItemManager->CreateItemInstance(ItemID, Quantity, NewInstance))
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("CreateItemInstance: Successfully created %s (x%d) [%s]"),
            *ItemID.ToString(), Quantity, *NewInstance.InstanceID.ToString().Left(8));
        return NewInstance;
    }
    
    LogError(TEXT("CreateItemInstance"), TEXT("Failed to create instance"), ItemID);
    return FInventoryItemInstance();
}

int32 USuspenseInventoryLibrary::CreateItemInstancesFromSpawnData(const TArray<FPickupSpawnData>& SpawnDataArray,
                                                                       const UObject* WorldContext,
                                                                       TArray<FInventoryItemInstance>& OutInstances)
{
    OutInstances.Empty();
    
    if (!ValidateWorldContext(WorldContext, TEXT("CreateItemInstancesFromSpawnData")))
    {
        return 0;
    }
    
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    if (!ItemManager)
    {
        LogError(TEXT("CreateItemInstancesFromSpawnData"), TEXT("ItemManager not available"));
        return 0;
    }
    
    // Делегируем batch creation ItemManager для оптимизации
    int32 SuccessCount = ItemManager->CreateItemInstancesFromSpawnData(SpawnDataArray, OutInstances);
    
    UE_LOG(LogInventory, Log, TEXT("CreateItemInstancesFromSpawnData: Created %d/%d instances"),
        SuccessCount, SpawnDataArray.Num());
    
    return SuccessCount;
}

bool USuspenseInventoryLibrary::GetUnifiedItemData(const FName& ItemID, const UObject* WorldContext, FSuspenseUnifiedItemData& OutItemData)
{
    if (ItemID.IsNone())
    {
        LogError(TEXT("GetUnifiedItemData"), TEXT("Empty ItemID"));
        return false;
    }
    
    if (!ValidateWorldContext(WorldContext, TEXT("GetUnifiedItemData")))
    {
        return false;
    }
    
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    if (!ItemManager)
    {
        LogError(TEXT("GetUnifiedItemData"), TEXT("ItemManager not available"), ItemID);
        return false;
    }
    
    bool bSuccess = ItemManager->GetUnifiedItemData(ItemID, OutItemData);
    if (!bSuccess)
    {
        LogError(TEXT("GetUnifiedItemData"), TEXT("Item not found in DataTable"), ItemID);
    }
    
    return bSuccess;
}

AActor* USuspenseInventoryLibrary::SpawnItemInWorld(const FInventoryItemInstance& ItemInstance,
                                                         UWorld* World,
                                                         const FTransform& Transform)
{
    // Валидация параметров
    if (!ItemInstance.IsValid())
    {
        LogError(TEXT("SpawnItemInWorld"), TEXT("Invalid ItemInstance"));
        return nullptr;
    }
    
    if (!World)
    {
        LogError(TEXT("SpawnItemInWorld"), TEXT("Invalid World"), ItemInstance.ItemID);
        return nullptr;
    }
    
    // Настройка параметров создания Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = false; // Немедленная инициализация
    
    // Создаем актор предмета в мире
    AActor* ItemActor = World->SpawnActor<ASuspenseInventoryItem>(ASuspenseInventoryItem::StaticClass(), Transform, SpawnParams);
    
    if (!ItemActor)
    {
        LogError(TEXT("SpawnItemInWorld"), TEXT("Failed to spawn actor"), ItemInstance.ItemID);
        return nullptr;
    }
    
    // Инициализируем актор с runtime экземпляром
    if (ASuspenseInventoryItem* InventoryItem = Cast<ASuspenseInventoryItem>(ItemActor))
    {
        if (InventoryItem->SetItemInstance(ItemInstance))
        {
            UE_LOG(LogInventory, Log, TEXT("SpawnItemInWorld: Successfully spawned %s (x%d) at %s"),
                *ItemInstance.ItemID.ToString(), 
                ItemInstance.Quantity,
                *Transform.GetLocation().ToString());
            
            return ItemActor;
        }
        else
        {
            // Уничтожаем актор если инициализация не удалась
            ItemActor->Destroy();
            LogError(TEXT("SpawnItemInWorld"), TEXT("Failed to initialize spawned actor"), ItemInstance.ItemID);
        }
    }
    else
    {
        ItemActor->Destroy();
        LogError(TEXT("SpawnItemInWorld"), TEXT("Spawned actor is not ASuspenseInventoryItem"), ItemInstance.ItemID);
    }
    
    return nullptr;
}

//==================================================================
// Enhanced Item Validation and Analysis Implementation
//==================================================================

bool USuspenseInventoryLibrary::ValidateItemInstance(const FInventoryItemInstance& ItemInstance,
                                                          const UObject* WorldContext,
                                                          TArray<FString>& OutErrors)
{
    OutErrors.Empty();
    
    // Базовая валидация runtime экземпляра
    if (!ItemInstance.IsValid())
    {
        OutErrors.Add(TEXT("Instance is not valid (empty ItemID or invalid GUID)"));
        return false;
    }
    
    if (!ValidateWorldContext(WorldContext, TEXT("ValidateItemInstance")))
    {
        OutErrors.Add(TEXT("Invalid world context"));
        return false;
    }
    
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    if (!ItemManager)
    {
        OutErrors.Add(TEXT("ItemManager not available"));
        return false;
    }
    
    // Проверяем существование в DataTable
    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, ItemData))
    {
        OutErrors.Add(FString::Printf(TEXT("Item '%s' not found in DataTable"), *ItemInstance.ItemID.ToString()));
        return false;
    }
    
    // Валидация количества против максимального размера стека
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
            OutErrors.Add(TEXT("Weapon missing required MaxAmmo runtime property"));
        }
        
        int32 CurrentAmmo = ItemInstance.GetCurrentAmmo();
        int32 MaxAmmo = FMath::RoundToInt(ItemInstance.GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
        
        if (CurrentAmmo > MaxAmmo)
        {
            OutErrors.Add(FString::Printf(TEXT("Current ammo %d exceeds max ammo %d"), CurrentAmmo, MaxAmmo));
        }
    }
    
    if (ItemData.bIsEquippable)
    {
        if (!ItemInstance.HasRuntimeProperty(TEXT("MaxDurability")))
        {
            OutErrors.Add(TEXT("Equippable item missing required MaxDurability runtime property"));
        }
        else
        {
            float CurrentDurability = ItemInstance.GetCurrentDurability();
            float MaxDurability = ItemInstance.GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
            
            if (CurrentDurability > MaxDurability)
            {
                OutErrors.Add(FString::Printf(TEXT("Current durability %.1f exceeds max durability %.1f"), 
                    CurrentDurability, MaxDurability));
            }
        }
    }
    
    // Валидация временных данных
    if (ItemInstance.LastUsedTime < 0.0f)
    {
        OutErrors.Add(TEXT("Invalid LastUsedTime (negative value)"));
    }
    
    bool bIsValid = OutErrors.Num() == 0;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("ValidateItemInstance: %s validation %s (%d errors)"),
        *ItemInstance.ItemID.ToString(),
        bIsValid ? TEXT("PASSED") : TEXT("FAILED"),
        OutErrors.Num());
    
    return bIsValid;
}

bool USuspenseInventoryLibrary::IsValidItemID(const FName& ItemID, const UObject* WorldContext)
{
    if (ItemID.IsNone())
    {
        return false;
    }
    
    if (!ValidateWorldContext(WorldContext, TEXT("IsValidItemID")))
    {
        return false;
    }
    
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    if (!ItemManager)
    {
        return false;
    }
    
    // Используем lightweight проверку существования
    return ItemManager->HasItem(ItemID);
}

bool USuspenseInventoryLibrary::IsValidQuantityForItem(const FName& ItemID, int32 Quantity, const UObject* WorldContext)
{
    if (Quantity <= 0)
    {
        return false;
    }
    
    int32 MaxStackSize = GetMaxStackSize(ItemID, WorldContext);
    return Quantity <= MaxStackSize;
}

//==================================================================
// Advanced Grid and Placement Operations Implementation
//==================================================================

bool USuspenseInventoryLibrary::IndexToGridCoords(int32 Index, int32 GridWidth, int32& OutX, int32& OutY)
{
    // Enhanced валидация с более строгими проверками
    if (Index < 0 || GridWidth <= 0)
    {
        OutX = -1;
        OutY = -1;
        UE_LOG(LogInventory, VeryVerbose, TEXT("IndexToGridCoords: Invalid parameters - Index: %d, Width: %d"), Index, GridWidth);
        return false;
    }
    
    OutX = Index % GridWidth;
    OutY = Index / GridWidth;
    
    return true;
}

int32 USuspenseInventoryLibrary::GridCoordsToIndex(int32 X, int32 Y, int32 GridWidth, int32 GridHeight)
{
    // Enhanced валидация с boundary checking
    if (X < 0 || Y < 0 || GridWidth <= 0)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("GridCoordsToIndex: Invalid coordinates - X: %d, Y: %d, Width: %d"), X, Y, GridWidth);
        return INDEX_NONE;
    }
    
    // Дополнительная проверка высоты если указана
    if (GridHeight > 0 && Y >= GridHeight)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("GridCoordsToIndex: Y coordinate %d exceeds grid height %d"), Y, GridHeight);
        return INDEX_NONE;
    }
    
    // Проверка X координаты
    if (X >= GridWidth)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("GridCoordsToIndex: X coordinate %d exceeds grid width %d"), X, GridWidth);
        return INDEX_NONE;
    }
    
    return Y * GridWidth + X;
}

bool USuspenseInventoryLibrary::CanItemFitAtPosition(const FName& ItemID,
                                                          int32 AnchorX, int32 AnchorY,
                                                          int32 GridWidth, int32 GridHeight,
                                                          bool bIsRotated,
                                                          const UObject* WorldContext)
{
    // Получаем размер предмета из DataTable
    FIntPoint ItemSize = GetEffectiveItemSize(ItemID, bIsRotated, WorldContext);
    
    // Проверяем выход за границы сетки
    if (AnchorX < 0 || AnchorY < 0 || 
        AnchorX + ItemSize.X > GridWidth || 
        AnchorY + ItemSize.Y > GridHeight)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("CanItemFitAtPosition: %s (size %dx%d) doesn't fit at (%d,%d) in grid %dx%d"),
            *ItemID.ToString(), ItemSize.X, ItemSize.Y, AnchorX, AnchorY, GridWidth, GridHeight);
        return false;
    }
    
    return true;
}

int32 USuspenseInventoryLibrary::FindOptimalPlacementForItem(const FName& ItemID,
                                                                  int32 GridWidth, int32 GridHeight,
                                                                  const TArray<int32>& OccupiedSlots,
                                                                  bool bAllowRotation,
                                                                  const UObject* WorldContext,
                                                                  bool& OutRotated)
{
    OutRotated = false;
    
    if (ItemID.IsNone() || GridWidth <= 0 || GridHeight <= 0)
    {
        return INDEX_NONE;
    }
    
    // Создаем occupancy map для быстрой проверки
    TSet<int32> OccupiedSet(OccupiedSlots);
    
    // Получаем размеры предмета для обеих ориентаций
    FIntPoint NormalSize = GetEffectiveItemSize(ItemID, false, WorldContext);
    FIntPoint RotatedSize = GetEffectiveItemSize(ItemID, true, WorldContext);
    
    // Функция для проверки размещения в конкретной позиции
    auto CanPlaceAtPosition = [&](int32 X, int32 Y, const FIntPoint& Size) -> bool
    {
        // Проверяем границы
        if (X + Size.X > GridWidth || Y + Size.Y > GridHeight)
        {
            return false;
        }
        
        // Проверяем пересечения с занятыми слотами
        for (int32 ItemY = Y; ItemY < Y + Size.Y; ++ItemY)
        {
            for (int32 ItemX = X; ItemX < X + Size.X; ++ItemX)
            {
                int32 SlotIndex = ItemY * GridWidth + ItemX;
                if (OccupiedSet.Contains(SlotIndex))
                {
                    return false;
                }
            }
        }
        
        return true;
    };
    
    // Пытаемся разместить в нормальной ориентации сначала
    for (int32 Y = 0; Y <= GridHeight - NormalSize.Y; ++Y)
    {
        for (int32 X = 0; X <= GridWidth - NormalSize.X; ++X)
        {
            if (CanPlaceAtPosition(X, Y, NormalSize))
            {
                OutRotated = false;
                int32 AnchorIndex = Y * GridWidth + X;
                
                UE_LOG(LogInventory, VeryVerbose, TEXT("FindOptimalPlacement: %s placed at (%d,%d) normal orientation"),
                    *ItemID.ToString(), X, Y);
                
                return AnchorIndex;
            }
        }
    }
    
    // Если нормальная ориентация не подходит и разрешен поворот, пробуем повернутую
    if (bAllowRotation && (NormalSize.X != RotatedSize.X || NormalSize.Y != RotatedSize.Y))
    {
        for (int32 Y = 0; Y <= GridHeight - RotatedSize.Y; ++Y)
        {
            for (int32 X = 0; X <= GridWidth - RotatedSize.X; ++X)
            {
                if (CanPlaceAtPosition(X, Y, RotatedSize))
                {
                    OutRotated = true;
                    int32 AnchorIndex = Y * GridWidth + X;
                    
                    UE_LOG(LogInventory, VeryVerbose, TEXT("FindOptimalPlacement: %s placed at (%d,%d) rotated orientation"),
                        *ItemID.ToString(), X, Y);
                    
                    return AnchorIndex;
                }
            }
        }
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FindOptimalPlacement: No suitable position found for %s"), *ItemID.ToString());
    return INDEX_NONE;
}

TArray<int32> USuspenseInventoryLibrary::GetOccupiedSlots(const FName& ItemID,
                                                              int32 AnchorIndex,
                                                              int32 GridWidth,
                                                              bool bIsRotated,
                                                              const UObject* WorldContext)
{
    TArray<int32> OccupiedSlots;
    
    if (ItemID.IsNone() || AnchorIndex < 0 || GridWidth <= 0)
    {
        return OccupiedSlots;
    }
    
    // Получаем размер предмета
    FIntPoint ItemSize = GetEffectiveItemSize(ItemID, bIsRotated, WorldContext);
    
    // Конвертируем anchor index в координаты
    int32 AnchorX, AnchorY;
    if (!IndexToGridCoords(AnchorIndex, GridWidth, AnchorX, AnchorY))
    {
        return OccupiedSlots;
    }
    
    // Генерируем все занимаемые слоты
    OccupiedSlots.Reserve(ItemSize.X * ItemSize.Y);
    
    for (int32 Y = AnchorY; Y < AnchorY + ItemSize.Y; ++Y)
    {
        for (int32 X = AnchorX; X < AnchorX + ItemSize.X; ++X)
        {
            int32 SlotIndex = Y * GridWidth + X;
            OccupiedSlots.Add(SlotIndex);
        }
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("GetOccupiedSlots: %s at anchor %d occupies %d slots"),
        *ItemID.ToString(), AnchorIndex, OccupiedSlots.Num());
    
    return OccupiedSlots;
}

FIntPoint USuspenseInventoryLibrary::GetEffectiveItemSize(const FName& ItemID, bool bIsRotated, const UObject* WorldContext)
{
    if (ItemID.IsNone() || !ValidateWorldContext(WorldContext, TEXT("GetEffectiveItemSize")))
    {
        return DEFAULT_ITEM_SIZE;
    }
    
    FSuspenseUnifiedItemData ItemData;
    if (!GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        LogError(TEXT("GetEffectiveItemSize"), TEXT("Failed to get item data"), ItemID);
        return DEFAULT_ITEM_SIZE;
    }
    
    FIntPoint BaseSize = ItemData.GridSize;
    
    // Применяем поворот если необходимо
    if (bIsRotated)
    {
        return FIntPoint(BaseSize.Y, BaseSize.X); // Меняем местами X и Y
    }
    
    return BaseSize;
}

//==================================================================
// Enhanced Weight and Resource Management Implementation
//==================================================================

float USuspenseInventoryLibrary::CalculateTotalWeightFromInstances(const TArray<FInventoryItemInstance>& ItemInstances,
                                                                        const UObject* WorldContext)
{
    if (!ValidateWorldContext(WorldContext, TEXT("CalculateTotalWeightFromInstances")))
    {
        return 0.0f;
    }
    
    float TotalWeight = 0.0f;
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    
    if (!ItemManager)
    {
        LogError(TEXT("CalculateTotalWeightFromInstances"), TEXT("ItemManager not available"));
        return 0.0f;
    }
    
    // Эффективно обрабатываем каждый экземпляр
    for (const FInventoryItemInstance& Instance : ItemInstances)
    {
        if (!Instance.IsValid())
        {
            continue;
        }
        
        float ItemWeight = GetItemWeight(Instance.ItemID, WorldContext);
        TotalWeight += ItemWeight * Instance.Quantity;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("CalculateTotalWeightFromInstances: Total weight %.2f from %d instances"),
        TotalWeight, ItemInstances.Num());
    
    return TotalWeight;
}

bool USuspenseInventoryLibrary::CanAddItemsByWeight(float CurrentWeight,
                                                         const TArray<FInventoryItemInstance>& ItemInstances,
                                                         float MaxWeight,
                                                         const UObject* WorldContext)
{
    if (MaxWeight <= 0.0f)
    {
        return true; // Unlimited weight
    }
    
    float AdditionalWeight = CalculateTotalWeightFromInstances(ItemInstances, WorldContext);
    float NewTotalWeight = CurrentWeight + AdditionalWeight;
    
    bool bCanAdd = NewTotalWeight <= MaxWeight;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("CanAddItemsByWeight: Current=%.2f + Additional=%.2f = %.2f / %.2f Max (Result: %s)"),
        CurrentWeight, AdditionalWeight, NewTotalWeight, MaxWeight, bCanAdd ? TEXT("OK") : TEXT("OVERWEIGHT"));
    
    return bCanAdd;
}

float USuspenseInventoryLibrary::GetItemWeight(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        return ItemData.Weight;
    }
    
    // Fallback weight для неизвестных предметов
    return 1.0f;
}

//==================================================================
// Enhanced Tag System and Compatibility Implementation
//==================================================================

bool USuspenseInventoryLibrary::IsItemCompatibleWithSlot(const FName& ItemID,
                                                              const FGameplayTag& SlotType,
                                                              const UObject* WorldContext)
{
    FGameplayTag ItemType = GetItemType(ItemID, WorldContext);
    
    if (!ItemType.IsValid() || !SlotType.IsValid())
    {
        return false;
    }
    
    // Проверяем прямое соответствие
    if (ItemType.MatchesTag(SlotType))
    {
        return true;
    }
    
    // Специальные правила совместимости для equipment slots
    if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Weapon"))))
    {
        return ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon")));
    }
    
    if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Armor"))))
    {
        return ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor")));
    }
    
    if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Helmet"))))
    {
        return ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Helmet")));
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("IsItemCompatibleWithSlot: %s (%s) not compatible with slot %s"),
        *ItemID.ToString(), *ItemType.ToString(), *SlotType.ToString());
    
    return false;
}

bool USuspenseInventoryLibrary::IsItemTypeAllowed(const FName& ItemID,
                                                       const FGameplayTagContainer& AllowedTypes,
                                                       const UObject* WorldContext)
{
    // Пустой контейнер означает разрешение всех типов
    if (AllowedTypes.IsEmpty())
    {
        return true;
    }
    
    FGameplayTag ItemType = GetItemType(ItemID, WorldContext);
    if (!ItemType.IsValid())
    {
        return false;
    }
    
    // Проверяем каждый разрешенный тег с hierarchical matching
    for (const FGameplayTag& AllowedTag : AllowedTypes)
    {
        if (ItemType.MatchesTag(AllowedTag))
        {
            return true;
        }
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("IsItemTypeAllowed: %s (%s) not allowed by type restrictions"),
        *ItemID.ToString(), *ItemType.ToString());
    
    return false;
}

FGameplayTag USuspenseInventoryLibrary::GetItemType(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        return ItemData.ItemType;
    }
    
    return FGameplayTag();
}

FGameplayTagContainer USuspenseInventoryLibrary::GetItemTags(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        FGameplayTagContainer AllTags = ItemData.ItemTags;
        
        // Добавляем primary type tag если его еще нет
        if (ItemData.ItemType.IsValid() && !AllTags.HasTag(ItemData.ItemType))
        {
            AllTags.AddTag(ItemData.ItemType);
        }
        
        return AllTags;
    }
    
    return FGameplayTagContainer();
}

//==================================================================
// Enhanced Stacking and Quantity Management Implementation
//==================================================================

bool USuspenseInventoryLibrary::CanStackInstances(const FInventoryItemInstance& Instance1,
                                                       const FInventoryItemInstance& Instance2,
                                                       const UObject* WorldContext)
{
    // Основные требования для stacking
    if (!Instance1.IsValid() || !Instance2.IsValid())
    {
        return false;
    }
    
    // Должны быть одного типа
    if (Instance1.ItemID != Instance2.ItemID)
    {
        return false;
    }
    
    // Проверяем максимальный размер стека
    int32 MaxStackSize = GetMaxStackSize(Instance1.ItemID, WorldContext);
    if (MaxStackSize <= 1)
    {
        return false; // Non-stackable item
    }
    
    // Проверяем что combined quantity не превысит limit
    if (Instance1.Quantity + Instance2.Quantity > MaxStackSize)
    {
        // Partial stacking still possible
        return Instance1.Quantity < MaxStackSize;
    }
    
    // Additional compatibility checks для runtime properties
    // Предметы с разными runtime properties могут не стекаться
    if (Instance1.HasRuntimeProperty(TEXT("Durability")) && Instance2.HasRuntimeProperty(TEXT("Durability")))
    {
        float Durability1 = Instance1.GetCurrentDurability();
        float Durability2 = Instance2.GetCurrentDurability();
        
        // Allowed small difference for floating point precision
        if (FMath::Abs(Durability1 - Durability2) > 0.1f)
        {
            return false; // Different durability values
        }
    }
    
    return true;
}

bool USuspenseInventoryLibrary::StackInstances(FInventoryItemInstance& SourceInstance,
                                                    const FInventoryItemInstance& TargetInstance,
                                                    const UObject* WorldContext,
                                                    FInventoryItemInstance& OutRemainder)
{
    OutRemainder = FInventoryItemInstance(); // Clear remainder
    
    if (!CanStackInstances(SourceInstance, TargetInstance, WorldContext))
    {
        return false;
    }
    
    int32 MaxStackSize = GetMaxStackSize(SourceInstance.ItemID, WorldContext);
    int32 AvailableSpace = MaxStackSize - SourceInstance.Quantity;
    int32 AmountToStack = FMath::Min(AvailableSpace, TargetInstance.Quantity);
    
    if (AmountToStack <= 0)
    {
        return false;
    }
    
    // Выполняем stacking
    SourceInstance.Quantity += AmountToStack;
    
    // Создаем remainder если необходимо
    if (AmountToStack < TargetInstance.Quantity)
    {
        OutRemainder = TargetInstance;
        OutRemainder.Quantity = TargetInstance.Quantity - AmountToStack;
        OutRemainder.InstanceID = FGuid::NewGuid(); // New GUID for remainder
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("StackInstances: Stacked %d of %s, remainder: %d"),
        AmountToStack, *SourceInstance.ItemID.ToString(), OutRemainder.Quantity);
    
    return true;
}

bool USuspenseInventoryLibrary::SplitInstance(FInventoryItemInstance& SourceInstance,
                                                   int32 SplitQuantity,
                                                   FInventoryItemInstance& OutNewInstance)
{
    OutNewInstance = FInventoryItemInstance(); // Clear output
    
    if (!SourceInstance.IsValid() || SplitQuantity <= 0 || SplitQuantity >= SourceInstance.Quantity)
    {
        return false;
    }
    
    // Создаем новый экземпляр
    OutNewInstance = SourceInstance;
    OutNewInstance.Quantity = SplitQuantity;
    OutNewInstance.InstanceID = FGuid::NewGuid(); // New unique GUID
    
    // Уменьшаем исходный экземпляр
    SourceInstance.Quantity -= SplitQuantity;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("SplitInstance: Split %s - Source: %d, New: %d"),
        *SourceInstance.ItemID.ToString(), SourceInstance.Quantity, OutNewInstance.Quantity);
    
    return true;
}

int32 USuspenseInventoryLibrary::GetMaxStackSize(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        return ItemData.MaxStackSize;
    }
    
    return 1; // Default non-stackable
}

//==================================================================
// Enhanced UI Support and Display Implementation
//==================================================================

FVector2D USuspenseInventoryLibrary::CalculateItemPositionInUI(int32 GridX, int32 GridY,
                                                                    const FVector2D& CellSize,
                                                                    float CellPadding,
                                                                    float GridBorderSize)
{
    return FVector2D(
        GridBorderSize + GridX * (CellSize.X + CellPadding),
        GridBorderSize + GridY * (CellSize.Y + CellPadding)
    );
}

FVector2D USuspenseInventoryLibrary::CalculateItemSizeInUI(const FName& ItemID,
                                                                const FVector2D& CellSize,
                                                                float CellPadding,
                                                                bool bIsRotated,
                                                                const UObject* WorldContext)
{
    FIntPoint ItemSize = GetEffectiveItemSize(ItemID, bIsRotated, WorldContext);
    
    return FVector2D(
        ItemSize.X * CellSize.X + FMath::Max(0, ItemSize.X - 1) * CellPadding,
        ItemSize.Y * CellSize.Y + FMath::Max(0, ItemSize.Y - 1) * CellPadding
    );
}

FText USuspenseInventoryLibrary::GetItemDisplayName(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        return ItemData.DisplayName;
    }
    
    return FText::FromString(ItemID.ToString());
}

FText USuspenseInventoryLibrary::GetItemDescription(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        return ItemData.Description;
    }
    
    return FText::FromString(TEXT("No description available"));
}

UTexture2D* USuspenseInventoryLibrary::GetItemIcon(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        if (!ItemData.Icon.IsNull())
        {
            return ItemData.Icon.LoadSynchronous();
        }
    }
    
    return nullptr;
}

FText USuspenseInventoryLibrary::FormatWeightForDisplay(float Weight, bool bShowUnit, int32 DecimalPlaces)
{
    DecimalPlaces = FMath::Clamp(DecimalPlaces, 0, 3);
    
    FNumberFormattingOptions FormattingOptions;
    FormattingOptions.MinimumFractionalDigits = DecimalPlaces;
    FormattingOptions.MaximumFractionalDigits = DecimalPlaces;
    
    if (bShowUnit)
    {
        return FText::Format(
            NSLOCTEXT("Inventory", "WeightWithUnit", "{0} kg"),
            FText::AsNumber(Weight, &FormattingOptions)
        );
    }
    else
    {
        return FText::AsNumber(Weight, &FormattingOptions);
    }
}

FLinearColor USuspenseInventoryLibrary::GetItemRarityColor(const FName& ItemID, const UObject* WorldContext)
{
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemID, WorldContext, ItemData))
    {
        // Get rarity color based on rarity tag
        FString RarityString = ItemData.Rarity.ToString();
        
        if (RarityString.Contains(TEXT("Common")))
        {
            return FLinearColor::White;
        }
        else if (RarityString.Contains(TEXT("Uncommon")))
        {
            return FLinearColor::Green;
        }
        else if (RarityString.Contains(TEXT("Rare")))
        {
            return FLinearColor::Blue;
        }
        else if (RarityString.Contains(TEXT("Epic")))
        {
            return FLinearColor(0.5f, 0.0f, 1.0f, 1.0f); // Purple
        }
        else if (RarityString.Contains(TEXT("Legendary")))
        {
            return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange
        }
    }
    
    return DEFAULT_RARITY_COLOR;
}

//==================================================================
// Enhanced Error Handling and Operations Implementation
//==================================================================

FText USuspenseInventoryLibrary::GetErrorMessage(EInventoryErrorCode ErrorCode, const FString& Context)
{
    FText BaseMessage;
    
    switch (ErrorCode)
    {
        case EInventoryErrorCode::Success:
            BaseMessage = NSLOCTEXT("Inventory", "Success", "Operation completed successfully");
            break;
        case EInventoryErrorCode::InvalidItem:
            BaseMessage = NSLOCTEXT("Inventory", "InvalidItem", "Invalid item");
            break;
        case EInventoryErrorCode::NoSpace:
            BaseMessage = NSLOCTEXT("Inventory", "NoSpace", "Not enough space in inventory");
            break;
        case EInventoryErrorCode::WeightLimit:
            BaseMessage = NSLOCTEXT("Inventory", "WeightLimit", "Weight limit exceeded");
            break;
        case EInventoryErrorCode::ItemNotFound:
            BaseMessage = NSLOCTEXT("Inventory", "ItemNotFound", "Item not found");
            break;
        case EInventoryErrorCode::InsufficientQuantity:
            BaseMessage = NSLOCTEXT("Inventory", "InsufficientQuantity", "Insufficient quantity");
            break;
        case EInventoryErrorCode::InvalidSlot:
            BaseMessage = NSLOCTEXT("Inventory", "InvalidSlot", "Invalid slot");
            break;
        case EInventoryErrorCode::SlotOccupied:
            BaseMessage = NSLOCTEXT("Inventory", "SlotOccupied", "Slot is occupied");
            break;
        case EInventoryErrorCode::TransactionActive:
            BaseMessage = NSLOCTEXT("Inventory", "TransactionActive", "Transaction in progress");
            break;
        case EInventoryErrorCode::NotInitialized:
            BaseMessage = NSLOCTEXT("Inventory", "NotInitialized", "Inventory not initialized");
            break;
        case EInventoryErrorCode::NetworkError:
            BaseMessage = NSLOCTEXT("Inventory", "NetworkError", "Network error");
            break;
        default:
            BaseMessage = NSLOCTEXT("Inventory", "UnknownError", "Unknown error");
            break;
    }
    
    // Add context if provided
    if (!Context.IsEmpty())
    {
        return FText::Format(
            NSLOCTEXT("Inventory", "ErrorWithContext", "{0}: {1}"),
            BaseMessage,
            FText::FromString(Context)
        );
    }
    
    return BaseMessage;
}

FInventoryOperationResult USuspenseInventoryLibrary::CreateSuccessResult(const FName& Context,
                                                                             UObject* ResultObject,
                                                                             const FString& AdditionalData)
{
    FInventoryOperationResult Result = FInventoryOperationResult::Success(Context, ResultObject);
    if (!AdditionalData.IsEmpty())
    {
        Result.AddResultData(TEXT("AdditionalData"), AdditionalData);
    }
    
    return Result;
}

FInventoryOperationResult USuspenseInventoryLibrary::CreateFailureResult(EInventoryErrorCode ErrorCode,
                                                                              const FText& ErrorMessage,
                                                                              const FName& Context,
                                                                              UObject* ResultObject)
{
    return FInventoryOperationResult::Failure(ErrorCode, ErrorMessage, Context, ResultObject);
}

//==================================================================
// Runtime Properties and Gameplay Integration Implementation
//==================================================================

float USuspenseInventoryLibrary::GetItemRuntimeProperty(const FInventoryItemInstance& ItemInstance,
                                                            const FName& PropertyName,
                                                            float DefaultValue)
{
    return ItemInstance.GetRuntimeProperty(PropertyName, DefaultValue);
}

void USuspenseInventoryLibrary::SetItemRuntimeProperty(FInventoryItemInstance& ItemInstance,
                                                           const FName& PropertyName,
                                                           float Value)
{
    ItemInstance.SetRuntimeProperty(PropertyName, Value);
}

bool USuspenseInventoryLibrary::HasItemRuntimeProperty(const FInventoryItemInstance& ItemInstance,
                                                           const FName& PropertyName)
{
    return ItemInstance.HasRuntimeProperty(PropertyName);
}

//==================================================================
// Debug and Development Utilities Implementation
//==================================================================

FString USuspenseInventoryLibrary::GetItemInstanceDebugInfo(const FInventoryItemInstance& ItemInstance,
                                                                const UObject* WorldContext)
{
    FString DebugInfo = FString::Printf(
        TEXT("=== ItemInstance Debug Info ===\n")
        TEXT("ItemID: %s\n")
        TEXT("InstanceID: %s\n")
        TEXT("Quantity: %d\n")
        TEXT("IsValid: %s\n")
        TEXT("LastUsedTime: %.2f\n"),
        *ItemInstance.ItemID.ToString(),
        *ItemInstance.InstanceID.ToString(),
        ItemInstance.Quantity,
        ItemInstance.IsValid() ? TEXT("Yes") : TEXT("No"),
        ItemInstance.LastUsedTime
    );
    
    // Add DataTable info if available
    FSuspenseUnifiedItemData ItemData;
    if (GetUnifiedItemData(ItemInstance.ItemID, WorldContext, ItemData))
    {
        DebugInfo += FString::Printf(
            TEXT("DisplayName: %s\n")
            TEXT("Type: %s\n")
            TEXT("GridSize: %dx%d\n")
            TEXT("Weight: %.2f (Total: %.2f)\n")
            TEXT("MaxStackSize: %d\n"),
            *ItemData.DisplayName.ToString(),
            *ItemData.ItemType.ToString(),
            ItemData.GridSize.X, ItemData.GridSize.Y,
            ItemData.Weight, ItemData.Weight * ItemInstance.Quantity,
            ItemData.MaxStackSize
        );
    }
    
    // Add runtime properties
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
    
    // Add validation info
    TArray<FString> ValidationErrors;
    bool bIsValid = ValidateItemInstance(ItemInstance, WorldContext, ValidationErrors);
    
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

FString USuspenseInventoryLibrary::GetItemManagerStatistics(const UObject* WorldContext)
{
    USuspenseItemManager* ItemManager = GetItemManager(WorldContext);
    if (!ItemManager)
    {
        return TEXT("ItemManager not available");
    }
    
    // Get statistics from ItemManager
    // NOTE: This assumes ItemManager has a GetStatistics method
    // You'll need to implement this in USuspenseItemManager
    
    return FString::Printf(
        TEXT("=== ItemManager Statistics ===\n")
        TEXT("ItemManager: %s\n")
        TEXT("Status: Available\n")
        TEXT("Note: Detailed statistics require implementation in USuspenseItemManager\n"),
        *ItemManager->GetClass()->GetName()
    );
}

//==================================================================
// Internal Helper Methods Implementation
//==================================================================

USuspenseItemManager* USuspenseInventoryLibrary::GetItemManager(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }
    
    UWorld* World = WorldContext->GetWorld();
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

void USuspenseInventoryLibrary::LogError(const FString& FunctionName, const FString& ErrorMessage, const FName& ItemID)
{
    if (ItemID.IsNone())
    {
        UE_LOG(LogInventory, Warning, TEXT("USuspenseInventoryLibrary::%s: %s"), *FunctionName, *ErrorMessage);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("USuspenseInventoryLibrary::%s: %s (ItemID: %s)"),
            *FunctionName, *ErrorMessage, *ItemID.ToString());
    }
}

bool USuspenseInventoryLibrary::ValidateWorldContext(const UObject* WorldContext, const FString& FunctionName)
{
    if (!WorldContext)
    {
        LogError(FunctionName, TEXT("WorldContext is null"));
        return false;
    }
    
    if (!WorldContext->GetWorld())
    {
        LogError(FunctionName, TEXT("World not available from context"));
        return false;
    }
    
    return true;
}
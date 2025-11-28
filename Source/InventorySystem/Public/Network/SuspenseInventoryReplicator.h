// MedComInventory/Network/SuspenseInventoryReplicator.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SuspenseInventoryReplicator.generated.h"

// Forward declarations для чистого разделения модулей
class USuspenseItemManager;
class ISuspenseInventoryItemInterface;

// Объявляем делегат для уведомления об обновлениях репликации
DECLARE_MULTICAST_DELEGATE(FOnReplicationUpdated);

/**
 * Compact cell representation for replication
 * Represents a single cell in the inventory grid
 */
USTRUCT()
struct FCompactReplicatedCell : public FFastArraySerializerItem
{
    GENERATED_BODY()
    
    // Index of the item in the metadata array or INDEX_NONE
    UPROPERTY()
    int16 ItemMetaIndex = INDEX_NONE;
    
    // Offset from anchor cell (for multi-cell items)
    UPROPERTY()
    FIntPoint Offset = FIntPoint::ZeroValue;
    
    // Default constructor - ensures proper initialization
    FCompactReplicatedCell()
    {
        ItemMetaIndex = INDEX_NONE;
        Offset = FIntPoint::ZeroValue;
    }
    
    // Constructor with parameters for convenience
    FCompactReplicatedCell(int16 InItemMetaIndex, const FIntPoint& InOffset = FIntPoint::ZeroValue)
        : ItemMetaIndex(InItemMetaIndex), Offset(InOffset) 
    {
    }
    
    // Checks if cell is occupied by any item
    bool IsOccupied() const { return ItemMetaIndex != INDEX_NONE; }
    
    // Checks if this is an anchor cell (top-left corner of item)
    bool IsAnchor() const { return IsOccupied() && Offset == FIntPoint::ZeroValue; }
    
    // Clears cell data
    void Clear() 
    { 
        ItemMetaIndex = INDEX_NONE; 
        Offset = FIntPoint::ZeroValue; 
    }
};

/**
 * ОБНОВЛЕНО: Enhanced item metadata for replication with DataTable integration
 * Содержит минимальные данные для восстановления предметов на клиенте
 * с полной интеграцией с новой DataTable архитектурой
 */
USTRUCT()
struct FReplicatedItemMeta : public FFastArraySerializerItem
{
    GENERATED_BODY()
    
    // Item ID для поиска в DataTable (основная связь с данными)
    UPROPERTY()
    FName ItemID = NAME_None;
    
    // Unique instance identifier для tracking в multiplayer
    UPROPERTY()
    FGuid InstanceID;
    
    // Stack size для данного экземпляра
    UPROPERTY()
    int32 Stack = 0;
    
    // Anchor cell index в сетке инвентаря
    UPROPERTY()
    int32 AnchorIndex = INDEX_NONE;
    
    // State flags для различных состояний предмета (упаковано для эффективности)
    UPROPERTY()
    uint8 ItemStateFlags = 0;
    
    // Data flags из DataTable (IsEquippable, IsStackable, etc)
    UPROPERTY()
    uint8 ItemDataFlags = 0;
    
    // Grid size упакованный в один байт (4 бита ширина, 4 бита высота)
    UPROPERTY()
    uint8 PackedGridSize = 0;
    
    // Weight одного предмета для client-side UI calculations
    UPROPERTY()
    float ItemWeight = 0.0f;
    
    // Current durability percentage (0-255 маппинг в 0-100%)
    UPROPERTY()
    uint8 DurabilityPercent = 255;
    
    // НОВОЕ: Количество runtime properties для эффективной репликации
    UPROPERTY()
    uint8 RuntimePropertiesCount = 0;

    /** 
     * Сохраненное количество патронов в магазине
     * Используется при переносе оружия между инвентарями для сохранения состояния
     */
    UPROPERTY()
    float SavedCurrentAmmo = 0.0f;
    
    /** 
     * Сохраненное количество патронов в резерве
     * Синхронизируется с AmmoAttributeSet когда оружие экипировано
     */
    UPROPERTY()
    float SavedRemainingAmmo = 0.0f;
    
    // НОВОЕ: Упакованные ключевые runtime properties (например, ammo count)
    // Для критически важных свойств, которые нужны на клиенте
    UPROPERTY()
    TMap<uint8, float> PackedRuntimeProperties;
    
    //==================================================================
    // Конструкторы и фабричные методы
    //==================================================================
    
    /**
     * Default constructor - ПУСТОЙ для совместимости с Fast Array Serialization
     * Система репликации UE создаёт экземпляры через default constructor
     * Реальная инициализация происходит через статические фабричные методы
     */
    FReplicatedItemMeta() = default;
    
    /**
     * Static factory method для создания нового метаданных с автогенерацией GUID
     * Используется при добавлении новых предметов в инвентарь
     */
    static FReplicatedItemMeta Create()
    {
        FReplicatedItemMeta Meta;
        Meta.ItemID = NAME_None;
        Meta.InstanceID = FGuid::NewGuid();
        Meta.Stack = 0;
        Meta.AnchorIndex = INDEX_NONE;
        Meta.ItemStateFlags = 0;
        Meta.ItemDataFlags = 0;
        Meta.PackedGridSize = 0;
        Meta.ItemWeight = 0.0f;
        Meta.DurabilityPercent = 255;
        Meta.SavedCurrentAmmo = 0.0f;
        Meta.SavedRemainingAmmo = 0.0f;
        Meta.RuntimePropertiesCount = 0;
        return Meta;
    }
    
    /**
     * Static factory method для создания с конкретным InstanceID
     * Используется при десериализации или восстановлении состояния
     */
    static FReplicatedItemMeta CreateWithID(const FGuid& InInstanceID)
    {
        FReplicatedItemMeta Meta;
        Meta.ItemID = NAME_None;
        Meta.InstanceID = InInstanceID;
        Meta.Stack = 0;
        Meta.AnchorIndex = INDEX_NONE;
        Meta.ItemStateFlags = 0;
        Meta.ItemDataFlags = 0;
        Meta.PackedGridSize = 0;
        Meta.ItemWeight = 0.0f;
        Meta.DurabilityPercent = 255;
        Meta.SavedCurrentAmmo = 0.0f;
        Meta.SavedRemainingAmmo = 0.0f;
        Meta.RuntimePropertiesCount = 0;
        return Meta;
    }
    
    //==================================================================
    // Flag bit positions и enums
    //==================================================================
    
    // Flag bit positions для ItemStateFlags
    enum EItemStateFlags : uint8
    {
        StateNone = 0,               // Переименовано из None для избежания конфликтов
        Rotated = 1 << 0,           // Предмет повернут на 90 градусов
        Locked = 1 << 1,            // Предмет нельзя перемещать
        HasAmmoState = 1 << 2,      // Предмет имеет сохраненные данные боеприпасов
        HasRuntimeProps = 1 << 3,   // Предмет имеет дополнительные runtime свойства
        Modified = 1 << 4,          // Переименовано из IsModified для устранения конфликта
        Reserved1 = 1 << 5,         // Зарезервировано для будущего использования
        Reserved2 = 1 << 6,
        Reserved3 = 1 << 7
    };
    
    // Flag bit positions для ItemDataFlags (соответствуют свойствам DataTable)
    enum EItemDataFlags : uint8
    {
        DataNone = 0,
        Stackable = 1 << 0,        // Можно складывать в стеки
        Consumable = 1 << 1,       // Можно потреблять/использовать
        Equippable = 1 << 2,       // Можно экипировать
        Droppable = 1 << 3,        // Можно выбрасывать
        Tradeable = 1 << 4,        // Можно торговать
        QuestItem = 1 << 5,        // Квестовый предмет
        CraftingMaterial = 1 << 6, // Материал для крафта
        HasDurability = 1 << 7     // Имеет систему прочности
    };
    
    //==================================================================
    // State flag utility methods
    //==================================================================
    
    bool IsRotated() const { return (ItemStateFlags & EItemStateFlags::Rotated) != 0; }
    bool IsLocked() const { return (ItemStateFlags & EItemStateFlags::Locked) != 0; }
    bool HasSavedAmmoState() const { return (ItemStateFlags & EItemStateFlags::HasAmmoState) != 0; }
    bool HasRuntimeProperties() const { return (ItemStateFlags & EItemStateFlags::HasRuntimeProps) != 0; }
    bool IsModified() const { return (ItemStateFlags & EItemStateFlags::Modified) != 0; }
    
    void SetRotated(bool bValue) 
    { 
        if (bValue) 
            ItemStateFlags |= EItemStateFlags::Rotated;
        else 
            ItemStateFlags &= ~EItemStateFlags::Rotated;
    }
    
    void SetLocked(bool bValue) 
    {
        if (bValue) 
            ItemStateFlags |= EItemStateFlags::Locked;
        else 
            ItemStateFlags &= ~EItemStateFlags::Locked;
    }
    
    void SetHasSavedAmmoState(bool bValue) 
    {
        if (bValue) 
            ItemStateFlags |= EItemStateFlags::HasAmmoState;
        else 
            ItemStateFlags &= ~EItemStateFlags::HasAmmoState;
    }
    
    /**
     * Установка сохраненного состояния патронов
     * В будущем будет интегрировано с AmmoAttributeSet
     */
    void SetSavedAmmoState(float CurrentAmmo, float RemainingAmmo)
    {
        SavedCurrentAmmo = CurrentAmmo;
        SavedRemainingAmmo = RemainingAmmo;
        SetHasSavedAmmoState(true);
    }
    
    /**
     * Получение сохраненного состояния патронов
     * Возвращает true если состояние было сохранено
     */
    bool GetSavedAmmoState(float& OutCurrentAmmo, float& OutRemainingAmmo) const
    {
        if (HasSavedAmmoState())
        {
            OutCurrentAmmo = SavedCurrentAmmo;
            OutRemainingAmmo = SavedRemainingAmmo;
            return true;
        }
        return false;
    }
    
    /**
     * Очистка сохраненного состояния патронов
     * Вызывается когда оружие синхронизировано с AttributeSet
     */
    void ClearSavedAmmoState()
    {
        SavedCurrentAmmo = 0.0f;
        SavedRemainingAmmo = 0.0f;
        SetHasSavedAmmoState(false);
    }
    
    void SetHasRuntimeProperties(bool bValue)
    {
        if (bValue) 
            ItemStateFlags |= EItemStateFlags::HasRuntimeProps;
        else 
            ItemStateFlags &= ~EItemStateFlags::HasRuntimeProps;
    }
    
    void SetIsModified(bool bValue)
    {
        if (bValue) 
            ItemStateFlags |= EItemStateFlags::Modified;
        else 
            ItemStateFlags &= ~EItemStateFlags::Modified;
    }
    
    //==================================================================
    // Data flag utility methods
    //==================================================================
    
    bool IsItemStackable() const { return (ItemDataFlags & EItemDataFlags::Stackable) != 0; }
    bool IsItemConsumable() const { return (ItemDataFlags & EItemDataFlags::Consumable) != 0; }
    bool IsItemEquippable() const { return (ItemDataFlags & EItemDataFlags::Equippable) != 0; }
    bool IsItemDroppable() const { return (ItemDataFlags & EItemDataFlags::Droppable) != 0; }
    bool IsItemTradeable() const { return (ItemDataFlags & EItemDataFlags::Tradeable) != 0; }
    bool IsItemQuestItem() const { return (ItemDataFlags & EItemDataFlags::QuestItem) != 0; }
    bool IsItemCraftingMaterial() const { return (ItemDataFlags & EItemDataFlags::CraftingMaterial) != 0; }
    bool ItemHasDurability() const { return (ItemDataFlags & EItemDataFlags::HasDurability) != 0; }
    
    //==================================================================
    // Grid size packing/unpacking methods
    //==================================================================
    
    void SetGridSize(const FVector2D& Size)
    {
        uint8 Width = FMath::Clamp((int32)Size.X, 0, 15);
        uint8 Height = FMath::Clamp((int32)Size.Y, 0, 15);
        PackedGridSize = (Width << 4) | Height;
    }
    
    void SetGridSize(const FIntPoint& Size)
    {
        uint8 Width = FMath::Clamp(Size.X, 0, 15);
        uint8 Height = FMath::Clamp(Size.Y, 0, 15);
        PackedGridSize = (Width << 4) | Height;
    }
    
    FVector2D GetGridSize() const
    {
        uint8 Width = (PackedGridSize >> 4) & 0x0F;
        uint8 Height = PackedGridSize & 0x0F;
        return FVector2D(Width, Height);
    }
    
    FIntPoint GetGridSizeInt() const
    {
        uint8 Width = (PackedGridSize >> 4) & 0x0F;
        uint8 Height = PackedGridSize & 0x0F;
        return FIntPoint(Width, Height);
    }
    
    //==================================================================
    // Durability helpers
    //==================================================================
    
    void SetDurabilityFromPercent(float Percent)
    {
        DurabilityPercent = FMath::Clamp(FMath::RoundToInt(Percent * 255.0f), 0, 255);
    }
    
    float GetDurabilityAsPercent() const
    {
        return DurabilityPercent / 255.0f;
    }
    
    //==================================================================
    // Runtime properties management
    //==================================================================
    
    void SetPackedRuntimeProperty(uint8 PropertyKey, float Value)
    {
        PackedRuntimeProperties.Add(PropertyKey, Value);
        RuntimePropertiesCount = PackedRuntimeProperties.Num();
        SetHasRuntimeProperties(RuntimePropertiesCount > 0);
    }
    
    float GetPackedRuntimeProperty(uint8 PropertyKey, float DefaultValue = 0.0f) const
    {
        if (const float* Value = PackedRuntimeProperties.Find(PropertyKey))
        {
            return *Value;
        }
        return DefaultValue;
    }
    
    bool HasPackedRuntimeProperty(uint8 PropertyKey) const
    {
        return PackedRuntimeProperties.Contains(PropertyKey);
    }
    
    // Предопределенные ключи для часто используемых свойств
    enum ERuntimePropertyKeys : uint8
    {
        AmmoCount = 0,           // Количество патронов в магазине
        ReserveAmmo = 1,         // Резервные патроны
        ModificationSlots = 2,   // Количество доступных слотов модификации
        CustomDurability = 3,    // Кастомная прочность (если отличается от базовой)
        ChargeCurrent = 4,       // Текущий заряд для энергетического оружия
        ChargeMax = 5,           // Максимальный заряд
        UserProperty1 = 6,       // Пользовательские свойства для модификаций
        UserProperty2 = 7,
        UserProperty3 = 8,
        UserProperty4 = 9
    };
    
    //==================================================================
    // Conversion methods
    //==================================================================
    
    // ОБНОВЛЕНО: Создание метаданных из FSuspenseInventoryItemInstance (новый основной метод)
    static FReplicatedItemMeta FromItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, USuspenseItemManager* ItemManager = nullptr);
    
    // ОБНОВЛЕНО: Создание метаданных из интерфейса предмета
    static FReplicatedItemMeta FromItemInterface(const ISuspenseInventoryItemInterface* ItemInterface);
    
    // ОБНОВЛЕНО: Создание метаданных из unified DataTable структуры
    static FReplicatedItemMeta FromUnifiedItemData(const FSuspenseUnifiedItemData& ItemData, int32 Amount, int32 AnchorIdx, const FGuid& InstanceID = FGuid());

    // НОВОЕ: Создание полного FSuspenseInventoryItemInstance из метаданных
    FSuspenseInventoryItemInstance ToItemInstance() const;
    
    // НОВОЕ: Обновление метаданных из измененного instance
    void UpdateFromItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, USuspenseItemManager* ItemManager = nullptr);
};

/**
 * Container for replicating cells efficiently
 * Uses FastArraySerializer for delta compression
 */
USTRUCT()
struct FReplicatedCellsState : public FFastArraySerializer
{
    GENERATED_BODY()
    
    // Array of compact cells representing the grid
    UPROPERTY()
    TArray<FCompactReplicatedCell> Cells;
    
    // Owner component reference (not replicated)
    UPROPERTY(NotReplicated)
    class USuspenseInventoryReplicator* OwnerComponent = nullptr;
    
    // Default constructor
    FReplicatedCellsState()
    {
        OwnerComponent = nullptr;
    }
    
    // FastArraySerializer callback methods
    void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
    void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
    void PostReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
    
    // NetDeltaSerialize implementation
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FCompactReplicatedCell, FReplicatedCellsState>(
            Cells, DeltaParams, *this);
    }
};

/**
 * Container for replicating item metadata efficiently
 */
USTRUCT()
struct FReplicatedItemsMetaState : public FFastArraySerializer
{
    GENERATED_BODY()
    
    // Array of item metadata
    UPROPERTY()
    TArray<FReplicatedItemMeta> Items;
    
    // Owner component reference (not replicated)
    UPROPERTY(NotReplicated)
    class USuspenseInventoryReplicator* OwnerComponent = nullptr;
    
    // Default constructor
    FReplicatedItemsMetaState()
    {
        OwnerComponent = nullptr;
    }
    
    // FastArraySerializer callback methods
    void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
    void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
    void PostReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
    
    // NetDeltaSerialize implementation
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedItemMeta, FReplicatedItemsMetaState>(
            Items, DeltaParams, *this);
    }
};

// Tell Unreal Engine that these structs use custom network serialization
template<>
struct TStructOpsTypeTraits<FReplicatedCellsState> : public TStructOpsTypeTraitsBase2<FReplicatedCellsState>
{
    enum { WithNetDeltaSerializer = true };
};

template<>
struct TStructOpsTypeTraits<FReplicatedItemsMetaState> : public TStructOpsTypeTraitsBase2<FReplicatedItemsMetaState>
{
    enum { WithNetDeltaSerializer = true };
};

/**
 * ОБНОВЛЕНО: Full inventory replication state with DataTable integration
 */
USTRUCT()
struct FInventoryReplicatedState
{
    GENERATED_BODY()
    
    // Grid cells replication data
    UPROPERTY()
    FReplicatedCellsState CellsState;
    
    // Item metadata replication data
    UPROPERTY()
    FReplicatedItemsMetaState ItemsState;
    
    // Owner component (not replicated)
    UPROPERTY(NotReplicated)
    class USuspenseInventoryReplicator* OwnerComponent = nullptr;
    
    // ОБНОВЛЕНО: Теперь храним FSuspenseInventoryItemInstance вместо простых UObject*
    // Это обеспечивает полную поддержку runtime properties и DataTable интеграцию
    UPROPERTY(NotReplicated)
    TArray<FSuspenseInventoryItemInstance> ItemInstances;
    
    // Legacy object references для backward compatibility
    UPROPERTY(NotReplicated)
    TArray<UObject*> ItemObjects;
    
    // Default constructor
    FInventoryReplicatedState()
    {
        OwnerComponent = nullptr;
    }
    
    // Initialize the replication state
    void Initialize(USuspenseInventoryReplicator* InOwner, int32 GridWidth, int32 GridHeight);
    
    // Reset all state
    void Reset();
    
    // ОБНОВЛЕНО: Добавление предмета через FSuspenseInventoryItemInstance (новый основной метод)
    int32 AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex);
    
    // ОБНОВЛЕНО: Legacy метод с автоматическим размером из DataTable
    int32 AddItem(UObject* ItemObject, const FReplicatedItemMeta& Meta, int32 AnchorIndex);
    
    // Remove item by metadata index
    bool RemoveItem(int32 MetaIndex);
    
    // ОБНОВЛЕНО: Обновление предмета с полной поддержкой runtime properties
    bool UpdateItemInstance(int32 MetaIndex, const FSuspenseInventoryItemInstance& NewInstance);
    
    // Update item metadata (lightweight update)
    bool UpdateItem(int32 MetaIndex, const FReplicatedItemMeta& NewMeta);
    
    // НОВОЕ: Поиск по instance ID для multiplayer tracking
    int32 FindMetaIndexByInstanceID(const FGuid& InstanceID) const;
    
    // Find item metadata index by object pointer (legacy)
    int32 FindMetaIndexByObject(UObject* ItemObject) const;
    
    // Find item metadata index by item ID
    int32 FindMetaIndexByItemID(const FName& ItemID) const;
    
    // НОВОЕ: Получение FSuspenseInventoryItemInstance по индексу
    const FSuspenseInventoryItemInstance* GetItemInstance(int32 MetaIndex) const;
    FSuspenseInventoryItemInstance* GetItemInstance(int32 MetaIndex);
    
    // Check if cells are free for placement
    bool AreCellsFree(int32 StartIndex, const FVector2D& Size) const;
    
    // НОВОЕ: Проверка с автоматическим получением размера из DataTable
    bool AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated = false) const;
    
    // Mark arrays as dirty for replication
    void MarkArrayDirty()
    {
        CellsState.MarkArrayDirty();
        ItemsState.MarkArrayDirty();
    }
    
    // НОВОЕ: Синхронизация с ItemManager для обновления DataTable данных
    void SynchronizeWithItemManager(USuspenseItemManager* ItemManager);
    
    // НОВОЕ: Валидация целостности state для debugging
    bool ValidateIntegrity(TArray<FString>& OutErrors) const;
};

/**
 * ОБНОВЛЕНО: Component responsible for optimizing inventory data replication
 * with full DataTable and FSuspenseInventoryItemInstance integration
 * 
 * АРХИТЕКТУРНЫЕ УЛУЧШЕНИЯ:
 * - Полная интеграция с FSuspenseUnifiedItemData как источником истины
 * - Поддержка FSuspenseInventoryItemInstance для runtime данных
 * - Автоматическое получение размеров предметов из DataTable
 * - Расширенная поддержка runtime properties репликации
 * - Интеграция с ItemManager для централизованного доступа к данным
 * - Улучшенная валидация и error handling
 */
UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseInventoryReplicator : public UActorComponent
{
    GENERATED_BODY()
    
public:
    USuspenseInventoryReplicator();
    
    // Network property replication setup
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // Component tick for batching network updates
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    /**
     * ОБНОВЛЕНО: Инициализация с поддержкой ItemManager
     */
    bool Initialize(int32 GridWidth, int32 GridHeight, USuspenseItemManager* InItemManager = nullptr);
    
    /**
     * Получение состояния репликации для прямой манипуляции
     */
    FInventoryReplicatedState& GetReplicationState() { return ReplicationState; }
    const FInventoryReplicatedState& GetReplicationState() const { return ReplicationState; }
    
    /**
     * НОВОЕ: Получение ItemManager для DataTable операций
     */
    USuspenseItemManager* GetItemManager() const { return ItemManager; }

    /**
     * НОВОЕ: Установка ItemManager для runtime изменений
     */
    void SetItemManager(USuspenseItemManager* InItemManager) { ItemManager = InItemManager; }
    
    /**
     * Установка частоты сетевых обновлений
     */
    void SetUpdateInterval(float IntervalSeconds);
    
    /**
     * Запрос на отправку сетевого обновления
     */
    void RequestNetUpdate();
    
    /**
     * НОВОЕ: Принудительная полная синхронизация всех клиентов
     */
    void ForceFullResync();
    
    /**
     * Делегат для уведомлений об обновлении состояния репликации
     */
    FOnReplicationUpdated OnReplicationUpdated;
    
    //==================================================================
    // НОВЫЕ МЕТОДЫ: Enhanced Item Management
    //==================================================================
    
    /**
     * Добавление предмета через FSuspenseInventoryItemInstance (основной метод)
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    int32 AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex);
    
    /**
     * Обновление runtime свойств предмета
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    bool UpdateItemRuntimeProperties(int32 MetaIndex, const TMap<FName, float>& NewProperties);
    
    /**
     * Получение предмета как FSuspenseInventoryItemInstance
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    bool GetItemInstanceByIndex(int32 MetaIndex, FSuspenseInventoryItemInstance& OutInstance) const;
    
    /**
     * Поиск предмета по Instance ID
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    int32 FindItemByInstanceID(const FGuid& InstanceID) const;
    
    /**
     * Валидация состояния репликации
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    bool ValidateReplicationState(TArray<FString>& OutErrors) const;
    
    /**
     * Получение статистики репликации для debugging
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    FString GetReplicationStats() const;
    
    /**
     * НОВОЕ: Оптимизированная установка интервала обновления на основе количества предметов
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    void SetUpdateIntervalOptimized(float BaseInterval, int32 ItemCount);
    
    /**
     * НОВОЕ: Попытка сжатия реплицированных данных
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    bool TryCompactReplication();
    
    /**
     * НОВОЕ: Получение детальной отладочной информации
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    FString GetDetailedReplicationDebugInfo() const;
    
    /**
     * НОВОЕ: Выполнение очистки и оптимизации
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    void PerformMaintenanceCleanup();
    
    /**
     * НОВОЕ: Аварийный сброс состояния репликации
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryReplication")
    bool EmergencyReset();
    
protected:
    /** Replicated inventory state data */
    UPROPERTY(ReplicatedUsing=OnRep_ReplicationState)
    FInventoryReplicatedState ReplicationState;
    
    /** НОВОЕ: Reference на ItemManager для DataTable доступа */
    UPROPERTY()
    USuspenseItemManager* ItemManager;
    
    /** How often to send network updates (in seconds) */
    UPROPERTY(EditAnywhere, Category = "Network", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float NetworkUpdateInterval = 0.1f;
    
    /** Timer tracking time since last update */
    float NetworkUpdateTimer = 0.0f;
    
    /** Flag indicating if network update is pending */
    bool bNetUpdateNeeded = false;
    
    /** НОВОЕ: Flag для принудительной полной синхронизации */
    bool bForceFullResync = false;
    
    /** НОВОЕ: Счетчики для статистики репликации */
    UPROPERTY()
    int32 ReplicationUpdateCount = 0;
    
    UPROPERTY()
    int32 BytesSentThisFrame = 0;
    
    UPROPERTY()
    float LastUpdateTime = 0.0f;
    
    /** Called when replication state is updated on client */
    UFUNCTION()
    void OnRep_ReplicationState();
    
    //==================================================================
    // НОВЫЕ МЕТОДЫ: Internal Helpers
    //==================================================================
    
    /**
     * Автоматическое получение ItemManager если не установлен
     */
    USuspenseItemManager* GetOrCreateItemManager();
    
    /**
     * Конвертация legacy объектов в FSuspenseInventoryItemInstance
     */
    bool ConvertLegacyObjectToInstance(UObject* ItemObject, FSuspenseInventoryItemInstance& OutInstance) const;
    
    /**
     * Синхронизация размеров предметов с DataTable
     */
    void SynchronizeItemSizesWithDataTable();
    
    /**
     * Обновление статистики репликации
     */
    void UpdateReplicationStats();
    
    /**
     * Очистка устаревших данных
     */
    void CleanupStaleData();
};
// Copyright Suspense Team. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCoreInventoryBaseTypes.generated.h"

// Forward declarations для структур из DataTable (единый источник истины)
struct FSuspenseCoreUnifiedItemData;
struct FMCPickupData;
struct FMCEquipmentData;

/**
 * Коды ошибок инвентаря для системы диагностики и UI обратной связи
 * Эти коды используются для унифицированной обработки ошибок во всей системе инвентаря
 */
UENUM(BlueprintType)
enum class ESuspenseInventoryErrorCode : uint8
{
    Success UMETA(DisplayName = "Success"),
    NoSpace UMETA(DisplayName = "No Space"),
    WeightLimit UMETA(DisplayName = "Weight Limit Exceeded"),
    InvalidItem UMETA(DisplayName = "Invalid Item"),
    ItemNotFound UMETA(DisplayName = "Item Not Found"),
    InsufficientQuantity UMETA(DisplayName = "Insufficient Quantity"),
    InvalidSlot UMETA(DisplayName = "Invalid Slot"),
    SlotOccupied UMETA(DisplayName = "Slot Occupied"),
    TransactionActive UMETA(DisplayName = "Transaction Active"),
    NotInitialized UMETA(DisplayName = "Not Initialized"),
    NetworkError UMETA(DisplayName = "Network Error"),
    UnknownError UMETA(DisplayName = "Unknown Error")
};

/**
 * Основная runtime структура для экземпляров предметов в инвентаре
 *
 * Архитектурная философия:
 * - Хранит ТОЛЬКО runtime состояние и позиционирование
 * - Все статические данные получает из DataTable по ItemID
 * - Оптимизирована для сетевой репликации (минимальный footprint)
 * - Использует универсальную систему runtime свойств для расширяемости
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryItemInstance
{
    GENERATED_BODY()

    //==================================================================
    // Идентификация и связь с источником истины
    //==================================================================

    /**
     * ID предмета для поиска в DataTable
     * Единственная связь со статическими данными - источник истины
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Core")
    FName ItemID = NAME_None;

    /**
     * Уникальный ID экземпляра для отслеживания в multiplayer
     * Генерируется автоматически и не реплицируется (локально уникальный)
     */
    UPROPERTY(BlueprintReadWrite, Category = "Inventory")
    FGuid InstanceID;

    //==================================================================
    // Runtime состояние предмета
    //==================================================================

    /**
     * Текущее количество в стеке
     * Ограничивается MaxStackSize из DataTable
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stack")
    int32 Quantity = 1;

    /**
     * Универсальные runtime свойства для динамических данных
     *
     * Стандартные ключи:
     * - "Durability" - текущая прочность предмета
     * - "MaxDurability" - максимальная прочность (из AttributeSet)
     * - "Ammo" - текущие патроны в оружии
     * - "MaxAmmo" - максимальные патроны (из AmmoAttributeSet)
     * - "CooldownEnd" - время окончания кулдауна
     * - "Charges" - оставшиеся заряды для расходуемых
     * - "Condition" - состояние предмета (для особых механик)
     */
    UPROPERTY(BlueprintReadWrite, Category = "Item|Runtime")
    TMap<FName, float> RuntimeProperties;

    //==================================================================
    // Позиционирование в инвентаре
    //==================================================================

    /**
     * Индекс якорной ячейки в линейном массиве сетки инвентаря
     * INDEX_NONE означает что предмет не размещен в инвентаре
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Position")
    int32 AnchorIndex = INDEX_NONE;

    /**
     * Повернут ли предмет в инвентаре на 90 градусов
     * Влияет на размеры занимаемого пространства (ширина <-> высота)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Position")
    bool bIsRotated = false;

    /**
     * Время последнего использования предмета
     * Используется для кулдаунов, статистики, и игровой логики
     */
    UPROPERTY(BlueprintReadWrite, Category = "Item|Runtime")
    float LastUsedTime = 0.0f;

    //==================================================================
    // Magazine Data (for Tarkov-style ammo system)
    // @see TarkovStyle_Ammo_System_Design.md
    //==================================================================

    /**
     * Magazine-specific runtime data (for Item.Magazine tagged items)
     * Contains current round count, loaded ammo type, and magazine state.
     * This data MUST be preserved during inventory <-> equipment transfers.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Magazine")
    FSuspenseCoreMagazineInstance MagazineData;

    /**
     * Weapon ammo state (for weapons with magazines)
     * Contains inserted magazine, chambered round, and ammo state.
     * This data MUST be preserved during inventory <-> equipment transfers.
     * @see TarkovStyle_Ammo_System_Design.md
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon")
    FSuspenseCoreWeaponAmmoState WeaponAmmoState;

    //==================================================================
    // Static Factory Methods (замена конструкторов)
    //==================================================================

    /**
     * Factory method для создания нового пустого экземпляра с генерацией InstanceID
     * Используется как замена конструктора по умолчанию
     */
    static FSuspenseCoreInventoryItemInstance Create()
    {
        FSuspenseCoreInventoryItemInstance Instance;
        Instance.ItemID = NAME_None;
        Instance.InstanceID = FGuid::NewGuid();
        Instance.Quantity = 1;
        Instance.AnchorIndex = INDEX_NONE;
        Instance.bIsRotated = false;
        Instance.LastUsedTime = 0.0f;
        return Instance;
    }

    /**
     * Factory method для создания экземпляра с ItemID и количеством
     * Автоматически генерирует уникальный InstanceID
     *
     * @param InItemID ID предмета из DataTable
     * @param InQuantity Количество предметов в стеке (минимум 1)
     */
    static FSuspenseCoreInventoryItemInstance Create(const FName& InItemID, int32 InQuantity = 1)
    {
        FSuspenseCoreInventoryItemInstance Instance;
        Instance.ItemID = InItemID;
        Instance.InstanceID = FGuid::NewGuid();
        Instance.Quantity = FMath::Max(1, InQuantity);
        Instance.AnchorIndex = INDEX_NONE;
        Instance.bIsRotated = false;
        Instance.LastUsedTime = 0.0f;
        return Instance;
    }

    /**
     * Factory method для создания с существующим GUID (для репликации/десериализации)
     *
     * @param InItemID ID предмета из DataTable
     * @param InInstanceID Существующий GUID для восстановления состояния
     * @param InQuantity Количество предметов в стеке
     */
    static FSuspenseCoreInventoryItemInstance CreateWithID(const FName& InItemID, const FGuid& InInstanceID, int32 InQuantity = 1)
    {
        FSuspenseCoreInventoryItemInstance Instance;
        Instance.ItemID = InItemID;
        Instance.InstanceID = InInstanceID;
        Instance.Quantity = FMath::Max(1, InQuantity);
        Instance.AnchorIndex = INDEX_NONE;
        Instance.bIsRotated = false;
        Instance.LastUsedTime = 0.0f;
        return Instance;
    }

    //==================================================================
    // Методы валидации и проверки состояния
    //==================================================================

    /** Проверяет базовую валидность экземпляра */
    bool IsValid() const
    {
        return !ItemID.IsNone() && Quantity > 0 && InstanceID.IsValid();
    }

    /** Проверяет, размещен ли предмет в инвентаре */
    bool IsPlacedInInventory() const
    {
        return AnchorIndex != INDEX_NONE;
    }

    /** Проверяет, имеет ли предмет валидный GUID для сетевой синхронизации */
    bool HasValidInstanceID() const
    {
        return InstanceID.IsValid();
    }

    //==================================================================
    // Работа с runtime свойствами
    //==================================================================

    /**
     * Получить значение runtime свойства с fallback
     * @param PropertyName Имя свойства для поиска
     * @param DefaultValue Значение возвращаемое если свойство не найдено
     */
    float GetRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const
    {
        const float* Value = RuntimeProperties.Find(PropertyName);
        return Value ? *Value : DefaultValue;
    }

    /**
     * Установить значение runtime свойства
     * @param PropertyName Имя свойства
     * @param Value Новое значение
     */
    void SetRuntimeProperty(const FName& PropertyName, float Value)
    {
        RuntimeProperties.Add(PropertyName, Value);
    }

    /**
     * Удалить runtime свойство полностью
     * @param PropertyName Имя свойства для удаления
     */
    void RemoveRuntimeProperty(const FName& PropertyName)
    {
        RuntimeProperties.Remove(PropertyName);
    }

    /**
     * Проверить наличие runtime свойства
     * @param PropertyName Имя свойства для проверки
     */
    bool HasRuntimeProperty(const FName& PropertyName) const
    {
        return RuntimeProperties.Contains(PropertyName);
    }

    /** Очистить все runtime свойства (для переинициализации) */
    void ClearRuntimeProperties()
    {
        RuntimeProperties.Empty();
    }

    //==================================================================
    // Convenience методы для часто используемых свойств
    //==================================================================

    /** Получить текущую прочность предмета */
    float GetCurrentDurability() const
    {
        return GetRuntimeProperty(TEXT("Durability"), 0.0f);
    }

    /**
     * Установить текущую прочность с автоматическим клампингом
     * @param Durability Новое значение прочности (будет ограничено максимумом)
     */
    void SetCurrentDurability(float Durability)
    {
        float MaxDurability = GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
        float ClampedDurability = FMath::Clamp(Durability, 0.0f, MaxDurability);
        SetRuntimeProperty(TEXT("Durability"), ClampedDurability);
    }

    /** Получить процент прочности от 0.0 до 1.0 */
    float GetDurabilityPercent() const
    {
        float MaxDurability = GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
        if (MaxDurability <= 0.0f) return 1.0f;

        float CurrentDurability = GetCurrentDurability();
        return FMath::Clamp(CurrentDurability / MaxDurability, 0.0f, 1.0f);
    }

    /** Получить текущие патроны в оружии */
    int32 GetCurrentAmmo() const
    {
        return FMath::RoundToInt(GetRuntimeProperty(TEXT("Ammo"), 0.0f));
    }

    /**
     * Установить текущие патроны с клампингом к максимуму
     * @param AmmoCount Новое количество патронов
     */
    void SetCurrentAmmo(int32 AmmoCount)
    {
        int32 MaxAmmo = FMath::RoundToInt(GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
        int32 ClampedAmmo = FMath::Clamp(AmmoCount, 0, MaxAmmo);
        SetRuntimeProperty(TEXT("Ammo"), static_cast<float>(ClampedAmmo));
    }

    /**
     * Проверить, есть ли активный кулдаун у предмета
     * @param CurrentTime Текущее время для сравнения
     */
    bool IsOnCooldown(float CurrentTime) const
    {
        float CooldownEndTime = GetRuntimeProperty(TEXT("CooldownEnd"), 0.0f);
        return CurrentTime < CooldownEndTime;
    }

    /**
     * Запустить кулдаун предмета на указанное время
     * @param CurrentTime Текущее время
     * @param CooldownDuration Длительность кулдауна в секундах
     */
    void StartCooldown(float CurrentTime, float CooldownDuration)
    {
        SetRuntimeProperty(TEXT("CooldownEnd"), CurrentTime + CooldownDuration);
    }

    /**
     * Получить оставшееся время кулдауна
     * @param CurrentTime Текущее время
     * @return Оставшееся время в секундах (0.0 если кулдауна нет)
     */
    float GetRemainingCooldown(float CurrentTime) const
    {
        float CooldownEndTime = GetRuntimeProperty(TEXT("CooldownEnd"), 0.0f);
        return FMath::Max(0.0f, CooldownEndTime - CurrentTime);
    }

    //==================================================================
    // Magazine Helper Methods
    // @see TarkovStyle_Ammo_System_Design.md - Tarkov-style ammo loading
    //==================================================================

    /**
     * Check if this item is a magazine
     * @return true if MagazineData is valid (has MagazineID set)
     */
    bool IsMagazine() const
    {
        return MagazineData.IsValid();
    }

    /**
     * Get current ammo count in magazine
     * @return Number of rounds currently loaded, 0 if not a magazine
     */
    int32 GetMagazineRoundCount() const
    {
        return IsMagazine() ? MagazineData.CurrentRoundCount : 0;
    }

    /**
     * Get magazine fill percentage
     * @return 0.0-1.0 fill percentage, 0 if not a magazine
     */
    float GetMagazineFillPercent() const
    {
        return IsMagazine() ? MagazineData.GetFillPercentage() : 0.0f;
    }

    //==================================================================
    // Weapon Ammo Helper Methods
    // @see TarkovStyle_Ammo_System_Design.md
    //==================================================================

    /**
     * Check if this item has weapon ammo state (is a weapon with magazine support)
     * @return true if WeaponAmmoState has a magazine or chambered round
     */
    bool HasWeaponAmmoState() const
    {
        return WeaponAmmoState.bHasMagazine || WeaponAmmoState.ChamberedRound.IsChambered();
    }

    /**
     * Get the inserted magazine from weapon ammo state
     * @return Reference to inserted magazine instance
     */
    const FSuspenseCoreMagazineInstance& GetInsertedMagazine() const
    {
        return WeaponAmmoState.InsertedMagazine;
    }

    /**
     * Check if weapon has a chambered round
     * @return true if there is a round in the chamber
     */
    bool HasChamberedRound() const
    {
        return WeaponAmmoState.ChamberedRound.IsChambered();
    }

    /**
     * Get total rounds in weapon (magazine + chamber)
     * @return Total round count
     */
    int32 GetWeaponTotalRounds() const
    {
        int32 Total = 0;
        if (WeaponAmmoState.bHasMagazine)
        {
            Total += WeaponAmmoState.InsertedMagazine.CurrentRoundCount;
        }
        if (WeaponAmmoState.ChamberedRound.IsChambered())
        {
            Total += 1;
        }
        return Total;
    }

    /**
     * Check if magazines don't stack (they have unique ammo state)
     */
    bool CanStackWith(const FSuspenseCoreInventoryItemInstance& Other) const
    {
        // Same item type required
        if (ItemID != Other.ItemID)
        {
            return false;
        }

        // Magazines never stack (they have unique ammo state)
        if (IsMagazine() || Other.IsMagazine())
        {
            return false;
        }

        // Items with runtime properties don't stack
        if (RuntimeProperties.Num() > 0 || Other.RuntimeProperties.Num() > 0)
        {
            return false;
        }

        return true;
    }

    //==================================================================
    // Операторы для использования в контейнерах и сравнениях
    //==================================================================

    bool operator==(const FSuspenseCoreInventoryItemInstance& Other) const
    {
        return InstanceID == Other.InstanceID;
    }

    bool operator!=(const FSuspenseCoreInventoryItemInstance& Other) const
    {
        return !(*this == Other);
    }

    friend uint32 GetTypeHash(const FSuspenseCoreInventoryItemInstance& Instance)
    {
        return GetTypeHash(Instance.InstanceID);
    }

    //==================================================================
    // Отладка и диагностика
    //==================================================================

    /** Получить подробную отладочную строку */
    FString GetDebugString() const
    {
        return FString::Printf(
            TEXT("ItemInstance[%s]: ID=%s, Qty=%d, Pos=%d, Rotated=%s, Props=%d, LastUsed=%.1f"),
            *InstanceID.ToString(),
            *ItemID.ToString(),
            Quantity,
            AnchorIndex,
            bIsRotated ? TEXT("Yes") : TEXT("No"),
            RuntimeProperties.Num(),
            LastUsedTime
        );
    }

    /** Получить краткую отладочную строку для логирования */
    FString GetShortDebugString() const
    {
        return FString::Printf(TEXT("%s x%d [%s]"),
            *ItemID.ToString(),
            Quantity,
            *InstanceID.ToString().Left(8)
        );
    }
};

/**
 * Структура для представления ячейки инвентаря в UI и логике размещения
 * Отслеживает состояние занятости и привязку к экземплярам предметов
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FInventoryCell
{
    GENERATED_BODY()

    /** Индекс ячейки в линейном массиве сетки инвентаря */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
    int32 CellIndex = INDEX_NONE;

    /** Занята ли ячейка каким-либо предметом */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
    bool bIsOccupied = false;

    /** ID экземпляра предмета который занимает эту ячейку */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
    FGuid OccupyingInstanceID;

    /** Конструктор по умолчанию создает пустую ячейку */
    FInventoryCell() = default;

    /**
     * Конструктор с индексом ячейки
     * @param InCellIndex Индекс ячейки в сетке
     */
    explicit FInventoryCell(int32 InCellIndex)
        : CellIndex(InCellIndex)
        , bIsOccupied(false)
    {
    }

    /** Освободить ячейку от любого предмета */
    void Clear()
    {
        bIsOccupied = false;
        OccupyingInstanceID = FGuid();
    }

    /**
     * Занять ячейку указанным экземпляром предмета
     * @param InstanceID GUID экземпляра который занимает ячейку
     */
    void Occupy(const FGuid& InstanceID)
    {
        bIsOccupied = true;
        OccupyingInstanceID = InstanceID;
    }

    /**
     * Проверить, занята ли ячейка конкретным экземпляром
     * @param InstanceID GUID экземпляра для проверки
     */
    bool IsOccupiedBy(const FGuid& InstanceID) const
    {
        return bIsOccupied && OccupyingInstanceID == InstanceID;
    }

    /** Проверить, занята ли ячейка вообще */
    bool IsOccupied() const
    {
        return bIsOccupied && OccupyingInstanceID.IsValid();
    }
};

/**
 * Упрощенная структура для создания экземпляров предметов из pickup
 * Содержит минимальную информацию - основные данные берутся из DataTable
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCorePickupSpawnData
{
    GENERATED_BODY()

    /** ID предмета из DataTable - связь с источником истины */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    FName ItemID = NAME_None;

    /** Количество предметов в pickup */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1"))
    int32 Quantity = 1;

    /**
     * Предустановленные runtime свойства для особых случаев
     * Например: поврежденное оружие, частично заряженные батареи
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    TMap<FName, float> PresetRuntimeProperties;

    /** Конструктор по умолчанию */
    FSuspenseCorePickupSpawnData() = default;

    /**
     * Конструктор с базовыми параметрами
     * @param InItemID ID предмета из DataTable
     * @param InQuantity Количество предметов
     */
    FSuspenseCorePickupSpawnData(const FName& InItemID, int32 InQuantity = 1)
        : ItemID(InItemID)
        , Quantity(FMath::Max(1, InQuantity))
    {
    }

    /**
     * Создать правильно инициализированный экземпляр инвентаря
     * @return Готовый к использованию экземпляр предмета
     */
    FSuspenseCoreInventoryItemInstance CreateInventoryInstance() const
    {
        FSuspenseCoreInventoryItemInstance Instance = FSuspenseCoreInventoryItemInstance::Create(ItemID, Quantity);

        // Применяем предустановленные runtime свойства
        for (const auto& PropertyPair : PresetRuntimeProperties)
        {
            Instance.SetRuntimeProperty(PropertyPair.Key, PropertyPair.Value);
        }

        return Instance;
    }

    /** Проверка валидности данных для создания pickup */
    bool IsValid() const
    {
        return !ItemID.IsNone() && Quantity > 0;
    }
};

/**
 * Структура для отслеживания экипированных предметов в слотах
 * Хранит runtime экземпляр и метаданные экипировки
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentSlotData
{
    GENERATED_BODY()

    /** Runtime экземпляр экипированного предмета */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    FSuspenseCoreInventoryItemInstance ItemInstance;

    /** Время когда предмет был экипирован (для кулдаунов смены) */
    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    float EquipTime = 0.0f;

    /** Время последней смены экипировки (для защиты от спама) */
    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    float LastChangeTime = 0.0f;

    /** Конструктор по умолчанию создает пустой слот */
    FEquipmentSlotData() = default;

    /**
     * Конструктор с экземпляром предмета
     * @param InInstance Экземпляр предмета для экипировки
     * @param InEquipTime Время экипировки
     */
    explicit FEquipmentSlotData(const FSuspenseCoreInventoryItemInstance& InInstance, float InEquipTime = 0.0f)
        : ItemInstance(InInstance)
        , EquipTime(InEquipTime)
        , LastChangeTime(InEquipTime)
    {
    }

    /** Проверить, есть ли экипированный предмет в слоте */
    bool HasEquippedItem() const
    {
        return ItemInstance.IsValid();
    }

    /** Очистить слот экипировки полностью */
    void Clear()
    {
        ItemInstance = FSuspenseCoreInventoryItemInstance::Create();
        EquipTime = 0.0f;
        LastChangeTime = 0.0f;
    }

    /** Получить ID экипированного предмета */
    FName GetEquippedItemID() const
    {
        return ItemInstance.ItemID;
    }

    /**
     * Проверить, можно ли сменить экипировку (учитывая кулдауны)
     * @param CurrentTime Текущее время
     * @param MinChangeInterval Минимальный интервал между сменами
     */
    bool CanChangeEquipment(float CurrentTime, float MinChangeInterval = 1.0f) const
    {
        return (CurrentTime - LastChangeTime) >= MinChangeInterval;
    }

    /**
     * Экипировать новый предмет в слот
     * @param NewInstance Новый экземпляр для экипировки
     * @param CurrentTime Текущее время
     */
    void EquipItem(const FSuspenseCoreInventoryItemInstance& NewInstance, float CurrentTime)
    {
        ItemInstance = NewInstance;
        EquipTime = CurrentTime;
        LastChangeTime = CurrentTime;
    }
};

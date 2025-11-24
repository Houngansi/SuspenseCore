// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Operations/InventoryResult.h"
#include "IMedComInventoryInterface.generated.h"

// Forward declarations для чистой архитектуры
struct FMedComUnifiedItemData;
struct FInventoryItemInstance;
struct FPickupSpawnData;
class UEventDelegateManager;
class UMedComItemManager;
enum class EInventoryErrorCode : uint8;

/**
 * Делегат для уведомлений об обновлениях инвентаря
 * Универсальный механизм подписки на изменения состояния инвентаря
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

/**
 * UInterface обертка для интеграции с системой рефлексии Unreal Engine
 */
UINTERFACE(MinimalAPI, BlueprintType, meta=(NotBlueprintable))
class UMedComInventoryInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Основной интерфейс для управления инвентарем в архитектуре DataTable
 * 
 * КЛЮЧЕВЫЕ АРХИТЕКТУРНЫЕ РЕШЕНИЯ:
 * 1. Полный отказ от UObject* в пользу FInventoryItemInstance
 * 2. Использование FGuid для уникальной идентификации экземпляров
 * 3. DataTable как единственный источник статических данных
 * 4. Четкое разделение между статикой (DataTable) и runtime данными
 * 5. Thread-safe дизайн для multiplayer окружения
 * 
 * УРОВНИ API:
 * - ID-based API: Простейший интерфейс, работающий только с ItemID
 * - DataTable API: Работает с полными данными FMedComUnifiedItemData
 * - Instance API: Работает с runtime экземплярами FInventoryItemInstance
 */
class MEDCOMSHARED_API IMedComInventoryInterface
{
    GENERATED_BODY()

public:
    //==================================================================
    // Основные операции с предметами - Ядро системы
    //==================================================================
    
    /**
     * Добавляет предмет в инвентарь по ID из DataTable
     * Центральный метод создания предметов из статических данных
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Core")
    bool AddItemByID(FName ItemID, int32 Quantity);
    
    /**
     * Добавляет готовый runtime экземпляр предмета
     * Используется при переносе предметов между инвентарями
     */
    virtual FInventoryOperationResult AddItemInstance(const FInventoryItemInstance& ItemInstance) = 0;
    
    /**
     * Добавляет экземпляр предмета в конкретный слот инвентаря
     * КРИТИЧЕСКИ ВАЖНЫЙ метод для системы переноса предметов
     * 
     * @param ItemInstance Экземпляр предмета для добавления
     * @param TargetSlot Целевой слот (INDEX_NONE для автоматического размещения)
     * @return Результат операции с детальной информацией
     */
    virtual FInventoryOperationResult AddItemInstanceToSlot(const FInventoryItemInstance& ItemInstance, int32 TargetSlot) = 0;
    
    /**
     * Удаляет предмет по ID с указанным количеством
     * Автоматически обрабатывает стеки и частичное удаление
     */
    virtual FInventoryOperationResult RemoveItemByID(const FName& ItemID, int32 Amount) = 0;
    
    /**
     * Удаляет конкретный экземпляр по его уникальному ID
     * Точечное удаление для multiplayer синхронизации
     */
    virtual FInventoryOperationResult RemoveItemInstance(const FGuid& InstanceID) = 0;
    
    /**
     * Удаляет предмет из конкретного слота
     * Важно для операций drag & drop
     * 
     * @param SlotIndex Индекс слота
     * @param OutRemovedInstance Удаленный экземпляр (для возможности отката)
     * @return true если удаление успешно
     */
    virtual bool RemoveItemFromSlot(int32 SlotIndex, FInventoryItemInstance& OutRemovedInstance) = 0;
    
    /**
     * Получает все экземпляры предметов в инвентаре
     * Снимок состояния для сохранения и репликации
     */
    virtual TArray<FInventoryItemInstance> GetAllItemInstances() const = 0;
    
    /**
     * Доступ к менеджеру предметов для работы с DataTable
     */
    virtual UMedComItemManager* GetItemManager() const = 0;
    
    //==================================================================
    // Работа с DataTable данными
    //==================================================================
    
    /**
     * Добавляет предмет используя полные данные из DataTable
     * 
     * Этот метод полезен когда у вас уже есть загруженная структура
     * FMedComUnifiedItemData (например, из UI выбора предметов или 
     * системы награждения). Внутренне создает FInventoryItemInstance.
     * 
     * @param ItemData Полные данные предмета из DataTable
     * @param Amount Количество для добавления
     * @return true если предмет успешно добавлен
     */
    virtual bool AddItem(const FMedComUnifiedItemData& ItemData, int32 Amount) = 0;
    
    /**
     * Добавляет предмет с детальной информацией об ошибке
     * 
     * Расширенная версия AddItem, которая предоставляет точный код
     * ошибки для обработки различных сценариев отказа.
     * 
     * @param ItemData Полные данные предмета из DataTable  
     * @param Amount Количество для добавления
     * @param OutErrorCode Детальный код ошибки при неудаче
     * @return true если предмет успешно добавлен
     */
    virtual bool AddItemWithErrorCode(const FMedComUnifiedItemData& ItemData, int32 Amount, EInventoryErrorCode& OutErrorCode) = 0;
    
    /**
     * Blueprint-совместимая версия добавления предмета
     * 
     * Может быть переопределена как в C++, так и в Blueprint.
     * Обеспечивает гибкость для дизайнеров создавать кастомную логику.
     * 
     * @param ItemData Полные данные предмета
     * @param Quantity Количество для добавления
     * @return true если предмет успешно добавлен
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|DataTable")
    bool TryAddItem(const FMedComUnifiedItemData& ItemData, int32 Quantity);
    
    /**
     * Удаляет предмет по ID
     * 
     * Простой интерфейс для удаления, дополняющий RemoveItemByID.
     * Сохранен для обратной совместимости и удобства использования.
     * 
     * @param ItemID Идентификатор предмета
     * @param Amount Количество для удаления
     * @return true если удаление прошло успешно
     */
    virtual bool RemoveItem(const FName& ItemID, int32 Amount) = 0;
    
    /**
     * Получает предмет с полной валидацией
     * 
     * Этот метод выполняет все проверки (вес, тип, место) перед
     * фактическим добавлением. Полезен при передаче предметов
     * между системами или при получении награждений.
     * 
     * @param ItemData Данные получаемого предмета
     * @param Quantity Количество для получения
     * @return true если предмет успешно получен
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Reception")
    bool ReceiveItem(const FMedComUnifiedItemData& ItemData, int32 Quantity);
    
    //==================================================================
    // Продвинутое управление предметами
    //==================================================================
    
    /**
     * Массовое создание предметов из конфигурационных данных
     * Используется для инициализации стартового снаряжения
     */
    virtual int32 CreateItemsFromSpawnData(const TArray<FPickupSpawnData>& SpawnDataArray) = 0;
    
    /**
     * Объединяет разрозненные стеки в единые
     * Оптимизация использования пространства инвентаря
     */
    virtual int32 ConsolidateStacks(const FName& ItemID = NAME_None) = 0;
    
    /**
     * Разделяет стек на две части
     * Позволяет игроку делить ресурсы
     */
    virtual FInventoryOperationResult SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot) = 0;
    
    //==================================================================
    // Валидация и проверки
    //==================================================================
    
    /**
     * Проверяет возможность получения предмета
     * Учитывает все ограничения (вес, тип, место)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Validation")
    bool CanReceiveItem(const FMedComUnifiedItemData& ItemData, int32 Quantity) const;
    
    /**
     * Получает список разрешенных типов предметов
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Configuration")
    FGameplayTagContainer GetAllowedItemTypes() const;
    
    //==================================================================
    // Управление сеткой размещения
    //==================================================================
    
    /**
     * Обменивает предметы между слотами
     * Простая версия для работы напрямую со слотами
     */
    virtual void SwapItemSlots(int32 SlotIndex1, int32 SlotIndex2) = 0;
    
    /**
     * Находит свободное место для предмета заданного размера
     */
    virtual int32 FindFreeSpaceForItem(const FVector2D& ItemSize, bool bAllowRotation = true) const = 0;
    
    /**
     * Проверяет возможность размещения в конкретном слоте
     */
    virtual bool CanPlaceItemAtSlot(const FVector2D& ItemSize, int32 SlotIndex, bool bIgnoreRotation = false) const = 0;
    
    /**
     * Проверяет возможность размещения экземпляра в конкретном слоте
     * 
     * @param ItemInstance Экземпляр предмета для проверки
     * @param SlotIndex Целевой слот
     * @return true если предмет можно разместить
     */
    virtual bool CanPlaceItemInstanceAtSlot(const FInventoryItemInstance& ItemInstance, int32 SlotIndex) const = 0;
    
    /**
     * Размещает экземпляр предмета в конкретном слоте
     * 
     * @param ItemInstance Экземпляр для размещения
     * @param SlotIndex Целевой слот
     * @param bForcePlace Принудительное размещение даже при конфликтах
     * @return true если размещение успешно
     */
    virtual bool PlaceItemInstanceAtSlot(const FInventoryItemInstance& ItemInstance, int32 SlotIndex, bool bForcePlace = false) = 0;
    
    /**
     * Автоматически размещает экземпляр предмета
     */
    virtual bool TryAutoPlaceItemInstance(const FInventoryItemInstance& ItemInstance) = 0;
    
    /**
     * Перемещает предмет между слотами
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Movement")
    bool MoveItemBySlots(int32 FromSlot, int32 ToSlot, bool bMaintainRotation);
    
    /**
     * Проверяет возможность перемещения
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Validation")
    bool CanMoveItemToSlot(int32 FromSlot, int32 ToSlot, bool bMaintainRotation) const;
    
    //==================================================================
    // Операции со слотами
    //==================================================================
    
    /**
     * Проверяет возможность обмена между слотами
     */
    virtual bool CanSwapSlots(int32 Slot1, int32 Slot2) const = 0;
    
    /**
     * Поворачивает предмет в указанном слоте
     */
    virtual bool RotateItemAtSlot(int32 SlotIndex) = 0;
    
    /**
     * Проверяет возможность поворота предмета
     */
    virtual bool CanRotateItemAtSlot(int32 SlotIndex) const = 0;
    
    //==================================================================
    // Система управления весом
    //==================================================================
    
    /**
     * Получает текущий общий вес
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Weight")
    float GetCurrentWeight() const;
    
    /**
     * Получает максимальную грузоподъемность
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Weight")
    float GetMaxWeight() const;
    
    /**
     * Вычисляет оставшуюся грузоподъемность
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Weight")
    float GetRemainingWeight() const;
    
    /**
     * Проверяет достаточность грузоподъемности
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Weight")
    bool HasWeightCapacity(float RequiredWeight) const;
    
    //==================================================================
    // Запросы информации о предметах
    //==================================================================
    
    /**
     * Получает размер сетки инвентаря
     */
    virtual FVector2D GetInventorySize() const = 0;
    
    /**
     * Получает экземпляр предмета в указанном слоте
     */
    virtual bool GetItemInstanceAtSlot(int32 SlotIndex, FInventoryItemInstance& OutInstance) const = 0;
    
    /**
     * Подсчитывает количество предметов по ID
     */
    virtual int32 GetItemCountByID(const FName& ItemID) const = 0;
    
    /**
     * Находит все экземпляры предметов указанного типа
     */
    virtual TArray<FInventoryItemInstance> FindItemInstancesByType(const FGameplayTag& ItemType) const = 0;
    
    /**
     * Получает общее количество уникальных предметов
     */
    virtual int32 GetTotalItemCount() const = 0;
    
    /**
     * Проверяет наличие предмета с указанным количеством
     */
    virtual bool HasItem(const FName& ItemID, int32 Amount = 1) const = 0;
    
    //==================================================================
    // UI поддержка
    //==================================================================
    
    /**
     * Обменивает предметы с обработкой ошибок
     */
    virtual bool SwapItemsInSlots(int32 Slot1, int32 Slot2, EInventoryErrorCode& OutErrorCode) = 0;
    
    /**
     * Обновляет визуальное представление
     */
    virtual void RefreshItemsUI() = 0;
    
    //==================================================================
    // Система транзакций
    //==================================================================
    
    /**
     * Начинает атомарную транзакцию
     */
    virtual void BeginTransaction() = 0;
    
    /**
     * Подтверждает транзакцию
     */
    virtual void CommitTransaction() = 0;
    
    /**
     * Откатывает транзакцию
     */
    virtual void RollbackTransaction() = 0;
    
    /**
     * Проверяет активность транзакции
     */
    virtual bool IsTransactionActive() const = 0;
    
    //==================================================================
    // Инициализация и конфигурация
    //==================================================================
    
    /**
     * Инициализирует из конфигурации loadout
     */
    virtual bool InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName = NAME_None) = 0;
    
    /**
     * Инициализирует с прямой конфигурацией
     */
    virtual void InitializeInventory(const struct FInventoryConfig& Config) = 0;
    
    /**
     * Устанавливает максимальный вес
     */
    virtual void SetMaxWeight(float NewMaxWeight) = 0;
    
    /**
     * Проверяет состояние инициализации
     */
    virtual bool IsInventoryInitialized() const = 0;
    
    /**
     * Устанавливает разрешенные типы предметов
     */
    virtual void SetAllowedItemTypes(const FGameplayTagContainer& Types) = 0;
    
    //==================================================================
    // Система событий
    //==================================================================
    
    /**
     * Транслирует обновление инвентаря
     */
    virtual void BroadcastInventoryUpdated() = 0;
    
    /**
     * Получает менеджер делегатов
     */
    virtual UEventDelegateManager* GetDelegateManager() const = 0;
    
    /**
     * Подписывается на обновления
     */
    virtual void BindToInventoryUpdates(const FOnInventoryUpdated::FDelegate& Delegate) = 0;
    
    /**
     * Отписывается от обновлений
     */
    virtual void UnbindFromInventoryUpdates(const FOnInventoryUpdated::FDelegate& Delegate) = 0;
    
    //==================================================================
    // Отладка и утилиты
    //==================================================================
    
    /**
     * Преобразует линейный индекс в координаты
     */
    virtual bool GetInventoryCoordinates(int32 Index, int32& OutX, int32& OutY) const = 0;
    
    /**
     * Преобразует координаты в линейный индекс
     */
    virtual int32 GetIndexFromCoordinates(int32 X, int32 Y) const = 0;
    
    /**
     * Вычисляет базовый индекс для предмета
     */
    virtual int32 GetFlatIndexForItem(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const = 0;
    
    /**
     * Получает все занятые слоты предметом
     */
    virtual TArray<int32> GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const = 0;
    
    /**
     * Получает отладочную информацию
     */
    virtual FString GetInventoryDebugInfo() const = 0;
    
    /**
     * Проверяет целостность данных
     */
    virtual bool ValidateInventoryIntegrity(TArray<FString>& OutErrors) const = 0;
    
    //==================================================================
    // Статические вспомогательные методы
    //==================================================================
    
    /**
     * Получает менеджер делегатов из контекста
     */
    static UEventDelegateManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Транслирует добавление предмета
     */
    static void BroadcastItemAdded(
        const UObject* Inventory,
        const FInventoryItemInstance& ItemInstance,
        int32 SlotIndex);
    
    /**
     * Транслирует удаление предмета
     */
    static void BroadcastItemRemoved(
        const UObject* Inventory,
        const FName& ItemID,
        int32 Quantity,
        int32 SlotIndex);
    
    /**
     * Транслирует перемещение предмета
     */
    static void BroadcastItemMoved(
        const UObject* Inventory,
        const FGuid& InstanceID,
        int32 OldSlotIndex,
        int32 NewSlotIndex,
        bool bWasRotated);
    
    /**
     * Транслирует ошибку инвентаря
     */
    static void BroadcastInventoryError(
        const UObject* Inventory,
        EInventoryErrorCode ErrorCode,
        const FString& Context);
    
    /**
     * Транслирует превышение лимита веса
     */
    static void BroadcastWeightLimitExceeded(
        const UObject* Inventory,
        const FInventoryItemInstance& ItemInstance,
        float RequiredWeight,
        float AvailableWeight);
    
    /**
     * Получает unified данные для broadcast событий
     */
    static bool GetUnifiedDataForBroadcast(
        const FInventoryItemInstance& ItemInstance, 
        FMedComUnifiedItemData& OutUnifiedData);
};
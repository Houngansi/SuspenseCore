// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Net/UnrealNetwork.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Types/Inventory/InventoryTypes.h"
#include "MedComItemBase.generated.h"

// Forward declarations
class UMedComItemManager;
struct FMedComUnifiedItemData;

/**
 * Структура для репликации runtime свойств предмета
 * Используется вместо TMap для поддержки сетевой репликации
 */
USTRUCT(BlueprintType)
struct FItemRuntimeProperty
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FName PropertyName;

    UPROPERTY(BlueprintReadWrite)
    float PropertyValue;

    FItemRuntimeProperty()
    {
        PropertyName = NAME_None;
        PropertyValue = 0.0f;
    }

    FItemRuntimeProperty(const FName& InName, float InValue)
        : PropertyName(InName)
        , PropertyValue(InValue)
    {
    }
};

/**
 * Runtime объект предмета в игре
 * 
 * Архитектурная философия:
 * - НЕ является источником статических данных (это роль DataTable)
 * - Хранит ТОЛЬКО runtime состояние и связь с DataTable
 * - Все статические данные получает через ItemManager из DataTable
 * - Оптимизирован для сетевой репликации
 * 
 * Использование:
 * - Создается для каждого экземпляра предмета в мире/инвентаре  
 * - Содержит изменяемые свойства (прочность, патроны, и т.д.)
 * - Ссылается на DataTable по ItemID для получения статики
 */
UCLASS(BlueprintType)
class MEDCOMINVENTORY_API UMedComItemBase : public UObject
{
    GENERATED_BODY()

public:
    UMedComItemBase();

    //==================================================================
    // Связь с источником истины (DataTable)
    //==================================================================
    
    /** 
     * ID предмета для поиска в DataTable
     * Единственная связь со статическими данными предмета
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Item|Core")
    FName ItemID = NAME_None;

    //==================================================================
    // Runtime состояние предмета (изменяемые данные)
    //==================================================================
    
    /** 
     * Текущая прочность предмета
     * Реплицируется для синхронизации между клиентами
     */
    UPROPERTY(BlueprintReadWrite, Replicated, Category = "Item|Runtime")
    float CurrentDurability = 0.0f;
    
    /** 
     * Универсальные runtime свойства (реплицируемый массив)
     * Для патронов, зарядов, модификаторов, и других динамических данных
     */
    UPROPERTY(BlueprintReadWrite, ReplicatedUsing=OnRep_RuntimeProperties, Category = "Item|Runtime")
    TArray<FItemRuntimeProperty> RuntimePropertiesArray;
    
    /** 
     * Время последнего использования предмета
     * Используется для кулдаунов и временных ограничений
     */
    UPROPERTY(BlueprintReadWrite, Replicated, Category = "Item|Runtime")
    float LastUsedTime = 0.0f;
    
    /** 
     * Уникальный ID экземпляра для отслеживания
     * Не реплицируется, генерируется локально на каждом клиенте
     */
    UPROPERTY(BlueprintReadOnly, Category = "Item|Runtime")
    FGuid InstanceID;

    //==================================================================
    // Кэширование для производительности (не реплицируется)
    //==================================================================
    
private:
    /** 
     * Кэшированные данные из DataTable для оптимизации
     * НЕ помечен UPROPERTY - управляется вручную
     * mutable для изменения в const методах
     */
    mutable FMedComUnifiedItemData* CachedItemData = nullptr;
    
    /** 
     * Локальный кэш runtime свойств для быстрого доступа
     * Синхронизируется с RuntimePropertiesArray
     */
    mutable TMap<FName, float> RuntimePropertiesCache;
    
    /** Время последнего обновления кэша */
    mutable float LastCacheUpdateTime = 0.0f;
    
    /** Интервал обновления кэша в секундах */
    static constexpr float CACHE_UPDATE_INTERVAL = 1.0f;

public:
    //==================================================================
    // Методы доступа к статическим данным (из DataTable)
    //==================================================================
    
    /** 
     * Получить статические данные предмета из DataTable (C++ версия)
     * Использует кэширование для производительности
     * @return Указатель на данные или nullptr
     */
    const FMedComUnifiedItemData* GetItemData() const;
    
    /** 
     * Получить копию статических данных предмета (Blueprint версия)
     * @param OutItemData Выходные данные предмета
     * @return True если данные найдены
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Data", DisplayName = "Get Item Data")
    bool GetItemDataCopy(FMedComUnifiedItemData& OutItemData) const;
    
    /** 
     * Принудительно обновить кэшированные данные из DataTable
     * Вызывать при изменении ItemID или при необходимости обновления
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Data")
    void RefreshItemData();
    
    /** 
     * Проверить, загружены ли данные предмета из DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Data")
    bool HasValidItemData() const;

    //==================================================================
    // Convenience методы для доступа к статическим свойствам
    //==================================================================
    
    /** Получить отображаемое имя предмета из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    FText GetItemName() const;
    
    /** Получить описание предмета из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")  
    FText GetItemDescription() const;
    
    /** Получить иконку предмета из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    UTexture2D* GetItemIcon() const;
    
    /** Получить тип предмета из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    FGameplayTag GetItemType() const;
    
    /** Получить размер в сетке из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    FIntPoint GetGridSize() const;
    
    /** Получить максимальный размер стека из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    int32 GetMaxStackSize() const;
    
    /** Получить вес предмета из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    float GetWeight() const;
    
    /** Получить базовую стоимость из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    int32 GetBaseValue() const;
    
    /** Проверить, экипируемый ли предмет (из DataTable) */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsEquippable() const;
    
    /** Получить тип слота экипировки из DataTable */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    FGameplayTag GetEquipmentSlot() const;
    
    /** Проверить, расходуемый ли предмет (из DataTable) */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsConsumable() const;
    
    /** Проверить, можно ли выбросить предмет (из DataTable) */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsDroppable() const;
    
    /** Проверить, можно ли торговать предметом (из DataTable) */
    UFUNCTION(BlueprintCallable, Category = "Item|Properties")
    bool IsTradeable() const;

    //==================================================================
    // Методы работы с runtime состоянием
    //==================================================================
    
    /** 
     * Получить значение runtime свойства
     * @param PropertyName Имя свойства
     * @param DefaultValue Значение по умолчанию если свойство не найдено
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Runtime")
    float GetRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const;
    
    /** 
     * Установить значение runtime свойства
     * @param PropertyName Имя свойства  
     * @param Value Новое значение
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Runtime")
    void SetRuntimeProperty(const FName& PropertyName, float Value);
    
    /** 
     * Удалить runtime свойство
     * @param PropertyName Имя свойства для удаления
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Runtime")
    void RemoveRuntimeProperty(const FName& PropertyName);
    
    /** 
     * Проверить наличие runtime свойства
     * @param PropertyName Имя свойства
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Runtime")
    bool HasRuntimeProperty(const FName& PropertyName) const;
    
    /** 
     * Получить все runtime свойства как массив (для Blueprint)
     * @return Массив всех runtime свойств
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Runtime")
    TArray<FItemRuntimeProperty> GetAllRuntimeProperties() const;

    //==================================================================
    // Методы работы с прочностью
    //==================================================================
    
    /** Получить максимальную прочность из DataTable или AttributeSet */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float GetMaxDurability() const;
    
    /** Проверить, имеет ли предмет систему прочности */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    bool HasDurability() const;
    
    /** Получить процент оставшейся прочности (0.0 - 1.0) */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float GetDurabilityPercent() const;
    
    /** 
     * Установить текущую прочность с автоматическим клампингом
     * @param NewDurability Новое значение прочности
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    void SetCurrentDurability(float NewDurability);
    
    /** 
     * Уменьшить прочность на указанное значение
     * @param Damage Количество урона прочности
     * @return Новое значение прочности
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float DamageDurability(float Damage);
    
    /** 
     * Восстановить прочность на указанное значение
     * @param RepairAmount Количество восстанавливаемой прочности
     * @return Новое значение прочности
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Durability")
    float RepairDurability(float RepairAmount);

    //==================================================================
    // Специализированные методы для оружия
    //==================================================================
    
    /** Получить текущие патроны в оружии */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    int32 GetCurrentAmmo() const;
    
    /** Установить текущие патроны с клампингом к максимуму */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    void SetCurrentAmmo(int32 AmmoCount);
    
    /** Получить максимальные патроны из AmmoAttributeSet */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    int32 GetMaxAmmo() const;
    
    /** Проверить, есть ли патроны в оружии */
    UFUNCTION(BlueprintCallable, Category = "Item|Weapon")
    bool HasAmmo() const;

    //==================================================================
    // Методы кулдаунов
    //==================================================================
    
    /** 
     * Проверить, есть ли активный кулдаун у предмета
     * @param CurrentTime Текущее время для сравнения
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Cooldown")
    bool IsOnCooldown(float CurrentTime) const;
    
    /** 
     * Запустить кулдаун предмета
     * @param CurrentTime Текущее время
     * @param CooldownDuration Длительность кулдауна в секундах
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Cooldown")
    void StartCooldown(float CurrentTime, float CooldownDuration);
    
    /** 
     * Получить оставшееся время кулдауна
     * @param CurrentTime Текущее время
     * @return Оставшееся время в секундах
     */
    UFUNCTION(BlueprintCallable, Category = "Item|Cooldown")
    float GetRemainingCooldown(float CurrentTime) const;

    //==================================================================
    // Основные методы предмета
    //==================================================================
    
    /** 
     * Использование предмета персонажем
     * Получает данные использования из DataTable
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Item")
    void UseItem(ACharacter* Character);
    virtual void UseItem_Implementation(ACharacter* Character);

    /** 
     * Проверка валидности предмета
     * Проверяет наличие ItemID и соответствующих данных в DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    bool IsValid() const;
    
    /** 
     * Получить отладочную строку с информацией о предмете
     * Комбинирует runtime и статические данные
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    FString GetDebugString() const;

    //==================================================================
    // Конверсия и совместимость
    //==================================================================
    
    /** 
     * Создать экземпляр инвентаря из этого предмета
     * Для интеграции с новой системой инвентаря
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    FInventoryItemInstance ToInventoryInstance(int32 Quantity = 1) const;
    
    /** 
     * Инициализировать предмет из экземпляра инвентаря
     * Для обратной конверсии из новой системы
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    void InitFromInventoryInstance(const FInventoryItemInstance& Instance);

    //==================================================================
    // Методы инициализации и настройки
    //==================================================================
    
    /** 
     * Инициализировать предмет с указанным ItemID
     * Автоматически настраивает runtime свойства из DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    void Initialize(const FName& InItemID);
    
    /** 
     * Сбросить все runtime данные к значениям по умолчанию
     * Полезно для переиспользования объектов из пула
     */
    UFUNCTION(BlueprintCallable, Category = "Item")
    void ResetToDefaults();

    //==================================================================
    // Сетевая репликация
    //==================================================================
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool IsSupportedForNetworking() const override { return true; }
    
    /** Обработчик репликации runtime свойств */
    UFUNCTION()
    void OnRep_RuntimeProperties();

protected:
    //==================================================================
    // Внутренние методы
    //==================================================================
    
    /** Получить ItemManager для доступа к DataTable */
    UMedComItemManager* GetItemManager() const;
    
    /** Внутренний метод обновления кэша */
    void UpdateCacheIfNeeded() const;
    
    /** Инициализировать runtime свойства из DataTable */
    void InitializeRuntimePropertiesFromData();
    
    /** Синхронизировать кэш с массивом свойств */
    void SyncPropertiesCacheWithArray() const;
    
    /** Синхронизировать массив с кэшем свойств */
    void SyncPropertiesArrayWithCache();

    //==================================================================
    // Lifecycle методы
    //==================================================================
    
    virtual void PostInitProperties() override;
    virtual void BeginDestroy() override;

#if WITH_EDITOR
    /** Обработка изменений в редакторе */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
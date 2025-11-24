// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Operations/InventoryResult.h"
#include "Types/Loadout/LoadoutSettings.h"
#include "IMedComEquipmentInterface.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UEventDelegateManager;
class UGameplayAbility;
class UGameplayEffect;
struct FGameplayAttribute;

/**
 * UInterface - это "паспорт" для нашего интерфейса в системе Unreal Engine
 * Он необходим для интеграции с reflection и Blueprint системами
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UMedComEquipmentInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Базовый интерфейс экипировки для всех экипируемых предметов
 * 
 * АРХИТЕКТУРНАЯ ФИЛОСОФИЯ:
 * Этот интерфейс служит "договором" между системой экипировки и конкретными
 * реализациями экипировки. Он определяет "что должно быть сделано", но не
 * диктует "как это должно быть сделано".
 * 
 * ПРИНЦИП РАЗДЕЛЕНИЯ ОТВЕТСТВЕННОСТИ:
 * - Общая функциональность экипировки находится здесь
 * - Оружейная специфика в IMedComWeaponInterface
 * - Броня и защита в специализированных интерфейсах
 * 
 * Это позволяет каждому интерфейсу быть экспертом в своей области,
 * не перегружаясь чужими обязанностями.
 */
class MEDCOMSHARED_API IMedComEquipmentInterface
{
    GENERATED_BODY()

public:
    //==================================================================
    // Управление жизненным циклом экипировки
    //==================================================================

    /**
     * Вызывается при экипировке предмета на владельца
     * 
     * Этот метод служит "точкой входа" в жизненный цикл экипированного предмета.
     * Здесь происходит инициализация состояния, применение пассивных эффектов,
     * и регистрация в различных системах игры.
     * 
     * @param NewOwner Актор, который экипирует этот предмет
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Lifecycle")
    void OnEquipped(AActor* NewOwner);

    /**
     * Вызывается при снятии экипировки с владельца
     * 
     * "Точка выхода" из жизненного цикла - здесь происходит очистка состояния,
     * удаление эффектов, и отписка от систем.
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Lifecycle")
    void OnUnequipped();

    /**
     * Вызывается при экипировке конкретного runtime экземпляра
     * 
     * Этот метод получает полную информацию о конкретном экземпляре предмета,
     * включая его уникальные свойства (прочность, модификации, enchantments).
     * 
     * @param ItemInstance Runtime экземпляр экипированного предмета
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Lifecycle")
    void OnItemInstanceEquipped(const FInventoryItemInstance& ItemInstance);

    /**
     * Вызывается при снятии конкретного runtime экземпляра
     * 
     * @param ItemInstance Runtime экземпляр снимаемого предмета
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Lifecycle")
    void OnItemInstanceUnequipped(const FInventoryItemInstance& ItemInstance);

    //==================================================================
    // Свойства и конфигурация экипировки
    //==================================================================

    /**
     * Получает текущий экипированный экземпляр предмета
     * 
     * Это как "инвентарная карточка" текущего предмета - содержит всю
     * информацию о его текущем состоянии и свойствах.
     * 
     * @return Текущий экипированный экземпляр или невалидный если слот пуст
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    FInventoryItemInstance GetEquippedItemInstance() const;

    /**
     * Получает копию конфигурации слота для Blueprint/C++
     * 
     * Blueprint-безопасная версия, которая создает копию для предотвращения
     * случайных модификаций оригинальных данных.
     * 
     * @return Копия конфигурации слота экипировки
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    FEquipmentSlotConfig GetSlotConfiguration() const;

    /**
     * Получает конфигурацию слота экипировки из LoadoutSettings (виртуальный метод для C++)
     * 
     * Этот метод предоставляет прямой доступ к конфигурации слота без создания копии.
     * Используется внутренними системами для высокопроизводительного доступа.
     * 
     * @return Указатель на конфигурацию или nullptr если не найдена
     */
    virtual const FEquipmentSlotConfig* GetSlotConfigurationPtr() const { return nullptr; }

    /**
     * Получает тип слота экипировки из конфигурации
     * 
     * @return Тип слота экипировки (оружие, броня, аксессуар и т.д.)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    EEquipmentSlotType GetEquipmentSlotType() const;

    /**
     * Получает gameplay tag слота для совместимости
     * 
     * Конвертирует enum в GameplayTag для интеграции с системами,
     * которые работают с тегами вместо enum.
     * 
     * @return Gameplay tag, представляющий тип слота
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    FGameplayTag GetEquipmentSlotTag() const;

    /**
     * Проверяет, есть ли в данный момент экипированный предмет
     * 
     * @return True если слот занят
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    bool IsEquipped() const;

    /**
     * Проверяет, является ли этот слот обязательным для валидного loadout
     * 
     * Некоторые слоты (например, основное оружие) могут быть обязательными
     * для определенных классов персонажей или ролей.
     * 
     * @return True если это обязательный слот экипировки
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    bool IsRequiredSlot() const;

    /**
     * Получает отображаемое имя для этого слота экипировки
     * 
     * @return Локализованное отображаемое имя
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    FText GetSlotDisplayName() const;
    
    /**
     * Получает имя socket для прикрепления экипировки
     * 
     * @return Имя socket или NAME_None
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    FName GetAttachmentSocket() const;
    
    /**
     * Получает offset transform для прикрепления
     * 
     * @return Transform offset для attachment
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    FTransform GetAttachmentOffset() const;

    //==================================================================
    // Совместимость предметов и валидация
    //==================================================================

    /**
     * Проверяет, может ли экземпляр предмета быть экипирован в этот слот
     * 
     * Выполняет comprehensive проверку совместимости, включая тип предмета,
     * требования к уровню, ограничения класса персонажа.
     * 
     * @param ItemInstance Экземпляр предмета для проверки совместимости
     * @return True если предмет может быть экипирован в этот слот
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Validation")
    bool CanEquipItemInstance(const FInventoryItemInstance& ItemInstance) const;

    /**
     * Получает разрешенные типы предметов для этого слота
     * 
     * Это как "дресс-код" слота - какие типы предметов здесь уместны.
     * 
     * @return Контейнер разрешенных тегов типов предметов
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Validation")
    FGameplayTagContainer GetAllowedItemTypes() const;

    /**
     * Валидирует требования экипировки для loadout
     * 
     * @param OutErrors Массив для получения сообщений об ошибках валидации
     * @return True если экипировка соответствует всем требованиям
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Validation")
    bool ValidateEquipmentRequirements(TArray<FString>& OutErrors) const;

    //==================================================================
    // Операции с экипировкой
    //==================================================================

    /**
     * Экипирует экземпляр предмета в этот слот
     * 
     * @param ItemInstance Экземпляр предмета для экипировки
     * @param bForceEquip Принудительная экипировка даже при неудачной валидации
     * @return Результат операции экипировки
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Operations")
    FInventoryOperationResult EquipItemInstance(const FInventoryItemInstance& ItemInstance, bool bForceEquip = false);

    /**
     * Снимает текущий предмет с этого слота
     * 
     * @param OutUnequippedInstance Экземпляр предмета, который был снят
     * @return Результат операции снятия
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Operations")
    FInventoryOperationResult UnequipItem(FInventoryItemInstance& OutUnequippedInstance);

    /**
     * Обменивает предметы между этим слотом и другим слотом экипировки
     * 
     * @param OtherEquipment Другой слот экипировки для обмена
     * @return Результат операции обмена
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Operations")
    FInventoryOperationResult SwapEquipmentWith(const TScriptInterface<IMedComEquipmentInterface>& OtherEquipment);

    //==================================================================
    // Интеграция с Gameplay Ability System
    //==================================================================

    /**
     * Получает Ability System Component для этой экипировки
     * 
     * Необходим для применения abilities и effects, предоставляемых экипировкой.
     * 
     * @return Ссылка на ASC или nullptr если недоступен
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|AbilitySystem")
    UAbilitySystemComponent* GetAbilitySystemComponent() const;

    /**
     * Получает AttributeSet для статистик экипировки
     * 
     * @return AttributeSet экипировки или nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|AbilitySystem")
    UAttributeSet* GetEquipmentAttributeSet() const;

    /**
     * Получает abilities, предоставляемые текущим экипированным предметом
     * 
     * @return Массив классов предоставляемых abilities
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|AbilitySystem")
    TArray<TSubclassOf<UGameplayAbility>> GetGrantedAbilities() const;

    /**
     * Получает пассивные эффекты, применяемые текущим экипированным предметом
     * 
     * @return Массив классов пассивных эффектов
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|AbilitySystem")
    TArray<TSubclassOf<UGameplayEffect>> GetPassiveEffects() const;

    /**
     * Применяет abilities и эффекты экипировки
     * 
     * Вызывается автоматически при экипировке предмета.
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|AbilitySystem")
    void ApplyEquipmentEffects();

    /**
     * Удаляет abilities и эффекты экипировки
     * 
     * Вызывается автоматически при снятии предмета.
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|AbilitySystem")
    void RemoveEquipmentEffects();

    //==================================================================
    // Управление состоянием экипировки
    //==================================================================

    /**
     * Получает текущее состояние экипировки
     * 
     * @return Текущий тег состояния (например, Equipment.State.Idle, Equipment.State.Active)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|State")
    FGameplayTag GetCurrentEquipmentState() const;

    /**
     * Устанавливает состояние экипировки с валидацией
     * 
     * @param NewState Новое состояние для перехода
     * @param bForceTransition Принудительный переход даже при невалидности
     * @return True если переход состояния был успешным
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|State")
    bool SetEquipmentState(const FGameplayTag& NewState, bool bForceTransition = false);

    /**
     * Проверяет, находится ли экипировка в определенном состоянии
     * 
     * @param StateTag Тег состояния для проверки
     * @return True если экипировка находится в указанном состоянии
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|State")
    bool IsInEquipmentState(const FGameplayTag& StateTag) const;

    /**
     * Получает доступные переходы состояний из текущего состояния
     * 
     * @return Массив тегов состояний, в которые можно перейти
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|State")
    TArray<FGameplayTag> GetAvailableStateTransitions() const;

    //==================================================================
    // Метрики экипировки и runtime свойства
    //==================================================================

    /**
     * Получает runtime свойство из экипированного предмета
     * 
     * Доступ к динамическим свойствам предмета (прочность, заряд и т.д.)
     * 
     * @param PropertyName Имя runtime свойства
     * @param DefaultValue Значение по умолчанию если свойство не найдено
     * @return Текущее значение свойства
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    float GetEquipmentRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const;

    /**
     * Устанавливает runtime свойство экипированного предмета
     * 
     * @param PropertyName Имя runtime свойства
     * @param Value Новое значение свойства
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    void SetEquipmentRuntimeProperty(const FName& PropertyName, float Value);

    /**
     * Получает процент состояния/прочности экипировки
     * 
     * @return Состояние от 0.0 до 1.0, или 1.0 если нет системы прочности
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Properties")
    float GetEquipmentConditionPercent() const;

    //==================================================================
    // Определение типа оружия
    //==================================================================

    /**
     * Проверяет, является ли эта экипировка оружием
     * 
     * Используется для определения, следует ли использовать оружейный интерфейс.
     * 
     * @return True если экипированный предмет является оружием
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Type")
    bool IsWeaponEquipment() const;

    /**
     * Получает тег архетипа оружия, если это оружие
     * 
     * @return Тег архетипа оружия или пустой если не оружие
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Type")
    FGameplayTag GetWeaponArchetype() const;

    /**
     * Проверяет, может ли оружие стрелять (базовая проверка)
     * 
     * Для детальной функциональности оружия используйте IMedComWeaponInterface.
     * 
     * @return True если оружие готово к стрельбе
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Type")
    bool CanFireWeapon() const;

 //==================================================================
 // Weapon Switching Support
 //==================================================================
    
 /**
  * Gets the currently active weapon slot index
  * @return Active slot index or INDEX_NONE if no weapon is active
  */
 UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Weapon")
 int32 GetActiveWeaponSlotIndex() const;
    
 /**
  * Switches to a specific equipment slot
  * @param SlotIndex Target slot index
  * @return True if switch was initiated successfully
  */
 UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Weapon")
 bool SwitchToSlot(int32 SlotIndex);
    
 /**
  * Gets all weapon slot indices in priority order
  * @return Array of slot indices sorted by priority
  */
 UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Weapon")
 TArray<int32> GetWeaponSlotsByPriority() const;
    
 /**
  * Gets the previous active weapon slot for quick switching
  * @return Previous slot index or INDEX_NONE
  */
 UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Weapon")
 int32 GetPreviousWeaponSlot() const;
    
 /**
  * Checks if a specific slot contains a weapon
  * @param SlotIndex Slot to check
  * @return True if the slot contains a weapon
  */
 UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Weapon")
 bool IsSlotWeapon(int32 SlotIndex) const;
    
 /**
  * Gets the total number of equipment slots
  * @return Number of slots
  */
 UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Weapon")
 int32 GetTotalSlotCount() const;

 
//==================================================================
    // Weapon Slot Management Methods
    //==================================================================
    
    /**
     * Получает индекс текущего активного слота оружия
     * 
     * @return Индекс активного слота или INDEX_NONE если нет активного оружия
     */
    //UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    //int32 GetActiveWeaponSlotIndex() const;
    
    /**
     * Переключается на указанный слот оружия
     * 
     * @param TargetSlotIndex Индекс целевого слота
     * @param bForceSwitch Принудительное переключение даже если слот пуст
     * @return True если переключение началось успешно
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    bool SwitchToWeaponSlot(int32 TargetSlotIndex, bool bForceSwitch = false);
    
    /**
     * Получает общее количество слотов для оружия
     * 
     * @return Количество доступных слотов для оружия
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    int32 GetWeaponSlotCount() const;
    
    /**
     * Проверяет, является ли указанный слот оружейным
     * 
     * @param SlotIndex Индекс слота для проверки
     * @return True если это слот для оружия
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    bool IsSlotWeaponSlot(int32 SlotIndex) const;
    
    /**
     * Получает экземпляр предмета в указанном слоте
     * 
     * @param SlotIndex Индекс слота
     * @return Экземпляр предмета или невалидный если слот пуст
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    FInventoryItemInstance GetItemInSlot(int32 SlotIndex) const;
    
    /**
     * Получает индексы всех слотов, содержащих оружие
     * 
     * @return Массив индексов слотов с оружием
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    TArray<int32> GetOccupiedWeaponSlots() const;
    
    /**
     * Получает последний активный слот оружия (для quick switch)
     * 
     * @return Индекс последнего активного слота или INDEX_NONE
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    int32 GetLastActiveWeaponSlot() const;
    
    /**
     * Устанавливает последний активный слот (для отслеживания quick switch)
     * 
     * @param SlotIndex Индекс слота
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|WeaponSlots")
    void SetLastActiveWeaponSlot(int32 SlotIndex);

 
    //==================================================================
    // Доступ к централизованной системе событий
    //==================================================================
    
    /**
     * Получает центральный менеджер делегатов для этой экипировки
     * 
     * Критически важен для межмодульной коммуникации событий.
     * 
     * @return Указатель на менеджер делегатов или nullptr если недоступен
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|EventSystem")
    UEventDelegateManager* GetDelegateManager() const;

    /**
     * Получает центральный менеджер делегатов (виртуальный метод для C++)
     * 
     * Высокопроизводительный доступ без overhead Blueprint системы.
     * 
     * @return Указатель на менеджер делегатов или nullptr если недоступен
     */
    virtual UEventDelegateManager* GetDelegateManagerPtr() const { return nullptr; }
    
    /**
     * Статический вспомогательный метод для получения менеджера делегатов из любого world context
     * 
     * @param WorldContextObject Любой объект с валидным world context
     * @return Менеджер делегатов или nullptr
     */
    static UEventDelegateManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Вспомогательный метод для безопасной передачи изменений состояния экипировки
     * 
     * @param Equipment Объект экипировки, передающий событие
     * @param OldState Предыдущее состояние
     * @param NewState Новое состояние
     * @param bInterrupted Было ли изменение состояния прервано
     */
    static void BroadcastEquipmentStateChanged(
        const UObject* Equipment,
        const FGameplayTag& OldState, 
        const FGameplayTag& NewState, 
        bool bInterrupted = false);
    
    /**
     * Вспомогательный метод для безопасной передачи событий операций экипировки
     * 
     * @param Equipment Объект экипировки
     * @param EventTag Тег типа события
     * @param ItemInstance Связанный экземпляр предмета (опционально)
     * @param EventData Дополнительные данные события
     */
    static void BroadcastEquipmentOperationEvent(
        const UObject* Equipment,
        const FGameplayTag& EventTag,
        const FInventoryItemInstance* ItemInstance = nullptr,
        const FString& EventData = TEXT(""));

    /**
     * Вспомогательный метод для передачи изменений runtime свойств экипировки
     * 
     * @param Equipment Объект экипировки
     * @param PropertyName Имя изменившегося свойства
     * @param OldValue Предыдущее значение
     * @param NewValue Новое значение
     */
    static void BroadcastEquipmentPropertyChanged(
        const UObject* Equipment,
        const FName& PropertyName,
        float OldValue,
        float NewValue);

    //==================================================================
    // Поддержка отладки и разработки
    //==================================================================

    /**
     * Получает детальную отладочную информацию о состоянии экипировки
     * 
     * @return Детальная отладочная строка
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Debug")
    FString GetEquipmentDebugInfo() const;

    /**
     * Валидирует целостность экипировки
     * 
     * @param OutErrors Массив для получения ошибок валидации
     * @return True если экипировка в валидном состоянии
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Equipment|Debug")
    bool ValidateEquipmentIntegrity(TArray<FString>& OutErrors) const;
};